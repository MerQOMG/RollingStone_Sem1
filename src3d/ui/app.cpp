// Application shell: window, main loop, layout, top bar, shortcuts, actions and the screenshot tour.
#include <cstdlib>
#include "ui/app_impl.h"
#include <rlgl.h>
#include <fstream>
#include <algorithm>
#include <chrono>
namespace rs3d::gui {
namespace fs=std::filesystem;
static fs::path findAsset(const std::string&rel){
    std::vector<fs::path> roots;roots.push_back(fs::path(GetApplicationDirectory()));roots.push_back(fs::current_path());
    for(auto base:std::vector<fs::path>{roots[0],roots[1]}){auto p=base;for(int i=0;i<5;++i){roots.push_back(p);if(!p.has_parent_path()||p.parent_path()==p)break;p=p.parent_path();}}
    for(auto&r:roots){std::error_code ec;auto c=r/rel;if(fs::exists(c,ec))return c;}
    return {};
}
App::App(const SceneDraft&draft,const AppOptions&options):opt_(options),editDraft_(draft){
    bindSession(std::make_unique<Session>(draft));
    form_=std::make_unique<Form>(session_->draft());refreshResolved();
}
void App::bindSession(std::unique_ptr<Session> s){
    session_=std::move(s);replay_.reset();trajVersion_=~0ull;trajOwner_=nullptr;mapAnalysis_.reset();hoverCell_.reset();
}
void App::refreshResolved(){
    auto v=session_->view();if(!v.config)return;auto&c=*v.config;
    form_->setResolved("barrier.center_x",c.barrier.center.x);form_->setResolved("barrier.center_z",c.barrier.center.z);
    form_->setResolved("statistics.collector_x",c.collectorX);form_->setResolved("history.interval",c.history.interval);
}
void App::toast(const std::string&t,Color c){if(opt_.tour)return;toasts_.push_back({t,c,now_+3.2});if(toasts_.size()>4)toasts_.erase(toasts_.begin());}
void App::confirm(const std::string&title,const std::string&body,const std::string&ok,std::function<void()> onOk){modal_={true,title,body,ok,std::move(onOk)};}
void App::setTab(Tab t){if(log_.is_open())log_<<"tab="<<int(t)<<std::endl;tab_=t;ui_.blur();if(t==Tab::analysis&&!replay_.hasAnalysis()&&!demo_){}}
void App::setDemo(bool on){demo_=on;ui_.blur();gfx().setScale(baseScale_*zoom_*(on?1.25f:1.f));}
// ---------------------------------------------------------------- actions
bool App::applyEdits(){
    if(session_->readOnly()){toast(u8"结果为只读，请先返回编辑场景",col::warn);return false;}
    if(running()){toast(u8"运行中无法修改配置",col::warn);return false;}
    form_->validate();
    if(!form_->valid()){toast(u8"配置有误，请先修正标红的项目",col::danger);return false;}
    if(form_->pendingTotal()==0)return true;
    try{
        auto d=form_->draft();bool geometry=form_->pendingCount("terrain")>0;
        session_->apply(d);editDraft_=session_->draft();form_->load(session_->draft());refreshResolved();replay_.reset();
        if(geometry)reframe_=true;
        toast(u8"配置已应用");return true;
    }catch(const std::exception&e){
        auto h=explainError(e.what());form_->setApplyError(h.message+(h.keys.empty()?std::string(" (")+e.what()+")":std::string()));
        for(auto&k:h.keys)if(findDef(k))form_->setError(k,h.message);
        toast(u8"应用失败："+h.message,col::danger);return false;
    }
}
void App::startSingle(){
    if(!applyEdits())return;
    try{session_->startSingle();replay_.reset();setTab(Tab::run);toast(u8"单次试验已开始",col::info);}catch(const std::exception&e){toast(friendly(e.what()),col::danger);}
}
void App::startBatch(){
    if(!applyEdits())return;
    try{session_->startBatch(previewMode_==0?2:0);replay_.reset();setTab(Tab::run);toast(u8"批量计算已开始",col::info);}catch(const std::exception&e){toast(friendly(e.what()),col::danger);}
}
void App::pauseResume(){if(!running())return;session_->pause(!paused());}
void App::cancelRun(){if(running()){session_->cancel();toast(u8"正在取消…",col::warn);}}
void App::newSpecimen(){
    if(running()||session_->readOnly())return;
    if(form_->pendingTotal()>0&&!applyEdits())return;
    try{session_->randomize();editDraft_=session_->draft();form_->rebase(session_->draft());refreshResolved();replay_.reset();}catch(const std::exception&e){toast(friendly(e.what()),col::danger);}
}
void App::openScene(){
    if(running()){toast(u8"请先取消或等待当前运行结束",col::warn);return;}
    auto p=pickSceneFile(false,fs::path("scene3d.ini"),nativeWindowHandle());if(p.empty())return;
    try{auto d=loadScene(p);bindSession(std::make_unique<Session>(d));editDraft_=d;form_=std::make_unique<Form>(session_->draft());refreshResolved();reframe_=true;setTab(Tab::scene);toast(u8"已载入场景："+p.filename().u8string());}
    catch(const std::exception&e){toast(u8"无法载入场景："+friendly(e.what()),col::danger);}
}
void App::saveSceneFile(){
    form_->validate();if(!form_->valid()){toast(u8"配置有误，无法保存",col::danger);return;}
    auto p=pickSceneFile(true,fs::path("scene3d.ini"),nativeWindowHandle());if(p.empty())return;
    try{saveScene(p,form_->draft());toast(u8"场景已保存："+p.filename().u8string());}catch(const std::exception&e){toast(u8"保存失败："+std::string(e.what()),col::danger);}
}
void App::openResults(fs::path dir){
    auto go=[this,dir]{
        try{
            auto archive=openResultArchive(dir,opt_.diagnostic);bindSession(std::make_unique<Session>(archive));
            reframe_=true;setTab(Tab::analysis);
            toast(u8"已打开结果："+dir.filename().u8string()+(archive->notice.empty()?"":u8"（有提示）"),archive->notice.empty()?col::ok:col::warn);
        }catch(const std::exception&e){toast(u8"无法打开结果："+friendly(e.what()),col::danger);}
    };
    if(running())confirm(u8"打开历史结果",u8"当前计算仍在进行。打开其他结果将取消它（已写入磁盘的数据保留）。",u8"取消计算并打开",[this,go]{session_->cancel();go();});
    else go();
}
void App::pickResults(){
    auto start=fs::exists(form_->outputRoot)?form_->outputRoot:fs::current_path();
    auto p=pickFolder(start,nativeWindowHandle(),L"选择一个结果文件夹（包含 manifest.ini）");if(!p.empty())openResults(p);
}
void App::backToEditing(){
    if(running())return;
    bindSession(std::make_unique<Session>(editDraft_));form_=std::make_unique<Form>(session_->draft());refreshResolved();reframe_=true;setTab(Tab::scene);toast(u8"已返回场景编辑");
}
// ---------------------------------------------------------------- loop
int App::run(){
    unsigned flags=FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT;if(!std::getenv("RS3D_NOVSYNC"))flags|=FLAG_VSYNC_HINT; // RS3D_NOVSYNC: diagnostic switch to measure frame-time headroom
    SetConfigFlags(flags);
    int w=opt_.width>0?opt_.width:1440,h=opt_.height>0?opt_.height:880;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(w,h,u8"滚石模拟 · RollingStone 3D");SetExitKey(KEY_NULL);initDialogs();
    baseScale_=opt_.scale>0?opt_.scale:std::max(1.f,GetWindowScaleDPI().x);
    if(opt_.width<=0){int mw=GetMonitorWidth(GetCurrentMonitor()),mh=GetMonitorHeight(GetCurrentMonitor());int tw=int(1440*baseScale_),th=int(880*baseScale_);if(mw>0&&mh>0){tw=std::min(tw,int(mw*.94));th=std::min(th,int(mh*.9));}SetWindowSize(tw,th);}
    {
        int mw=GetMonitorWidth(GetCurrentMonitor()),mh=GetMonitorHeight(GetCurrentMonitor());Vector2 mp=GetMonitorPosition(GetCurrentMonitor());
        int ww=GetScreenWidth(),wh=GetScreenHeight(); // the grown window must not hang off the screen, or the top-right gizmo is unreachable
        if(mw>0&&mh>0)SetWindowPosition(int(mp.x)+std::max(0,(mw-ww)/2),int(mp.y)+std::max(0,(mh-wh)/2-12));
    }
    SetWindowMinSize(int(940*baseScale_),int(580*baseScale_));
    auto reg=findAsset("assets/fonts/NotoSansSC-Regular.ttf"),bold=findAsset("assets/fonts/NotoSansSC-Bold.ttf");
    if(reg.empty()||!gfx().init(reg,bold.empty()?reg:bold))TraceLog(LOG_ERROR,"font assets/fonts/NotoSansSC-Regular.ttf not found; Chinese text will not render");
    gfx().setScale(baseScale_);
    viz::setTerrainTexture(opt_.terrainTex);
    if(!opt_.inputLog.empty())log_.open(opt_.inputLog,std::ios::trunc);
    if(log_.is_open())ui_.trace=[this](std::uint32_t id,R r,int k){if(k==0&&!dumpCtl_)return;log_<<(k==0?"ctl ":k==1?"press ":"CLICK ")<<id<<" "<<r.x<<","<<r.y<<","<<r.w<<","<<r.h<<std::endl;}; // input probe
    if(opt_.tab=="run")tab_=Tab::run;else if(opt_.tab=="analysis")tab_=Tab::analysis;
    if(!opt_.viewResults.empty())openResults(opt_.viewResults);
    if(opt_.demo)setDemo(true);
    if(!opt_.tour){if(opt_.autoStartSingle)startSingle();else if(opt_.autoStartBatch)startBatch();}
    auto started=std::chrono::steady_clock::now();
    while(!WindowShouldClose()&&!quit_){
        float dt=std::min(GetFrameTime(),.1f);now_=GetTime();
        try
        {
            frame(dt);failedFrames_=0;
        }
        catch(const std::exception&e) // never leave the program because one frame failed
        {
            EndScissorMode();EndDrawing();toast(u8"界面出错（已忽略）："+std::string(e.what()),col::danger);if(log_.is_open())log_<<"frame error: "<<e.what()<<std::endl;
            if(++failedFrames_>600)throw;
        }
        if(opt_.autoExitSeconds>0&&std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()>opt_.autoExitSeconds)quit_=true;
    }
    if(running())session_->cancel();
    vp_.release();LineBuffer::releaseShared();viz::release();gfx().shutdown();CloseWindow();return 0;
}
void App::layout(){
    float s=gfx().scale();W_=GetScreenWidth()/s;H_=GetScreenHeight()/s;
    float topH=50,leftW=std::clamp(W_*.27f,330.f,420.f);
    float dockH=tab_==Tab::analysis?124.f:tab_==Tab::run?84.f:34.f;
    if(demo_){topR_={0,0,0,0};leftR_={0,0,0,0};dockR_={0,0,0,0};viewR_={0,0,W_,H_};return;}
    topR_={0,0,W_,topH};leftR_={0,topH,leftW,H_-topH};viewR_={leftW,topH,W_-leftW,H_-topH-dockH};dockR_={leftW,H_-dockH,W_-leftW,dockH};
}
void App::frame(float dt){
    BeginDrawing();ClearBackground(col::bgTop);
    float want=baseScale_*zoom_*(demo_?1.25f:1.f);if(std::abs(gfx().scale()-want)>1e-4f)gfx().setScale(want);
    sv_=session_->view();replay_.sync(sv_,dt);layout();
    if(!ui_.typing()&&!modal_.open&&!help_)shortcuts();
    ui_.begin(W_,H_);occluders_.clear();
    if(log_.is_open()){Vector2 mp=GetMousePosition();int b=(ui_.in.pressed?1:0)|(ui_.in.down?2:0)|(ui_.in.released?4:0)|(ui_.in.focused?8:0);
        if(mp.x!=lastMouse_.x||mp.y!=lastMouse_.y||b!=lastBtn_){log_<<"mouse raw="<<mp.x<<","<<mp.y<<" ui="<<ui_.in.mx<<","<<ui_.in.my<<" btn="<<b<<" scale="<<gfx().scale()<<" logical="<<W_<<"x"<<H_<<" screen="<<GetScreenWidth()<<"x"<<GetScreenHeight()<<" render="<<GetRenderWidth()<<"x"<<GetRenderHeight()<<std::endl;lastMouse_=mp;lastBtn_=b;}}
    if(log_.is_open()){static int ls=-1,lp=-1;static bool lr=false;if(int(sv_.state)!=ls||int(sv_.phase)!=lp||bool(sv_.result)!=lr){log_<<"session state="<<sessionStateName(sv_.state)<<" phase="<<int(sv_.phase)<<" processed="<<sv_.processed<<" completed="<<sv_.completed<<" result="<<(sv_.result?1:0)<<" error="<<sv_.error<<std::endl;ls=int(sv_.state);lp=int(sv_.phase);lr=bool(sv_.result);}} // session probe
    if(log_.is_open()){static int nf=0;dumpCtl_=(++dumpFrame_==150);static double t0=0;nf++;if(now_-t0>=1.0){log_<<"fps="<<nf/(now_-t0)<<std::endl;nf=0;t0=now_;}} // frame rate probe
    ui_.setBlocked(modal_.open||help_);
    syncViewportContent();
    if(demo_)drawDemo();else drawChrome();
    ui_.setBlocked(false);
    if(help_)drawHelp();
    if(modal_.open)drawModal();
    drawToasts();
    ui_.end();
    gfx().flushFonts();
    tourTick();
    if(shotPending_)flushShot();
    EndDrawing();
}
void App::shortcuts(){
    bool ctrl=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    if(IsKeyPressed(KEY_F1))help_=true;
    if(IsKeyPressed(KEY_F5))setDemo(!demo_);
    if(IsKeyPressed(KEY_F12))captureShot("screenshot");
    if(IsKeyPressed(KEY_ESCAPE)){if(demo_)setDemo(false);}
    if(ctrl&&(IsKeyPressed(KEY_EQUAL)||IsKeyPressed(KEY_KP_ADD))){zoom_=std::min(1.6f,zoom_+.1f);}
    if(ctrl&&(IsKeyPressed(KEY_MINUS)||IsKeyPressed(KEY_KP_SUBTRACT))){zoom_=std::max(.8f,zoom_-.1f);}
    if(ctrl&&IsKeyPressed(KEY_ZERO))zoom_=1;
    if(ctrl&&IsKeyPressed(KEY_ENTER)&&tab_!=Tab::analysis)applyEdits();
    if(ctrl&&IsKeyPressed(KEY_S))saveSceneFile();
    if(ctrl&&IsKeyPressed(KEY_O))pickResults();
    if(IsKeyPressed(KEY_TAB)&&!demo_){int t=(int(tab_)+(IsKeyDown(KEY_LEFT_SHIFT)?2:1))%3;setTab(Tab(t));}
    if(IsKeyPressed(KEY_ONE)){vp_.setPreset(ViewPreset::perspective);}
    if(IsKeyPressed(KEY_TWO)){vp_.setPreset(ViewPreset::top);}
    if(IsKeyPressed(KEY_THREE)){vp_.setPreset(ViewPreset::side);}
    if(IsKeyPressed(KEY_FOUR)){vp_.setPreset(ViewPreset::follow);}
    if(IsKeyPressed(KEY_R)&&tab_==Tab::scene&&!ctrl)newSpecimen();
    bool ana=tab_==Tab::analysis||demo_;
    if(ana&&replay_.hasAnalysis()){
        if(IsKeyPressed(KEY_SPACE))replay_.setPlaying(!replay_.playing());
        if(IsKeyPressed(KEY_E))replay_.jumpToFirstContact();
        if(IsKeyPressed(KEY_RIGHT))replay_.stepFrame(1);
        if(IsKeyPressed(KEY_LEFT))replay_.stepFrame(-1);
        if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_PAGE_UP))replay_.select(std::max<std::uint64_t>(1,replay_.selected()-1));
        if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_PAGE_DOWN))replay_.select(replay_.selected()+1);
    }
    if(tab_==Tab::run&&IsKeyPressed(KEY_SPACE)&&running())pauseResume();
}
// ---------------------------------------------------------------- viewport content
Snapshot App::shownSnapshot(std::shared_ptr<const PhysicsScene>&scene)const{
    if(tab_!=Tab::scene||demo_){if(auto p=replay_.displayPose()){scene=p->first;return p->second;}}
    scene=sv_.scene;return sv_.snapshot;
}
void App::syncViewportContent(){
    std::shared_ptr<const PhysicsScene> scene;shownSnapshot(scene);
    if(scene){vp_.setScene(scene);if(reframe_){vp_.frameScene();reframe_=false;}}
    bool want=tab_!=Tab::scene||demo_;
    if(want){
        auto key=reinterpret_cast<const void*>(replay_.analysis().get());std::uint64_t ver=replay_.trajectoryVersion();
        if(ver!=trajVersion_||key!=trajOwner_){vp_.setTrajectories(replay_.trajectories());trajVersion_=ver;trajOwner_=key;}
    }else if(trajOwner_){vp_.setTrajectories({});trajVersion_=~0ull;trajOwner_=nullptr;}
    // heat overlay
    bool heat=(tab_==Tab::analysis||demo_)&&showHeat_&&replay_.analysis()&&sv_.config;
    if(heat){
        auto a=replay_.analysis();auto m=buildMap(*sv_.config,*a,mapKind_,metric_,!replay_.error().empty());
        bool drapes=(m.kind==MapKind::route||m.kind==MapKind::toeXY)&&m.usable&&m.hits&&sv_.config->terrain;
        if(drapes&&(!running()||now_-lastHeatAt_>.5)){
            HeatSpec h;h.key=sfmt("%d/%d/%llu/%p",int(m.kind),int(m.metric),(unsigned long long)a->version,(const void*)a.get());h.terrain=sv_.config->terrain.get();h.grid=m.grid;h.hits=m.hits;h.denominator=m.denominator;h.valid=true;
            mapAnalysis_=a;vp_.setHeat(h);lastHeatAt_=now_;
        }else if(!drapes)vp_.clearHeat();
    }else vp_.clearHeat();
}
// ---------------------------------------------------------------- screenshots
void App::captureShot(const std::string&name){shotName_=name;shotPending_=true;}
void App::flushShot(){
    shotPending_=false;rlDrawRenderBatchActive();
    int sw=GetRenderWidth(),sh=GetRenderHeight();unsigned char*px=static_cast<unsigned char*>(rlReadScreenPixels(sw,sh));if(!px)return;Image img{px,sw,sh,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};int size=0;unsigned char*png=ExportImageToMemory(img,".png",&size);
    fs::path dir=opt_.shotsDir.empty()?fs::path("screenshots"):opt_.shotsDir;std::error_code ec;fs::create_directories(dir,ec);
    std::string stem=shotName_;if(opt_.shotsDir.empty()){char b[32];std::snprintf(b,sizeof(b),"_%lld",(long long)(now_*1000));stem+=b;}
    auto path=dir/fs::u8path(stem+".png");
    if(png){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(png),size);RL_FREE(png);toast(u8"截图已保存："+path.filename().u8string(),col::info);}
    RL_FREE(px);
}
// ---------------------------------------------------------------- tour (automated walkthrough used for verification screenshots)
void App::tourTick(){
    if(!opt_.tour)return;static fs::path dummy;
    if(tourWait_>0){--tourWait_;return;}
    auto wait=[&](int n){tourWait_=n;++tourStep_;};
    auto shot=[&](const char*n,int after=6){captureShot(n);wait(after);};
    switch(tourStep_){
    case 0:setTab(Tab::scene);wait(50);break;
    case 1:shot("01_scene");break;
    case 2:open_["barrier"]=true;sceneScroll_.y=0;wait(4);break;
    case 3:setTab(Tab::run);wait(20);break;
    case 4:shot("02_run_idle");break;
    case 5:startBatch();wait(30);break;
    case 6:if(sv_.completed>=std::max<std::uint64_t>(3,editDraft_.count/3)||!running())++tourStep_;break;
    case 7:shot("03_run_progress");break;
    case 8:if(!running())wait(40);break;
    case 9:if(sv_.result)tourDir_=sv_.result->directory;shot("04_run_done");break;
    case 10:setTab(Tab::analysis);mapKind_=MapKind::route;replay_.select(1);wait(60);break;
    case 11:shot("05_analysis_route");break;
    case 12:mapKind_=MapKind::contact;wait(20);break;
    case 13:shot("06_analysis_contact");break;
    case 14:mapKind_=MapKind::toeXY;wait(20);break;
    case 15:shot("07_analysis_toe");break;
    case 16:{mapKind_=MapKind::contact;metric_=Metric::frequency;showHeat_=false;std::uint64_t pick=1;if(auto an=replay_.analysis())for(auto&e:an->events)if(e&&e->status==FirstContactStatus::hit){pick=e->trialId;break;}replay_.select(pick);wait(40);break;}
    case 17:replay_.jumpToFirstContact();wait(40);break;
    case 18:shot("08_contact_moment");break;
    case 19:replay_.setFollow(false);replay_.seek(replay_.trial()?replay_.trial()->lastTime*.45:1.0);wait(40);break;
    case 20:shot("09_replay_mid");break;
    case 21:metric_=Metric::conditional;showHeat_=true;mapKind_=MapKind::contact;wait(20);break;
    case 22:shot("10_contact_conditional");break;
    case 23:mapKind_=MapKind::toeXZ;metric_=Metric::frequency;wait(20);break;
    case 24:shot("11_toe_xz");break;
    case 25:mapKind_=MapKind::route;setDemo(true);wait(30);break;
    case 26:shot("12_demo");break;
    case 27:setDemo(false);help_=true;wait(10);break;
    case 28:shot("13_help");break;
    case 29:help_=false;setTab(Tab::scene);open_["terrain"]=true;sceneScroll_.y=0;form_->value("terrain.height").a="62";formDirty_=true;wait(15);break;
    case 30:shot("14_scene_pending");break;
    case 31:form_->value("terrain.steep_degrees").a="-3";formDirty_=true;wait(15);break;
    case 32:shot("15_scene_error");break;
    case 33:form_->revert();formDirty_=true;open_["terrain"]=false;open_["barrier"]=true;wait(15);break;
    case 34:shot("16_scene_barrier");break;
    case 35:vp_.setPreset(ViewPreset::top);wait(25);break;
    case 36:shot("17_scene_top");break;
    case 37:setTab(Tab::analysis);if(!tourDir_.empty())openResults(tourDir_);mapKind_=MapKind::contact;showHeat_=true;metric_=Metric::frequency;wait(120);break;
    case 38:shot("18_open_results");break;
    case 39:{std::uint64_t pick=1;if(auto an=replay_.analysis())for(auto&e:an->events)if(e&&e->status==FirstContactStatus::hit){pick=e->trialId;break;}replay_.select(pick);wait(50);break;}
    case 40:replay_.jumpToFirstContact();wait(50);break;
    case 41:shot("19_open_results_replay");break;
    case 42:setTab(Tab::scene);vp_.setPreset(ViewPreset::perspective);wait(70);break;
    case 43:shot("20_gizmo_perspective");break;
    case 44:vp_.setPreset(ViewPreset::top);wait(70);break;
    case 45:shot("21_gizmo_top");break;
    case 46:vp_.setPreset(ViewPreset::side);wait(70);break;
    case 47:shot("22_gizmo_side");break;
    case 48:vp_.setPreset(ViewPreset::perspective);wait(70);break;
    case 49:vp_.alignView(0);wait(70);break;
    case 50:shot("23_gizmo_plus_x");break;
    case 51:vp_.alignView(2);wait(70);break;
    case 52:shot("24_gizmo_plus_y");break;
    case 53:setDemo(true);vp_.setPreset(ViewPreset::perspective);wait(70);break;
    case 54:shot("25_gizmo_demo");break;
    case 55:setDemo(false);setTab(Tab::scene);vp_.setPreset(ViewPreset::follow);wait(90);break;
    case 56:shot("26_follow");break;
    default:quit_=true;break;
    }
}
int runApplication(const SceneDraft&draft,const AppOptions&options){App app(draft,options);return app.run();}
}
