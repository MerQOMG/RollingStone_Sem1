#pragma once
#include "geometry.h"
#include <string>
namespace rs3d {
enum class TerrainType { parameterized, flat };
struct TerrainSettings {
    TerrainType type=TerrainType::parameterized;
    double topHeight=55.6,steepDegrees=40,benchStart=25,benchLength=15,benchDegrees=7,toeLength=20;
    double width=40,crossSlopeDegrees=0,grooveAmplitude=0,cellSize=1;
    double flatLength=20,flatHeight=0,flatSlopeX=0;
    bool flipDiagonals=false; // D4 comparison, preserved in replay/config.
};
struct TerrainMesh {
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    std::vector<double> xLines,yLines;
    std::vector<Vec3> profile; // x,z; y=0
    double xMin=0,xFoot=0,xEnd=0,yMin=0,yMax=0;
    std::string contentHash;
    // Exact piecewise planar height of the stored grid, not the continuous groove formula.
    double height(double x,double y)const;
    double toeCenter()const{return height(xEnd,0);}
    double toeMin()const;
};
TerrainMesh buildTerrain(const TerrainSettings& settings);
double profileHeight(const std::vector<Vec3>& profile,double x);
// Exact vertical separation of a convex mesh from the terrain at fixed x/y/rotation.
// All overlapping triangle projections are clipped; vertex-only clearance is insufficient.
double supportHeight(const Polyhedron& shape,const TerrainMesh& terrain,double x,double y,const Mat3& rotation=Mat3::identity());
}
