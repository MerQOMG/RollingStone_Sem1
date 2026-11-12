#include "geometry.h"
#include "jolt_runtime.h"
#include "random.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
using namespace rs3d;
int main(int argc,char**argv){try{
    JoltRuntime runtime;
    auto folder=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/L"RollingStone3D morphology";
    std::filesystem::create_directories(folder);
    std::ofstream vertices(folder/"vertices.csv"),faces(folder/"faces.csv"),metrics(folder/"metrics.csv");
    vertices<<std::setprecision(17)<<"sample,vertex,x,y,z\n";
    faces<<"sample,face,a,b,c\n";
    metrics<<std::setprecision(17)<<"sample,k,r,V,seed,max_degree,second_degree,hub_fraction,degree_signature,attempts\n";
    std::set<std::string> signatures;double maximumHub=0;int bipyramids=0;
    for(int sample=0;sample<72;++sample){
        // 48 same-k/default-rV shapes measure real diversity, not variation in k.
        // Remaining rows expose both the user thin case and large-k/high-volume boundaries.
        GeometrySettings settings{30,30,2,7};
        if(sample>=48&&sample<60)settings={10+(sample-48)%11,10+(sample-48)%11,7,20};
        if(sample>=60){int k=std::array<int,4>{10,11,20,100}[sample%4];settings={k,k,2,.9*referenceVolume(k,2)};}
        auto seed=trialSeed(20261112,sample+1);auto generated=generatePolyhedron(settings,seed);const auto&p=generated.geometry;
        double smallest=1e300,largest=0;for(auto v:p.vertices){double distance=norm(v-p.center);smallest=std::min(smallest,distance);largest=std::max(largest,distance);}
        if(std::abs(largest/settings.radius-1)>1e-8)throw std::runtime_error("maximum COM radius changed");
        if(sample<48&&smallest/settings.radius>=.9)throw std::runtime_error("default shapes remained nearly cospherical");
        std::vector<std::set<int>> neighbours(p.vertices.size());
        for(size_t i=0;i<p.faces.size();++i){auto f=p.faces[i];faces<<sample<<','<<i<<','<<f.a<<','<<f.b<<','<<f.c<<'\n';
            for(auto edge:{std::pair<int,int>{f.a,f.b},{f.b,f.c},{f.c,f.a}}){neighbours[edge.first].insert(edge.second);neighbours[edge.second].insert(edge.first);}}
        std::vector<int> degrees;for(auto&adj:neighbours)degrees.push_back(int(adj.size()));std::sort(degrees.begin(),degrees.end(),std::greater<int>());
        std::ostringstream signature;for(auto d:degrees)signature<<d<<':';
        double hub=double(degrees[0]+degrees[1])/(2*(p.vertices.size()-2));
        if(sample<48){signatures.insert(signature.str());maximumHub=std::max(maximumHub,hub);bipyramids+=degrees[1]>=int(p.vertices.size())-2;}
        for(size_t i=0;i<p.vertices.size();++i){auto v=p.vertices[i];vertices<<sample<<','<<i<<','<<v.x<<','<<v.y<<','<<v.z<<'\n';}
        metrics<<sample<<','<<p.vertices.size()<<','<<settings.radius<<','<<settings.volume<<','<<seed<<','<<degrees[0]<<','<<degrees[1]<<','<<hub<<','<<signature.str()<<','<<generated.diagnostics.attempts<<'\n';
    }
    vertices.close();faces.close();metrics.close();
    if(!vertices||!faces||!metrics)throw std::runtime_error("morphology output failed");
    std::cout<<"same-k samples=48 distinct_degree_signatures="<<signatures.size()<<" bipyramids="<<bipyramids<<" maximum_two_hub_fraction="<<maximumHub<<'\n';
    // Guards against the old repeated two-hub topology; not a geological realism score.
    if(bipyramids!=0||maximumHub>=.75||signatures.size()<12)throw std::runtime_error("shape diversity regression: dominant hubs or repeated topology");
    std::cout<<"PASS actual 72-shape geometry gallery and same-k topology diversity\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
