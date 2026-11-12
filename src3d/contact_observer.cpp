#include "contact_observer.h"
namespace rs3d {
namespace {
Vec3 v(JPH::RVec3Arg x){return {x.GetX(),x.GetY(),x.GetZ()};}
Vec3 v(JPH::Vec3Arg x){return {x.GetX(),x.GetY(),x.GetZ()};}
ContactBodyPose pose(const JPH::Body& b){auto q=b.GetRotation();return {v(b.GetCenterOfMassPosition()),v(b.GetPosition()),v(b.GetLinearVelocity()),v(b.GetAngularVelocity()),{q.GetW(),q.GetX(),q.GetY(),q.GetZ()}};}
}
void ContactObserver::begin(std::uint64_t step,double time){step_=step;time_=substepTime_=time;substep_=0;bindingCount_=0;records_.clear();overflow_=false;last_=false;}
void ContactObserver::OnStep(const JPH::PhysicsStepListenerContext& c){
    if((c.mIsFirstStep!=(substep_==0))||last_)overflow_=true;
    ++substep_;last_=c.mIsLastStep;time_=substepTime_;substepTime_+=c.mDeltaTime;bindingCount_=0;
}
SolvedContact* ContactObserver::append(const JPH::Body& a,const JPH::Body& b) noexcept {
    if(records_.size()==records_.capacity()){overflow_=true;return nullptr;}
    records_.emplace_back();auto& r=records_.back();r.body1=a.GetID().GetIndexAndSequenceNumber();r.body2=b.GetID().GetIndexAndSequenceNumber();
    r.object1=a.GetUserData();r.object2=b.GetUserData();r.pose1=pose(a);r.pose2=pose(b);r.step=step_;r.substep=substep_;r.timeLower=time_;r.timeUpper=substepTime_;return &r;
}
void ContactObserver::bind(const void* p,const JPH::SubShapeIDPair& key) noexcept {
    if(bindingCount_==bindings_.size()){overflow_=true;return;}bindings_[bindingCount_++]={p,key};
}
void ContactObserver::solved(const void* p,const JPH::Body& a,const JPH::Body& b,JPH::RVec3Arg p1,JPH::RVec3Arg p2,JPH::Vec3Arg n,float lambda) noexcept {
    auto r=append(a,b);if(!r)return;r->source=ContactSource::constraint;r->point1=v(p1);r->point2=v(p2);r->normal=v(n);r->gap=dot(r->point2-r->point1,r->normal);r->normalImpulse=lambda;r->retained=true;
    for(std::size_t i=0;i<bindingCount_;++i)if(bindings_[i].constraint==p){r->subshape1=bindings_[i].key.GetSubShapeID1().GetValue();r->subshape2=bindings_[i].key.GetSubShapeID2().GetValue();return;}
    r->retained=false;overflow_=true;
}
void ContactObserver::candidate(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& m,JPH::RVec3Arg c1,JPH::RVec3Arg c2,float fraction) noexcept {
    for(unsigned i=0;i<m.mRelativeContactPointsOn1.size();++i){auto r=append(a,b);if(!r)return;r->source=ContactSource::ccdCandidate;
        r->subshape1=m.mSubShapeID1.GetValue();r->subshape2=m.mSubShapeID2.GetValue();r->point1=v(m.GetWorldSpaceContactPointOn1(i));r->point2=v(m.GetWorldSpaceContactPointOn2(i));r->normal=v(m.mWorldSpaceNormal);
        r->gap=dot(r->point2-r->point1,r->normal);r->ccdFraction=fraction;
        r->pose1.shapeOrigin+=v(c1)-r->pose1.center;r->pose1.center=v(c1);r->pose2.shapeOrigin+=v(c2)-r->pose2.center;r->pose2.center=v(c2);
    }
}
void ContactObserver::resolved(JPH::BodyID a,JPH::BodyID b,float lambda,float fraction) noexcept {
    bool found=false;for(auto& r:records_)if(r.substep==substep_&&r.source==ContactSource::ccdCandidate&&r.body1==a.GetIndexAndSequenceNumber()&&r.body2==b.GetIndexAndSequenceNumber()){
        r.source=ContactSource::ccdResolved;r.retained=true;r.normalImpulse=lambda;r.ccdFraction=fraction;found=true;}
    if(!found)overflow_=true;
}
void ContactObserver::discarded(JPH::BodyID a,JPH::BodyID b) noexcept {
    for(auto& r:records_)if(r.substep==substep_&&r.source==ContactSource::ccdCandidate&&r.body1==a.GetIndexAndSequenceNumber()&&r.body2==b.GetIndexAndSequenceNumber())r.source=ContactSource::ccdDiscarded;
}
}
