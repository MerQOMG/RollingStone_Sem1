#include "first_barrier_contact.h"
#include <tuple>
namespace rs3d {
namespace {
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Grid3D contactGrid(const BarrierSettings&b,double cell){return Grid3D({{0,0,0},{b.width,b.height,1},{cell,cell,1}});}
bool pair(const SolvedContact&r){return (r.object1==rockObjectId&&r.object2==barrierObjectId)||(r.object2==rockObjectId&&r.object1==barrierObjectId);}
std::array<double,46> pointKey(const FirstContactPoint&p){std::array<double,46> k{};size_t i=0;auto vector=[&](Vec3 v){k[i++]=v.x;k[i++]=v.y;k[i++]=v.z;};vector(p.local);vector(p.world);vector(p.normal);k[i++]=p.gap;k[i++]=p.impulse;k[i++]=p.fraction;k[i++]=int(p.method);k[i++]=int(p.source);for(const auto*b:{&p.rock,&p.barrier}){vector(b->center);vector(b->shapeOrigin);vector(b->velocity);vector(b->angularVelocity);k[i++]=b->rotation.w;k[i++]=b->rotation.x;k[i++]=b->rotation.y;k[i++]=b->rotation.z;}for(double value:k)require(std::isfinite(value),"non-finite first-contact point/pose");return k;}
std::string classify(Vec3 p,const BarrierSettings&b,double tol){
    Vec3 half{b.thickness/2,b.width/2,b.height/2};
    for(int i=0;i<3;++i)require(std::abs(p[i])<=half[i]+tol,"first-contact point outside barrier dimensions");
    std::string face;int count=0;
    for(int i=0;i<3;++i)if(std::abs(std::abs(p[i])-half[i])<=tol){++count;face=i==0?(p.x<0?"front":"back"):i==1?"side":(p.z<0?"bottom":"top");}
    require(count>0,"first-contact point is not on a barrier face");return count>1?"mixed":face;
}
}
const char*firstContactStatusName(FirstContactStatus s){switch(s){case FirstContactStatus::notStarted:return "not_started";case FirstContactStatus::observing:return "observing";case FirstContactStatus::hit:return "hit";case FirstContactStatus::noHit:return "no_hit";case FirstContactStatus::unresolved:return "unresolved";case FirstContactStatus::disabled:return "disabled";case FirstContactStatus::notRecorded:return "not_recorded";}return "invalid";}
const char*contactMethodName(ContactMethod m){switch(m){case ContactMethod::geometricTouch:return "geometric_touch";case ContactMethod::solvedConstraint:return "solved_constraint";case ContactMethod::resolvedCcd:return "resolved_ccd";}return "invalid";}
FirstBarrierContactCollector::FirstBarrierContactCollector(std::uint64_t id,const BarrierSettings&b,double slop,double mass,double cell):barrier_(b),grid_(contactGrid(b,cell)){
    require(id>0&&std::isfinite(slop)&&slop>=0&&std::isfinite(mass)&&mass>0,"invalid first-contact observation parameters");event_.trialId=id;event_.status=b.enabled?FirstContactStatus::observing:FirstContactStatus::disabled;event_.slop=slop;event_.impulseThreshold=std::max(1e-6,1e-8*mass);event_.dedupTolerance=std::max(1e-6,std::min(slop,1e-4));event_.boundaryTolerance=std::max(1e-6,slop);
}
void validateFirstContact(FirstContactEvent&e,const BarrierSettings&b,const Grid3D&grid){
    if(e.status!=FirstContactStatus::hit){require(e.points.empty(),"non-hit first-contact event has points");return;}
    require(!e.points.empty()&&e.substep>0&&std::isfinite(e.timeLower)&&std::isfinite(e.timeUpper)&&e.timeLower<=e.timeUpper&&e.timeLower>=0,"invalid first-contact time/points");
    require(e.substep<=4&&e.timeUpper>e.timeLower&&e.timeUpper-e.timeLower<=.00250001,"first-contact substep/time interval exceeds supported physics bounds");
    require(std::isfinite(e.boundaryTolerance)&&e.boundaryTolerance>=1e-6&&std::isfinite(e.dedupTolerance)&&e.dedupTolerance>=1e-6,"invalid first-contact tolerances");
    require(std::isfinite(e.slop)&&e.slop>=0&&e.slop<=.001&&e.boundaryTolerance==std::max(1e-6,e.slop)&&e.dedupTolerance==std::max(1e-6,std::min(e.slop,1e-4))&&std::isfinite(e.impulseThreshold)&&e.impulseThreshold>=1e-6,"inconsistent first-contact numerical tolerances");
    // Sorting makes grouping and representative location independent of callback order.
    for(const auto&p:e.points)pointKey(p);
    std::sort(e.points.begin(),e.points.end(),[](const auto&a,const auto&b){return pointKey(a)<pointKey(b);});
    std::vector<Vec3> representatives;e.face.clear();
    for(auto&p:e.points){require(finite(p.world)&&finite(p.local)&&finite(p.normal)&&std::isfinite(p.gap)&&std::isfinite(p.impulse)&&p.impulse>=0&&std::isfinite(p.fraction),"non-finite first-contact data");
        require(p.source==ContactSource::constraint||p.source==ContactSource::ccdResolved,"first-contact source is unconfirmed");
        if(p.method==ContactMethod::resolvedCcd)require(p.source==ContactSource::ccdResolved&&p.fraction>=0&&p.fraction<=1,"invalid resolved CCD evidence");
        else if(p.method==ContactMethod::solvedConstraint)require(p.source==ContactSource::constraint&&p.impulse>e.impulseThreshold,"missing solved first-contact impulse");
        else require(p.source==ContactSource::constraint&&p.gap<=e.slop,"missing geometric-touch evidence");
        require(norm(p.barrier.center+p.barrier.rotation.matrix()*p.local-p.world)<=std::max(1e-8,e.boundaryTolerance*.1),"first-contact world/local mismatch");
        p.face=classify(p.local,b,e.boundaryTolerance);if(e.face.empty())e.face=p.face;else if(e.face!=p.face)e.face="mixed";
        auto i=std::find_if(representatives.begin(),representatives.end(),[&](Vec3 v){return norm(v-p.local)<=e.dedupTolerance;});
        p.group=unsigned(i-representatives.begin());if(i==representatives.end())representatives.push_back(p.local);
    }
    Vec3 mean{};for(auto p:representatives)mean+=p;mean=mean/double(representatives.size());
    e.u=mean.y+b.width/2;e.v=mean.z+b.height/2;
    require(e.u>=-e.boundaryTolerance&&e.u<=b.width+e.boundaryTolerance&&e.v>=-e.boundaryTolerance&&e.v<=b.height+e.boundaryTolerance,"first-contact representative out of bounds");
    e.u=std::clamp(e.u,0.,b.width);e.v=std::clamp(e.v,0.,b.height);auto c=grid.cellAt({e.u,e.v,.5});require(bool(c),"first-contact grid index missing");e.cell=*c;
}
void FirstBarrierContactCollector::accept(const std::vector<const SolvedContact*>&records){
    const auto&r=*records.front();event_.step=r.step;event_.substep=r.substep;event_.timeLower=r.timeLower;event_.timeUpper=r.timeUpper;event_.status=FirstContactStatus::hit;
    for(const auto*value:records){const auto&v=*value;bool rockFirst=v.object1==rockObjectId;FirstContactPoint p;p.world=rockFirst?v.point2:v.point1;p.normal=rockFirst?-v.normal:v.normal;p.rock=rockFirst?v.pose1:v.pose2;p.barrier=rockFirst?v.pose2:v.pose1;p.local=transpose(p.barrier.rotation.matrix())*(p.world-p.barrier.center);p.gap=v.gap;p.impulse=v.normalImpulse;p.fraction=v.ccdFraction;p.source=v.source;p.method=v.source==ContactSource::ccdResolved?ContactMethod::resolvedCcd:v.normalImpulse>event_.impulseThreshold?ContactMethod::solvedConstraint:ContactMethod::geometricTouch;event_.points.push_back(p);event_.timeLower=std::min(event_.timeLower,v.timeLower);event_.timeUpper=std::max(event_.timeUpper,v.timeUpper);}
    validateFirstContact(event_,barrier_,grid_);event_.rock=event_.points.front().rock;event_.barrier=event_.points.front().barrier;
}
void FirstBarrierContactCollector::observe(const std::vector<SolvedContact>&records,bool lost){
    if(!barrier_.enabled||event_.status==FirstContactStatus::hit||unresolved_)return;
    if(lost){unresolved_=true;event_.status=FirstContactStatus::unresolved;event_.reason="lost or overflowed observations before first confirmation";return;}
    std::vector<const SolvedContact*> sorted;for(const auto&r:records)if(pair(r))sorted.push_back(&r);
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return std::tie(a->step,a->substep,a->ccdFraction,a->point1.x,a->point1.y,a->point1.z)<std::tie(b->step,b->substep,b->ccdFraction,b->point1.x,b->point1.y,b->point1.z);});
    for(size_t begin=0;begin<sorted.size();){size_t end=begin;std::vector<const SolvedContact*> confirmed;bool ambiguous=false;
        while(end<sorted.size()&&sorted[end]->step==sorted[begin]->step&&sorted[end]->substep==sorted[begin]->substep){const auto&r=*sorted[end++];
            if(r.source==ContactSource::ccdDiscarded)continue;
            if(r.source==ContactSource::ccdCandidate||!r.retained||!std::isfinite(r.gap)||!std::isfinite(r.normalImpulse)){ambiguous=true;continue;}
            if(r.source==ContactSource::ccdResolved||r.gap<=event_.slop||r.normalImpulse>event_.impulseThreshold)confirmed.push_back(&r);
            else if(r.normalImpulse>event_.impulseThreshold*.1)ambiguous=true;
        }
        if(ambiguous){unresolved_=true;event_.status=FirstContactStatus::unresolved;event_.reason="earlier or same-substep unresolved candidate/impulse threshold ambiguity";return;}
        if(!confirmed.empty()){try{accept(confirmed);}catch(const std::exception&e){event_.points.clear();event_.status=FirstContactStatus::unresolved;event_.reason=e.what();unresolved_=true;}return;}begin=end;
    }
}
void FirstBarrierContactCollector::finish(StopReason reason){event_.trialStatus=reason;if(event_.status==FirstContactStatus::observing){event_.status=normalCompletion(reason)?FirstContactStatus::noHit:FirstContactStatus::unresolved;if(!normalCompletion(reason))event_.reason="trial did not complete normally";}}
FirstContactAccumulator::FirstContactAccumulator(const BarrierSettings&b,std::uint64_t n,double cell):grid_(contactGrid(b,cell)),enabled_(b.enabled),requested_(n),hits_(grid_.count()){require(n>0,"first-contact target count must be positive");}
void FirstContactAccumulator::finish(const FirstContactEvent&e){
    if(!enabled_||!normalCompletion(e.trialStatus))return;
    if(e.status==FirstContactStatus::hit){require(e.cell<hits_.size()&&known()<requested_,"invalid first-contact count");++hits_[e.cell];++hitsTotal_;}
    else if(e.status==FirstContactStatus::noHit){require(known()<requested_,"too many first-contact observations");++misses_;}
}
Probability FirstContactAccumulator::probability(std::uint64_t h,bool complete)const{require(h<=hitsTotal_,"cell hits exceed first-contact hits");double lo=double(h)/requested_,hi=double(h+requested_-known())/requested_;return {enabled_&&complete&&known()==requested_?std::optional<double>(lo):std::nullopt,lo,hi};}
std::optional<double> FirstContactAccumulator::conditional(std::uint64_t h,bool complete)const{require(h<=hitsTotal_,"cell hits exceed first-contact hits");return enabled_&&complete&&known()==requested_&&hitsTotal_?std::optional<double>(double(h)/hitsTotal_):std::nullopt;}
}
