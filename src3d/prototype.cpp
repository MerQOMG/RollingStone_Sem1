#include "prototype.h"
#include "platform.h"
#include "static_lines.h"
#include "input_router.h"
#include "prototype_camera.h"
#include "../tests3d/camera_tests.h"
#include <raylib.h>
#include <raymath.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
constexpr const char* glyphs = u8"界面原型／预设运动非物理模拟俯视侧视重置实体线框轨迹显示半径米应用参数场景基准压力选择无地形石块挡板坐标焦点非法输入必须是有限数值范围关闭窗口输出性能检测拖动旋转平移滚轮缩放当前负载完成统计区域支持上限近裁剪单位每格开始暂停继续取消错误配置保存加载背景版本调整亮度种子右中上";
Font loadChinese(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot load CJK font; use --font with a readable TTF/TTC path");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    int count=0; int* cps=LoadCodepoints(glyphs,&count);
    std::vector<int> codes(cps,cps+count); UnloadCodepoints(cps);
    for(int i=32;i<127;++i) codes.push_back(i);
    std::sort(codes.begin(),codes.end()); codes.erase(std::unique(codes.begin(),codes.end()),codes.end());
    Font f=LoadFontFromMemory(".ttf",bytes.data(),static_cast<int>(bytes.size()),24,codes.data(),static_cast<int>(codes.size()));
    if(f.texture.id==0 || f.texture.id==GetFontDefault().texture.id) throw std::runtime_error("CJK font decoding failed");
    for(auto c:codes) if(f.glyphs[GetGlyphIndex(f,c)].value!=c) throw std::runtime_error("CJK font is missing a required glyph");
    SetTextureFilter(f.texture,TEXTURE_FILTER_BILINEAR);
    return f;
}
struct Tri { Vector3 a,b,c; Color color; };
Model makeModel(const std::vector<Tri>& triangles) {
    Mesh mesh{}; mesh.triangleCount=static_cast<int>(triangles.size()); mesh.vertexCount=3*mesh.triangleCount;
    mesh.vertices=static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount*3*sizeof(float))));
    mesh.colors=static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount*4)));
    int j=0;
    for(auto& t:triangles) for(Vector3 v : {t.a,t.b,t.c}) {
        mesh.vertices[3*j]=v.x;mesh.vertices[3*j+1]=v.y;mesh.vertices[3*j+2]=v.z;
        mesh.colors[4*j]=t.color.r;mesh.colors[4*j+1]=t.color.g;mesh.colors[4*j+2]=t.color.b;mesh.colors[4*j+3]=255;++j;
    }
    UploadMesh(&mesh,false); return LoadModelFromMesh(mesh);
}
float height(float x,float y) { return 20.0f-0.18f*x+0.6f*std::sin(y*0.12f); }
Model terrain(bool stress) {
    int nx=stress?240:120,ny=stress?160:80;std::vector<Tri> tris;tris.reserve(2*nx*ny);
    for(int x=0;x<nx;++x)for(int y=0;y<ny;++y) {
        float a=float(x),b=float(y-ny/2);
        Vector3 p{a,b,height(a,b)},q{a+1,b,height(a+1,b)},r{a+1,b+1,height(a+1,b+1)},s{a,b+1,height(a,b+1)};
        Color c=((x/5+y/5)%2)?Color{98,128,116,255}:Color{115,146,131,255};
        tris.push_back({p,q,r,c});tris.push_back({p,r,s,c});
    } return makeModel(tris);
}
Model stone() {
    std::vector<Tri> tris;
    for(int i=0;i<18;++i) {
        float a=2*PI*i/18,b=2*PI*(i+1)/18;
        Vector3 p{std::cos(a),std::sin(a),0},q{std::cos(b),std::sin(b),0};
        Color c=i%2?Color{214,155,69,255}:Color{239,185,97,255};
        tris.push_back({{0,0,1},p,q,c});tris.push_back({{0,0,-1},q,p,c});
    }return makeModel(tris);
}
using Orbit=rs3d::OrbitCamera;
struct Pick {int id=0;float distance=std::numeric_limits<float>::infinity();Vector3 point{};};
Ray viewportRay(Vector2 pixel,Camera3D c,int width,int height) {
    // Analytic perspective inverse avoids far-plane float cancellation in raylib's unproject.
    Vector3 forward=Vector3Normalize(Vector3Subtract(c.target,c.position));
    Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,c.up));
    Vector3 up=Vector3CrossProduct(right,forward);
    float half=std::tan(c.fovy*DEG2RAD*0.5f);
    float x=(2*pixel.x/width-1)*half*float(width)/height,y=(1-2*pixel.y/height)*half;
    return {c.position,Vector3Normalize(Vector3Add(forward,Vector3Add(Vector3Scale(right,x),Vector3Scale(up,y))))};
}
Pick pick(Ray ray,const Model& ground,const Model& rock,const Model& barrier) {
    Pick best;
    const Model* models[]={&ground,&rock,&barrier};
    for(int i=0;i<3;++i) for(int j=0;j<models[i]->meshCount;++j) {
        auto hit=GetRayCollisionMesh(ray,models[i]->meshes[j],models[i]->transform);
        if(hit.hit&&hit.distance<best.distance) best={i+1,hit.distance,hit.point};
    }return best;
}
void checkCoordinates(const Model& ground,const Model& rock,const Model& barrier,std::ostream& log) {
    // Exercise world projection and viewport-local ray construction over sizes and DPI scales.
    const Vector3 world{35,0,height(35,0)+2.2f};
    for(int w:{640,1000,1440})for(float scale:{1.0f,1.25f,1.5f,2.0f})for(int view=0;view<3;++view) {
        Orbit orbit;if(view==1)orbit.top();if(view==2)orbit.side();auto c=orbit.camera();
        int rw=int(w*scale),rh=int(624*scale);Vector2 pixel=GetWorldToScreenEx(world,c,rw,rh);
        Vector2 mouse{pixel.x/scale+280,pixel.y/scale+56};
        auto ray=viewportRay({(mouse.x-280)*scale,(mouse.y-56)*scale},c,rw,rh);
        auto offset=Vector3Subtract(world,ray.position);
        float distance=Vector3Length(Vector3CrossProduct(offset,ray.direction));
        if(distance>0.002f)throw std::runtime_error("projection / viewport / DPI roundtrip failed");
        log<<"roundtrip width="<<w<<" scale="<<scale<<" view="<<view<<" perpendicular_error_m="<<distance<<'\n';
    }
    // From directly above, rock must win; below the surface the terrain must occlude it.
    auto upper=pick({{35,0,40},{0,0,-1}},ground,rock,barrier);
    if(upper.id!=2)throw std::runtime_error("nearest rock picking failed");
    auto lower=pick({{35,0,-10},{0,0,1}},ground,rock,barrier);
    // Raylib triangles are two-sided for picking; visibility must still use nearest hit.
    if(lower.id!=1)throw std::runtime_error("terrain occlusion picking failed");
    auto miss=pick({{-100,-100,40},{0,0,1}},ground,rock,barrier);
    if(miss.id!=0)throw std::runtime_error("empty selection failed");
    auto board=pick({{60,0,40},{0,0,-1}},ground,rock,barrier);
    if(board.id!=3)throw std::runtime_error("barrier picking failed");
    log<<"PASS coordinate roundtrip (36), nearest mesh IDs, empty selection, occlusion\n";
}
double percentile(std::vector<double> values,double p) {
    if(values.empty())return 0;std::sort(values.begin(),values.end());return values[size_t(p*(values.size()-1))];
}
void labels(Font f,const char* text,float x,float y,int size=20,Color c=Color{218,225,235,255}) {DrawTextEx(f,text,{x,y},float(size),1,c);}
}

