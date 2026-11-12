#pragma once
#include "terrain.h"
#include <memory>
#include <map>
namespace rs3d {
struct BarrierSettings;
constexpr const char* legacyObstacleAlgorithm="fixed-box-obstacles-v1";
constexpr const char* obstacleAlgorithm="fixed-box-obstacles-v2";
constexpr int firstObstacleObjectId=1000;
struct ObstacleSettings {
    bool enabled=false,relativeToRadius=false,automaticRegion=true,wholeTerrain=true;
    int count=24;
    std::uint64_t seed=20261112;
    double lengthMin=.20,lengthMax=.50,widthMin=.20,widthMax=.50,heightMin=.06,heightMax=.18;
    double xMin=0,xMax=100,yMin=-5,yMax=5,minSpacing=.75,embedDepth=.03,maxTotalHeight=1;
    double friction=.45,restitution=.20;
};
// Missing placement in an old scene preserves its original layout and hashes.
ObstacleSettings legacyObstacleSettings();
using Footprint=std::array<Vec3,4>;
struct Obstacle {
    int id=firstObstacleObjectId;
    Vec3 center,dimensions;
    Quaternion rotation;
    double yawDegrees=0,exposedHeight=0,friction=.45,restitution=.20;
    Polyhedron geometry;
    Footprint footprint() const;
};
struct ObstacleSet {
    std::string algorithm=obstacleAlgorithm;
    std::vector<Obstacle> boxes;
    std::string hash,inputHash;
    double xMin=0,xMax=0,yMin=0,yMax=0,actualMaxTotalHeight=0;
    unsigned candidates=0;
    std::map<std::string,unsigned> rejected;
};
Footprint rectangleFootprint(double x,double y,double length,double width,double yaw);
double footprintDistance(const Footprint&,const Footprint&);
void validateObstacleSettings(const ObstacleSettings&);
void validateObstacle(const Obstacle&);
std::string obstacleContentHash(const ObstacleSet&);
std::shared_ptr<const ObstacleSet> freezeObstacles(const ObstacleSettings&,const TerrainMesh&,double radius,
    double releaseXMin,double releaseXMax,double releaseYMin,double releaseYMax,const BarrierSettings&,std::shared_ptr<const ObstacleSet> saved={});
}
