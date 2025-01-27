#pragma once

#include "UI/UIControl.h"
#include "EditorSystemsManager.h"

#include <Functional/TrackedObject.h>
#include <Base/Introspection.h>
#include <Math/Color.h>

#include <memory>

class VisibleValueProperty;

class HUDControlsPreferences : public DAVA::InspBase
{
public:
    HUDControlsPreferences();

#ifdef DECLARE_PREFERENCE
#error "DECLARE_PREFERENCE is already declared";
#endif //DECLARE_PREFERENCE

#define DECLARE_PREFERENCE(T, pref) \
    static T pref; \
    T Get##pref() const; \
    void Set##pref(const T& pref);

    DECLARE_PREFERENCE(DAVA::Color, selectionRectColor);
    DECLARE_PREFERENCE(DAVA::Color, highlightColor);
    DECLARE_PREFERENCE(DAVA::Color, hudRectColor);
    DECLARE_PREFERENCE(DAVA::String, cornerRectPath2);
    DECLARE_PREFERENCE(DAVA::String, borderRectPath2);
    DECLARE_PREFERENCE(DAVA::String, pivotPointPath2);
    DECLARE_PREFERENCE(DAVA::String, rotatePath2);
    DECLARE_PREFERENCE(DAVA::String, magnetLinePath2);
    DECLARE_PREFERENCE(DAVA::String, magnetRectPath2);

#undef DECLARE_PREFERENCE
    //old introspection dont compile on DAVA::Color as member
public:
    INTROSPECTION(HUDControlsPreferences,
                  PROPERTY("selectionRectColor", "User graphic/Selection rect color", GetselectionRectColor, SetselectionRectColor, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("highlightColor", "User graphic/Highlight color", GethighlightColor, SethighlightColor, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("hudRectColor", "User graphic/Frame rect color", GethudRectColor, SethudRectColor, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("cornerRectPath2", "User graphic/Corner rect path", GetcornerRectPath2, SetcornerRectPath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("borderRectPath2", "User graphic/Border rect path", GetborderRectPath2, SetborderRectPath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("pivotPointPath2", "User graphic/Pivot point control path", GetpivotPointPath2, SetpivotPointPath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("rotatePath2", "User graphic/Rotate control path", GetrotatePath2, SetrotatePath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("magnetLinePath2", "User graphic/Magnet line path", GetmagnetLinePath2, SetmagnetLinePath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("magnetRectPath2", "User graphic/Magnet rect path", GetmagnetRectPath2, SetmagnetRectPath2, DAVA::I_SAVE | DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  )
};

class ControlContainer
{
public:
    explicit ControlContainer(const HUDAreaInfo::eArea area);
    virtual ~ControlContainer();

    ControlContainer(const ControlContainer&) = delete;
    ControlContainer(ControlContainer&&) = delete;

    ControlContainer& operator=(const ControlContainer&) = delete;
    ControlContainer& operator==(ControlContainer&&) = delete;

    HUDAreaInfo::eArea GetArea() const;
    virtual void InitFromGD(const DAVA::UIGeometricData& gd_) = 0;

    //this methods are used to temporally show/hide some HUD controls, like pivot or rotate
    //we can not use control visibility flag, because visibility state will be overwritten by show/hide whole HUD
    void SetSystemVisible(bool visible);
    bool GetSystemVisible() const;

    void AddChild(std::unique_ptr<ControlContainer>&& child);

    void AddToParent(DAVA::UIControl* parent);
    void RemoveFromParent(DAVA::UIControl* parent);

    void SetVisible(bool visible);
    void SetName(const DAVA::String& name);

    bool IsPointInside(const DAVA::Vector2& point) const;
    bool IsHiddenForDebug() const;
    bool GetVisibilityFlag() const;

protected:
    const HUDAreaInfo::eArea area = HUDAreaInfo::NO_AREA;
    bool systemVisible = true;

    DAVA::RefPtr<DAVA::UIControl> drawable;
    DAVA::Vector<std::unique_ptr<ControlContainer>> children;
};

class HUDContainer : public ControlContainer, public DAVA::TrackedObject
{
public:
    explicit HUDContainer(const ControlNode* node);
    void InitFromGD(const DAVA::UIGeometricData& geometricData) override;

private:
    void OnUpdate();

    ~HUDContainer() override = default;
    const ControlNode* node = nullptr;
    VisibleValueProperty* visibleProperty = nullptr;
    //weak pointer to control to wrap around
    DAVA::UIControl* control = nullptr;
};

class FrameControl : public ControlContainer
{
public:
    enum eBorder
    {
        TOP,
        BOTTOM,
        LEFT,
        RIGHT,
        COUNT
    };

    enum eType
    {
        SELECTION,
        HIGHLIGHT,
        SELECTION_RECT
    };
    explicit FrameControl(eType type);
    void SetRect(const DAVA::Rect& rect);
    DAVA::Rect GetAbsoluteRect() const;

protected:
    void InitFromGD(const DAVA::UIGeometricData& geometricData) override;
    DAVA::Rect GetSubControlRect(const DAVA::Rect& rect, eBorder border) const;

    eType type = SELECTION;
    DAVA::float32 lineThickness = 1.0f;
};

class FrameRectControl : public ControlContainer
{
public:
    explicit FrameRectControl(const HUDAreaInfo::eArea area_);

private:
    ~FrameRectControl() = default;
    void InitFromGD(const DAVA::UIGeometricData& geometricData) override;
    DAVA::Vector2 GetPos(const DAVA::Rect& rect) const;

    DAVA::Vector2 rectSize;
};

class PivotPointControl : public ControlContainer
{
public:
    explicit PivotPointControl();

private:
    ~PivotPointControl() = default;
    void InitFromGD(const DAVA::UIGeometricData& geometricData) override;
};

class RotateControl : public ControlContainer
{
public:
    explicit RotateControl();

private:
    ~RotateControl() = default;
    void InitFromGD(const DAVA::UIGeometricData& geometricData) override;
};

void SetupHUDMagnetLineControl(DAVA::UIControl* control);
void SetupHUDMagnetRectControl(DAVA::UIControl* control);

std::unique_ptr<ControlContainer> CreateHighlightRect(const ControlNode* node);
