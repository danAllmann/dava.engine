#pragma once

#include "Classes/Qt/Scene/System/EditorSceneSystem.h"
#include "Classes/Qt/Scene/System/SystemDelegates.h"

#include <TArc/Utils/QtConnections.h>

#include <Entity/SceneSystem.h>
#include <Scene3D/Components/SlotComponent.h>
#include <Base/BaseTypes.h>

namespace DAVA
{
namespace TArc
{
class ContextAccessor;
} // namespace TArc
} // namespace DAVA

class EditorSlotSystem : public DAVA::SceneSystem,
                         public EditorSceneSystem,
                         public EntityModificationSystemDelegate
{
public:
    static const DAVA::FastName emptyItemName;

    EditorSlotSystem(DAVA::Scene* scene, DAVA::TArc::ContextAccessor* accessor);

    void AddEntity(DAVA::Entity* entity) override;
    void RemoveEntity(DAVA::Entity* entity) override;

    void AddComponent(DAVA::Entity* entity, DAVA::Component* component) override;
    void RemoveComponent(DAVA::Entity* entity, DAVA::Component* component) override;

    void Process(DAVA::float32 timeElapsed) override;

    void WillClone(DAVA::Entity* originalEntity) override;
    void DidCloned(DAVA::Entity* originalEntity, DAVA::Entity* newEntity) override;

protected:
    friend class AttachEntityToSlot;

    void DetachEntity(DAVA::SlotComponent* component, DAVA::Entity* entity);
    void AttachEntity(DAVA::SlotComponent* component, DAVA::Entity* entity, DAVA::FastName itemName);
    DAVA::Entity* AttachEntity(DAVA::SlotComponent* component, DAVA::FastName itemName);

    void AccumulateDependentCommands(REDependentCommandsHolder& holder) override;
    void ProcessCommand(const RECommandNotificationObject& commandNotification) override;

    void Draw() override;

protected:
    std::unique_ptr<DAVA::Command> PrepareForSave(bool saveForGame) override;
    void SetScene(DAVA::Scene* scene) override;

private:
    DAVA::Vector<DAVA::Entity*> entities;
    DAVA::Set<DAVA::Entity*> pendingOnInitialize;
    DAVA::TArc::QtConnections connections;
    DAVA::TArc::ContextAccessor* accessor;

    struct AttachedItemInfo
    {
        DAVA::SlotComponent* component = nullptr;
        DAVA::Entity* entity = nullptr;
        DAVA::FastName itemName;
    };

    DAVA::UnorderedMap<DAVA::Entity*, DAVA::Vector<AttachedItemInfo>> inClonedState;
    DAVA::Set<DAVA::Entity*> clonedEntityes;
};
