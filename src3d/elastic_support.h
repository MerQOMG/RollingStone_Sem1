#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include "physics.h"
namespace JPH { class PhysicsSystem; }
namespace rs3d {
// Owns one slider and its diagnostics; only the numerical worker calls it.
class ElasticSupport final:public JPH::PhysicsStepListener {
    JPH::PhysicsSystem&system_;
    JPH::BodyID body_;
    JPH::Ref<JPH::SliderConstraint> slider_;
    BarrierSettings settings_;
    Vec3 axis_;
    double mass_,step_=0,oldQ_=0,oldV_=0,effectiveMass_=0,freeV_=0;
    double logicalImpulse_=0;
    const std::uint64_t&contactCounter_;std::uint64_t previousContacts_=0;
    bool pending_=false;
    BarrierSnapshot state_;
    void finishSubstep();
public:
    ElasticSupport(JPH::PhysicsSystem&system,JPH::BodyID body,const BarrierSettings&settings,const std::uint64_t&contactCounter);
    ~ElasticSupport();
    void OnStep(const JPH::PhysicsStepListenerContext&context)override;
    void beginUpdate(){logicalImpulse_=0;}
    void endUpdate(double dt);
    BarrierSnapshot snapshot()const{return state_;}
};
}
