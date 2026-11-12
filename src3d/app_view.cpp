#include "app_view.h"
#include <rlgl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <locale>
#include "native_dialog.h"
namespace rs3d::ui {
namespace {
Model mesh(const std::vector<Vec3>&vertices,const std::vector<Face>&faces,Vec3 center,bool ground){
    Mesh m{};m.triangleCount=int(faces.size());m.vertexCount=3*m.triangleCount;
    m.vertices=static_cast<float*>(MemAlloc(unsigned(m.vertexCount*3*sizeof(float))));m.colors=static_cast<unsigned char*>(MemAlloc(unsigned(m.vertexCount*4)));
    int i=0;for(size_t f=0;f<faces.size();++f){auto face=faces[f];auto centroid=(vertices[face.a]+vertices[face.b]+vertices[face.c])/3;bool checker=(int(std::floor(centroid.x/5))+int(std::floor(centroid.y/5)))%2!=0;Color color=ground?(checker?Color{102,134,121,255}:Color{121,151,135,255}):(f%2?Color{217,155,72,255}:Color{240,190,109,255});
        for(int id:{face.a,face.b,face.c}){auto p=vertices[id]-center;for(int axis=0;axis<3;++axis)m.vertices[3*i+axis]=float(p[axis]);m.colors[4*i]=color.r;m.colors[4*i+1]=color.g;m.colors[4*i+2]=color.b;m.colors[4*i+3]=255;++i;}}
    UploadMesh(&m,false);return LoadModelFromMesh(m);
}
void unload(Model&model){if(model.meshCount)UnloadModel(model);model={};}
}
Vector3 vector(Vec3 v){return {float(v.x),float(v.y),float(v.z)};}
Matrix pose(Quaternion q,Vec3 p){::Quaternion rotation{float(q.x),float(q.y),float(q.z),float(q.w)};return MatrixMultiply(QuaternionToMatrix(QuaternionNormalize(rotation)),MatrixTranslate(float(p.x),float(p.y),float(p.z)));}
Ray viewportRay(Vector2 pixel,Camera3D c,int width,int height){auto forward=Vector3Normalize(Vector3Subtract(c.target,c.position)),right=Vector3Normalize(Vector3CrossProduct(forward,c.up)),up=Vector3CrossProduct(right,forward);float half=std::tan(c.fovy*DEG2RAD*.5f);float x=(2*pixel.x/width-1)*half*float(width)/height,y=(1-2*pixel.y/height)*half;return {c.position,Vector3Normalize(Vector3Add(forward,Vector3Add(Vector3Scale(right,x),Vector3Scale(up,y))))};}
SceneModels::~SceneModels(){unload(terrain);unload(rock);unload(barrier);}
void SceneModels::update(const std::shared_ptr<const PhysicsScene>&scene,const Snapshot&snapshot){
    if(scene!=source_){
        if(scene->terrain->contentHash!=terrainHash_){auto next=mesh(scene->terrain->vertices,scene->terrain->faces,{},true);unload(terrain);terrain=next;terrainHash_=scene->terrain->contentHash;}
        auto next=mesh(scene->shape.vertices,scene->shape.faces,scene->shape.center,false);unload(rock);rock=next;unload(barrier);barrierEnabled=scene->barrier.enabled;
        if(barrierEnabled){const auto&b=scene->barrier;barrier=LoadModelFromMesh(GenMeshCube(float(b.thickness),float(b.width),float(b.height)));barrier.transform=pose(Quaternion::fromAxisAngle({0,0,1},b.yawDegrees*pi/180),b.center);}source_=scene;
    }rock.transform=pose(snapshot.rotation,snapshot.shapeOrigin);
    if(barrierEnabled)barrier.transform=pose(snapshot.barrier.rotation,snapshot.barrier.center);
}
Pick SceneModels::pick(Ray ray)const{Pick best;const Model*all[]{&terrain,&rock,&barrier};for(int id=0;id<3;++id){if(id==2&&!barrierEnabled)continue;for(int j=0;j<all[id]->meshCount;++j){auto hit=GetRayCollisionMesh(ray,all[id]->meshes[j],all[id]->transform);if(hit.hit&&hit.distance<best.distance)best={id+1,hit.distance,hit.point};}}return best;}
void SceneModels::draw(bool wire,int selected)const{
    rlDisableBackfaceCulling();auto draw=[&](const Model&model,int id,Color color){if(selected==id)color=YELLOW;if(wire)DrawModelWires(model,{0,0,0},1,color);else DrawModel(model,{0,0,0},1,color);};draw(terrain,terrainObjectId,WHITE);draw(rock,rockObjectId,WHITE);if(barrierEnabled)draw(barrier,barrierObjectId,{146,181,232,255});rlEnableBackfaceCulling();
}
Font loadFont(const std::filesystem::path&path,const std::string&glyphs){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read CJK font; use --font path");std::vector<unsigned char>bytes((std::istreambuf_iterator<char>(in)),{});int count=0;int*cps=LoadCodepoints(glyphs.c_str(),&count);std::vector<int>codes(cps,cps+count);UnloadCodepoints(cps);for(int c=32;c<127;++c)codes.push_back(c);std::sort(codes.begin(),codes.end());codes.erase(std::unique(codes.begin(),codes.end()),codes.end());auto font=LoadFontFromMemory(".ttf",bytes.data(),int(bytes.size()),24,codes.data(),int(codes.size()));if(!font.texture.id||font.texture.id==GetFontDefault().texture.id)throw std::runtime_error("CJK font decoding failed");for(int c:codes)if(font.glyphs[GetGlyphIndex(font,c)].value!=c){UnloadFont(font);throw std::runtime_error("CJK font lacks required glyph");}SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);return font;}
void text(Font font,const std::string&content,float x,float y,int size,Color color){DrawTextEx(font,content.c_str(),{x,y},float(size),1,color);}
void capture(const std::filesystem::path&file){rlDrawRenderBatchActive();Image image{};image.width=GetRenderWidth();image.height=GetRenderHeight();image.mipmaps=1;image.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;image.data=rlReadScreenPixels(image.width,image.height);int count=0;auto bytes=ExportImageToMemory(image,".png",&count);std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<char*>(bytes),count);out.close();MemFree(bytes);UnloadImage(image);if(!out)throw std::runtime_error("framebuffer capture failed");}
std::filesystem::path chooseSceneFile(bool save,const std::filesystem::path&current){
    return nativeSceneDialog(save,current,GetWindowHandle());
}
void verifyScene(SceneModels&models,const SessionView&view,std::ostream&log){
    auto scene=view.scene;auto snapshot=view.snapshot;models.update(scene,snapshot);auto require=[](bool b,const char*s){if(!b)throw std::runtime_error(s);};
    for(auto local:scene->shape.vertices){Vec3 expected=snapshot.shapeOrigin+snapshot.rotation.matrix()*(local-scene->shape.center);auto actual=Vector3Transform(vector(local-scene->shape.center),models.rock.transform);require(norm(expected-Vec3{actual.x,actual.y,actual.z})<2e-5,"actual pose/render mismatch");}
    for(int width:{640,1000,1440})for(float dpi:{1.0f,1.25f,1.5f,2.0f})for(int mode=0;mode<3;++mode){OrbitCamera orbit;orbit.target=vector(snapshot.center);if(mode==1)orbit.top();if(mode==2)orbit.side();auto camera=orbit.camera();int w=int(width*dpi),h=int(584*dpi);auto pixel=GetWorldToScreenEx(vector(snapshot.center),camera,w,h);Vector2 mouse{pixel.x/dpi+320,pixel.y/dpi+52};auto ray=viewportRay({(mouse.x-320)*dpi,(mouse.y-52)*dpi},camera,w,h);auto offset=Vector3Subtract(vector(snapshot.center),ray.position);require(Vector3Length(Vector3CrossProduct(offset,ray.direction))<=.002f,"actual scene projection/DPI mismatch");}
    Vec3 center=snapshot.center;auto upper=models.pick({vector(center+Vec3{0,0,20}),{0,0,-1}});require(upper.id==rockObjectId,"actual rock nearest pick");double z=scene->terrain->height(center.x,center.y);auto lower=models.pick({{float(center.x),float(center.y),float(z-20)},{0,0,1}});require(lower.id==terrainObjectId,"actual terrain occlusion pick");require(models.pick({{-100,-100,100},{0,0,1}}).id==0,"empty pick");
    if(scene->barrier.enabled){const auto&b=scene->barrier;require(models.pick({vector(snapshot.barrier.center+Vec3{0,0,b.height+10}),{0,0,-1}}).id==barrierObjectId,"finite barrier pose/pick");}
    Trial physical(*scene);auto diagnostic=physical.diagnostics();
    require(diagnostic.hasBarrier==models.barrierEnabled,"barrier enabled/render mismatch");
    if(diagnostic.hasBarrier){double maximum=0;const auto&m=models.barrier.meshes[0];
        for(int i=0;i<m.vertexCount;++i){auto actual=Vector3Transform({m.vertices[3*i],m.vertices[3*i+1],m.vertices[3*i+2]},models.barrier.transform);double nearest=1e300;
            for(auto corner:diagnostic.barrierWorldCorners)nearest=std::min(nearest,norm(corner-Vec3{actual.x,actual.y,actual.z}));maximum=std::max(maximum,nearest);}
        require(maximum<2e-5,"rendered barrier differs from actual Jolt body/shape");log<<"PASS actual Jolt barrier readback versus rendered mesh max_m="<<maximum<<'\n';}
    if(scene->barrier.enabled&&scene->barrier.elastic){
        const int steps=view.config->source.terrain.type==TerrainType::parameterized?6000:2000;
        Trial moving(*scene,{steps*.002,true});double maximum=0,displacement=0;
        for(int step=0;step<steps;++step){auto state=moving.step();require(state.reason!=StopReason::numericFailure,"moving barrier physics failure");
            if(step%100!=0)continue;models.update(scene,state);auto actual=moving.diagnostics();const auto&m=models.barrier.meshes[0];
            displacement=std::max(displacement,std::abs(state.barrier.displacement));
            for(int i=0;i<m.vertexCount;++i){auto rendered=Vector3Transform({m.vertices[3*i],m.vertices[3*i+1],m.vertices[3*i+2]},models.barrier.transform);double nearest=1e300;
                for(auto corner:actual.barrierWorldCorners)nearest=std::min(nearest,norm(corner-Vec3{rendered.x,rendered.y,rendered.z}));maximum=std::max(maximum,nearest);}
            auto normal=barrierAxis(scene->barrier);auto target=state.barrier.center;auto ray=Ray{vector(target+normal*10),vector(normal*-1)};
            auto hit=GetRayCollisionMesh(ray,models.barrier.meshes[0],models.barrier.transform);require(hit.hit&&std::abs(hit.distance-(10-scene->barrier.thickness/2))<2e-5,"moving barrier ray mesh mismatch");
        }
        require(maximum<2e-5,"moving Jolt barrier/render mismatch");models.update(scene,snapshot);
        log<<"PASS moving elastic barrier Jolt/readback/render/ray max_error_m="<<maximum<<" observed_displacement_m="<<displacement<<'\n';
    }
    log<<"PASS actual canonical model pose, 36 viewport/DPI/camera cases, nearest rock/terrain/empty/barrier picks\n";
}
void saveCamera(const std::filesystem::path&file,const OrbitCamera&c){std::ofstream out(file,std::ios::binary);out.imbue(std::locale::classic());out<<std::setprecision(9)<<"RS3D_CAMERA_1\n"<<c.target.x<<' '<<c.target.y<<' '<<c.target.z<<' '<<c.yaw<<' '<<c.pitch<<' '<<c.distance<<'\n';out.close();if(!out)throw std::runtime_error("cannot save separate camera file");}
OrbitCamera loadCamera(const std::filesystem::path&file){std::ifstream in(file,std::ios::binary);in.imbue(std::locale::classic());std::string schema;OrbitCamera c;in>>schema>>c.target.x>>c.target.y>>c.target.z>>c.yaw>>c.pitch>>c.distance;if(!in||schema!="RS3D_CAMERA_1")throw std::runtime_error("invalid camera file");for(float v:{c.target.x,c.target.y,c.target.z,c.yaw,c.pitch,c.distance})if(!std::isfinite(v))throw std::runtime_error("non-finite camera value");if(c.distance<.08f||c.distance>800||c.pitch<-1.55f||c.pitch>1.5698f)throw std::runtime_error("camera out of range");return c;}
}
