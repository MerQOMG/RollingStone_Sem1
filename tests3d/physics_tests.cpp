#include "jolt_runtime.h"
#include "trial.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <map>

using namespace rs3d;
namespace {
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
Polyhedron box(Vec3 half={0.5,0.5,0.5}){std::vector<Vec3>v;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})v.push_back({x*half.x,y*half.y,z*half.z});return convexPolyhedron(v);}
std::shared_ptr<const TerrainMesh> plane(bool flipped=false,double slope=0,double length=20,double width=20){TerrainSettings s;s.type=TerrainType::flat;s.flatLength=length;s.width=width;s.flatSlopeX=slope;s.flatHeight=slope==0?0:5;s.flipDiagonals=flipped;return std::make_shared<TerrainMesh>(buildTerrain(s));}
PhysicsScene scene(bool ground=true){PhysicsScene s;s.shape=box();s.radius=std::sqrt(.75);if(ground)s.terrain=plane();s.dynamics.dragCoefficient=0;s.dynamics.rollingCoefficient=0;s.dynamics.friction=0;s.dynamics.restitution=.45;s.initial.shapeOrigin={10,0,5};return s;}
const Snapshot&step(Trial&t,double dt=.002){auto&s=t.step(dt);if(s.reason==StopReason::numericFailure)throw std::runtime_error(s.error);require(s.reason==StopReason::running||normalCompletion(s.reason)||s.reason==StopReason::timeout,"unexpected trial state");return s;}
double energy(const Snapshot&s){return s.kineticEnergy+s.potentialEnergy;}
double angle(Quaternion a,Quaternion b){double da=a.w*a.w+a.x*a.x+a.y*a.y+a.z*a.z,db=b.w*b.w+b.x*b.x+b.y*b.y+b.z*b.z;return 2*std::acos(std::clamp(std::abs(a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z)/std::sqrt(da*db),0.0,1.0));}
double penetration(const PhysicsScene&s,const Snapshot&state){double low=1e300;auto rot=state.rotation.matrix();for(auto p:s.shape.vertices){auto world=state.shapeOrigin+rot*(p-s.shape.center);low=std::min(low,world.z);}return std::max(0.0,-low);}
void d1(){
    auto s=scene(false);s.shape=box({.4,.6,.8});s.radius=norm(Vec3{.4,.6,.8});s.initial.angularVelocity={1.2,2.1,.7};
    Trial t(s,{2,true});auto initial=t.snapshot();double maxL=0;
    for(int i=0;i<500;++i){auto&now=step(t);maxL=std::max(maxL,norm(now.angularMomentum-initial.angularMomentum)/norm(initial.angularMomentum));}
    auto now=t.snapshot();double vError=norm(now.velocity-Vec3{0,0,-9.81})/9.81,zError=std::abs(now.center.z-(initial.center.z-4.905));
    std::cout<<"D1 velocity_rel="<<vError<<" position_error_m="<<zError<<" angular_momentum_max_rel="<<maxL<<'\n';
    require(vError<=.001&&zError<=.02,"D1 free-fall error");require(maxL<=.005,"D1 angular momentum drift");
    require(t.diagnostics().updates==500&&t.diagnostics().internalSteps==2000,"Update/substep accounting");
}
double impact(double speed,double e,bool tilted=false){
    auto s=scene();s.dynamics.gravity={};s.dynamics.restitution=e;s.dynamics.allowSleeping=false;
    s.initial=supportedRelease(s.shape,*s.terrain,10,0,speed<.15?.005:.04,tilted?Quaternion::fromAxisAngle({0,1,0},.2):Quaternion{});s.initial.velocity={0,0,-speed};
    Trial t(s,{2,true});double rebound=0,wy=0,observed=-1;bool contact=false;
    for(int i=0;i<400;++i){auto&state=step(t);if(state.contacts.ground){contact=true;observed=state.contacts.groundRestitution;}rebound=std::max(rebound,state.velocity.z);wy=std::min(wy,state.angularVelocity.y);}
    require(contact&&std::abs(observed-e)<1e-6,"D2 effective pair material callback");
    std::cout<<"D2 speed="<<speed<<" target_e="<<e<<" measured_e="<<rebound/speed<<" tilted="<<tilted<<" min_omega_y="<<wy<<'\n';
    if(tilted)require(wy<-.01,"D2 eccentric angular momentum direction");else if(speed>.15)require(std::abs(rebound/speed-e)<=.02,"D2 restitution mismatch");else require(rebound<.005,"D2 below threshold restitution bias");
    return rebound/speed;
}
void d2(){impact(2,.45);impact(5,.45);impact(.1,1);impact(2,.45,true);}
void d3(){
    auto s=scene();s.dynamics.friction=.45;s.initial=supportedRelease(s.shape,*s.terrain,10,0,.05);
    Trial t(s,{30,true});double quiet=0,maxEnergy=energy(t.snapshot()),initialEnergy=maxEnergy;
    for(int i=0;i<5000&&quiet<2;++i){auto&state=step(t);maxEnergy=std::max(maxEnergy,energy(state));if(norm(state.velocity)<.05&&norm(state.angularVelocity)<.05)quiet+=.002;else quiet=0;}
    require(quiet>=2,"D3 never settled");auto resting=t.snapshot();double drift=0,rotation=0;
    for(int i=0;i<5000;++i){auto&state=step(t);drift=std::max(drift,norm(state.center-resting.center));rotation=std::max(rotation,angle(state.rotation,resting.rotation));maxEnergy=std::max(maxEnergy,energy(state));}
    std::cout<<"D3 rest_time="<<resting.time<<" subsequent_s=10 position_drift_m="<<drift<<" rotation_degrees="<<rotation*180/pi<<" max_energy_ratio="<<maxEnergy/initialEnergy<<'\n';
    require(drift<=.002&&rotation<=.2*pi/180,"D3 resting drift");require(maxEnergy<=initialEnergy*1.01,"D3 contact energy explosion");
}
void d4(){
    auto a=scene();a.terrain=plane(false,-.2);a.dynamics.restitution=0;a.initial=supportedRelease(a.shape,*a.terrain,4,0,0,Quaternion::fromAxisAngle({0,1,0},std::atan(.2)));a.initial.velocity={3,0,-.6};
    auto b=a;b.terrain=plane(true,-.2);Trial one(a,{3,true}),two(b,{3,true});double maximum=0;
    for(int i=0;i<1000;++i){auto first=step(one);auto second=step(two);maximum=std::max(maximum,norm(first.center-second.center));}
    std::cout<<"D4 flipped_diagonal_max_path_difference_m="<<maximum<<'\n';require(maximum<=.01,"D4 seam trajectory discrepancy");
}
void d5(){
    auto s=scene();s.dynamics.restitution=1;s.dynamics.allowSleeping=false;Trial t(s,{4,true});double e0=energy(t.snapshot()),maximum=e0,depth=0;int hits=0;
    for(int i=0;i<1500;++i){auto&state=step(t);maximum=std::max(maximum,energy(state));depth=std::max({depth,penetration(s,state),state.contacts.maxPenetration});hits+=state.contacts.ground;}
    std::cout<<"D5 elastic_energy_growth="<<maximum/e0-1<<" max_actual_plane_penetration_m="<<depth<<" contacts="<<hits<<'\n';
    require(hits>0&&maximum/e0-1<=.01,"D5 elastic energy growth");require(depth<=std::max(.002,.002*s.radius),"D5 regular penetration");
    for(bool thin:{false,true}){
        auto fast=scene();fast.terrain=plane(false,0,100,80);fast.initial.shapeOrigin={50,0,3};fast.initial.velocity={0,0,-50};
        if(thin){auto generated=generatePolyhedron({10,20,7,20},20261111);fast.shape=generated.geometry;fast.radius=7;fast.initial=supportedRelease(fast.shape,*fast.terrain,50,0,3);fast.initial.velocity={0,0,-50};fast.initial.angularVelocity={2,1,.3};}
        Trial run(fast,{2,true});double deepest=0,lowest=1e300;int contacts=0;
        for(int i=0;i<500;++i){auto&state=step(run);deepest=std::max({deepest,penetration(fast,state),state.contacts.maxPenetration});lowest=std::min(lowest,state.center.z);contacts+=state.contacts.ground;}
        std::cout<<"D5 50mps thin="<<thin<<" max_penetration_m="<<deepest<<" min_COM_z="<<lowest<<" contacts="<<contacts<<'\n';
        require(contacts>0&&lowest>0,"D5 terrain tunnelling");require(deepest<=std::max(.002,.002*fast.radius),"D5 high-speed penetration");
        fast.terrain.reset();fast.dynamics.gravity={};fast.barrier.enabled=true;fast.barrier.center={50,0,0};fast.barrier.width=100;fast.barrier.height=100;fast.initial.shapeOrigin={35,0,0};fast.initial.velocity={50,0,0};
        Trial board(fast,{2,true});deepest=0;contacts=0;
        for(int i=0;i<300;++i){auto&state=step(board);deepest=std::max(deepest,state.contacts.maxPenetration);contacts+=state.contacts.barrier;auto rotation=state.rotation.matrix();
            for(auto p:fast.shape.vertices)deepest=std::max(deepest,state.shapeOrigin.x+(rotation*(p-fast.shape.center)).x-49.9);}
        std::cout<<"D5 50mps board thin="<<thin<<" max_penetration_m="<<deepest<<" contacts="<<contacts<<'\n';
        require(contacts>0&&board.snapshot().center.x<50,"D5 board tunnelling");require(deepest<=std::max(.002,.002*fast.radius),"D5 fast board penetration");
    }
}
double bounce(double dt){auto s=scene();s.dynamics.restitution=.6;s.dynamics.allowSleeping=false;Trial t(s,{3,true});bool bounced=false;double peak=-1;int steps=int(std::lround(2/dt));
    for(int i=0;i<steps;++i){auto&state=step(t,dt);if(state.velocity.z>0)bounced=true;if(bounced)peak=std::max(peak,state.center.z);}
    require(bounced,"D6 no bounce");require(t.diagnostics().internalSteps==4*std::uint64_t(steps),"D6 collision steps must stay four");std::cout<<"D6 dt="<<dt<<" collision_steps=4 peak_z="<<peak<<'\n';return peak-.5;}
