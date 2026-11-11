#include "jolt_runtime.h"
#include "geometry.h"
#include "engine_geometry.h"
#include "random.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>

using namespace rs3d;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid input accepted");}
void analyticTests(){
    const std::vector<Vec3> tetra{{1,1,1},{1,-1,-1},{-1,1,-1},{-1,-1,1}},octa{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    std::vector<Vec3> cube;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})cube.push_back({double(x),double(y),double(z)});
    int i=0;for(const auto&vertices:{tetra,cube,octa}){
        double v=i==0?8.0/3:(i==1?8.0:4.0/3),inertia=i==0?16.0/15:(i==1?16.0/3:4.0/15);
        auto p=convexPolyhedron(vertices);check(std::abs(p.volume/v-1)<=1e-8,"G2 analytic volume");check(norm(p.center)<=1e-8,"G2 analytic COM");
        check(frobenius(p.inertia-Mat3::identity()*inertia)/frobenius(p.inertia)<=1e-8,"G2 analytic inertia");
        for(double scale:{0.2,1.0,7.0}){
            auto rotation=axisRotation({1,2,3},0.731);Vec3 translation{100,-37,52};std::vector<Vec3> transformed;
            for(auto point:vertices)transformed.push_back(rotation*point*scale+translation);
            auto q=convexPolyhedron(transformed);check(std::abs(q.volume/(v*std::pow(scale,3))-1)<=1e-8,"G3 scaled volume");
            check(norm(q.center-translation)<=1e-8*scale,"G3 translated COM");
            auto expected=rotation*p.inertia*transpose(rotation)*std::pow(scale,5);
            check(frobenius(q.inertia-expected)/frobenius(expected)<=1e-8,"G3 rotated/scaled inertia");
            auto principal=eigenvalues(q.inertia);for(auto e:principal)check(std::abs(e/(inertia*std::pow(scale,5))-1)<=1e-8,"G3 principal inertia");
        }
        ShapeDiagnostics d;createCollisionShape(p,i==0?std::sqrt(3.0):1,2600,d);
        std::cout<<"PASS G2/G3 explicit k="<<vertices.size()<<" V="<<p.volume<<" Ixx="<<p.inertia(0,0)<<" engine_V_rel="<<d.engineVolumeError<<'\n';++i;
    }
    // Rotation covariance also on an asymmetric solid (symmetric analytic bodies alone are weak).
    auto irregular=tetra;irregular[0].z=1.4;auto a=convexPolyhedron(irregular);auto rot=axisRotation({2,3,1},1.17);
    for(auto&v:irregular)v=rot*v+Vec3{-9,3,12};auto b=convexPolyhedron(irregular);
    check(frobenius(b.inertia-rot*a.inertia*transpose(rot))/frobenius(a.inertia)<1e-8,"G3 asymmetric tensor rotation");
    auto ea=eigenvalues(a.inertia),eb=eigenvalues(b.inertia);for(int j=0;j<3;++j)check(std::abs(ea[j]/eb[j]-1)<1e-8,"G3 asymmetric eigenvalues");
    rejects([&]{convexPolyhedron({{0,0,0},{1,0,0},{0,1,0},{1,1,0}});});
    auto bad=tetra;bad[1]=bad[0];rejects([&]{convexPolyhedron(bad);});
    bad=tetra;bad[0].x=std::numeric_limits<double>::quiet_NaN();rejects([&]{convexPolyhedron(bad);});
    bad[0].x=std::numeric_limits<double>::infinity();rejects([&]{convexPolyhedron(bad);});
    for(auto s:{GeometrySettings{9,20,2,7},{20,101,2,7},{30,20,2,7},{20,50,0.1,7},{20,50,11,7},{20,50,2,-1},{20,50,2,100},{20,50,2,std::numeric_limits<double>::quiet_NaN()}})rejects([&]{validateGeometrySettings(s);});
    validateGeometrySettings({10,10,0.2,0.00016});
    rejects([]{validateGeometrySettings({10,10,0.2,0.00016*(1-1e-12)});});
    rejects([]{validateGeometrySettings({10,10,0.2,0.9*referenceVolume(10,0.2)*(1+1e-12)});});
    // Legacy k=4..9 kernel regressions retained by user instruction; no production-domain bypass.
    for(int k=4;k<=9;++k){std::vector<Vec3> v;if(k==4)v=tetra;else{v={{0,0,1},{0,0,-1}};for(int j=0;j<k-2;++j)v.push_back({std::cos(2*pi*j/(k-2)),std::sin(2*pi*j/(k-2)),0});}
        auto p=convexPolyhedron(v);ShapeDiagnostics d;createCollisionShape(p,k==4?std::sqrt(3.0):1,2600,d);check(p.vertices.size()==size_t(k),"legacy kernel k");}
    check(mix(0)==0xE220A8397B1DCDAFULL,"Mix gold");
    const std::uint64_t gold[]={0xFBFC5102A7266B8BULL,0x0E8F22F720AD06FFULL,0xF0AC3778B341DAEDULL};
    for(int j=0;j<3;++j)check(trialSeed(20261111,j+1)==gold[j],"trial seed gold");
    std::cout<<"PASS G4 invalid inputs, legacy k4..9 and seed gold\n";
}
void retryAndCancelTests(){
    GeometrySettings s{10,20,7,20};const auto seed=trialSeed(20261111,1);
    auto original=generatePolyhedron(s,seed);auto repeated=generatePolyhedron(s,seed);
    check(original.diagnostics.k==repeated.diagnostics.k&&original.geometry.vertices.size()==repeated.geometry.vertices.size(),"same-seed k");
    for(size_t i=0;i<original.geometry.vertices.size();++i)check(norm(original.geometry.vertices[i]-repeated.geometry.vertices[i])==0,"same-seed geometry");
    auto retried=generatePolyhedron(s,seed,{},1);
    check(retried.diagnostics.k==original.diagnostics.k&&retried.diagnostics.attempts>=2&&retried.diagnostics.rejections.front()=="injected attempt rejection","retry keeps k and records reason");
    check(retried.diagnostics.attemptSeed==mix((seed^shapeDomain)+std::uint64_t(retried.diagnostics.attempts-1)),"retry domain seed");
    auto next=generatePolyhedron(s,trialSeed(20261111,2));
    rejects([&]{generatePolyhedron(s,seed,{},64);});
    auto nextAgain=generatePolyhedron(s,trialSeed(20261111,2));
    for(size_t i=0;i<next.geometry.vertices.size();++i)check(norm(next.geometry.vertices[i]-nextAgain.geometry.vertices[i])==0,"failed trial changed next trial");
    int polls=0;rejects([&]{generatePolyhedron(s,seed,[&]{return ++polls>=6;});});check(polls>=6&&polls<=7,"cancel not checked during bounded geometry iterations");
    std::cout<<"PASS deterministic generation, same-k retry, 64-attempt failure, independent next trial, bounded cancellation\n";
}
double percentile(std::vector<double>v,double q){std::sort(v.begin(),v.end());return v.at(size_t(q*(v.size()-1)));}
void samples(int count,const std::filesystem::path&dir){
    std::filesystem::create_directories(dir);std::ofstream csv(dir/"samples.csv"),reject(dir/"attempt_rejections.csv"),fail(dir/"failures.txt"),summary(dir/"summary.txt");
    check(bool(csv)&&bool(reject)&&bool(fail)&&bool(summary),"sample logs not writable");csv<<std::setprecision(17);reject<<"sample,attempt,reason\n";
    csv<<"sample,trial_seed,k,r,V,actual_V,attempts,root_iterations,ms,tau,d_in,slenderness,double_fallback,engine_V_rel,engine_mass_rel,engine_COM_m,engine_I_rel,engine_vertex_m,engine_d_in\n";
    int failures=0,retries=0,totalRetries=0,fallback=0;double minInner=1e300,maxThin=0,maxV=0,maxI=0,maxC=0;std::vector<double> ms;std::map<int,int>hist;
    for(int i=0;i<count;++i){
        GeometrySettings s;
        if(i<1100)s={10+i/100,10+i/100,7,20};
        else {int j=i-1100,k=10+j%91;double r=std::array<double,4>{0.2,2,7,10}[(j/91)%4],cap=0.9*referenceVolume(k,1);int band=(j/(91*4))%3;double ratio=band==0?0.02:(band==1?(0.02+cap)/2:cap);s={k,k,r,ratio*r*r*r};}
        auto seed=trialSeed(20261111,std::uint64_t(i)+1);
        try{auto result=generatePolyhedron(s,seed);const auto&p=result.geometry;const auto&d=result.diagnostics;
            check(p.vertices.size()==size_t(s.kMin),"sample actual k");validatePolyhedron(p);++hist[d.attempts];if(d.attempts>1)++retries;totalRetries+=d.attempts-1;fallback+=p.doubleFallback?1:0;
            for(size_t a=0;a<d.rejections.size();++a)reject<<i+1<<','<<a+1<<",\""<<d.rejections[a]<<"\"\n";
            minInner=std::min(minInner,p.innerRadius/s.radius);maxThin=std::max(maxThin,p.slenderness);maxV=std::max(maxV,d.engineVolumeError);maxI=std::max(maxI,d.engineInertiaError);maxC=std::max(maxC,d.engineCenterError/s.radius);ms.push_back(d.milliseconds);
            csv<<i+1<<','<<seed<<','<<d.k<<','<<s.radius<<','<<s.volume<<','<<p.volume<<','<<d.attempts<<','<<d.rootIterations<<','<<d.milliseconds<<','<<p.tau<<','<<p.innerRadius<<','<<p.slenderness<<','<<p.doubleFallback<<','<<d.engineVolumeError<<','<<d.engineMassError<<','<<d.engineCenterError<<','<<d.engineInertiaError<<','<<d.engineVertexError<<','<<d.engineInnerRadius<<'\n';
        }catch(const std::exception&e){++failures;fail<<"sample="<<i+1<<" seed="<<seed<<" k="<<s.kMin<<" r="<<s.radius<<" V="<<s.volume<<": "<<e.what()<<'\n';}
        if((i+1)%100==0){csv.flush();fail.flush();std::cout<<"sample "<<i+1<<'/'<<count<<" final_failures="<<failures<<" retry_trials="<<retries<<'\n'<<std::flush;}
    }
    summary<<std::setprecision(12)<<"sampler="<<samplerVersion<<" requested="<<count<<" success="<<ms.size()<<" final_failures="<<failures<<" retry_trials="<<retries<<" total_rejections="<<totalRetries<<" final_double_fallback="<<fallback<<'\n';
    if(!ms.empty())summary<<"generation_ms median="<<percentile(ms,.5)<<" p95="<<percentile(ms,.95)<<" min_d_in_over_r="<<minInner<<" max_slenderness="<<maxThin<<" max_engine_V_rel="<<maxV<<" max_engine_I_rel="<<maxI<<" max_engine_COM_over_r="<<maxC<<'\n';
    for(auto item:hist)summary<<"attempts="<<item.first<<" trials="<<item.second<<'\n';
    csv.close();reject.close();fail.close();summary.close();check(bool(csv)&&bool(reject)&&bool(fail)&&bool(summary),"sample log write failed");check(failures==0,"G1/G5 final generation failure(s), see failures.txt");
}
}
int main(int argc,char**argv){try{
    JoltRuntime runtime;analyticTests();retryAndCancelTests();
    if(argc>1){std::string mode=argv[1];int count=mode=="--user-probe"?1100:(mode=="--full"?5000:0);check(count>0,"unknown geometry test mode");samples(count,argc>2?argv[2]:(count==1100?"results3d/geometry_1100":"results3d/geometry_5000"));}
    std::cout<<"PASS requested geometry suite\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
