#pragma once
#include "session.h"
#include "prototype_camera.h"
#include <filesystem>
#include <ostream>
#include <future>
namespace rs3d::ui {
Vector3 vector(Vec3 v);
Matrix pose(Quaternion rotation,Vec3 origin);
Ray viewportRay(Vector2 pixel,Camera3D camera,int width,int height);
struct Pick {int id=0;float distance=1e30f;Vector3 point{};};
class SceneModels {
    std::shared_ptr<const PhysicsScene>source_;
    std::string terrainHash_;
    std::string obstacleHash_;
    std::vector<std::pair<int,Model>> obstacles_;
public:
    Model terrain{},rock{},barrier{};
    bool barrierEnabled=false;
    ~SceneModels();
    void update(const std::shared_ptr<const PhysicsScene>&scene,const Snapshot&snapshot);
    Pick pick(Ray ray)const;
    void draw(bool wire,int selected,bool showRock=true,bool showBarrier=true)const;
};
Font loadFont(const std::filesystem::path&path,const std::string&glyphs);
void text(Font font,const std::string&content,float x,float y,int size=18,Color color={218,225,235,255});
void capture(const std::filesystem::path&file);
// GPU readback stays on the UI thread; one bounded PNG encode/write runs off it.
class FrameCapture {
    std::future<void> pending_;
public:
    bool submit(const std::filesystem::path&file);
    void finish(){if(pending_.valid())pending_.get();}
};
std::filesystem::path chooseSceneFile(bool save,const std::filesystem::path&current);
void verifyScene(SceneModels&models,const SessionView&view,std::ostream&log);
void saveCamera(const std::filesystem::path&file,const OrbitCamera&camera);
OrbitCamera loadCamera(const std::filesystem::path&file);
}
