#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
namespace rs3d {
// One instance in main, before workers/shapes; destroy after every Jolt object.
class JoltRuntime {
public:
    JoltRuntime(){JPH::RegisterDefaultAllocator();JPH::Factory::sInstance=new JPH::Factory();JPH::RegisterTypes();}
    ~JoltRuntime(){JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}
    JoltRuntime(const JoltRuntime&)=delete;
    JoltRuntime& operator=(const JoltRuntime&)=delete;
};
}
