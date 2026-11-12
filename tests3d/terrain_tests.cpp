#include "jolt_runtime.h"
#include "terrain.h"
#include <iostream>
using namespace rs3d;
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
    JoltRuntime runtime;TerrainSettings s;auto t=buildTerrain(s);
    require(std::abs(t.xFoot-79.06656562)<1e-8&&std::abs(t.xEnd-99.06656562)<1e-8,"default foot/end");
    require(std::abs(t.height(30,0)-34.00858642)<1e-8,"default x30 elevation");
    require(t.vertices.size()==t.xLines.size()*t.yLines.size(),"shared grid vertices");
    require(t.contentHash==buildTerrain(s).contentHash,"terrain hash repeatability");
    for(auto f:t.faces)require(cross(t.vertices[f.b]-t.vertices[f.a],t.vertices[f.c]-t.vertices[f.a]).z>0,"terrain face winding");
    s.benchStart=11.25;auto noninteger=buildTerrain(s);require(std::find(noninteger.xLines.begin(),noninteger.xLines.end(),11.25)!=noninteger.xLines.end(),"noninteger bench line missing");
    require(noninteger.xLines.back()==noninteger.xEnd,"exact endpoint missing");
    s=TerrainSettings{};s.crossSlopeDegrees=5;auto crossSlope=buildTerrain(s);
    require(std::abs(crossSlope.toeCenter())<1e-12,"toe_center");require(std::abs(crossSlope.toeMin()+20*std::tan(5*pi/180))<1e-12,"toe_min cross slope");
    s=TerrainSettings{};s.grooveAmplitude=2;auto groove=buildTerrain(s);
    require(std::abs(groove.height(groove.xEnd,20)-2)<1e-12&&std::abs(groove.toeMin())<1e-12,"groove heights");
    s=TerrainSettings{};s.type=TerrainType::flat;s.width=20;s.flatHeight=5;s.flatSlopeX=-0.7;s.crossSlopeDegrees=5;
    auto plane=buildTerrain(s);std::vector<Vec3> vertices;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})vertices.push_back({double(x),double(y),double(z)});auto cube=convexPolyhedron(vertices);
    double support=supportHeight(cube,plane,10,0),expected=plane.height(10,0)+1+0.7+std::tan(5*pi/180);
    require(std::abs(support-expected)<1e-10,"exact tilted-plane support");
    s.flipDiagonals=true;auto flipped=buildTerrain(s);require(std::abs(supportHeight(cube,flipped,10,0)-support)<1e-10,"support diagonal invariance");
    // Ridge through the interior of a downward face: no rock vertex sits on its peak.
    for(auto&v:plane.vertices)v.z=2-std::abs(v.x-10);
    require(std::abs(supportHeight(cube,plane,10,0)-3)<1e-10,"face-interior support must include terrain ridge");
    double verticesOnly=-1e300;for(auto v:cube.vertices)verticesOnly=std::max(verticesOnly,plane.height(v.x+10,v.y)-v.z);
    require(verticesOnly<2.01,"ridge fixture must defeat vertex-only release");
    std::cout<<"PASS terrain default x_end="<<t.xEnd<<" h(30)="<<t.height(30,0)<<" triangles="<<t.faces.size()<<" hash="<<t.contentHash<<'\n';
    std::cout<<"PASS exact break/end lines, shared winding, cross-slope/groove references, plane/diagonal/ridge release support\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
