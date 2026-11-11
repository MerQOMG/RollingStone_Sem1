#include "config.h"
#include "jolt_runtime.h"
#include <iostream>
#include <fstream>
using namespace rs3d;
void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}check(rejected,"invalid input accepted");}
int main(){try{
    JoltRuntime runtime;SceneDraft draft;auto frozen=freezeConfig(draft);auto text=sceneText(draft);check(sceneText(parseScene(text))==text,"round trip");
    check(std::abs(frozen->terrain->xEnd-99.06656562)<1e-8,"default terrain");
    draft.geometry.radius=3;check(frozen->source.geometry.radius==2,"snapshot mutated");
    SceneDraft side;side.terrain.crossSlopeDegrees=10;auto center=freezeConfig(side);side.heightReference=HeightReference::toeMin;auto low=freezeConfig(side);
    check(std::abs(center->zReference-low->zReference-20*std::tan(10*3.141592653589793/180))<1e-10,"toe references");
    SceneDraft seed;seed.masterSeed=0;seed.properties[releaseY]={-1,1};auto seedConfig=freezeConfig(seed);auto input=sampleTrialInput(*seedConfig,1);Random reference(input.propertySeed);reference.uniform();double y=-1+2*reference.uniform();check(input.values[releaseY]==y,"fixed field must consume RNG");check(input.seed==mix(0),"seed zero");
    auto originalHash=seedConfig->hash;seed.outputRoot=L"different";seed.trajectoryStride=0;check(freezeConfig(seed)->hash==originalHash,"output changes physical hash");
    rejects([&]{parseScene(text+"unknown=1\n");});rejects([&]{parseScene("schema=2\n");});rejects([&]{parseScene("schema=1\nschema=1\n");});rejects([&]{parseScene("schema=1\n[geometry]\nr=nan\n");});rejects([&]{parseScene("schema=1\n[run]\ndt=.004\n");});rejects([&]{parseScene("schema=1\n[initial]\nx=5,4\n");});rejects([&]{parseScene("schema=1\n[output]\nroot=\xff\n");});rejects([&]{parseScene("schema=1\n[geometry]\nk_min=4\n");});
    auto folder=std::filesystem::temp_directory_path()/L"RollingStone3D 中文目录 with spaces";std::filesystem::create_directories(folder);auto path=folder/L"场景.ini";seed.outputRoot=folder/L"输出";saveScene(path,seed);check(sceneText(loadScene(path))==sceneText(seed),"UTF-8 path save/load");seed.masterSeed=42;saveScene(path,seed);check(loadScene(path).masterSeed==42,"atomic replace");
    std::cout<<"PASS config strict parsing, immutable snapshot, seed stream, toe references, UTF-8 atomic round trip\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
