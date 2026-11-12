#include "trial.h"
#include "statistics.h"
#include "hash.h"
#include <set>
#include <tuple>
#include "jolt_runtime.h"
#include "contact_observer.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <fstream>
#include <iostream>
#include <iomanip>
using namespace rs3d;
namespace {
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
Polyhedron box(double x=.5){std::vector<Vec3> v;for(int a:{-1,1})for(int b:{-1,1})for(int c:{-1,1})v.push_back({a*x,b*.5,c*.5});return convexPolyhedron(v);}
PhysicsScene make(int mode){PhysicsScene s;s.shape=box(mode==4||mode==5?.005:.5);s.radius=1;s.dynamics.gravity={};s.dynamics.density=2000;s.dynamics.dragCoefficient=0;s.dynamics.rollingCoefficient=0;
    s.barrier.enabled=true;s.barrier.center={10,0,5};s.barrier.width=6;s.barrier.height=6;s.barrier.friction=0;s.barrier.restitution=.9;s.barrier.elastic=mode==2||mode==5;
    s.initial.shapeOrigin={8.013,0,5};s.initial.velocity={50,0,0};
    if(mode==0){s.initial.shapeOrigin.x=9.39995;s.initial.velocity.x=.01;}
    if(mode==3){s.initial.shapeOrigin={9.39,0,5};s.initial.velocity={0,20,0};}
    if(mode==4){s.initial.shapeOrigin.x=9.873;s.initial.velocity.x=50;}
    if(mode==5){s.initial.shapeOrigin.x=9.80;s.initial.velocity.x=500;}
    if(mode==6){TerrainSettings t;t.type=TerrainType::flat; t.flatLength=30;t.width=20;s.terrain=std::make_shared<TerrainMesh>(buildTerrain(t));s.initial.shapeOrigin.z=.5;s.barrier.center.z=3;s.dynamics.gravity={0,0,-9.81};}
    return s;
}
class Broad final:public JPH::BroadPhaseLayerInterface {public:JPH::uint GetNumBroadPhaseLayers()const override{return 1;}JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer)const override{return JPH::BroadPhaseLayer(0);}};
class Pair final:public JPH::ObjectLayerPairFilter {public:bool ShouldCollide(JPH::ObjectLayer,JPH::ObjectLayer)const override{return true;}};
class Filter final:public JPH::ObjectVsBroadPhaseLayerFilter {public:bool ShouldCollide(JPH::ObjectLayer,JPH::BroadPhaseLayer)const override{return true;}};
void cancellation(bool enabled,bool expect,std::ofstream&trace){
    Broad broad;Pair pair;Filter filter;JPH::PhysicsSystem world;world.Init(16,0,1024,1024,broad,filter,pair);world.SetGravity(JPH::Vec3::sZero());
    ContactObserver observer;world.AddStepListener(&observer);JPH::TempAllocatorImpl memory(10*1024*1024);JPH::JobSystemSingleThreaded jobs(JPH::cMaxPhysicsJobs);
    std::vector<JPH::BodyID> ids;auto&bi=world.GetBodyInterface();
    for(int i=0;i<3;++i){JPH::RefConst<JPH::Shape> shape=new JPH::BoxShape(JPH::Vec3::sReplicate(.05f),0);
        JPH::BodyCreationSettings b(shape,JPH::RVec3(i==0?.5:i==1?1.:2.,0,0),JPH::Quat::sIdentity(),i==2?JPH::EMotionType::Static:JPH::EMotionType::Dynamic,0);
        b.mLinearVelocity=JPH::Vec3(i==0?4000.f:i==1?2000.f:0,0,0);b.mMaxLinearVelocity=10000;b.mLinearDamping=0;b.mAngularDamping=0;b.mMotionQuality=JPH::EMotionQuality::LinearCast;b.mUserData=i+10;
        ids.push_back(bi.CreateAndAddBody(b,i==2?JPH::EActivation::DontActivate:JPH::EActivation::Activate));}
    world.OptimizeBroadPhase();observer.begin(0,0);rs3d_observer::sink=enabled?&observer:nullptr;auto error=world.Update(.001f,1,&memory,&jobs);rs3d_observer::sink=nullptr;check(error==JPH::EPhysicsUpdateError::None,"CCD cancellation update error");
    int discarded=0,resolved=0;for(const auto&r:observer.records()){if(r.source==ContactSource::ccdDiscarded){++discarded;check(!r.retained,"discarded candidate retained");}if(r.source==ContactSource::ccdResolved)++resolved;}
    std::cout<<"CCD cancellation discarded="<<discarded<<" resolved="<<resolved<<'\n';if(enabled&&expect)check(discarded>0&&resolved>0,"CCD cancellation branch not exercised");
    for(auto id:ids){auto p=bi.GetPosition(id);auto v=bi.GetLinearVelocity(id);if(trace)trace<<"cancel,"<<id.GetIndex()<<','<<p.GetX()<<','<<p.GetY()<<','<<p.GetZ()<<','<<v.GetX()<<','<<v.GetY()<<','<<v.GetZ()<<'\n';bi.RemoveBody(id);bi.DestroyBody(id);}world.RemoveStepListener(&observer);
}
}
int main(int argc,char**argv){try{JoltRuntime runtime;bool enabled=argc<2||std::string(argv[1])!="off";bool expect=argc<2||std::string(argv[1])!="upstream";
    std::ofstream trace;if(argc>2){trace.open(argv[2]);trace<<std::setprecision(17);}
    for(int mode=0;mode<7;++mode){auto s=make(mode);s.dynamics.observeContacts=enabled;Trial t(s,{1,true});int ordinary=0,ccd=0,geometric=0,candidates=0,discarded=0,ground=0;double positiveGap=0,maxImpulse=0;std::array<int,3>thresholdCounts{};
        Statistics stats({{-10,-10,-2},{30,10,12},{1,1,1}},15,1);auto visits=stats.newTrial();auto previous=t.snapshot().center;
        for(int i=0;i<150;++i){auto state=t.step();check(state.reason!=StopReason::numericFailure,state.error.c_str());check(!t.diagnostics().observerOverflow,"observer overflow/missing binding");
            if(trace)trace<<mode<<','<<i<<','<<state.center.x<<','<<state.center.y<<','<<state.center.z<<','<<state.velocity.x<<','<<state.velocity.y<<','<<state.velocity.z<<','<<state.rotation.w<<','<<state.rotation.x<<','<<state.rotation.y<<','<<state.rotation.z<<','<<state.angularVelocity.x<<','<<state.angularVelocity.y<<','<<state.angularVelocity.z<<','<<state.barrier.displacement<<','<<state.barrier.velocity<<','<<state.kineticEnergy<<','<<state.contacts.ground<<','<<state.contacts.barrier<<'\n';
            visits.visitSegment(previous,state.center);previous=state.center;
            std::set<std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned,double,double,double>> seen;
            for(auto&r:t.observations()){
                if(r.source==ContactSource::constraint)check(seen.emplace(r.substep,r.body1,r.body2,r.subshape1,r.subshape2,r.point1.x,r.point1.y,r.point1.z).second,"final lambda read twice for same point");check(r.substep>=1&&r.substep<=4&&r.step==unsigned(i)&&r.timeUpper>r.timeLower,"wrong observer step");
                if(r.object1==terrainObjectId||r.object2==terrainObjectId){++ground;continue;}
                if(!((r.object1==rockObjectId&&r.object2==barrierObjectId)||(r.object2==rockObjectId&&r.object1==barrierObjectId)))continue;
                ++candidates;check(finite(r.point1)&&finite(r.point2)&&std::isfinite(r.normalImpulse),"nonfinite contact observation");
                if(r.source==ContactSource::ccdDiscarded)++discarded;
                if(!r.retained)continue;
                double threshold=std::max(1e-6,1e-8*t.diagnostics().mass);
                for(int j=0;j<3;++j)if(r.normalImpulse>threshold*std::pow(10.,j-1))++thresholdCounts[j];
                if(r.gap<=t.diagnostics().slop)++geometric;
                if(r.normalImpulse>threshold){if(r.source==ContactSource::constraint)++ordinary;else if(r.source==ContactSource::ccdResolved)++ccd;positiveGap=std::max(positiveGap,r.gap);maxImpulse=std::max(maxImpulse,r.normalImpulse);}
            }
        }
        stats.finishTrial(visits,true);ContentHash counts;for(const auto*table:{&stats.voxelHits(),&stats.xyHits(),&stats.xzHits(),&stats.collectorHits()})for(auto value:*table)counts.word(value);if(trace)trace<<"statistics,"<<mode<<','<<counts.hex()<<'\n';
        std::cout<<std::setprecision(12)<<"mode="<<mode<<" ordinary="<<ordinary<<" resolved_ccd="<<ccd<<" geometric="<<geometric<<" candidates="<<candidates<<" discarded="<<discarded<<" ground="<<ground<<" positive_gap="<<positiveGap<<" max_lambda="<<maxImpulse<<" thresholds="<<thresholdCounts[0]<<'/'<<thresholdCounts[1]<<'/'<<thresholdCounts[2]<<'\n';
        if(enabled&&expect){if(mode==3)check(ordinary+ccd+geometric==0&&candidates>0,"predicted tangential pass was confirmed");else check(ordinary+ccd+geometric>0,"real hit lacks confirmation");
            if(mode==1)check(ordinary>0&&positiveGap>t.diagnostics().slop,"high restitution positive-gap response missing");
            if(mode==4||mode==5)check(ccd>0,"CCD response path not covered");
            if(mode==6)check(ground>0,"simultaneous ground missing");}
    }
    cancellation(enabled,expect,trace);std::cout<<"PASS solver observation fixtures\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
