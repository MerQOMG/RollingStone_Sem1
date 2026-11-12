#include "elastic_support.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>
namespace rs3d {
namespace {
JPH::RVec3 position(Vec3 v){return {v.x,v.y,v.z};}
JPH::Vec3 vector(Vec3 v){return {float(v.x),float(v.y),float(v.z)};}
Vec3 vector(JPH::RVec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
Vec3 velocity(JPH::Vec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
}
ElasticSupport::ElasticSupport(JPH::PhysicsSystem&system,JPH::BodyID body,const BarrierSettings&settings,const std::uint64_t&contactCounter)
    :system_(system),body_(body),settings_(settings),axis_(barrierAxis(settings)),mass_(double(float(settings.mass))),contactCounter_(contactCounter){
    JPH::SliderConstraintSettings guide;guide.SetSliderAxis(vector(axis_));
    guide.mPoint1=position(settings.center);guide.mPoint2=position(initialBarrierCenter(settings));
    slider_=static_cast<JPH::SliderConstraint*>(system.GetBodyInterface().CreateConstraint(&guide,JPH::BodyID(),body));
    system.AddConstraint(slider_);system.AddStepListener(this);
    state_.enabled=true;state_.elastic=true;state_.rotation=Quaternion::fromAxisAngle({0,0,1},settings.yawDegrees*pi/180);
    finishSubstep();
}
ElasticSupport::~ElasticSupport(){system_.RemoveStepListener(this);system_.RemoveConstraint(slider_);}
void ElasticSupport::OnStep(const JPH::PhysicsStepListenerContext&context){
    finishSubstep();auto&bi=system_.GetBodyInterfaceNoLock();step_=context.mDeltaTime;
    oldQ_=dot(vector(bi.GetPosition(body_))-settings_.center,axis_);oldV_=dot(velocity(bi.GetLinearVelocity(body_)),axis_);
    // Eliminate the implicit axial support equation into the contact mass:
    // (M+h*c+h*h*k)*v_next = M*v_old-h*k*q_old+J_contact.
    // The guide fixes the other axes; physical mass/energy remain M.
    double effective=mass_+step_*settings_.damping+step_*step_*settings_.stiffness;
    JPH::BodyLockWrite body(system_.GetBodyLockInterfaceNoLock(),body_);
    auto motion=body.GetBody().GetMotionProperties();motion->SetInverseMass(float(1/effective));
    effectiveMass_=1.0/double(motion->GetInverseMass());
    bi.SetLinearVelocity(body_,vector(axis_*((mass_*oldV_-step_*settings_.stiffness*oldQ_)/effective)));
    freeV_=dot(velocity(bi.GetLinearVelocity(body_)),axis_);
    previousContacts_=contactCounter_;pending_=true;
}
void ElasticSupport::finishSubstep(){
    const auto&bi=system_.GetBodyInterfaceNoLock();state_.center=vector(bi.GetPosition(body_));
    auto rotation=bi.GetRotation(body_);state_.rotation={rotation.GetW(),rotation.GetX(),rotation.GetY(),rotation.GetZ()};
    double q=dot(state_.center-settings_.center,axis_),v=dot(velocity(bi.GetLinearVelocity(body_)),axis_);
    if(pending_){
        double contact=contactCounter_!=previousContacts_?effectiveMass_*(v-freeV_):0;
        double impulse=mass_*(v-oldV_)-contact;
        double loss=step_*settings_.damping*v*v;
        logicalImpulse_+=contact;state_.contactImpulse+=contact;
        // This is a substep-average axial force, not an instantaneous impact peak.
        state_.peakContactForce=std::max(state_.peakContactForce,std::abs(contact)/step_);
        state_.touched=state_.touched||std::abs(contact)>1e-4;
        state_.dampingEnergy+=loss;
        state_.supportWorkResidual+=-impulse*.5*(oldV_+v)-.5*settings_.stiffness*(q*q-oldQ_*oldQ_)-loss;
        pending_=false;
    }
    state_.displacement=q;state_.velocity=v;state_.supportForce=settings_.stiffness*q+settings_.damping*v;
    state_.kineticEnergy=.5*mass_*v*v;state_.springEnergy=.5*settings_.stiffness*q*q;
    state_.maxDisplacement=std::max(state_.maxDisplacement,std::abs(q));
    state_.peakSupportForce=std::max(state_.peakSupportForce,std::abs(state_.supportForce));
    state_.settled=std::sqrt(v*v+settings_.stiffness*q*q/mass_)<.02;

}
void ElasticSupport::endUpdate(double dt){finishSubstep();state_.contactForce=logicalImpulse_/dt;
    if(!finite(state_.center)||!std::isfinite(state_.displacement+state_.velocity+state_.dampingEnergy+state_.supportWorkResidual))throw std::runtime_error("non-finite elastic support state");}
}
