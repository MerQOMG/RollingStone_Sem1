#include "app.h"
#include "app_view.h"
#include "editor.h"
#include "platform.h"
#include <raygui.h>
#include <rlgl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <deque>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
namespace {
using namespace rs3d;
const char*labels=u8"单次运行批量暂停继续取消重置重新随机保存加载相机俯视侧视线框轨迹统计落石初值材料地形挡板应用更改设置释放点结束选点上一页下一页当前选择无空闲正在生成真实预览后台计算完成失败取消中已处理正常截断请求数量耗时秒本批参数哈希鼠标右键拖动旋转中键平移滚轮缩放世界向上比例每轴米质心速度自旋配置加载失败输入必须是有限数值范围错误保留原场景保存成功相机文件中文目录输出场景验证测试文件空间独立种子仅在空闲重新随机请点击地形释放点已更新运行期间参数锁定固定值或逗号分隔区间例显示关闭窗口正在安全取消未完成批次正式概率为空文件全部写入已完成总计静止上游下游横向退出超时几何数值错误单击对象高亮保存视角读取视角请先应用未应用更改帧率不限恢复即将关闭选择参数组正确最近交点单位确认文本编辑菜单弹性位移支承力阻尼耗能储能回弹刚性";
struct Window {Window(){SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_ALWAYS_RUN);InitWindow(1280,720,"RollingStone3D - 3D rigid-body simulation");if(!IsWindowReady())throw std::runtime_error("OpenGL window initialization failed");SetWindowMinSize(800,600);SetExitKey(KEY_NULL);}~Window(){CloseWindow();}};
// raylib 5.5 defaults to one stream buffer. Reusing it for offscreen 3D and
// GUI flushes can stall on the Intel driver; rotate the public rlgl batches.
struct RenderBatchOwner {
    rlRenderBatch value{};
    RenderBatchOwner(){value=rlLoadRenderBatch(3,8192);rlSetRenderBatchActive(&value);}
    ~RenderBatchOwner(){rlSetRenderBatchActive(nullptr);rlUnloadRenderBatch(value);}
};
struct FontOwner {Font value{};~FontOwner(){if(value.texture.id)UnloadFont(value);}};
struct Target {RenderTexture2D value{};~Target(){if(value.id)UnloadRenderTexture(value);}void resize(int width,int height){if(value.texture.width==width&&value.texture.height==height)return;if(value.id)UnloadRenderTexture(value);value=LoadRenderTexture(width,height);if(!value.id||!value.depth.id)throw std::runtime_error("viewport color/depth allocation failed");}};
std::string number(double value){std::ostringstream out;out<<std::setprecision(17)<<value;return out.str();}
double percentile(std::vector<double>v,double p){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[size_t(p*(v.size()-1))];}
OrbitCamera fitted(const PhysicsScene&scene){OrbitCamera c;const auto&t=*scene.terrain;double low=1e300,high=-1e300;for(auto v:t.vertices){low=std::min(low,v.z);high=std::max(high,v.z);}high=std::max(high,scene.initial.shapeOrigin.z+scene.radius);c.target={float((t.xMin+t.xEnd)/2),0,float((low+high)/2)};double radius=.5*norm(Vec3{t.xEnd-t.xMin,t.yMax-t.yMin,high-low});c.distance=float(std::clamp(radius/std::sin(22.5*pi/180)*1.12,25.0,750.0));return c;}
const char* phaseName(SessionPhase phase){switch(phase){case SessionPhase::idle:return u8"空闲";case SessionPhase::generating:return u8"正在生成";case SessionPhase::preview:return u8"真实预览";case SessionPhase::background:return u8"后台计算";case SessionPhase::finished:return u8"结束";}return "";}
const char* stateName(SessionState state){switch(state){case SessionState::idle:return u8"空闲";case SessionState::running:return u8"运行";case SessionState::paused:return u8"暂停";case SessionState::cancelling:return u8"取消中";case SessionState::completed:return u8"完成";case SessionState::failed:return u8"未完成";}return "";}
void wrap(Font font,const std::string&message,float x,float y,float width,int size,Color color,int maxLines){
    int length=0;auto cps=LoadCodepoints(message.c_str(),&length);std::string line;int lines=0;
    for(int i=0;i<length&&lines<maxLines;++i){int bytes=0;auto encoded=CodepointToUTF8(cps[i],&bytes);std::string next(encoded,bytes);if(MeasureTextEx(font,(line+next).c_str(),float(size),1).x>width&&!line.empty()){ui::text(font,line,x,y+lines*(size+3),size,color);++lines;line.clear();}line+=next;}if(lines<maxLines)ui::text(font,line,x,y+lines*(size+3),size,color);UnloadCodepoints(cps);
}
}
int runApplication(const rs3d::SceneDraft&draft,const AppOptions&opt){
#ifdef _DEBUG
    if(opt.selfTest){for(int type:{_CRT_WARN,_CRT_ERROR,_CRT_ASSERT}){_CrtSetReportMode(type,_CRTDBG_MODE_FILE);_CrtSetReportFile(type,_CRTDBG_FILE_STDERR);}}
#endif
    using namespace rs3d;if(!std::filesystem::is_regular_file(opt.font))throw std::runtime_error("CJK font not found; use --font path");
    Session session(draft);SceneEditor editor(draft);std::filesystem::create_directories(opt.evidence);std::ofstream log(opt.evidence/L"events.txt"),framesFile(opt.evidence/L"frames.csv");if(!log||!framesFile)throw std::runtime_error("GUI evidence directory is not writable");
    framesFile<<"repeat,frames,p50_ms,p95_ms,p99_ms,peak_working_set_bytes,input_p95_ms,input_max_ms,input_samples,width,height,viewport_w,viewport_h,dpi_x,dpi_y,fps_limit\n";
    Window window;RenderBatchOwner renderBatch;FontOwner font;font.value=ui::loadFont(opt.font,std::string(labels)+u8"：，。；（）已参数读取加载应用"+editor.glyphs()+draft.outputRoot.u8string()+opt.evidence.u8string());GuiSetFont(font.value);GuiSetStyle(DEFAULT,TEXT_SIZE,18);GuiSetStyle(DEFAULT,BACKGROUND_COLOR,0x1a232fff);
    GuiSetStyle(DEFAULT,BASE_COLOR_NORMAL,0x293646ff);GuiSetStyle(DEFAULT,TEXT_COLOR_NORMAL,0xe0e8f2ff);GuiSetStyle(DEFAULT,BORDER_COLOR_NORMAL,0x65758aff);GuiSetStyle(DEFAULT,BASE_COLOR_FOCUSED,0x405873ff);GuiSetStyle(DEFAULT,TEXT_COLOR_FOCUSED,0xffffffff);GuiSetStyle(DEFAULT,TEXT_COLOR_PRESSED,0xffffffff);GuiSetStyle(DEFAULT,BASE_COLOR_PRESSED,0x397b93ff);GuiSetStyle(DEFAULT,TEXT_COLOR_DISABLED,0x8492a4ff);GuiSetStyle(DEFAULT,BASE_COLOR_DISABLED,0x27303bff);
    int fps=opt.fps;SetTargetFPS(fps);ui::SceneModels models;auto view=session.view();models.update(view.scene,view.snapshot);OrbitCamera orbit=fitted(*view.scene);Target target;InputRouter router;ui::Pick selected;
    bool wire=false,showTrails=true,showStats=true,menu=false,releaseMode=false,dirty=false,closing=false,resetPending=false,autoStarted=false;int group=0,page=0,active=-1,frame=0,lastGroup=0,lastEdited=-1,errorField=-1;
    std::string message,error;std::filesystem::path configPath=L"configs3d/scene.ini",cameraPath=opt.evidence/L"camera.ini";
    std::deque<std::vector<Vector3>>trails;std::shared_ptr<const PhysicsScene>lastScene=view.scene;double lastSnapshotTime=-1,start=GetTime(),previous=start,lastCapture=-10,lastPulse=start,segmentStart=start;int repeat=0,pulseSign=1,exerciseStep=0;size_t peak=0;
    std::vector<double>times,latencies;SessionState previousState=view.state;SessionPhase previousPhase=view.phase;std::uint64_t previousTrial=0;double requestTime=-1;std::string requestName;
    log<<std::setprecision(17)<<machineDescription()<<"\nraylib="<<RAYLIB_VERSION<<" raygui=4.0 font="<<opt.font.u8string()<<" fps="<<fps<<" vsync=off\nsource=GUI actual physics; helper lines use depth test; latest snapshot bounded to one\n";
    auto apply=[&](){auto next=editor.parse();session.apply(next);editor.sync(session.draft());dirty=false;active=-1;error.clear();errorField=-1;message=u8"参数已应用";releaseMode=false;selected={};trails.clear();lastSnapshotTime=-1;log<<"apply config_hash="<<session.view().config->hash<<" barrier="<<next.barrier.enabled<<'\n';};
    auto action=[&](auto operation,const char*name){try{operation();error.clear();errorField=-1;log<<"action="<<name<<"\n";}catch(const std::exception&e){error=std::string(u8"错误，保留原场景： ")+e.what();errorField=std::string(name)=="apply"?lastEdited:-1;log<<"reject action="<<name<<" error="<<e.what()<<'\n';}};
    if(opt.selfTest){ui::verifyScene(models,view,log);Trial probe(*view.scene,{.1});for(int i=0;i<50;++i)probe.step();auto moving=view;moving.snapshot=probe.snapshot();models.update(moving.scene,moving.snapshot);for(auto local:moving.scene->shape.vertices){auto expected=moving.snapshot.shapeOrigin+moving.snapshot.rotation.matrix()*(local-moving.scene->shape.center);auto rendered=Vector3Transform(ui::vector(local-moving.scene->shape.center),models.rock.transform);if(norm(expected-Vec3{rendered.x,rendered.y,rendered.z})>2e-5)throw std::runtime_error("moving physical pose/render mismatch");}models.update(view.scene,view.snapshot);log<<"PASS real stepped pose render transformation\n";}
    for(;;){
        double now=GetTime(),frameSeconds=now-previous;previous=now;view=session.view();bool busy=!view.workerFinished;
        if(WindowShouldClose()||(opt.duration>0&&now-start>=opt.duration)||(opt.selfTest&&frame>=3)){closing=true;session.cancel();}
        if(closing&&!busy)break;
        if(resetPending&&!busy){action([&]{session.reset();editor.sync(session.draft());trails.clear();lastSnapshotTime=-1;dirty=false;},"reset_after_cancel");resetPending=false;view=session.view();}
        if(opt.exitAfterRun&&autoStarted&&!busy)break;
        if(!autoStarted&&frame>=2&&(opt.startSingle||opt.startBatch)){autoStarted=true;action([&]{if(opt.startSingle)session.startSingle();else session.startBatch(opt.preview);},"auto_start");view=session.view();busy=!view.workerFinished;}
        // Repeated real trials keep the preview benchmark occupied for all 3x30s.
        // No physics/timing tolerances are changed; each run has its own output transaction.
        if(opt.benchmark&&autoStarted&&!busy&&!closing){action([&]{if(opt.startSingle)session.startSingle();else if(opt.startBatch)session.startBatch(opt.preview);},"benchmark_restart");view=session.view();busy=!view.workerFinished;}
        if(opt.exerciseView){double elapsed=now-start;
            if(exerciseStep==0&&elapsed>.6){orbit.top();showTrails=false;log<<"exercise camera_top trails_off\n";++exerciseStep;}
            if(exerciseStep==1&&elapsed>1){SetWindowSize(1100,680);log<<"exercise resize=1100x680\n";++exerciseStep;}
            if(exerciseStep==2&&elapsed>1.5){MinimizeWindow();log<<"exercise minimized\n";++exerciseStep;}
            if(exerciseStep==3&&elapsed>2){RestoreWindow();SetWindowSize(1280,720);log<<"exercise restored\n";++exerciseStep;}
            if(exerciseStep==4&&elapsed>2.5){session.pause(true);log<<"exercise pause sim="<<view.snapshot.time<<'\n';++exerciseStep;}
            if(exerciseStep==5&&elapsed>2.8){session.pause(false);log<<"exercise resume sim="<<view.snapshot.time<<'\n';++exerciseStep;}
        }
        if(view.state!=previousState||view.phase!=previousPhase||view.trialId!=previousTrial){log<<"state="<<sessionStateName(view.state)<<" phase="<<int(view.phase)<<" trial="<<view.trialId<<" processed="<<view.processed<<" C="<<view.completed<<" failures="<<view.failures<<" hash="<<view.config->hash<<" sim="<<view.snapshot.time<<" wall="<<view.wallSeconds<<'\n';previousState=view.state;previousPhase=view.phase;previousTrial=view.trialId;}
        if(requestTime>=0&&((requestName=="pause"&&view.pauseAcknowledged)||(requestName=="resume"&&view.state==SessionState::running)||(requestName=="cancel"&&view.workerFinished))){log<<"request="<<requestName<<" confirmation_ms="<<(now-requestTime)*1000<<'\n';requestTime=-1;}
        Vector2 dpi=GetWindowScaleDPI();int width=int(std::lround(GetRenderWidth()/dpi.x)),height=int(std::lround(GetRenderHeight()/dpi.y));SetMouseScale(1/dpi.x,1/dpi.y);
        Rectangle viewport{320,52,float(std::max(1,width-320)),float(std::max(1,height-136))};int tw=std::max(1,int(viewport.width*dpi.x)),th=std::max(1,int(viewport.height*dpi.y));bool resized=target.value.texture.width!=tw||target.value.texture.height!=th;target.resize(tw,th);if(resized)log<<"resize window="<<width<<'x'<<height<<" viewport="<<tw<<'x'<<th<<" dpi="<<dpi.x<<','<<dpi.y<<'\n';
        if(view.scene!=lastScene){if(view.phase==SessionPhase::preview){if(trails.size()==2)trails.pop_front();trails.push_back({});}else if(view.phase==SessionPhase::idle)trails.clear();lastScene=view.scene;lastSnapshotTime=-1;}
        models.update(view.scene,view.snapshot);
        if(view.phase==SessionPhase::preview&&view.snapshot.time!=lastSnapshotTime){if(trails.empty())trails.push_back({});auto&trail=trails.back();if(trail.size()>=2000)trail.erase(trail.begin(),trail.begin()+2);trail.push_back(ui::vector(view.snapshot.center));lastSnapshotTime=view.snapshot.time;}
        auto mouse=GetMousePosition();bool inside=CheckCollisionPointRec(mouse,viewport),press=IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE),down=IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        bool pulse=opt.benchmark&&now-segmentStart>5&&now-lastPulse>=.5;
        bool input=press||GetMouseWheelMove()!=0||(down&&Vector2Length(GetMouseDelta())>0)||IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_BACKSPACE)||pulse;
        double inputTime=input?GetTime():-1;if(!IsWindowFocused()){active=-1;menu=false;}
        CameraInput cameraInput{IsWindowFocused(),press,down,inside,active>=0,menu,IsMouseButtonDown(MOUSE_BUTTON_RIGHT),IsMouseButtonDown(MOUSE_BUTTON_MIDDLE),GetMouseDelta(),GetMouseWheelMove()};
        if(pulse){lastPulse=now;pulseSign=-pulseSign;cameraInput={true,true,true,true,false,false,true,false,{float(pulseSign)*.5f,0},0};}
        if(opt.exerciseView&&now-start>2.8)cameraInput={true,true,true,true,false,false,true,false,{.5f,0},0};
        auto owner=updateCamera(orbit,router,cameraInput);auto camera=orbit.camera();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&owner==InputOwner::viewport&&!menu&&active<0){selected=models.pick(ui::viewportRay({(mouse.x-viewport.x)*dpi.x,(mouse.y-viewport.y)*dpi.y},camera,tw,th));log<<"pick id="<<selected.id<<" distance="<<selected.distance<<" world="<<selected.point.x<<','<<selected.point.y<<','<<selected.point.z<<'\n';
            if(releaseMode&&!busy){if(selected.id==terrainObjectId){editor.set("initial.x",number(selected.point.x));editor.set("initial.y",number(selected.point.y));dirty=true;action(apply,"release_point");if(error.empty())message=u8"释放点已更新";}else message=u8"请点击地形";}else if(selected.id){group=selected.id==terrainObjectId?2:(selected.id==barrierObjectId?3:0);page=0;}}
        double renderStart=GetTime();
        BeginTextureMode(target.value);ClearBackground({24,35,49,255});BeginMode3D(camera);double targetReady=GetTime();models.draw(wire,selected.id);double modelEnd=GetTime();
        if(showTrails)for(const auto&trail:trails)for(size_t i=1;i<trail.size();++i)DrawLine3D(trail[i-1],trail[i],{246,191,83,255});
        if(showStats){auto lo=view.config->grid.lower,hi=view.config->grid.upper;DrawCubeWiresV(ui::vector((lo+hi)/2),ui::vector(hi-lo),SKYBLUE);float x=float(view.config->collectorX);DrawLine3D({x,float(lo.y),float(lo.z)},{x,float(hi.y),float(lo.z)},MAGENTA);DrawLine3D({x,float(hi.y),float(lo.z)},{x,float(hi.y),float(hi.z)},MAGENTA);DrawLine3D({x,float(hi.y),float(hi.z)},{x,float(lo.y),float(hi.z)},MAGENTA);DrawLine3D({x,float(lo.y),float(hi.z)},{x,float(lo.y),float(lo.z)},MAGENTA);}
        float axisLength=5;auto origin=ui::vector(view.snapshot.center);DrawLine3D(origin,Vector3Add(origin,{axisLength,0,0}),RED);DrawLine3D(origin,Vector3Add(origin,{0,axisLength,0}),GREEN);DrawLine3D(origin,Vector3Add(origin,{0,0,axisLength}),BLUE);double helpersEnd=GetTime();EndMode3D();double flushEnd=GetTime();EndTextureMode();
        double renderEnd=GetTime();
        BeginDrawing();ClearBackground({18,25,35,255});DrawTexturePro(target.value.texture,{0,0,float(tw),-float(th)},viewport,{0,0},0,WHITE);DrawRectangle(0,52,320,height-136,{26,35,47,255});
        if(menu)GuiLock();if(busy||dirty||closing)GuiDisable();
        if(GuiButton({12,10,70,32},u8"单次"))action([&]{session.startSingle();active=-1;trails.clear();},"single");
        if(GuiButton({88,10,70,32},u8"批量"))action([&]{session.startBatch();active=-1;trails.clear();},"batch");GuiEnable();
        if(!busy||closing)GuiDisable();if(GuiButton({164,10,70,32},view.state==SessionState::paused?u8"继续":u8"暂停")){bool pause=view.state!=SessionState::paused;session.pause(pause);requestTime=GetTime();requestName=pause?"pause":"resume";active=-1;log<<"request="<<requestName<<'\n';}
        if(GuiButton({240,10,70,32},u8"取消")){session.cancel();requestTime=GetTime();requestName="cancel";log<<"request=cancel\n";}GuiEnable();
        if(closing)GuiDisable();if(GuiButton({316,10,70,32},u8"重置"))action([&]{if(busy){session.cancel();resetPending=true;}else{session.reset();editor.sync(session.draft());dirty=false;trails.clear();lastSnapshotTime=-1;}active=-1;},"reset");
        if(busy||closing)GuiDisable();if(GuiButton({392,10,90,32},u8"重新随机"))action([&]{session.randomize();editor.sync(session.draft());dirty=false;active=-1;},"randomize");
        if(GuiButton({488,10,70,32},u8"保存")){inputTime=-1;action([&]{auto parsed=editor.parse();auto path=ui::chooseSceneFile(true,configPath);if(!path.empty()){saveScene(path,parsed);configPath=path;message=u8"配置保存成功";log<<"save_scene path="<<path.u8string()<<'\n';}},"save_scene");active=-1;}
        if(GuiButton({564,10,70,32},u8"加载")){inputTime=-1;action([&]{auto path=ui::chooseSceneFile(false,configPath);if(!path.empty()){auto loaded=loadScene(path);session.apply(loaded);editor.sync(loaded);configPath=path;dirty=false;trails.clear();orbit=fitted(*session.view().scene);message=u8"配置已加载";log<<"load_scene path="<<path.u8string()<<'\n';}},"load_scene");active=-1;}GuiEnable();
        if(GuiButton({float(width-136),10,124,32},fps?TextFormat("FPS %d",fps):u8"FPS 不限")){fps=fps==60?30:(fps==30?0:60);SetTargetFPS(fps);log<<"fps_limit="<<fps<<'\n';}
        if(GuiButton({16,62,84,28},u8"俯视"))orbit.top();if(GuiButton({110,62,84,28},u8"侧视"))orbit.side();if(GuiButton({204,62,100,28},u8"相机重置"))orbit=fitted(*view.scene);
        GuiCheckBox({18,104,18,18},u8"线框",&wire);GuiCheckBox({118,104,18,18},u8"轨迹",&showTrails);GuiCheckBox({218,104,18,18},u8"统计",&showStats);
        std::vector<int>visible;for(int i=0;i<int(editor.fields.size());++i)if(editor.fields[i].group==group)visible.push_back(i);int rows=std::max(1,(height-350)/68),pages=std::max(1,(int(visible.size())+rows-1)/rows);if(group!=lastGroup){page=0;active=-1;lastGroup=group;}page=std::clamp(page,0,pages-1);
        if(GuiButton({224,136,34,30},"<")){page=std::max(0,page-1);active=-1;}if(GuiButton({270,136,34,30},">")){page=std::min(pages-1,page+1);active=-1;}
        if(busy||closing)GuiDisable();for(int row=0;row<rows;++row){int index=page*rows+row;if(index>=int(visible.size()))break;int id=visible[index];auto&field=editor.fields[id];float y=176+float(row)*68;ui::text(font.value,field.label,16,y,17,(!error.empty()&&(error.find(field.key)!=error.npos||id==errorField))?ORANGE:Color{215,225,238,255});std::string old=field.text.data();if(GuiTextBox({16,y+23,288,28},field.text.data(),int(field.text.size()),active==id))active=active==id?-1:id;if(old!=field.text.data()){dirty=true;lastEdited=id;}
            if(!error.empty()&&(error.find(field.key)!=error.npos||id==errorField)){auto cut=error.find(": ");wrap(font.value,cut==error.npos?error:error.substr(cut+2),16,y+53,284,11,ORANGE,1);}}
        if(GuiButton({16,float(height-136),140,32},dirty?u8"应用更改 *":u8"应用更改"))action(apply,"apply");if(GuiButton({164,float(height-136),140,32},releaseMode?u8"结束选点":u8"设置释放点")){releaseMode=!releaseMode;active=-1;message=releaseMode?u8"请点击地形":u8"单击对象高亮";}GuiEnable();
        if(GuiButton({16,float(height-174),140,28},u8"保存视角"))action([&]{ui::saveCamera(cameraPath,orbit);message=u8"相机保存成功";},"save_camera");if(GuiButton({164,float(height-174),140,28},u8"读取视角"))action([&]{orbit=ui::loadCamera(cameraPath);message=u8"相机已加载";},"load_camera");
        ui::text(font.value,TextFormat("%d / %d",page+1,pages),228,170,14);ui::text(font.value,busy?u8"运行期间参数锁定":u8"固定值或区间：例 0,1",16,float(height-100),14,{164,187,207,255});
        GuiUnlock();if(GuiDropdownBox({16,136,194,30},u8"落石 / 批次;初值 / 材料;地形;挡板;统计 / 输出",&group,menu)){menu=!menu;active=-1;}
        DrawRectangle(328,60,std::max(1,width-336),view.snapshot.barrier.elastic?121:75,{16,24,36,218});ui::text(font.value,std::string(u8"本批参数哈希 ")+view.config->hash,338,67,16);ui::text(font.value,TextFormat("t %.3f s   COM (%.3f, %.3f, %.3f) m",view.snapshot.time,view.snapshot.center.x,view.snapshot.center.y,view.snapshot.center.z),338,90,16);ui::text(font.value,TextFormat("|v| %.3f m/s   |omega| %.3f rad/s   k %d   r %.2f m",norm(view.snapshot.velocity),norm(view.snapshot.angularVelocity),int(view.scene->shape.vertices.size()),view.scene->radius),338,112,16);
        if(view.snapshot.barrier.elastic){const auto&b=view.snapshot.barrier;
            ui::text(font.value,TextFormat(u8"弹性挡板 q %.3f m  v %.3f m/s  支承力 %.2f kN",b.displacement,b.velocity,b.supportForce/1000),338,135,16);
            ui::text(font.value,TextFormat(u8"储能 %.2f kJ  阻尼耗能 %.2f kJ",b.springEnergy/1000,b.dampingEnergy/1000),338,157,16);}
        const char*names[]{u8"无",u8"地形",u8"落石",u8"挡板"};ui::text(font.value,std::string(u8"选择：")+names[selected.id]+"   "+(releaseMode?u8"请点击地形":""),334,float(height-133),16,YELLOW);ui::text(font.value,u8"右拖旋转 / 中拖平移 / 滚轮缩放 | Z 向上 | 每轴 5 m",334,float(height-108),15);
        DrawRectangle(0,height-84,width,84,{14,20,29,255});std::string summary=std::string(stateName(view.state))+" / "+phaseName(view.phase)+"  "+u8"已处理 "+std::to_string(view.processed)+"/"+std::to_string(view.config->source.count)+u8"  正常 "+std::to_string(view.completed)+u8"  失败/截断 "+std::to_string(view.failures)+"  "+std::string(TextFormat("%.1f",view.wallSeconds))+" s";ui::text(font.value,summary,16,float(height-76),18);
        std::string detail=closing?u8"关闭窗口：正在安全取消":(!error.empty()?error:(!view.error.empty()?view.error:message));if(view.result&&error.empty()&&view.error.empty()&&!closing){detail=std::string(u8"输出： ")+view.result->directory.u8string()+"  status="+batchStatusName(view.result->status);if(view.result->status!=BatchStatus::complete)detail+=u8"  未完成批次，正式概率为空";}
        wrap(font.value,detail,16,float(height-49),float(width-32),16,error.empty()?Color{174,198,219,255}:ORANGE,2);
        if((opt.capture&&now-lastCapture>=2)||(opt.selfTest&&frame==2)){ui::capture(opt.evidence/L"framebuffer.png");lastCapture=now;log<<"capture frame="<<frame<<'\n';}
        double uiEnd=GetTime();EndDrawing();double drawEnd=GetTime();
        if(opt.benchmark&&drawEnd-previous>.05)log<<"slow_frame prepare_ms="<<(renderStart-previous)*1000<<" render3d_ms="<<(renderEnd-renderStart)*1000<<" target_ms="<<(targetReady-renderStart)*1000<<" models_ms="<<(modelEnd-targetReady)*1000<<" helpers_ms="<<(helpersEnd-modelEnd)*1000<<" flush_ms="<<(flushEnd-helpersEnd)*1000<<" ui_ms="<<(uiEnd-renderEnd)*1000<<" present_ms="<<(drawEnd-uiEnd)*1000<<" focused="<<IsWindowFocused()<<" minimized="<<IsWindowMinimized()<<"\n";
        if(inputTime>=0){double latency=(GetTime()-inputTime)*1000;latencies.push_back(latency);log<<"input_to_draw_ms="<<latency<<" source="<<(pulse?"scripted_controller":"native")<<" owner="<<int(owner)<<" editor="<<active<<" menu="<<menu<<" yaw="<<orbit.yaw<<" pitch="<<orbit.pitch<<" distance="<<orbit.distance<<'\n';}
        if(++frame%30==0){peak=std::max(peak,peakWorkingSet());log.flush();}
        if(opt.benchmark&&now-segmentStart>5)times.push_back(frameSeconds*1000);if(opt.benchmark&&now-segmentStart>=35){framesFile<<++repeat<<','<<times.size()<<','<<percentile(times,.5)<<','<<percentile(times,.95)<<','<<percentile(times,.99)<<','<<peak<<','<<percentile(latencies,.95)<<','<<percentile(latencies,1)<<','<<latencies.size()<<','<<width<<','<<height<<','<<tw<<','<<th<<','<<dpi.x<<','<<dpi.y<<','<<fps<<'\n';framesFile.flush();times.clear();latencies.clear();segmentStart=GetTime();if(repeat==3){closing=true;session.cancel();}}
    }
    auto final=session.view();log<<"exit worker_finished="<<final.workerFinished<<" state="<<sessionStateName(final.state)<<" hash="<<final.config->hash<<'\n';if(final.result)log<<"result_directory="<<final.result->directory.u8string()<<" status="<<batchStatusName(final.result->status)<<'\n';log.close();framesFile.close();if(!log||!framesFile)throw std::runtime_error("GUI evidence flush failed");
#ifdef _DEBUG
    if(opt.selfTest&&!_CrtCheckMemory())throw std::runtime_error("CRT heap corruption before renderer teardown");
#endif
    if(!error.empty()||!final.error.empty())return 1;
    return final.result&&final.result->status!=BatchStatus::complete?(final.result->status==BatchStatus::cancelled?2:1):0;
}
