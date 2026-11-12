#include "trial.h"
#include "jolt_runtime.h"
#include <iostream>
#include <iomanip>
using namespace rs3d;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
Polyhedron cube(){std::vector<Vec3>v;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})v.push_back({x*.5,y*.5,z*.5});return convexPolyhedron(v);}
PhysicsScene scene(){PhysicsScene s;s.shape=cube();s.radius=1;s.dynamics.gravity={};s.dynamics.density=2000;s.dynamics.dragCoefficient=0;s.dynamics.rollingCoefficient=0;
 s.initial.shapeOrigin={-100,0,5};s.barrier.enabled=true;s.barrier.elastic=true;s.barrier.center={10,0,5};s.barrier.width=6;s.barrier.height=6;s.barrier.mass=2000;s.barrier.stiffness=500000;s.barrier.damping=30000;s.barrier.friction=0;return s;}
Snapshot advance(Trial&t,int steps,double dt){for(int i=0;i<steps;++i){t.step(dt);check(t.snapshot().reason!=StopReason::numericFailure,t.snapshot().error.c_str());}return t.snapshot();}
double oscillator(double dt){auto s=scene();s.barrier.initialDisplacement=.4;s.barrier.initialVelocity=.2;Trial t(s,{5,true});auto r=advance(t,int(1/dt),dt).barrier;
 double a=s.barrier.damping/(2*s.barrier.mass),w=std::sqrt(s.barrier.stiffness/s.barrier.mass-a*a);
 double expected=std::exp(-a)*(s.barrier.initialDisplacement*std::cos(w)+(s.barrier.initialVelocity+a*s.barrier.initialDisplacement)/w*std::sin(w));
 double error=std::abs(r.displacement-expected);std::cout<<"E1 oscillator dt="<<dt<<" q="<<r.displacement<<" analytic="<<expected<<" abs_error="<<error<<" damp_J="<<r.dampingEnergy<<" support_residual_J="<<r.supportWorkResidual<<'\n';
 check(error<.0002,"oscillator does not match damped analytical solution");check(r.dampingEnergy>0,"damper did not dissipate");check(!r.touched&&r.contactImpulse==0,"free oscillator reported fictitious contact");
 double initial=.5*s.barrier.mass*.2*.2+.5*s.barrier.stiffness*.4*.4;
 check(std::abs((r.kineticEnergy+r.springEnergy+r.dampingEnergy+r.supportWorkResidual)/initial-1)<1e-5,"free support work/energy bookkeeping");return error;}
void impact(double yaw){auto s=scene();s.barrier.yawDegrees=yaw;s.barrier.stiffness=0;s.barrier.damping=0;auto n=barrierAxis(s.barrier);s.initial.shapeOrigin=s.barrier.center-n*2;s.initial.velocity=n*5;s.initial.rotation=Quaternion::fromAxisAngle({0,0,1},yaw*pi/180);Trial t(s,{1,true});auto r=advance(t,180,.002);double v=dot(r.velocity,n);
 std::cout<<"E2 yaw="<<yaw<<" rock_v="<<v<<" plate_v="<<r.barrier.velocity<<" impulse="<<r.barrier.contactImpulse<<'\n';
 check(std::abs(v-2.5)<.025&&std::abs(r.barrier.velocity-2.5)<.025,"finite-mass inelastic impact is not coupled");
 check(std::abs((2000*v+2000*r.barrier.velocity)/10000-1)<1e-5,"axial momentum not conserved");
 check(norm(r.barrier.center-(s.barrier.center+n*r.barrier.displacement))<1e-5,"guide transverse drift");
 check(r.barrier.displacement>.02&&r.barrier.touched,"barrier did not move after impact");
 check(std::abs(r.barrier.contactImpulse-5000)<50,"contact impulse readback");}