int runPrototype(const PrototypeOptions& opt) {
    // Read the Unicode path before opening a window, so missing fonts fail clearly.
    if(!std::filesystem::is_regular_file(opt.font)) throw std::runtime_error("CJK font not found (use --font)");
    std::filesystem::create_directories(opt.output);
    std::ofstream log(opt.output/L"events.txt"),perf(opt.output/L"frames.csv");
    if(!log||!perf)throw std::runtime_error("Prototype output directory is not writable");
    perf<<"scene,repeat,frames,p50_ms,p95_ms,p99_ms,peak_working_set_bytes,input_p95_ms,input_samples,width,height,viewport_w,viewport_h,dpi_x,dpi_y\n";
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIGHDPI);
    InitWindow(1280,720,"RollingStone3D - UI prototype / preset motion");
    if(!IsWindowReady())throw std::runtime_error("OpenGL window initialization failed");
    SetWindowMinSize(800,500);SetExitKey(KEY_NULL);SetTargetFPS(opt.benchmark?0:60);
    Font font=loadChinese(opt.font);GuiSetFont(font);GuiSetStyle(DEFAULT,TEXT_SIZE,20);
    Model ground=terrain(opt.stress),rock=stone(),barrier=LoadModelFromMesh(GenMeshCube(1,12,5));
    barrier.transform=MatrixTranslate(60,0,height(60,0)+2.5f);
    rock.transform=MatrixMultiply(MatrixMultiply(MatrixScale(2,2,2),MatrixRotateZ(0.71f)),MatrixTranslate(35,0,height(35,0)+2.2f));
    if(opt.selfTest) {
        cameraSequenceTests(log);
        checkCoordinates(ground,rock,barrier,log);
        UnloadModel(ground);UnloadModel(rock);UnloadModel(barrier);UnloadFont(font);CloseWindow();
        log.close();if(!log)throw std::runtime_error("self-test log write failed");
        std::cout<<"PASS UI coordinate / picking self-test (real OpenGL context)\n";return 0;
    }
    // This fixed load is static: preserve every original point/segment, but
    // compute trigonometry once and submit a single line batch per frame.
    // rlVertex3f automatically splits full buffers only between primitives.
    auto makeTrail=[](bool pressure){std::vector<Vector3> points;points.reserve((pressure?50:1)*2000);
        for(int j=0;j<(pressure?50:1);++j)for(int i=0;i<2000;++i){float x=10+100.0f*i/1999,y=float(j-(pressure?25:0))*0.8f;points.push_back({x,y,::height(x,y)+2+std::abs(std::sin(x*0.17f))*5});}return points;};
    const auto baselineTrail=makeTrail(false),stressTrail=makeTrail(true);
    StaticLines baselineLines,stressLines;baselineLines.load(baselineTrail,2000);stressLines.load(stressTrail,2000);
    log<<"fixed cached trail vertices="<<baselineTrail.size()<<','<<stressTrail.size()<<" segments=1999,99950 (unchanged load)\n";
    RenderTexture2D target{};Orbit orbit;Pick selected;
    bool stress=opt.stress,wire=false,trail=true,edit=false,dropdown=false;
    char radiusText[64]="2.0";float radius=2,brightness=1;int scene=stress?1:0;
    std::string error;rs3d::InputRouter inputRouter;
    double start=GetTime(),segmentStart=start,previous=start,lastCapture=start,lastPulse=start;int repetition=0,pulses=0;
    std::vector<double> frames,latencies;size_t peak=0;int frame=0;
    log<<"raylib="<<RAYLIB_VERSION<<" raygui=4.0 CRT=MT vsync=off FPS="<<(opt.benchmark?0:60)<<" font="<<opt.font.u8string()<<'\n';
    log<<machineDescription()<<'\n';
    while(!WindowShouldClose()) {
        double now=GetTime();double dt=now-previous;previous=now;
        if(opt.renderCheck) {
            if(frame==30)orbit.top();
            if(frame==60)orbit.side();
            if(frame==90){orbit=Orbit{};wire=true;}
            if(frame==120){wire=false;orbit.target={35,0,::height(35,0)+2.2f};orbit.distance=0.08f;}
            if(frame==150){orbit=Orbit{};SetWindowSize(1100,800);}
            if(frame==180)break;
        }
        if(opt.duration>0&&now-start>=opt.duration)break;
        Vector2 dpi=GetWindowScaleDPI();
        // GLFW reports framebuffer pixels after resize on Windows. Keep our UI in DIPs.
        int width=int(std::lround(GetRenderWidth()/dpi.x)),heightPx=int(std::lround(GetRenderHeight()/dpi.y));
        SetMouseScale(1.0f/dpi.x,1.0f/dpi.y);
        Rectangle viewport{280,56,float(std::max(1,width-280)),float(std::max(1,heightPx-96))};
        int tw=std::max(1,int(viewport.width*dpi.x)),th=std::max(1,int(viewport.height*dpi.y));
        if(target.texture.width!=tw||target.texture.height!=th) {
            if(target.id)UnloadRenderTexture(target);target=LoadRenderTexture(tw,th);
            if(!target.id||!target.depth.id)throw std::runtime_error("Viewport color/depth allocation failed");
            log<<"resize window="<<width<<'x'<<heightPx<<" texture="<<tw<<'x'<<th<<" dpi="<<dpi.x<<','<<dpi.y<<'\n';
        }
        Vector2 mouse=GetMousePosition();bool inside=CheckCollisionPointRec(mouse,viewport);
        bool press=IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
        bool down=IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        const bool pulse=opt.benchmark&&now-segmentStart>5&&now-lastPulse>=0.5;
        bool input=press||GetMouseWheelMove()!=0||(down&&Vector2Length(GetMouseDelta())>0)||IsKeyPressed(KEY_ENTER)||pulse;
        double inputTime=input?GetTime():-1;
        if(!IsWindowFocused()){edit=false;dropdown=false;}
        rs3d::CameraInput cameraInput{IsWindowFocused(),press,down,inside,edit,dropdown,
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT),IsMouseButtonDown(MOUSE_BUTTON_MIDDLE),GetMouseDelta(),GetMouseWheelMove()};
        // Reproducible software-side input-to-draw probe; never represented as an OS event.
        if(pulse){lastPulse=now;cameraInput={true,true,true,true,false,false,true,false,{(++pulses%2)?0.5f:-0.5f,0},0};}
        const auto owner=rs3d::updateCamera(orbit,inputRouter,cameraInput);
        Camera3D camera=orbit.camera();
        rock.transform=MatrixMultiply(MatrixMultiply(MatrixScale(radius,radius,radius),MatrixRotateZ(float((now-start)*0.2))),MatrixTranslate(35,0,::height(35,0)+radius+0.2f));
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&owner==rs3d::InputOwner::viewport) {
            Ray ray=viewportRay({(mouse.x-viewport.x)*dpi.x,(mouse.y-viewport.y)*dpi.y},camera,tw,th);
            selected=pick(ray,ground,rock,barrier);
            log<<std::setprecision(10)<<"pick id="<<selected.id<<" distance="<<selected.distance<<" world="<<selected.point.x<<','<<selected.point.y<<','<<selected.point.z<<'\n';
        }
        BeginTextureMode(target);ClearBackground(Color{26,36,51,255});BeginMode3D(camera);
        auto draw=[&](Model model,Color tint) {if(wire)DrawModelWires(model,{0,0,0},1,tint);else DrawModel(model,{0,0,0},1,tint);};
        Color shade{static_cast<unsigned char>(255*brightness),static_cast<unsigned char>(255*brightness),static_cast<unsigned char>(255*brightness),255};
        draw(ground,selected.id==1?Color{200,255,205,255}:shade);draw(rock,selected.id==2?YELLOW:WHITE);draw(barrier,selected.id==3?YELLOW:Color{140,175,230,255});
        if(trail)(stress?stressLines:baselineLines).draw();
        DrawCubeWiresV({90,0,5},{40,40,10},SKYBLUE);
        DrawLine3D({0,0,21},{10,0,21},RED);DrawLine3D({0,0,21},{0,10,21},GREEN);DrawLine3D({0,0,21},{0,0,31},BLUE);
        EndMode3D();EndTextureMode();
        BeginDrawing();ClearBackground(Color{20,26,36,255});
        DrawTexturePro(target.texture,{0,0,float(tw),-float(th)},viewport,{0,0},0,WHITE);
        labels(font,u8"界面原型／预设运动  |  非物理模拟",18,16,22,Color{242,191,103,255});
        if(dropdown)GuiLock();
        if(GuiButton({16,74,75,30},u8"俯视"))orbit.top();
        if(GuiButton({98,74,75,30},u8"侧视"))orbit.side();
        if(GuiButton({180,74,75,30},u8"重置"))orbit=Orbit{};
        GuiCheckBox({20,124,20,20},u8"线框",&wire);GuiCheckBox({140,124,20,20},u8"轨迹",&trail);
        labels(font,u8"半径 r (m)",18,168);
        if(GuiTextBox({18,196,150,32},radiusText,64,edit))edit=!edit;
        if(GuiButton({180,196,75,32},u8"应用")) {
            try {size_t used=0;float v=std::stof(radiusText,&used);if(used!=std::string(radiusText).size()||!std::isfinite(v)||v<0.2f||v>10)throw std::runtime_error("range");radius=v;error.clear();edit=false;log<<"apply radius="<<v<<'\n';}
            catch(...) {error=u8"非法输入: 0.2 <= r <= 10";log<<"reject radius="<<radiusText<<'\n';}
        }
        labels(font,error.c_str(),16,240,16,ORANGE);
        labels(font,u8"调整亮度",18,282);GuiSlider({24,316,220,22},nullptr,nullptr,&brightness,0.35f,1.0f);
        labels(font,u8"选择",18,410);const char* names[]={u8"无",u8"地形",u8"石块",u8"挡板"};labels(font,names[selected.id],94,410,20,YELLOW);
        labels(font,TextFormat("x %.3f  y %.3f",selected.point.x,selected.point.y),18,446,16);
        labels(font,TextFormat("z %.3f m",selected.point.z),18,470,16);
        labels(font,TextFormat("%d triangles",stress?76800:19200),18,510,17);
        labels(font,TextFormat("%d x 2000 points",stress?50:1),18,536,17);
        // Draw the dropdown last so its modal list owns every click above the viewport.
        GuiUnlock();
        if(GuiDropdownBox({18,364,237,32},u8"基准;压力",&scene,dropdown))dropdown=!dropdown;
        if((scene==1)!=stress) {stress=scene==1;UnloadModel(ground);ground=terrain(stress);frames.clear();latencies.clear();segmentStart=GetTime();}
        labels(font,u8"右拖旋转 / 中拖平移 / 滚轮缩放 | Z 上 | 每格 1 m",292,float(heightPx-31),17);
        DrawFPS(width-88,18);
        if(opt.capture&&((!opt.renderCheck&&now-lastCapture>2)||(opt.renderCheck&&frame%30==25))) {
            SetWindowTitle(TextFormat("RollingStone3D - capture frame %d (preset motion)",frame));
            rlDrawRenderBatchActive();Image screen{};
            screen.width=GetRenderWidth();screen.height=GetRenderHeight();screen.mipmaps=1;screen.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            screen.data=rlReadScreenPixels(screen.width,screen.height);int byteCount=0;
            unsigned char* bytes=ExportImageToMemory(screen,".png",&byteCount);
            auto filename=opt.renderCheck?std::filesystem::path("view_"+std::to_string(frame/30)+".png"):std::filesystem::path("framebuffer.png");
            std::ofstream capture(opt.output/filename,std::ios::binary);
            capture.write(reinterpret_cast<char*>(bytes),byteCount);capture.close();
            MemFree(bytes);UnloadImage(screen);lastCapture=now;
            if(!capture)throw std::runtime_error("Frame capture write failed");
        }
        EndDrawing();
        if(inputTime>=0) {double latency=(GetTime()-inputTime)*1000;latencies.push_back(latency);log<<"input_to_draw_ms="<<latency<<" source="<<(pulse?"scripted_controller":"native")<<" owner="<<int(owner)<<" edit="<<edit<<" menu="<<dropdown<<" brightness="<<brightness<<" yaw="<<orbit.yaw<<" pitch="<<orbit.pitch<<" distance="<<orbit.distance<<'\n';}
        if(++frame%30==0) {peak=std::max(peak,peakWorkingSet());log.flush();}
        if(opt.benchmark&&now-segmentStart>5)frames.push_back(dt*1000);
        if(opt.benchmark&&now-segmentStart>=35) {
            perf<<(stress?"stress":"baseline")<<','<<repetition+1<<','<<frames.size()<<','<<percentile(frames,.50)<<','<<percentile(frames,.95)<<','<<percentile(frames,.99)<<','<<peak<<',';
            if(!latencies.empty())perf<<percentile(latencies,.95);
            perf<<','<<latencies.size()<<','<<width<<','<<heightPx<<','<<tw<<','<<th<<','<<dpi.x<<','<<dpi.y<<'\n';perf.flush();
            if(!perf)throw std::runtime_error("Benchmark write failed");
            std::cout<<"benchmark "<<(stress?"stress":"baseline")<<" repeat="<<repetition+1<<" p95="<<percentile(frames,.95)<<" ms\n"<<std::flush;
            frames.clear();latencies.clear();segmentStart=GetTime();
            if(++repetition==3) {if(stress)break;repetition=0;stress=true;scene=1;UnloadModel(ground);ground=terrain(true);}
        }
    }
    log<<"exit clean; no physics data generated\n";
    baselineLines.clear();stressLines.clear();UnloadRenderTexture(target);UnloadModel(ground);UnloadModel(rock);UnloadModel(barrier);UnloadFont(font);CloseWindow();
    perf.close();log.close();if(!perf||!log)throw std::runtime_error("Prototype evidence flush failed");return 0;
}
