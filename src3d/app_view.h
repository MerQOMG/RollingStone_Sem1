#pragma once
#include "session.h"
#include "prototype_camera.h"
#include <filesystem>
#include <ostream>
namespace rs3d::ui {
Vector3 vector(Vec3 v);
Matrix pose(Quaternion rotation,Vec3 origin);
Ray viewportRay(Vector2 pixel,Camera3D camera,int width,int height);
struct Pick {int id=0;float distance=1e30f;Vector3 point{};};
class SceneModels {
    std::shared_ptr<const PhysicsScene>source_;
    std::string terrainHash_;
public:
    Model terrain{},rock{},barrier{};
    bool barrierEnabled=false;
    ~SceneModels();
    void update(const std::shared_ptr<const PhysicsScene>&scene,const Snapshot&snapshot);
    Pick pick(Ray ray)const;
    void draw(bool wire,int selected)const;
};
Font loadFont(const std::filesystem::path&path,const std::string&glyphs);
void text(Font font,const std::string&content,float x,float y,int size=18,Color color={218,225,235,255});
void capture(const std::filesystem::path&file);
std::filesystem::path chooseSceneFile(bool save,const std::filesystem::path&current);
void verifyScene(SceneModels&models,const SessionView&view,std::ostream&log);
void saveCamera(const std::filesystem::path&file,const OrbitCamera&camera);
OrbitCamera loadCamera(const std::filesystem::path&file);
}
