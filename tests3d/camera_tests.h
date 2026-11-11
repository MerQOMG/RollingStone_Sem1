#pragma once
#include "prototype_camera.h"
#include <stdexcept>
#include <ostream>

inline void cameraSequenceTests(std::ostream& log) {
    using namespace rs3d;
    auto require=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
    auto same=[](const OrbitCamera&a,const OrbitCamera&b){return a.yaw==b.yaw&&a.pitch==b.pitch&&a.distance==b.distance&&Vector3Distance(a.target,b.target)==0;};
    OrbitCamera camera;InputRouter router;CameraInput f;
    f.inside=true;f.pressed=true;f.down=true;f.right=true;f.delta={30,-20};
    updateCamera(camera,router,f);
    require(std::abs(camera.yaw+1.18f)<1e-6f&&std::abs(camera.pitch-0.48f)<1e-6f,"right orbit delta");
    f.pressed=false;f.inside=false;f.delta={10,10};updateCamera(camera,router,f);
    require(std::abs(camera.yaw+1.24f)<1e-6f,"viewport drag outside");
    f={};updateCamera(camera,router,f);
    f.inside=true;f.pressed=true;f.down=true;f.middle=true;f.delta={30,20};
    auto before=camera;updateCamera(camera,router,f);
    require(Vector3Distance(camera.target,before.target)>1&&camera.yaw==before.yaw,"middle pan");
    f={};f.inside=true;f.wheel=2;before=camera;updateCamera(camera,router,f);
    require(camera.distance<before.distance,"wheel zoom");
    // The real camera update consumes the same multi-frame sequence as the GUI.
    for(int mode=0;mode<4;++mode){
        router=InputRouter{};before=camera;f={};f.down=true;f.pressed=true;
        f.inside=mode!=0;f.editor=mode==1;f.modal=mode==2;f.focused=mode!=3;
        f.right=true;f.middle=true;f.delta={100,100};f.wheel=3;
        updateCamera(camera,router,f);
        f.pressed=false;f.inside=true;updateCamera(camera,router,f);
        require(same(before,camera),"control drag/blocked input changed camera");
    }
    router=InputRouter{};f={};f.inside=true;f.pressed=true;f.down=true;f.right=true;
    updateCamera(camera,router,f);before=camera;f.focused=false;f.pressed=false;f.delta={200,200};updateCamera(camera,router,f);
    require(same(before,camera)&&router.owner()==InputOwner::none,"focus loss changed camera");
    camera.top();auto c=camera.camera();require(Vector3Length(Vector3CrossProduct(Vector3Subtract(c.target,c.position),c.up))>0,"top pole basis");
    camera.side();require(camera.pitch==0,"side preset");camera=OrbitCamera{};
    require(camera.distance==95&&camera.yaw==-1,"reset preset");
    log<<"PASS actual camera controller: right orbit, middle pan, zoom, viewport drag-out, panel/editor/modal/focus isolation, top/side/reset\n";
}
