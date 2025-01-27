#pragma once

#include <Entity/SceneSystem.h>

namespace physx
{
class PxScene;
}

namespace DAVA
{
class Scene;
class PhysicsComponent;
class CollisionComponent;

class PhysicsSystem : public SceneSystem
{
public:
    PhysicsSystem(Scene* scene);
    ~PhysicsSystem() override;

    void RegisterEntity(Entity* entity) override;
    void UnregisterEntity(Entity* entity) override;

    void RegisterComponent(Entity* entity, Component* component) override;
    void UnregisterComponent(Entity* entity, Component* component) override;

    void Process(float32 timeElapsed) override;

    void SetSimulationEnabled(bool isEnabled);
    bool IsSimulationEnabled() const;

    void SetDebugDrawEnabled(bool drawDebugInfo);
    bool IsDebugDrawEnabled() const;

private:
    bool FetchResults(bool waitForFetchFinish);

    void DrawDebugInfo();
    void InitNewObjects();
    void SyncTransformToPhysx();

private:
    friend class PhysicsSystemPrivate; // for tests only

    void* simulationBlock = nullptr;
    uint32 simulationBlockSize = 0;

    bool isSimulationEnabled = true;
    bool isSimulationRunning = false;
    physx::PxScene* physicsScene = nullptr;

    Vector<PhysicsComponent*> physicsComponents;
    Vector<PhysicsComponent*> pendingAddPhysicsComponents;

    Vector<CollisionComponent*> collisionComponents;
    Vector<CollisionComponent*> pendingAddCollisionComponents;

    bool drawDebugInfo = false;
};
} // namespace DAVA