void d6(){double coarse=bounce(.002),middle=bounce(.001),fine=bounce(.0005);std::cout<<"D6 coarse_fine_rel="<<std::abs(coarse/fine-1)<<" middle_fine_rel="<<std::abs(middle/fine-1)<<'\n';require(std::abs(coarse/fine-1)<=.03&&std::abs(middle/fine-1)<=.03,"D6 timestep convergence");
    auto generated=generatePolyhedron({},20261111);PhysicsScene s;s.shape=generated.geometry;s.terrain=std::make_shared<TerrainMesh>(buildTerrain({}));s.initial=supportedRelease(s.shape,*s.terrain,10,0,.05);
    std::vector<Vec3>endpoints;for(double dt:{.002,.001,.0005}){Trial run(s,{4,true});for(int i=0;i<int(std::lround(3/dt));++i)step(run,dt);auto p=run.snapshot().center;endpoints.push_back(p);std::cout<<"D6 complex_slope dt="<<dt<<" time=3 COM="<<p.x<<','<<p.y<<','<<p.z<<'\n';}
    std::cout<<"D6 complex_slope endpoint_difference_coarse_fine_m="<<norm(endpoints[0]-endpoints[2])<<" middle_fine_m="<<norm(endpoints[1]-endpoints[2])<<" (reported sensitivity, no long-chaotic pointwise gate)\n";
}
void d7(){
    for(int mode=0;mode<6;++mode){
        auto s=scene(false);s.dynamics.gravity={};s.barrier.enabled=mode!=1;s.barrier.center={10,0,2.5};s.barrier.width=4;s.barrier.height=5;s.barrier.friction=.2;s.barrier.restitution=.35;
        s.initial.shapeOrigin={6,0,2.5};s.initial.velocity={5,0,0};Vec3 normal{1,0,0};
        if(mode==2)s.initial.shapeOrigin.z=6;
        if(mode==3)s.initial.shapeOrigin.y=3;
        // COM outside the board edge, but a finite strip of the rock face still hits.
        // With COM inside the contact patch, the solver can correctly balance zero torque.
        if(mode==4)s.initial.shapeOrigin.y=2.25;
        if(mode==5){s.barrier.center={12,1,3};s.barrier.yawDegrees=25;auto r=axisRotation({0,0,1},25*pi/180);normal=r*normal;s.initial.shapeOrigin=s.barrier.center-normal*4;s.initial.velocity=normal*5;s.initial.rotation=Quaternion::fromAxisAngle({0,0,1},25*pi/180);}
        Trial t(s,{2,true});int hits=0;double omegaZ=0,firstDistance=0,seenMu=-1,seenE=-1;
        for(int i=0;i<600;++i){auto&state=step(t);if(state.contacts.barrier){if(!hits)firstDistance=dot(s.barrier.center-state.center,normal);hits+=state.contacts.barrier;seenMu=state.contacts.barrierFriction;seenE=state.contacts.barrierRestitution;}omegaZ=std::min(omegaZ,state.angularVelocity.z);}
        auto end=t.snapshot();std::cout<<"D7 mode="<<mode<<" hits="<<hits<<" final_normal_v="<<dot(end.velocity,normal)<<" first_distance="<<firstDistance<<" min_omega_z="<<omegaZ<<'\n';
        if(mode==1||mode==2||mode==3){require(hits==0&&end.center.x>11,"D7 infinite-plane barrier or disabled collision");}
        else{require(hits>0&&std::abs(seenMu-.2)<1e-6&&std::abs(seenE-.35)<1e-6,"D7 actual barrier pair material");if(mode==4)require(omegaZ<-.01,"D7 eccentric angular direction");else{require(std::abs(dot(end.velocity,normal)/-5-.35)<=.02,"D7 normal restitution");require(std::abs(firstDistance-.6)<=.02,"D7 edited barrier position/rotation");}}
    }
}
void dissipative(){
    auto s=scene(false);s.initial.angularVelocity={5,1,.2};s.dynamics.rollingCoefficient=100;Trial air(s,{2,true});for(int i=0;i<500;++i)step(air);require(air.snapshot().rollingEnergyRemoved==0,"rolling resistance applied in air");
    double positive=0;
    for(double coefficient:{0.0,0.2,100.0}){
        auto g=scene();g.dynamics.rollingCoefficient=coefficient;g.dynamics.restitution=0;g.initial=supportedRelease(g.shape,*g.terrain,10,0,0);g.initial.angularVelocity={4,1,0};Trial contact(g,{3,true});double last=0;
        for(int i=0;i<1000;++i){auto&state=step(contact);require(state.rollingEnergyRemoved+1e-9>=last,"rolling work negative");last=state.rollingEnergyRemoved;}
        if(coefficient==0)require(last==0,"c_rr=0 must remove no energy");else{require(last>0,"c_rr>0 removed no energy in contact");positive=last;}
        std::cout<<"RR coefficient="<<coefficient<<" removed_rotational_energy_J="<<last<<'\n';
    }
    auto drag=scene(false);drag.dynamics.gravity={};drag.dynamics.dragCoefficient=1;drag.initial.velocity={10,0,0};Trial falling(drag,{2,true});double e0=energy(falling.snapshot());for(int i=0;i<500;++i)step(falling);require(energy(falling.snapshot())<e0&&falling.snapshot().airWork<0,"air drag did not dissipate");
    std::cout<<"PASS airborne/zero/positive rolling resistance and air work; capped RR removed="<<positive<<'\n';
    double withoutFriction=0;
    for(double mu:{0.0,0.6}){auto g=scene();g.dynamics.friction=mu;g.dynamics.restitution=0;g.initial=supportedRelease(g.shape,*g.terrain,10,0,0);g.initial.velocity={3,0,0};Trial slide(g,{1,true});for(int i=0;i<100;++i)step(slide);
        double vx=slide.snapshot().velocity.x;std::cout<<"friction pair_mu="<<mu<<" vx_after_0.2s="<<vx<<'\n';if(mu==0)withoutFriction=vx;else require(vx<withoutFriction-.2,"ground pair friction did not affect tangential response");}
}
void termination(){
    auto s=scene();s.dynamics.restitution=0;s.initial=supportedRelease(s.shape,*s.terrain,10,0,0);Trial rest(s,{10,false});while(rest.snapshot().reason==StopReason::running)step(rest);require(rest.snapshot().reason==StopReason::resting&&rest.snapshot().time>=2,"rest must persist two seconds");
    auto moving=scene();moving.dynamics.gravity={};moving.initial.shapeOrigin={1,0,2};moving.initial.velocity={-5,0,0};Trial upstream(moving);while(upstream.snapshot().reason==StopReason::running)step(upstream);require(upstream.snapshot().reason==StopReason::upstream,"upstream must not time out");
    Trial timeout(scene(false),{.01,false});for(int i=0;i<5;++i)step(timeout);require(timeout.snapshot().reason==StopReason::timeout,"timeout reason");
    Trial cancel(scene(false));cancel.cancel();double time=cancel.snapshot().time;cancel.step();require(cancel.snapshot().reason==StopReason::cancelled&&cancel.snapshot().time==time,"cancel advanced simulation");
    for(bool lateral:{false,true}){auto departing=scene();departing.dynamics.gravity={};departing.initial.shapeOrigin={19,0,2};departing.initial.velocity={5,0,0};if(lateral){departing.initial.shapeOrigin={10,9,2};departing.initial.velocity={0,5,0};}Trial run(departing);while(run.snapshot().reason==StopReason::running)step(run);require(run.snapshot().reason==(lateral?StopReason::lateral:StopReason::downstream),"absorbing boundary reason");}
    auto overlap=scene(false);overlap.barrier.enabled=true;overlap.barrier.center=overlap.initial.shapeOrigin;bool rejected=false;try{Trial invalid(overlap);}catch(const std::exception&e){rejected=std::string(e.what()).find("penetrates barrier")!=std::string::npos;}require(rejected,"initial barrier overlap accepted");
    overlap=scene();overlap.initial.shapeOrigin.z=.4;rejected=false;try{Trial invalid(overlap);}catch(const std::exception&e){rejected=std::string(e.what()).find("penetrates terrain")!=std::string::npos;}require(rejected,"initial terrain overlap accepted");
    std::cout<<"PASS termination resting/upstream/downstream/lateral/timeout/cancel and initial overlap rejection\n";
}
}
int main(int argc,char**argv){JoltRuntime runtime;std::cout<<std::setprecision(12);std::map<std::string,std::function<void()>>tests{{"D1",d1},{"D2",d2},{"D3",d3},{"D4",d4},{"D5",d5},{"D6",d6},{"D7",d7},{"dissipation",dissipative},{"termination",termination}};int failures=0,ran=0;
    for(auto&test:tests){if(argc>1&&test.first!=argv[1])continue;++ran;try{test.second();std::cout<<"PASS "<<test.first<<'\n';}catch(const std::exception&e){++failures;std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<'\n';}std::cout.flush();}
    std::cout<<"physics tests="<<ran<<" failed="<<failures<<'\n';return failures||!ran?1:0;
}
