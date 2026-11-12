#include "config.h"
#include "trial.h"
#include "obstacle_io.h"
#include "jolt_runtime.h"
#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdarg>
#include <cstdio>
using namespace rs3d;
namespace {
void trace(const char*format,...){va_list args;va_start(args,format);vfprintf(stderr,format,args);fputc('\n',stderr);va_end(args);}
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&e){failed=true;std::cout<<"expected rejection: "<<e.what()<<'\n';}check(failed,"invalid case accepted");}
Polyhedron box(Vec3 half){std::vector<Vec3> v;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})v.push_back({x*half.x,y*half.y,z*half.z});return convexPolyhedron(v);}
std::uint64_t memory(){PROCESS_MEMORY_COUNTERS_EX p{};p.cb=sizeof(p);check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&p),sizeof(p))!=0,"memory query");return p.PrivateUsage;}
SceneDraft draft(){SceneDraft s;s.obstacles.enabled=true;s.barrier.enabled=true;s.properties[releaseX]={10,12};s.properties[releaseY]={-2,2};return s;}
void geometry(){
    TerrainMesh t;t.xMin=-2;t.xEnd=2;t.yMin=-2;t.yMax=2;t.vertices={{-2,-2,0},{2,-2,0},{-2,0,1},{2,0,1},{-2,2,0},{2,2,0}};t.faces={{0,1,3},{0,3,2},{2,3,5},{2,5,4}};
    auto f=rectangleFootprint(0,0,1,1,0);auto z=terrainHeightRange(t,{f.begin(),f.end()});check(z.first==.75&&z.second==1,"edge-edge intersection extremum omitted");
    f=rectangleFootprint(0,.25,1,.5,0);z=terrainHeightRange(t,{f.begin(),f.end()});check(z.first==.75&&z.second==1,"collinear clipping failed");
    rejects([&]{terrainHeightRange(t,{{0,0,0},{.1,0,0},{.2,0,0}});});
    auto s=draft();s.terrain.crossSlopeDegrees=9;s.terrain.grooveAmplitude=3;s.heightReference=HeightReference::toeMin;auto c=freezeConfig(s);
    for(const auto&o:c->obstacles->boxes){auto foot=o.footprint();auto range=terrainHeightRange(*c->terrain,{foot.begin(),foot.end()});check(std::abs((o.center.z-o.dimensions.z/2)-(range.first-.03))<1e-10,"buried base");check(std::abs((o.center.z+o.dimensions.z/2)-(range.second+o.exposedHeight))<1e-10,"exposed top");for(const auto&p:c->obstacles->boxes)if(o.id!=p.id)check(footprintDistance(foot,p.footprint())>=.20-1e-10,"spacing");}
    s.obstacles.automaticRegion=false;s.obstacles.xMin=11;s.obstacles.xMax=11.1;s.obstacles.yMin=0;s.obstacles.yMax=.1;rejects([&]{freezeConfig(s);});
    s.obstacles.count=0;check(freezeConfig(s)->obstacles->boxes.empty(),"zero obstacles rejected");s.obstacles.count=501;rejects([&]{freezeConfig(s);});
    std::cout<<"O3 exact clipping, edge-only extrema, collinear and invalid/empty layouts PASS\n";
}
void deterministic(const std::filesystem::path&folder){
    auto s=draft();auto first=freezeConfig(s);auto hash=first->obstacles->hash;
    for(auto n:{2,3,1000}){s.count=n;auto c=freezeConfig(s);check(c->obstacles->hash==hash&&c->environmentHash==first->environmentHash,"N changes environment");}
    auto saved=sceneText(s);check(freezeConfig(parseScene(saved))->obstacles->hash==hash,"restart scene changed layout");
    std::filesystem::create_directories(folder);std::map<std::string,std::ofstream>files;
    writeObstacleFiles(*first->obstacles,[&](const std::string&name,const std::string&text){auto&f=files[name];if(!f.is_open())f.open(folder/name,std::ios::binary);f<<text;});for(auto&f:files)f.second.close();
    auto restored=readObstacleFiles(folder,first->obstacles->inputHash,hash);check(restored->hash==hash,"obstacle reload");check(freezeConfig(s,restored)->obstacles==restored,"saved geometry was resampled");
    auto source=sampleTrialInput(*first,1);s.obstacles.seed=0;auto changed=freezeConfig(s);check(changed->obstacles->hash!=hash,"seed zero ignored");auto input=sampleTrialInput(*changed,1);check(input.seed==source.seed&&input.kSeed==source.kSeed&&input.propertySeed==source.propertySeed&&input.values==source.values,"obstacle RNG leaks into rock sample");
    s.obstacles.enabled=false;auto off=freezeConfig(s);input=sampleTrialInput(*off,1);check(input.values==source.values&&input.seed==source.seed,"obstacle switch leaks RNG");
    for(std::uint64_t id=1;id<=3;++id){
        auto a=sampleTrialInput(*first,id),b=sampleTrialInput(*changed,id),d=sampleTrialInput(*off,id);
        auto pa=generatePolyhedron(first->source.geometry,a.seed).geometry,pb=generatePolyhedron(changed->source.geometry,b.seed).geometry,pd=generatePolyhedron(off->source.geometry,d.seed).geometry;
        check(pa.vertices.size()==pb.vertices.size()&&pa.vertices.size()==pd.vertices.size(),"obstacle changes actual rock vertex count");
        for(size_t i=0;i<pa.vertices.size();++i)check(norm(pa.vertices[i]-pb.vertices[i])==0&&norm(pa.vertices[i]-pd.vertices[i])==0,"obstacle changes actual rock vertices");
        auto sa=makePhysicsScene(*first,a,pa),sb=makePhysicsScene(*changed,b,pb),sd=makePhysicsScene(*off,d,pd);
        check(a.values==b.values&&a.values==d.values&&norm(sa.initial.shapeOrigin-sb.initial.shapeOrigin)==0&&norm(sa.initial.shapeOrigin-sd.initial.shapeOrigin)==0&&frobenius(sa.initial.rotation.matrix()-sb.initial.rotation.matrix())==0&&frobenius(sa.initial.rotation.matrix()-sd.initial.rotation.matrix())==0,"obstacle changes physical input/initial state");
    }
    // Preview count is only a notification policy. Construct every trial against
    // the shared immutable layout, including the largest requested batch.
    for(int preview:{0,2})for(std::uint64_t n:{2,3,1000}){auto all=draft();all.count=n;auto c=freezeConfig(all);auto p=box({.5,.5,.5});
        for(std::uint64_t id=1;id<=n;++id){auto scene=makePhysicsScene(*c,sampleTrialInput(*c,id),p);check(scene.obstacles==c->obstacles&&scene.obstacles->hash==hash,"trial replaced frozen layout");Trial trial(scene,{.002,true});auto actual=trial.obstacleReadback();check(actual.size()==c->obstacles->boxes.size(),"trial obstacle count differs");for(size_t i=0;i<actual.size();++i){const auto&expected=c->obstacles->boxes[i];check(norm(actual[i].center-expected.center)<1e-12&&frobenius(actual[i].rotation.matrix()-expected.rotation.matrix())<1e-6&&std::abs(actual[i].friction-expected.friction)<1e-7&&std::abs(actual[i].restitution-expected.restitution)<1e-7,"trial body differs from frozen layout");}}
        std::cout<<"O1 body readback N="<<n<<" preview="<<preview<<" PASS\n";
    }
    std::cout<<"O1/O2 frozen N=2/3/1000, restart, actual geometry reload, independent seed=0 PASS hash="<<hash<<'\n';
}
void capacity(){
    for(int count:{0,40,500}){auto s=draft();s.terrain.type=TerrainType::flat;s.terrain.flatLength=100;s.terrain.width=100;s.terrain.cellSize=2;s.barrier.enabled=false;s.obstacles.count=count;s.obstacles.automaticRegion=false;s.obstacles.xMin=5;s.obstacles.xMax=95;s.obstacles.yMin=-45;s.obstacles.yMax=45;
        auto c=freezeConfig(s);auto scene=makePhysicsScene(*c,sampleTrialInput(*c,1),box({.5,.5,.5}));auto before=memory();Trial trial(scene,{1,true});auto allocation=memory()-before;auto initial=trial.obstacleReadback();check(initial.size()==size_t(count),"body lost during allocation");for(int i=0;i<10;++i)check(trial.step().reason!=StopReason::numericFailure,"capacity update overflow");auto final=trial.obstacleReadback();
        for(size_t i=0;i<initial.size();++i){check(norm(initial[i].center-final[i].center)==0&&frobenius(initial[i].rotation.matrix()-final[i].rotation.matrix())==0,"static obstacle moved");check(norm(initial[i].center-c->obstacles->boxes[i].center)<1e-12&&norm(initial[i].dimensions-c->obstacles->boxes[i].dimensions)<1e-6,"actual body geometry differs");check(std::abs(initial[i].friction-.45)<1e-7&&std::abs(initial[i].restitution-.20)<1e-7,"actual body material differs");}
        auto d=trial.diagnostics();std::cout<<"O5 count="<<count<<" bodies="<<d.capacity.bodies<<" pairs="<<d.capacity.pairs<<" contacts="<<d.capacity.contacts<<" temp_bytes="<<d.temporaryAllocatorBytes<<" temp_peak_bytes="<<d.temporaryAllocatorPeakBytes<<" measured_private_allocation_delta="<<allocation<<'\n';
        scene.diagnosticBodyCapacity=1;rejects([&]{Trial bad(scene);});}
}
std::shared_ptr<const ObstacleSet> manual(Vec3 center,Vec3 dims,double restitution=.2){auto set=std::make_shared<ObstacleSet>();Obstacle o;o.center=center;o.dimensions=dims;o.geometry=box(dims*.5);o.exposedHeight=dims.z;o.restitution=restitution;set->boxes.push_back(o);set->hash=obstacleContentHash(*set);return set;}
void collision(){
    for(auto object:{terrainObjectId,barrierObjectId,firstObstacleObjectId}){auto a=contactMaterial(rockObjectId,.731f,.817f,object,.45f,.20f),b=contactMaterial(object,.45f,.20f,rockObjectId,.731f,.817f);check(a.recognized&&b.recognized&&a.friction==b.friction&&a.restitution==b.restitution&&a.restitution==.20f,"asymmetric material");}
    check(!contactMaterial(barrierObjectId,.4f,0,firstObstacleObjectId,.4f,.2f).recognized,"obstacle-barrier material accidentally enabled");
    for(int mode=0;mode<4;++mode){PhysicsScene s;s.shape=box({.5,.5,.5});s.radius=1;s.dynamics.gravity={};s.dynamics.dragCoefficient=0;s.dynamics.rollingCoefficient=0;s.initial.shapeOrigin={3,0,2};s.initial.velocity={50,0,0};s.obstacles=manual({5,0,2},{.3,3,3},.8);
        if(mode==1)s.initial.shapeOrigin.z=5;if(mode==2)s.initial.shapeOrigin.y=3;if(mode==3)s.initial.shapeOrigin.y=1.7;
        Trial trial(s,{1,true});int contacts=0;double minV=50,maxOmega=0,maxDepth=0;auto initialBodies=trial.obstacleReadback();for(int i=0;i<100;++i){auto r=trial.step();check(r.reason!=StopReason::numericFailure,"obstacle impact numeric failure");contacts+=r.contacts.obstacle;minV=std::min(minV,r.velocity.x);maxOmega=std::max(maxOmega,norm(r.angularVelocity));if(mode==0){check(r.center.x<5,"50 m/s tunneled through obstacle");for(auto v:s.shape.vertices)maxDepth=std::max(maxDepth,(r.shapeOrigin+r.rotation.matrix()*(v-s.shape.center)).x-4.85);}}
        auto finalBodies=trial.obstacleReadback();check(norm(initialBodies[0].center-finalBodies[0].center)==0&&frobenius(initialBodies[0].rotation.matrix()-finalBodies[0].rotation.matrix())==0,"collision moved static obstacle");check(maxDepth<=.002,"obstacle penetration exceeds existing 2mm limit");
        if(mode==0)check(contacts>0&&minV<0,"obstacle had no rebound");else if(mode<3)check(contacts==0&&trial.snapshot().center.x>5,"finite obstacle became infinite wall");else check(contacts>0&&maxOmega>0.1,"edge obstacle had no deflection");
        std::cout<<"O4 mode="<<mode<<" contacts="<<contacts<<" min_v="<<minV<<" max_depth="<<maxDepth<<" max_omega="<<maxOmega<<'\n';
        auto overlap=s;overlap.initial.shapeOrigin={5,0,2};rejects([&]{Trial bad(overlap);});}
    PhysicsScene s;s.shape=box({.5,.5,.5});s.radius=1;s.dynamics.dragCoefficient=0;s.dynamics.rollingCoefficient=.1;s.initial.shapeOrigin={5,0,5};s.obstacles=manual({5,0,1.5},{5,5,3},0);Trial supported(s,{10});while(supported.snapshot().reason==StopReason::running)supported.step();check(supported.snapshot().reason==StopReason::resting&&supported.snapshot().center.z>3,"obstacle top did not support rest");
    s.initial.shapeOrigin={-100,0,10};s.dynamics.gravity={};s.barrier.enabled=true;s.barrier.elastic=true;s.barrier.center={5,0,2};s.barrier.initialVelocity=1;s.barrier.stiffness=0;s.barrier.damping=0;Trial filter(s,{1,true});for(int i=0;i<100;++i)filter.step();check(filter.snapshot().barrier.contactImpulse==0&&!filter.snapshot().barrier.touched,"obstacle injected elastic support impulse");
    TerrainSettings ground;ground.type=TerrainType::flat;ground.flatLength=20;ground.width=20;
    PhysicsScene dock;s.shape=box({.5,.5,.5});dock.shape=s.shape;dock.radius=1;dock.terrain=std::make_shared<TerrainMesh>(buildTerrain(ground));dock.dynamics.dragCoefficient=0;dock.dynamics.restitution=0;dock.dynamics.rollingCoefficient=.1;dock.initial.shapeOrigin={4.34,0,.5};dock.initial.velocity={.5,0,0};dock.obstacles=manual({5,0,1.5},{.3,5,3},0);
    Trial docking(dock,{10});int touches=0;while(docking.snapshot().reason==StopReason::running){auto r=docking.step();touches+=r.contacts.obstacle;}check(touches>0&&docking.snapshot().reason==StopReason::resting,"low-speed large obstacle docking failed");
    std::cout<<"O4 support rest and obstacle/elastic filtering PASS\n";
}
}
int main(int argc,char**argv){try{JoltRuntime runtime;JPH::Trace=trace;std::cout<<std::unitbuf<<std::setprecision(12);geometry();deterministic(argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/"rs3d_obstacles");capacity();collision();std::cout<<"PASS obstacle core tests\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
