#include "Utils/Dump/SceneDumper.h"
#include "Utils/SceneUtils/SceneUtils.h"
#include "Classes/Project/ProjectManagerData.h"

#include "FileSystem/KeyedArchive.h"
#include "Render/2D/Sprite.h"
#include "Render/Highlevel/RenderObject.h"
#include "Render/Highlevel/Landscape.h"
#include "Render/Highlevel/Vegetation/VegetationRenderObject.h"
#include "Render/TextureDescriptor.h"
#include "Render/Material/NMaterial.h"
#include "Scene3D/Scene.h"
#include "Scene3D/Components/ComponentHelpers.h"
#include "Scene3D/Components/ParticleEffectComponent.h"
#include "Scene3D/Components/SlotComponent.h"
#include "Scene3D/Systems/SlotSystem.h"

#include "Main/QtUtils.h"

#include "StringConstants.h"

DAVA::Set<DAVA::FilePath> SceneDumper::DumpLinks(const DAVA::FilePath& scenePath, SceneDumper::eMode mode, const DAVA::Vector<DAVA::eGPUFamily>& compressedGPUs)
{
    DAVA::Set<DAVA::FilePath> dumpedLinks;
    return DumpLinks(scenePath, mode, compressedGPUs, dumpedLinks);
}

DAVA::Set<DAVA::FilePath> SceneDumper::DumpLinks(const DAVA::FilePath& scenePath, SceneDumper::eMode mode, const DAVA::Vector<DAVA::eGPUFamily>& compressedGPUs, DAVA::Set<DAVA::FilePath>& dumpedLinks)
{
    DAVA::Set<DAVA::FilePath> links;
    SceneDumper dumper(scenePath, mode, compressedGPUs);

    DAVA::Set<DAVA::FilePath> redumpScenes;
    if (nullptr != dumper.scene)
    {
        dumper.DumpLinksRecursive(dumper.scene, links, redumpScenes);
    }

    dumpedLinks.insert(links.begin(), links.end());

    for (const DAVA::FilePath& scenePath : redumpScenes)
    {
        DAVA::Set<DAVA::FilePath> result = SceneDumper::DumpLinks(scenePath, mode, compressedGPUs, dumpedLinks);
        links.insert(result.begin(), result.end());
    }

    return links;
}

SceneDumper::SceneDumper(const DAVA::FilePath& scenePath, SceneDumper::eMode mode_, const DAVA::Vector<DAVA::eGPUFamily>& compressedGPUs_)
    : scenePathname(scenePath)
    , compressedGPUs(compressedGPUs_)
    , mode(mode_)
{
    scene = new DAVA::Scene();
    if (DAVA::SceneFileV2::ERROR_NO_ERROR != scene->LoadScene(scenePathname))
    {
        DAVA::Logger::Error("[SceneDumper::SceneDumper] Can't open file %s", scenePathname.GetStringValue().c_str());
        DAVA::SafeRelease(scene);
    }
}

SceneDumper::~SceneDumper()
{
    SafeRelease(scene);
}

void SceneDumper::DumpLinksRecursive(DAVA::Entity* entity, DAVA::Set<DAVA::FilePath>& links, DAVA::Set<DAVA::FilePath>& redumpScenes) const
{
    //Recursiveness
    const DAVA::uint32 count = entity->GetChildrenCount();
    for (DAVA::uint32 ch = 0; ch < count; ++ch)
    {
        DumpLinksRecursive(entity->GetChild(ch), links, redumpScenes);
    }

    // Custom Property Links
    DumpCustomProperties(GetCustomPropertiesArchieve(entity), links);

    //Render object
    DumpRenderObject(GetRenderObject(entity), links);

    //Effects
    DumpEffect(GetEffectComponent(entity), links);

    for (DAVA::uint32 i = 0; i < entity->GetComponentCount(DAVA::Component::SLOT_COMPONENT); ++i)
    {
        DumpSlot(static_cast<DAVA::SlotComponent*>(entity->GetComponent(DAVA::Component::SLOT_COMPONENT, i)), links, redumpScenes);
    }
}

