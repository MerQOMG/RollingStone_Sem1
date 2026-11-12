#pragma once
// Project-owned read-only bridge. Used only with JobSystemSingleThreaded.
// No physics objects are mutated, no BodyInterface/locks, allocations or I/O here.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
namespace rs3d_observer {
struct Sink {
    virtual ~Sink() = default;
    virtual void bind(const void*, const JPH::SubShapeIDPair&) noexcept = 0;
    virtual void solved(const void*, const JPH::Body&, const JPH::Body&, JPH::RVec3Arg,
        JPH::RVec3Arg, JPH::Vec3Arg, float) noexcept = 0;
    virtual void candidate(const JPH::Body&, const JPH::Body&, const JPH::ContactManifold&,
        JPH::RVec3Arg, JPH::RVec3Arg, float) noexcept = 0;
    virtual void resolved(JPH::BodyID, JPH::BodyID, float, float) noexcept = 0;
    virtual void discarded(JPH::BodyID, JPH::BodyID) noexcept = 0;
};
inline thread_local Sink* sink = nullptr;
}
