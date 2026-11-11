#pragma once
#include "input_router.h"
#include <raylib.h>
#include <raymath.h>
#include <cmath>

namespace rs3d {
struct OrbitCamera {
    Vector3 target{45,0,10}; float yaw=-1.0f,pitch=0.60f,distance=95;
    Camera3D camera() const {
        Vector3 d{distance*std::cos(pitch)*std::cos(yaw),distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch)};
        return {Vector3Add(target,d),target,{0,0,1},45,CAMERA_PERSPECTIVE};
    }
    void top() {yaw=-PI/2;pitch=PI/2-0.001f;}
    void side() {yaw=-PI/2;pitch=0;}
};
struct CameraInput {
    bool focused=true,pressed=false,down=false,inside=false,editor=false,modal=false;
    bool right=false,middle=false;
    Vector2 delta{};
    float wheel=0;
};
inline InputOwner updateCamera(OrbitCamera& orbit,InputRouter& router,const CameraInput& f) {
    const auto owner=router.update(f.focused,f.pressed,f.down,f.inside,f.editor,f.modal);
    if(!router.cameraAllowed(f.focused,f.down,f.inside,f.editor,f.modal))return owner;
    if(f.right){orbit.yaw-=f.delta.x*0.006f;orbit.pitch=Clamp(orbit.pitch+f.delta.y*0.006f,-1.55f,1.5698f);}
    if(f.middle){
        Vector3 right{std::sin(orbit.yaw),-std::cos(orbit.yaw),0};
        Vector3 up=Vector3Normalize(Vector3CrossProduct(right,Vector3Subtract(orbit.camera().position,orbit.target)));
        orbit.target=Vector3Add(orbit.target,Vector3Scale(Vector3Add(Vector3Scale(right,-f.delta.x),Vector3Scale(up,f.delta.y)),orbit.distance*0.001f));
    }
    orbit.distance=Clamp(orbit.distance*std::exp(-f.wheel*0.13f),0.08f,800.0f);
    return owner;
}
}
