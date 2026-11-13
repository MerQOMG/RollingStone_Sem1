#pragma once
// 3D scene view: camera, terrain/obstacle/barrier/rock drawing, trajectory bundles and heat overlays.
#include "ui/gfx.h"
#include "ui/widgets.h"
#include "batch.h"
#include "input_router.h"
#include "ui/viz.h"
#include <raymath.h>
#include <map>
#include <memory>
namespace rs3d::gui {
Vector3 toV3(Vec3 v);
Color heatRamp(double t,unsigned char alphaByte=255); // t in [0,1], dark violet -> orange -> pale yellow
Color stopColor(StopReason reason,unsigned char alphaByte=255);
// A static set of segments uploaded once (contours, trajectories, obstacle edges).
class LineBuffer {
    unsigned vao_=0,vbo_=0;int count_=0;
public:
    LineBuffer()=default;
    LineBuffer(const LineBuffer&)=delete;LineBuffer&operator=(const LineBuffer&)=delete;
    ~LineBuffer(){clear();}
    void set(const std::vector<Vector3>&segmentEnds); // pairs of points
    void draw(Color c)const;
    void clear();
    int count()const{return count_;}
    static void releaseShared();
};
enum class ViewPreset { perspective,top,side,follow };
struct OrbitView {
    Vector3 target{45,0,10};float yaw=-1.0f,pitch=.6f,distance=95;
    Camera3D camera()const;
};
struct HeatSpec {
    std::string key;
    const TerrainMesh*terrain=nullptr;
    Grid3D grid{{{0,0,0},{1,1,1},{1,1,1}}};
    const std::vector<std::uint64_t>*hits=nullptr;
    double denominator=1;
    bool valid=false;
};
struct StatBox { // the statistics box of the frozen run configuration (RunConfig::grid), drawn as a translucent band
    bool valid=false;Vec3 lower{},upper{};double collectorX=0;
};
struct DrawOptions {
    bool showStatBox=true;StatBox statBox;
    bool showRock=true,showBarrier=true,showTrails=true,showHeat=true,showContours=true,showRuler=true;
    std::uint64_t selectedTrial=0;double selectedTime=-1;float trailAlpha=.7f;
    bool restBarrierGhost=true;
    std::vector<Vec3> contactPoints;
};
class Viewport3D {
    std::shared_ptr<const PhysicsScene> scene_;std::string terrainHash_,obstacleHash_;
    Model terrain_{},obstacles_{},heat_{},skirt_{},ground_{};bool hasTerrain_=false,hasObstacles_=false,hasHeat_=false,hasSkirt_=false,hasGround_=false,fancy_=false;
    Vector2 hRange_{0,1};Vector3 bbMin_{0,0,0},bbMax_{1,1,1};Vector3 groundCentre_{};float groundZ_=0;
    std::map<std::uint64_t,std::unique_ptr<viz::Ribbon>> ribbons_;
    LineBuffer contourMinor_,contourMajor_,obstacleEdges_;
    std::vector<Vector3> rockLocal_;std::vector<Face> rockFaces_;std::vector<std::pair<int,int>> rockEdges_;std::vector<Vector3> rockNormals_;
    std::map<std::uint64_t,std::pair<std::shared_ptr<const DisplayTrajectory>,std::unique_ptr<LineBuffer>>> trails_;
    std::string heatKey_;
    OrbitView goal_;bool animating_=false;
    int gzHover_=-1,gzPress_=-1;bool gzOwns_=false,gzDrag_=false;Vector2 gzStart_{};
    std::vector<Vector2> gzCenters_; // last drawn item centres (+x,-x,+y,-y,+z,-z), for diagnostics
    InputRouter router_;
    void buildTerrain(const TerrainMesh&);
    void buildTerrainFancy(const TerrainMesh&);
    void buildObstacles(const ObstacleSet*);
    void buildRock(const PhysicsScene&);
    void unloadHeat();
public:
    OrbitView view;ViewPreset preset=ViewPreset::perspective;
    ~Viewport3D();
    void release();
    void setScene(const std::shared_ptr<const PhysicsScene>&scene);
    void setTrajectories(const std::vector<std::shared_ptr<const DisplayTrajectory>>&trajectories);
    void setHeat(const HeatSpec&spec);
    void clearHeat(){unloadHeat();heatKey_.clear();}
    void frameScene();
    void setPreset(ViewPreset p);
    // Returns true while the viewport owns the pointer (drag in progress).
    bool updateCamera(const Input&in,R rect,bool pointerOverUi,bool typing,bool modal,float dt,const Vector3*followTarget);
    void render(R rect,const Snapshot&pose,const DrawOptions&opt);
    Vector2 project(Vector3 p,R rect)const;
    std::size_t trailCount()const{return trails_.size();}
    bool hasScene()const{return bool(scene_);}
    // Blender-style navigation gizmo: six axis discs in a translucent disc; click aligns the view, drag orbits.
    static R gizmoRect(R viewport,bool demo=false){return demo?R{viewport.x+10,viewport.bottom()-148,124,138}:R{viewport.right()-132,viewport.y+8,124,138};} // demo mode keeps the top-right free for its stat cards
    bool gizmo(R area,const Input&in,bool uiFree);
    void alignView(int axisItem);
    const std::vector<Vector2>&gizmoCenters()const{return gzCenters_;}
};
}
