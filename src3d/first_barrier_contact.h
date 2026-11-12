#pragma once
#include "trial.h"
#include "statistics.h"
namespace rs3d {
enum class FirstContactStatus { notStarted,observing,hit,noHit,unresolved,disabled,notRecorded };
const char* firstContactStatusName(FirstContactStatus s);
enum class ContactMethod { geometricTouch,solvedConstraint,resolvedCcd };
const char* contactMethodName(ContactMethod m);
struct FirstContactPoint {
    Vec3 world,local,normal;
    ContactBodyPose rock,barrier;
    double gap=0,impulse=0,fraction=-1;
    unsigned group=0;
    ContactMethod method=ContactMethod::geometricTouch;
    ContactSource source=ContactSource::constraint;
    std::string face;
};
struct FirstContactEvent {
    std::uint64_t trialId=0,step=0;
    unsigned substep=0;
    FirstContactStatus status=FirstContactStatus::notStarted;
    StopReason trialStatus=StopReason::running;
    double timeLower=0,timeUpper=0,slop=0,impulseThreshold=0,dedupTolerance=0,boundaryTolerance=0;
    double u=0,v=0;
    std::size_t cell=0;
    ContactBodyPose rock,barrier;
    std::vector<FirstContactPoint> points;
    std::string reason,face;
};
class FirstBarrierContactCollector {
    BarrierSettings barrier_;
    Grid3D grid_;
    FirstContactEvent event_;
    bool unresolved_=false;
    void accept(const std::vector<const SolvedContact*>&points);
public:
    FirstBarrierContactCollector(std::uint64_t id,const BarrierSettings&barrier,double slop,double mass,double cell=1);
    // Complete Update observations only, after CCD cancellations and final lambda reads.
    void observe(const std::vector<SolvedContact>&records,bool lostObservations=false);
    void finish(StopReason reason);
    const FirstContactEvent& event()const{return event_;}
};
class FirstContactAccumulator {
    Grid3D grid_;
    bool enabled_;
    std::uint64_t requested_,hitsTotal_=0,misses_=0;
    std::vector<std::uint64_t> hits_;
public:
    FirstContactAccumulator(const BarrierSettings&barrier,std::uint64_t n,double cell=1);
    void finish(const FirstContactEvent&event);
    const Grid3D& grid()const{return grid_;}
    const std::vector<std::uint64_t>& hits()const{return hits_;}
    bool enabled()const{return enabled_;}
    std::uint64_t requested()const{return requested_;}
    std::uint64_t hitTrials()const{return hitsTotal_;}
    std::uint64_t missTrials()const{return misses_;}
    std::uint64_t known()const{return hitsTotal_+misses_;}
    std::uint64_t unknown()const{return enabled_?requested_-known():0;}
    Probability probability(std::uint64_t h,bool filesComplete)const;
    std::optional<double> conditional(std::uint64_t h,bool filesComplete)const;
};
// Validates exported/imported local/world geometry and recomputes the unique cell.
void validateFirstContact(FirstContactEvent&event,const BarrierSettings&barrier,const Grid3D&grid);
}
