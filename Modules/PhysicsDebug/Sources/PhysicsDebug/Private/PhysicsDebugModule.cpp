#include "PhysicsDebug/PhysicsDebugModule.h"
#include "Reflection/Reflection.h"
#include "Reflection/ReflectionRegistrator.h"

#include <physx/PxPhysicsAPI.h>
#include <PxShared/pvd/PxPvd.h>
#include <PxShared/pvd/PxPvdTransport.h>

namespace DAVA
{
PhysicsDebugModule::PhysicsDebugModule(Engine* engine)
    : IModule(engine)
{
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PhysicsDebugModule);
}

PhysicsDebugModule::~PhysicsDebugModule()
{
    DVASSERT(pvd == nullptr);
}

void PhysicsDebugModule::Init()
{
    if (pvd != nullptr)
    {
        const char* pvdHostIp = "127.0.0.1"; // IP of the PC which is running PVD
        int port = 5425; // TCP port to connect to, where PVD is listening
        unsigned int timeout = 100; // timeout in milliseconds to wait for PVD to respond,
        // consoles and remote PCs need a higher timeout.
        transport = physx::PxDefaultPvdSocketTransportCreate(pvdHostIp, port, timeout);
        pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
    }
}

void PhysicsDebugModule::Shutdown()
{
    if (pvd != nullptr)
    {
        DVASSERT(transport != nullptr);
        if (pvd->isConnected())
        {
            pvd->disconnect();
        }

        transport->release();
        transport = nullptr;
    }
}

physx::PxPvd* PhysicsDebugModule::CreatePvd(physx::PxFoundation* foundation)
{
    DVASSERT(pvd == nullptr);
    DVASSERT(foundation != nullptr);
    pvd = physx::PxCreatePvd(*foundation);
    return pvd;
}

void PhysicsDebugModule::ReleasePvd()
{
    DVASSERT(pvd != nullptr);
    pvd->release();
    pvd = nullptr;
}

DAVA_VIRTUAL_REFLECTION_IMPL(PhysicsDebugModule)
{
    ReflectionRegistrator<PhysicsDebugModule>::Begin()
    .Method("CreatePvd", &PhysicsDebugModule::CreatePvd)
    .Method("ReleasePvd", &PhysicsDebugModule::ReleasePvd)
    .End();
}

} // namespace DAVA
