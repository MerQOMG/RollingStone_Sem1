#pragma once
// Visual layer of the 3D viewport: GLSL 330 programs (embedded), screen-space ribbons and glow helpers.
// Everything degrades gracefully: if a program fails to compile, ok() is false and the viewport keeps its plain look.
#include <raylib.h>
#include <string>
#include <vector>
namespace rs3d::gui::viz {
struct Programs {
    bool tried=false,ok=false;
    Shader terrain{},solid{},ribbon{},sky{},ground{};
    Material terrainMat{},solidMat{},ribbonMat{},groundMat{};
};
// Optional photo-scan terrain textures (CC0): name of a candidate, empty = procedural look. Set before the first frame.
void setTerrainTexture(const std::string&name);
bool terrainTextureOn();
const std::string& terrainTextureName();
Programs& programs(); // compiles on first use; needs a GL context
void release();
inline bool ok(){return programs().ok;}
struct FrameParams {
    Vector3 viewPos{};float distance=100;
    Vector2 heightRange{0,1};Vector4 rock{0,0,0,0};Vector3 gridCenter{};float groundZ=0;
    bool contours=true,shadow=false;
};
Vector3 sunDir(); // unit vector towards the sun
// Sun shadow map: orthographic depth pass of the casters, sampled with PCF by the terrain shader.
bool shadowReady(); // creates the depth target on first use; false means shadows are off
void shadowBegin(const Matrix&view,float l,float r,float b,float t,float n,float f);
void drawDepthMesh(const Mesh&m); // depth-only draw inside the pass
void shadowEnd(int fbW,int fbH);
void bindShadow(); // before drawing the terrain
void beginFrame(const FrameParams&p); // uploads the shared uniforms to every program
// Sky dome painted into the viewport rectangle (device pixels, GL origin bottom-left for the viewport).
void drawSky(int x,int yTop,int w,int h,int fbH,Vector3 right,Vector3 up,Vector3 forward,float tanV,float aspect);
// A polyline expanded to a camera-facing ribbon of constant pixel width in the vertex shader.
class Ribbon {
    Mesh mesh_{};bool has_=false;
public:
    Ribbon()=default;Ribbon(const Ribbon&)=delete;Ribbon&operator=(const Ribbon&)=delete;~Ribbon(){clear();}
    // points with times; base colour is faded and brightened along the path
    void build(const std::vector<Vector3>&p,const std::vector<float>&time,Color base);
    void clear();
    void draw(float widthPx,float whiten,float cutTime,int vpW,int vpH,float alphaMul);
};
void glowDisc(Vector3 c,float r,Vector3 right,Vector3 up,Color inner,int seg=40); // radial fade to transparent, call inside additive blend
void ring(Vector3 c,float r,float w,Vector3 right,Vector3 up,Color col,int seg=56);
}
