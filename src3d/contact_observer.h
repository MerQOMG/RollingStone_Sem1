#pragma once
#include "physics.h"
#include "observer/jolt_observer.h"
#include <Jolt/Physics/PhysicsStepListener.h>
namespace rs3d {
// Fixed-capacity synchronous readout; inspect/copy only after Update returns.
// Registration is scoped to Update on the numerical worker thread.
class ContactObserver final : public rs3d_observer::Sink, public JPH::PhysicsStepListener {
    struct Binding { const void* constraint=nullptr; JPH::SubShapeIDPair key; };
    std::array<Binding,4096> bindings_{};
    std::size_t bindingCount_=0;
    std::vector<SolvedContact> records_;
    std::uint64_t step_=0;
    unsigned substep_=0;
    double time_=0,substepTime_=0;
    bool overflow_=false,last_=false;
    SolvedContact* append(const JPH::Body&,const JPH::Body&) noexcept;
public:
    ContactObserver(){records_.reserve(4096);}
    void begin(std::uint64_t step,double time);
    void OnStep(const JPH::PhysicsStepListenerContext&) override;
    void bind(const void*,const JPH::SubShapeIDPair&) noexcept override;
    void solved(const void*,const JPH::Body&,const JPH::Body&,JPH::RVec3Arg,JPH::RVec3Arg,JPH::Vec3Arg,float) noexcept override;
    void candidate(const JPH::Body&,const JPH::Body&,const JPH::ContactManifold&,JPH::RVec3Arg,JPH::RVec3Arg,float) noexcept override;
    void resolved(JPH::BodyID,JPH::BodyID,float,float) noexcept override;
    void discarded(JPH::BodyID,JPH::BodyID) noexcept override;
    const std::vector<SolvedContact>& records() const {return records_;}
    bool overflow() const {return overflow_;}
    unsigned substeps() const {return substep_;}
};
}
