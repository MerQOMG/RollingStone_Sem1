#pragma once
#include "physics.h"
#include "statistics.h"
#include "random.h"
#include <filesystem>
namespace rs3d {
constexpr int sceneSchema=1;
constexpr const char* seedScheme="splitmix64-domains-v1";
struct Range {
    double lower=0,upper=0;
    double sample(Random&rng)const{double u=rng.uniform();return lower+(upper-lower)*u;}
};
enum Property { releaseX,releaseY,velocityX,velocityY,velocityZ,omegaX,omegaY,omegaZ,frictionValue,restitutionValue,dragValue,rollingValue,propertyCount };
extern const std::array<const char*,propertyCount> propertyNames;
enum class HeightReference { toeCenter,toeMin };
struct SceneDraft {
    GeometrySettings geometry;
    TerrainSettings terrain;
    BarrierSettings barrier;
    bool barrierAutoX=true,barrierAutoZ=true;
    std::array<Range,propertyCount> properties{{{10,10},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{.45,.45},{.45,.45},{1,1},{.02,.02}}};
    double density=2600,airDensity=1.225,releaseGap=.05,maxTime=60;
    bool stableFaceRelease=false; // Explicit benchmark fixture; ordinary scenes retain sampled orientation.
    std::uint64_t count=100,masterSeed=20261111;
    std::uint64_t previewSeed=20261111; // Saved independently; never consumed by formal batches.
    HeightReference heightReference=HeightReference::toeCenter;
    bool automaticGrid=true,automaticCollector=true;
    GridSettings grid{{0,0,0},{1,1,1},{1,1,1}};
    double collectorX=0;
    std::filesystem::path outputRoot=L"results3d";
    int trajectoryStride=50; // 0 disables trajectory file rows; statistics remain every step.
};
struct RunConfig {
    SceneDraft source;
    std::shared_ptr<const TerrainMesh> terrain;
    BarrierSettings barrier;
    GridSettings grid;
    double zReference=0,collectorX=0;
    std::string hash;
};
std::shared_ptr<const RunConfig> freezeConfig(const SceneDraft& draft);
std::string sceneText(const SceneDraft&draft,bool includeOutput=true);
SceneDraft parseScene(const std::string&utf8);
SceneDraft loadScene(const std::filesystem::path&path);
void saveScene(const std::filesystem::path&path,const SceneDraft&draft);
struct TrialInput {
    std::uint64_t id=0,seed=0,kSeed=0,propertySeed=0;
    std::array<double,propertyCount> values{};
};
TrialInput sampleTrialInput(const RunConfig&config,std::uint64_t id);
PhysicsScene makePhysicsScene(const RunConfig&config,const TrialInput&input,const Polyhedron&shape);
}