void finiteAndEnergy(){
 for(int mode=0;mode<4;++mode){auto s=scene();s.initial.shapeOrigin={7,0,5};s.initial.velocity={10,0,0};
    if(mode==0)s.barrier.enabled=false;if(mode==1)s.initial.shapeOrigin.z=9;if(mode==2)s.initial.shapeOrigin.y=4;if(mode==3)s.initial.shapeOrigin.y=3.25;
    Trial t(s,{2,true});double initial=t.snapshot().kineticEnergy,maximum=initial,minOmega=0;int hits=0;
    for(int i=0;i<750;++i){auto r=advance(t,1,.002);maximum=std::max(maximum,r.kineticEnergy+r.barrier.kineticEnergy+r.barrier.springEnergy);minOmega=std::min(minOmega,r.angularVelocity.z);hits+=r.contacts.barrier;}
    if(mode<3)check(hits==0&&t.snapshot().center.x>10,"elastic board became an infinite plane");
    else{check(hits>0&&minOmega<-.01,"eccentric elastic impact lost rock torque");check(maximum<=initial*1.01,"elastic impact generated energy");}
    std::cout<<"E4 finite mode="<<mode<<" hits="<<hits<<" min_omega_z="<<minOmega<<" energy_ratio="<<maximum/initial<<'\n';}
 for(double mass:{1.,2000.,1e7})for(double k:{0.,500000.,1e8})for(double c:{0.,30000.,1e7}){
    auto s=scene();s.barrier.mass=mass;s.barrier.stiffness=k;s.barrier.damping=c;s.initial.shapeOrigin={8,0,5};s.initial.velocity={50,0,0};
    Trial t(s,{.2,true});double maximumEnergy=0,depth=0,manifoldDepth=0;for(int step=0;step<100;++step){auto result=advance(t,1,.002);
        manifoldDepth=std::max(manifoldDepth,result.contacts.maxPenetration);
        maximumEnergy=std::max(maximumEnergy,result.barrier.kineticEnergy+result.barrier.springEnergy+result.kineticEnergy);
        double front=result.barrier.center.x-s.barrier.thickness/2,rockMax=-1e300;for(auto local:s.shape.vertices)rockMax=std::max(rockMax,(result.shapeOrigin+result.rotation.matrix()*(local-s.shape.center)).x);
        if(result.center.x<result.barrier.center.x)depth=std::max(depth,std::max(0.0,rockMax-front));
        check(result.center.x<result.barrier.center.x,"rock tunneled through elastic plate");}
    std::cout<<"E4 impact boundary M="<<mass<<" k="<<k<<" c="<<c<<" depth="<<depth<<" manifold_depth="<<manifoldDepth<<" energy_ratio="<<maximumEnergy/2500000<<'\n';
    check(depth<=.002,"dynamic support high-speed penetration");check(maximumEnergy<=2500000*1.01,"support range energy explosion");}
 std::cout<<"E4 27 mass/stiffness/damping boundary impact combinations PASS\n";
}
void impactRefinement(){
    std::array<double,3>peak{},returned{};int sample=0;
    for(double dt:{.002,.001,.0005}){auto s=scene();s.initial.shapeOrigin={8,0,5};s.initial.velocity={5,0,0};Trial t(s,{2,true});
        for(int step=0;step<int(1/dt);++step){auto state=advance(t,1,dt);peak[sample]=std::max(peak[sample],state.barrier.maxDisplacement);}
        returned[sample]=t.snapshot().velocity.x;
        std::cout<<"E5 impact dt="<<dt<<" peak_q="<<peak[sample]<<" return_v="<<returned[sample]<<'\n';++sample;}
    // Freeze acceptance before measurement: 2% for peak travel and rebound speed.
    check(peak[2]>.02&&returned[2]<-.1,"impact refinement fixture has no spring rebound");
    for(int i=0;i<2;++i){check(std::abs(peak[i]/peak[2]-1)<.02,"peak support travel not timestep converged");
        check(std::abs(returned[i]/returned[2]-1)<.02,"rebound speed not timestep converged");}
}
void decayAndLimits(){auto s=scene();s.barrier.stiffness=0;s.barrier.initialVelocity=2;Trial t(s,{2,true});auto r=advance(t,500,.002).barrier;
 check(std::abs(r.velocity-2*std::exp(-15))<1e-7,"zero-k damping continuous solution");check(std::abs(r.dampingEnergy+r.supportWorkResidual+r.kineticEnergy-4000)<.01&&r.dampingEnergy>0,"zero-k physical versus numerical energy accounting");
 for(double k:{0.,1e8})for(double c:{0.,1e7}){s.barrier.stiffness=k;s.barrier.damping=c;s.barrier.initialDisplacement=.02;s.barrier.initialVelocity=0;Trial edge(s,{1,true});auto end=advance(edge,100,.002).barrier;check(std::isfinite(end.springEnergy+end.kineticEnergy),"support boundary nonfinite");}
 s=scene();s.barrier.damping=0;s.barrier.initialDisplacement=.4;Trial active(s,{3});advance(active,1100,.002);check(active.snapshot().reason==StopReason::running,"rock rest prematurely ended oscillating support");
 std::cout<<"E3 zero stiffness/damping, stiff boundaries, support-aware termination PASS\n";}
}
int main(){try{JoltRuntime runtime;std::cout<<std::setprecision(12);double coarse=oscillator(.002),fine=oscillator(.001);check(fine<coarse,"oscillator step refinement did not improve error");impact(0);impact(35);finiteAndEnergy();decayAndLimits();impactRefinement();std::cout<<"PASS elastic support numerical tests\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