void SceneDumper::DumpCustomProperties(DAVA::KeyedArchive* properties, DAVA::Set<DAVA::FilePath>& links) const
{
    if (nullptr == properties)
        return;

    auto SaveProp = [&properties, &links](const DAVA::String& name)
    {
        DAVA::String str = properties->GetString(name);
        if (!str.empty())
        {
            links.insert(str);
        }
    };

    SaveProp("touchdownEffect");

    if (mode == eMode::EXTENDED)
    {
        SaveProp(ResourceEditor::EDITOR_REFERENCE_TO_OWNER);

        //save custom colors
        DAVA::String pathname = properties->GetString(ResourceEditor::CUSTOM_COLOR_TEXTURE_PROP);
        if (!pathname.empty())
        {
            DAVA::FilePath projectPath = ProjectManagerData::CreateProjectPathFromPath(scenePathname);
            links.emplace(projectPath + pathname);
        }
    }
}

void SceneDumper::DumpRenderObject(DAVA::RenderObject* renderObject, DAVA::Set<DAVA::FilePath>& links) const
{
    using namespace DAVA;

    if (nullptr == renderObject)
        return;

    Set<FilePath> descriptorPathnames;

    switch (renderObject->GetType())
    {
    case RenderObject::TYPE_LANDSCAPE:
    {
        Landscape* landscape = static_cast<Landscape*>(renderObject);
        links.insert(landscape->GetHeightmapPathname());
        break;
    }
    case RenderObject::TYPE_VEGETATION:
    {
        VegetationRenderObject* vegetation = static_cast<VegetationRenderObject*>(renderObject);
        links.insert(vegetation->GetHeightmapPath());

        if (mode == eMode::EXTENDED)
        {
            links.insert(vegetation->GetCustomGeometryPath());
        }

        Set<DataNode*> dataNodes;
        vegetation->GetDataNodes(dataNodes);
        for (DataNode* node : dataNodes)
        {
            NMaterial* material = dynamic_cast<NMaterial*>(node);
            if (material != nullptr)
            {
                DumpMaterial(material, links, descriptorPathnames);
            }
        }

        descriptorPathnames.insert(vegetation->GetLightmapPath());
        break;
    }

    default:
        break;
    }

    const uint32 count = renderObject->GetRenderBatchCount();
    for (uint32 rb = 0; rb < count; ++rb)
    {
        RenderBatch* renderBatch = renderObject->GetRenderBatch(rb);
        NMaterial* material = renderBatch->GetMaterial();
        DumpMaterial(material, links, descriptorPathnames);
    }

    // create pathnames for textures
    for (const auto& descriptorPath : descriptorPathnames)
    {
        DVASSERT(descriptorPath.IsEmpty() == false);

        links.insert(descriptorPath);

        std::unique_ptr<TextureDescriptor> descriptor(TextureDescriptor::CreateFromFile(descriptorPath));
        if (descriptor)
        {
            if (descriptor->IsCompressedFile())
            {
                Vector<FilePath> compressedTexureNames;
                descriptor->CreateLoadPathnamesForGPU(descriptor->gpu, compressedTexureNames);

                if (compressedTexureNames.empty() == false)
                {
                    if (descriptor->IsCubeMap() && (TextureDescriptor::IsCompressedTextureExtension(compressedTexureNames[0].GetExtension()) == false))
                    {
                        Vector<FilePath> faceNames;
                        descriptor->GetFacePathnames(faceNames);
                        links.insert(faceNames.cbegin(), faceNames.cend());
                    }
                    else
                    {
                        links.insert(compressedTexureNames.begin(), compressedTexureNames.end());
                    }
                }
            }
            else
            {
                bool isCompressedSource = TextureDescriptor::IsSupportedCompressedFormat(descriptor->dataSettings.sourceFileFormat);
                if (descriptor->IsCubeMap() && !isCompressedSource)
                {
                    Vector<FilePath> faceNames;
                    descriptor->GetFacePathnames(faceNames);

                    links.insert(faceNames.cbegin(), faceNames.cend());
                }
                else
                {
                    links.insert(descriptor->GetSourceTexturePathname());
                }

                for (eGPUFamily gpu : compressedGPUs)
                {
                    const TextureDescriptor::Compression& compression = descriptor->compression[gpu];
                    if (compression.format != FORMAT_INVALID)
                    {
                        Vector<FilePath> compressedTexureNames;
                        descriptor->CreateLoadPathnamesForGPU(static_cast<eGPUFamily>(gpu), compressedTexureNames);
                        links.insert(compressedTexureNames.begin(), compressedTexureNames.end());
                    }
                }
            }
        }
    }
}

