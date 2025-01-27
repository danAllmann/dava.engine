#include "UI/RichContent/Private/XMLRichContentBuilder.h"

#include "Logger/Logger.h"
#include "UI/DefaultUIPackageBuilder.h"
#include "UI/Layouts/UIFlowLayoutHintComponent.h"
#include "UI/Layouts/UILayoutSourceRectComponent.h"
#include "UI/Layouts/UISizePolicyComponent.h"
#include "UI/Render/UIDebugRenderComponent.h"
#include "UI/RichContent/Private/RichContentUIPackageBuilder.h"
#include "UI/RichContent/Private/RichLink.h"
#include "UI/RichContent/UIRichAliasMap.h"
#include "UI/RichContent/UIRichContentAliasesComponent.h"
#include "UI/RichContent/UIRichContentComponent.h"
#include "UI/RichContent/UIRichContentObjectComponent.h"
#include "UI/UIControl.h"
#include "UI/UIPackageLoader.h"
#include "UI/UIStaticText.h"
#include "Utils/StringUtils.h"
#include "Utils/UTF8Utils.h"
#include "Utils/UTF8Walker.h"
#include "Utils/Utils.h"

namespace DAVA
{
XMLRichContentBuilder::XMLRichContentBuilder(RichLink* link_, bool editorMode /*= false*/, bool debugDraw /*= false*/)
    : link(link_)
    , isEditorMode(editorMode)
    , isDebugDraw(debugDraw)
{
    DVASSERT(link);
    defaultClasses = link->component->GetBaseClasses();
    classesInheritance = link->component->GetClassesInheritance();

    PutClass(defaultClasses);
}

bool XMLRichContentBuilder::Build(const String& text)
{
    controls.clear();
    direction = bidiHelper.GetDirectionUTF8String(text); // Detect text direction
    RefPtr<XMLParser> parser(new XMLParser());
    return parser->ParseBytes(reinterpret_cast<const unsigned char*>(text.c_str()), static_cast<int32>(text.length()), this);
}

const Vector<RefPtr<UIControl>>& XMLRichContentBuilder::GetControls() const
{
    return controls;
}

void XMLRichContentBuilder::PutClass(const String& clazz)
{
    String compositeClass;
    if (classesInheritance)
    {
        compositeClass = GetClass();
        if (!clazz.empty())
        {
            compositeClass += " ";
        }
    }
    compositeClass += clazz;

    classesStack.push_back(compositeClass);
}

void XMLRichContentBuilder::PopClass()
{
    classesStack.pop_back();
}

const String& XMLRichContentBuilder::GetClass() const
{
    if (classesStack.empty())
    {
        static const String EMPTY;
        return EMPTY;
    }
    return classesStack.back();
}

void XMLRichContentBuilder::PrepareControl(UIControl* ctrl, bool autosize)
{
    ctrl->SetClassesFromString(ctrl->GetClassesAsString() + " " + GetClass());

    if (isEditorMode)
    {
        UILayoutSourceRectComponent* src = ctrl->GetOrCreateComponent<UILayoutSourceRectComponent>();
        src->SetSize(ctrl->GetSize());
        src->SetPosition(ctrl->GetPosition());
    }

    if (autosize)
    {
        UISizePolicyComponent* sp = ctrl->GetOrCreateComponent<UISizePolicyComponent>();
        sp->SetHorizontalPolicy(UISizePolicyComponent::eSizePolicy::PERCENT_OF_CONTENT);
        sp->SetVerticalPolicy(UISizePolicyComponent::eSizePolicy::PERCENT_OF_CONTENT);
    }

    UIFlowLayoutHintComponent* flh = ctrl->GetOrCreateComponent<UIFlowLayoutHintComponent>();
    flh->SetContentDirection(direction);
    if (needLineBreak)
    {
        flh->SetNewLineBeforeThis(true);
    }
    else if (!needSpace)
    {
        flh->SetStickItemBeforeThis(true);
        if (!needSoftStick)
        {
            flh->SetStickHardBeforeThis(true);
        }
    }

    if (isDebugDraw)
    {
        UIDebugRenderComponent* debug = ctrl->GetOrCreateComponent<UIDebugRenderComponent>();
        debug->SetEnabled(true);
        if (needLineBreak)
        {
            debug->SetDrawColor(Color::Yellow);
        }
        else if (!needSpace)
        {
            if (needSoftStick)
            {
                debug->SetDrawColor(Color::Cyan);
            }
            else
            {
                debug->SetDrawColor(Color::Blue);
            }
        }
        else
        {
            debug->SetDrawColor(Color::Magenta);
        }
    }

    needSpace = false;
    needLineBreak = false;
    needSoftStick = false;
}

void XMLRichContentBuilder::AppendControl(UIControl* ctrl)
{
    controls.emplace_back(SafeRetain(ctrl));
}

void XMLRichContentBuilder::OnElementStarted(const String& elementName, const String& namespaceURI, const String& qualifedName, const Map<String, String>& attributes)
{
    for (UIRichContentAliasesComponent* c : link->aliasesComponents)
    {
        const UIRichAliasMap& aliases = c->GetAliases();
        const UIRichAliasMap::Alias& alias = aliases.GetAlias(elementName);
        if (!alias.tag.empty())
        {
            ProcessTagBegin(alias.tag, alias.attributes);
            return;
        }
    }
    ProcessTagBegin(elementName, attributes);
}

void XMLRichContentBuilder::OnElementEnded(const String& elementName, const String& namespaceURI, const String& qualifedName)
{
    for (UIRichContentAliasesComponent* c : link->aliasesComponents)
    {
        const UIRichAliasMap& aliases = c->GetAliases();
        const UIRichAliasMap::Alias& alias = aliases.GetAlias(elementName);
        if (!alias.tag.empty())
        {
            ProcessTagEnd(alias.tag);
            return;
        }
    }
    ProcessTagEnd(elementName);
}

void XMLRichContentBuilder::OnFoundCharacters(const String& chars)
{
    // Concat input characters in one single string, because some kind
    // of intarnalization characters break XML parser to few
    // OnFoundCharacters callbacks per one word
    fullText += chars;
}

void XMLRichContentBuilder::ProcessTagBegin(const String& tag, const Map<String, String>& attributes)
{
    FlushText();

    // Global attributes
    String classes;
    if (!GetAttribute(attributes, "class", classes) && !classesInheritance)
    {
        classes = defaultClasses;
    }
    PutClass(classes);

    // Tag
    if (tag == "p")
    {
        needLineBreak = true;
    }
    else if (tag == "br")
    {
        needLineBreak = true;
    }
    else if (tag == "ul")
    {
        needLineBreak = true;
    }
    else if (tag == "li")
    {
        needLineBreak = true;
    }
    else if (tag == "img")
    {
        String src;
        if (GetAttribute(attributes, "src", src))
        {
            RefPtr<UIControl> img(new UIControl());
            PrepareControl(img.Get(), true);
            UIControlBackground* bg = img->GetOrCreateComponent<UIControlBackground>();
            bg->SetDrawType(UIControlBackground::DRAW_STRETCH_BOTH);
            bg->SetSprite(FilePath(src));
            AppendControl(img.Get());
        }
    }
    else if (tag == "object")
    {
        String path;
        GetAttribute(attributes, "path", path);
        String controlName;
        GetAttribute(attributes, "control", controlName);
        String prototypeName;
        GetAttribute(attributes, "prototype", prototypeName);
        String name;
        GetAttribute(attributes, "name", name);

        if (!path.empty() && (!controlName.empty() || !prototypeName.empty()))
        {
            // Check that we not load self as rich object
            bool valid = true;
            {
                UIControl* ctrl = link->control;
                while (ctrl != nullptr)
                {
                    UIRichContentObjectComponent* objComp = ctrl->GetComponent<UIRichContentObjectComponent>();
                    if (objComp)
                    {
                        if (path == objComp->GetPackagePath() &&
                            controlName == objComp->GetControlName() &&
                            prototypeName == objComp->GetPrototypeName())
                        {
                            valid = false;
                            break;
                        }
                    }
                    ctrl = ctrl->GetParent();
                }
            }

            if (valid)
            {
                std::unique_ptr<DefaultUIPackageBuilder> pkgBuilder = isEditorMode ? std::make_unique<RichContentUIPackageBuilder>() : std::make_unique<DefaultUIPackageBuilder>();
                UIPackageLoader().LoadPackage(path, pkgBuilder.get());
                UIControl* obj = nullptr;
                UIPackage* pkg = pkgBuilder->GetPackage();
                if (pkg != nullptr)
                {
                    if (!controlName.empty())
                    {
                        obj = pkg->GetControl(controlName);
                    }
                    else if (!prototypeName.empty())
                    {
                        obj = pkg->GetPrototype(prototypeName);
                    }
                }
                if (obj != nullptr)
                {
                    if (!name.empty())
                    {
                        obj->SetName(name);
                    }

                    PrepareControl(obj, false);

                    UIRichContentObjectComponent* objComp = obj->GetOrCreateComponent<UIRichContentObjectComponent>();
                    objComp->SetPackagePath(path);
                    objComp->SetControlName(controlName);
                    objComp->SetPrototypeName(prototypeName);

                    link->component->onCreateObject.Emit(obj);
                    AppendControl(obj);
                }
            }
            else
            {
                Logger::Error("Recursive object in rich content from '%s' with name '%s'!",
                              path.c_str(),
                              controlName.empty() ? prototypeName.c_str() : controlName.c_str());
            }
        }
    }
}

void XMLRichContentBuilder::ProcessTagEnd(const String& tag)
{
    FlushText();

    PopClass();

    if (tag == "p")
    {
        needLineBreak = true;
    }
    else if (tag == "ul")
    {
        needLineBreak = true;
    }
    else if (tag == "li")
    {
        needLineBreak = true;
    }
}

void XMLRichContentBuilder::ProcessText(const String& text)
{
    const static String LTR_MARK = UTF8Utils::EncodeToUTF8(L"\u200E");
    const static String RTL_MARK = UTF8Utils::EncodeToUTF8(L"\u200F");
    const static uint32 ZERO_WIDTH_SPACE = 0x200B;
    const static uint32 NO_BREAK_SPACE = 0xA0;
    const static uint32 NEW_LINE = 0x0A;

    UTF8Walker walker(text);
    String token;
    while (walker.Next())
    {
        token += walker.GetUtf8Character();

        StringUtils::eLineBreakType br = walker.GetLineBreak();

        bool allowBreak = br == StringUtils::LB_ALLOWBREAK || (walker.IsWhitespace() && walker.GetUnicodeCodepoint() != NO_BREAK_SPACE);
        if (!allowBreak && br == StringUtils::LB_NOBREAK && walker.HasNext())
        {
            continue;
        }

        token = StringUtils::Trim(token);
        if (!token.empty())
        {
            BiDiHelper::Direction wordDirection = bidiHelper.GetDirectionUTF8String(token);
            if (wordDirection == BiDiHelper::Direction::NEUTRAL)
            {
                if (direction == BiDiHelper::Direction::RTL)
                {
                    token = RTL_MARK + token;
                }
                else if (direction == BiDiHelper::Direction::LTR)
                {
                    token = LTR_MARK + token;
                }
            }
            else
            {
                direction = wordDirection;
            }

            RefPtr<UIStaticText> ctrl(new UIStaticText());
            PrepareControl(ctrl.Get(), true);
            ctrl->SetUtf8Text(token);
            AppendControl(ctrl.Get());

            token.clear();
        }

        needLineBreak |= br == StringUtils::LB_MUSTBREAK || walker.GetUnicodeCodepoint() == NEW_LINE;
        needSoftStick |= allowBreak;
        needSpace |= walker.IsWhitespace() && walker.GetUnicodeCodepoint() != ZERO_WIDTH_SPACE;
    }
}

void XMLRichContentBuilder::FlushText()
{
    if (!fullText.empty())
    {
        ProcessText(fullText);
        fullText.clear();
    }
}
}