void SceneDumper::DumpMaterial(DAVA::NMaterial* material, DAVA::Set<DAVA::FilePath>& links, DAVA::Set<DAVA::FilePath>& descriptorPathnames) const
{
    //Enumerate textures from materials
    DAVA::Set<DAVA::MaterialTextureInfo*> materialTextures;

    while (nullptr != material)
    {
        material->CollectLocalTextures(materialTextures);
        material = material->GetParent();
    }

    // enumerate descriptor pathnames
    for (const DAVA::MaterialTextureInfo* matTex : materialTextures)
    {
        descriptorPathnames.insert(matTex->path);
    }
}

void SceneDumper::DumpEffect(DAVA::ParticleEffectComponent* effect, DAVA::Set<DAVA::FilePath>& links) const
{
    if (nullptr == effect)
    {
        return;
    }

    DAVA::Set<DAVA::FilePath> gfxFolders;

    const DAVA::int32 emittersCount = effect->GetEmittersCount();
    for (DAVA::int32 em = 0; em < emittersCount; ++em)
    {
        DumpEmitter(effect->GetEmitterInstance(em), links, gfxFolders);
    }

    for (const DAVA::FilePath& folder : gfxFolders)
    {
        DAVA::FilePath flagsTXT = folder + "flags.txt";
        if (DAVA::FileSystem::Instance()->Exists(flagsTXT))
        {
            links.insert(flagsTXT);
        }
    }
}

void SceneDumper::DumpEmitter(DAVA::ParticleEmitterInstance* instance, DAVA::Set<DAVA::FilePath>& links, DAVA::Set<DAVA::FilePath>& gfxFolders) const
{
    using namespace DAVA;
    DVASSERT(nullptr != instance);

    ParticleEmitter* emitter = instance->GetEmitter();

    links.insert(emitter->configPath);
    for (ParticleLayer* layer : emitter->layers)
    {
        DVASSERT(nullptr != layer);

        if (layer->type == ParticleLayer::TYPE_SUPEREMITTER_PARTICLES)
        {
            ScopedPtr<ParticleEmitterInstance> instance(new ParticleEmitterInstance(nullptr, layer->innerEmitter, true));
            DumpEmitter(instance, links, gfxFolders);
        }
        else
        {
            ProcessSprite(layer->sprite, links, gfxFolders);
            ProcessSprite(layer->flowmap, links, gfxFolders);
            ProcessSprite(layer->noise, links, gfxFolders);
            ProcessSprite(layer->alphaRemapSprite, links, gfxFolders);
        }
    }
}

void SceneDumper::ProcessSprite(DAVA::Sprite* sprite, DAVA::Set<DAVA::FilePath>& links, DAVA::Set<DAVA::FilePath>& gfxFolders) const
{
    using namespace DAVA;

    if (sprite == nullptr)
        return;

    FilePath psdPath = ReplaceInString(sprite->GetRelativePathname().GetAbsolutePathname(), "/Data/", "/DataSource/");
    psdPath.ReplaceExtension(".psd");
    links.insert(psdPath);

    gfxFolders.insert(psdPath.GetDirectory());
}

void SceneDumper::DumpSlot(DAVA::SlotComponent* slot, DAVA::Set<DAVA::FilePath>& links, DAVA::Set<DAVA::FilePath>& redumpScenes) const
{
    DAVA::FilePath configPath = slot->GetConfigFilePath();
    links.insert(configPath.GetAbsolutePathname());
    DAVA::Vector<DAVA::SlotSystem::ItemsCache::Item> items = scene->slotSystem->GetItems(configPath);
    for (const DAVA::SlotSystem::ItemsCache::Item& item : items)
    {
        links.insert(item.scenePath.GetAbsolutePathname());
        redumpScenes.insert(item.scenePath.GetAbsolutePathname());
    }
}
