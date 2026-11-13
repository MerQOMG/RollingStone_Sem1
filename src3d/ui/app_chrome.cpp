// Frame chrome: viewport overlays, top bar, dock, demo mode, modal, help and toasts.
#include "ui/app_impl.h"
#include <algorithm>
namespace rs3d::gui {
namespace fs=std::filesystem;
static Color stateColor(const SessionView&v,bool readOnly){
    if(v.state==SessionState::running)return col::accent;if(v.state==SessionState::paused)return col::amber;if(v.state==SessionState::cancelling)return col::warn;
    if(v.state==SessionState::failed)return col::danger;if(v.state==SessionState::completed)return col::ok;(void)readOnly;return col::faint;
}
void App::drawViewport(){
    R v=viewR_;auto&g=gfx();
    R gzR=Viewport3D::gizmoRect(v,demo_);occluders_.push_back(gzR); // the gizmo owns the pointer in its corner
    bool over=ui_.anyPopup()||modal_.open||help_;for(auto&o:occluders_)if(o.contains(ui_.in.mx,ui_.in.my))over=true;
    std::shared_ptr<const PhysicsScene> scene;Snapshot snap=shownSnapshot(scene);
    Vector3 follow{};const Vector3*fp=nullptr;if(vp_.preset==ViewPreset::follow){follow=toV3(snap.shapeOrigin);fp=&follow;}
    vp_.updateCamera(ui_.in,v,over,ui_.typing(),modal_.open||help_,GetFrameTime(),fp);
    DrawOptions opt;opt.showRock=showRock_;opt.showTrails=showTrails_&&(tab_!=Tab::scene||demo_);opt.showHeat=showHeat_&&(tab_==Tab::analysis||demo_);opt.trailAlpha=float(trailAlpha_);
    opt.showStatBox=showBand_;if(sv_.config){const auto&gr=sv_.config->grid;opt.statBox.valid=true;opt.statBox.lower=Vec3{gr.lower[0],gr.lower[1],gr.lower[2]};opt.statBox.upper=Vec3{gr.upper[0],gr.upper[1],gr.upper[2]};opt.statBox.collectorX=sv_.config->collectorX;} // frozen statistics box, same numbers the statistics use
    if((tab_==Tab::analysis||demo_)&&!replay_.following()&&replay_.hasAnalysis()){opt.selectedTrial=replay_.selected();opt.selectedTime=replay_.cursor();}
    if(replay_.eventShown()&&replay_.event())for(auto&p:replay_.event()->points)opt.contactPoints.push_back(p.world);
    vp_.render(v,snap,opt);
    vp_.gizmo(gzR,ui_.in,!ui_.anyPopup()&&!modal_.open&&!help_);
    if(log_.is_open())
    {
        static float ly=1e9f,lp=1e9f;
        bool moved=std::abs(vp_.view.yaw-ly)>.002f||std::abs(vp_.view.pitch-lp)>.002f;
        if(moved){log_<<"view yaw="<<vp_.view.yaw<<" pitch="<<vp_.view.pitch<<std::endl;ly=vp_.view.yaw;lp=vp_.view.pitch;}
        if(moved||dumpCtl_){log_<<"gizmo rect="<<gzR.x<<","<<gzR.y<<","<<gzR.w<<","<<gzR.h;int k=0;for(auto&c:vp_.gizmoCenters())log_<<" c"<<k++<<"="<<c.x<<","<<c.y;log_<<std::endl;}
    }
    if(!vp_.hasScene())g.textCenter(u8"正在准备场景…",v,16,col::dim);
    if(!demo_){drawViewOverlay(v);drawHud(v);drawLegend(v);}
    drawRunPill(v);
}
void App::drawViewOverlay(R v){
    auto&g=gfx();R bar{v.x+12,v.y+12,300,32};R bg=bar;bg.w+=40;occluders_.push_back(bg);
    g.fill(bg,alpha(col::panel,.82f),8);g.stroke(bg,alpha(col::line,.9f),1,8);
    int sel=int(vp_.preset);ui_.pushId("vp");
    if(ui_.segmented({bar.x+3,bar.y+3,256,26},"preset",{u8"透视",u8"俯视",u8"侧视",u8"跟随"},sel,true,{u8"透视（1）",u8"俯视（2）",u8"侧视（3）",u8"跟随石块（4）"}))vp_.setPreset(ViewPreset(sel));
    if(ui_.iconButton({bar.x+262,bar.y+3,26,26},Icon::target,true,u8"复位视角"))reframe_=true;
    ui_.popId();
}
void App::drawHud(R v){
    if(!sv_.scene)return;auto&g=gfx();std::shared_ptr<const PhysicsScene> sc;Snapshot s=shownSnapshot(sc);
    std::vector<std::string> lines;
    lines.push_back(sfmt(u8"时间  %.3f s",s.time));lines.push_back(sfmt(u8"速度  %.2f m/s",norm(s.velocity)));
    lines.push_back(sfmt(u8"位置  x %.1f  y %.1f  z %.1f m",s.center.x,s.center.y,s.center.z));
    if(s.barrier.enabled&&s.barrier.elastic)lines.push_back(sfmt(u8"挡板位移  %.3f m（峰值 %.3f）",s.barrier.displacement,s.barrier.maxDisplacement));
    float h=12+lines.size()*19.f;R box{v.x+12,v.bottom()-h-12,262,h};occluders_.push_back(box);
    g.fill(box,alpha(col::panel,.82f),8);g.stroke(box,alpha(col::line,.9f),1,8);
    for(size_t i=0;i<lines.size();++i)g.text(lines[i],box.x+12,box.y+8+i*19.f,13,i==0?col::text:col::dim);
}
void App::drawLegend(R v){
    auto&g=gfx();struct Item{std::string t;Color c;};std::vector<Item> items{{u8"石块",rgba(0xec9a45)},{u8"挡板",rgba(0x78aefc)}};
    if(sv_.config&&sv_.config->obstacles&&sv_.config->source.obstacles.enabled)items.push_back({u8"障碍",rgba(0x8f9aa6)});
    if(showBand_&&sv_.config)items.push_back({u8"统计带",rgba(0x6fe3b5)});
    if(tab_!=Tab::scene&&replay_.hasAnalysis()&&showTrails_){items.push_back({u8"轨迹：静止",stopColor(StopReason::resting)});items.push_back({u8"越过坡脚",stopColor(StopReason::downstream)});items.push_back({u8"侧向出界",stopColor(StopReason::lateral)});items.push_back({u8"超时",stopColor(StopReason::timeout)});}
    float w=0;for(auto&i:items)w=std::max(w,g.textWidth(i.t,12));w+=44;float h=12+items.size()*20.f;R box{v.right()-w-12,v.y+12+Viewport3D::gizmoRect(v).h+4,w,h};occluders_.push_back(box);
    g.fill(box,alpha(col::panel,.78f),8);g.stroke(box,alpha(col::line,.9f),1,8);
    for(size_t i=0;i<items.size();++i){float y=box.y+8+i*20.f;g.fill({box.x+12,y+4,14,9},items[i].c,3);g.text(items[i].t,box.x+34,y,12,col::dim);}
}
void App::drawRunPill(R v){
    if(!running()||tab_==Tab::run&&!demo_)return;auto&g=gfx();
    std::string t=sessionStateZh(sv_);if(sv_.config&&sv_.config->source.count>1)t+=sfmt(u8"  %llu/%llu",(unsigned long long)sv_.processed,(unsigned long long)sv_.config->source.count);
    float w=g.textWidth(t,13,true)+190;R box{v.cx()-w/2,v.y+12,w,36};occluders_.push_back(box);
    g.fill(box,alpha(col::panel,.9f),18);g.stroke(box,alpha(col::accent,.8f),1,18);g.circle(box.x+18,box.cy(),4,stateColor(sv_,false));g.text(t,box.x+30,box.y+9,13,col::text,true);
    ui_.pushId("pill");
    if(ui_.iconButton({box.right()-86,box.y+4,38,28},paused()?Icon::play:Icon::pause,true,paused()?u8"继续计算":u8"暂停计算"))pauseResume();
    if(ui_.iconButton({box.right()-44,box.y+4,38,28},Icon::stop,sv_.state!=SessionState::cancelling,u8"取消计算"))cancelRun();
    ui_.popId();
}
void App::drawChrome(){
    drawViewport();drawLeft();drawDock();drawTopBar();
}
void App::drawTopBar(){
    auto&g=gfx();R t=topR_;g.fill(t,col::panel);g.fill({t.x,t.bottom()-1,t.w,1},col::line);
    g.text(u8"滚石模拟",18,8,18,col::text,true);g.text("ROLLING STONE 3D",19,30,9,col::faint);
    float x=168;struct T{Tab id;std::string label;std::string badge;};
    std::string anaBadge=replay_.hasAnalysis()?formatCount(sv_.analysis?sv_.analysis->normalTrials:0):"";
    std::vector<T> tabs{{Tab::scene,u8"场景",""},{Tab::run,u8"运行",running()?u8"●":""},{Tab::analysis,u8"分析",anaBadge}};
    ui_.pushId("tabs");
    for(auto&tb:tabs){
        float w=g.textWidth(tb.label,15,true)+(tb.badge.empty()?0:g.textWidth(tb.badge,11)+14)+36;R r{x,t.y,w,t.h};bool on=tab_==tb.id;
        if(log_.is_open()&&!tabLogged_&&tb.id==Tab::analysis){log_<<"tabrects scale="<<g.scale()<<" analysis="<<r.x<<","<<r.y<<","<<r.w<<","<<r.h<<std::endl;tabLogged_=true;}
        bool hv=ui_.hovered(r);if(hv)ui_.setCursor(MOUSE_CURSOR_POINTING_HAND);
        if(on)g.fill(r,alpha(col::accent,.08f));else if(hv)g.fill(r,alpha(col::text,.04f));
        g.text(tb.label,r.x+18,r.y+14,15,on?col::text:col::dim,true);
        if(!tb.badge.empty()){float bx=r.x+18+g.textWidth(tb.label,15,true)+8;g.text(tb.badge,bx,r.y+18,11,tb.id==Tab::run?col::accent:col::faint);}
        if(on)g.fill({r.x+10,r.bottom()-3,r.w-20,3},col::accent,1.5f);
        if(hv&&ui_.in.pressed)setTab(tb.id);
        x+=w+2;
    }
    ui_.popId();
    // right cluster
    float rx=t.right()-14;
    if(ui_.iconButton({rx-34,9,34,32},Icon::help,true,u8"帮助与快捷键（F1）"))help_=true;rx-=42;
    if(ui_.iconButton({rx-96,9,96,32},Icon::present,true,u8"演示模式（F5）：隐藏面板，放大数字，适合投屏",false,u8"演示"))setDemo(true);rx-=104;
    R file{rx-92,9,92,32};if(ui_.iconButton(file,Icon::folder,true,u8"场景与结果文件",false,u8"文件"))drawFileMenu(file);rx-=100;
    std::string st=sessionStateZh(sv_);float cw=g.textWidth(st,12)+30;ui_.chip({rx-cw,14,cw,22},st,stateColor(sv_,session_->readOnly()),alpha(stateColor(sv_,session_->readOnly()),.14f),12,true);
    if(session_->readOnly()){std::string ro=u8"只读结果";float w2=g.textWidth(ro,12)+20;ui_.chip({rx-cw-w2-8,14,w2,22},ro,col::info,alpha(col::info,.14f),12);}
    int pick=-1;if(ui_.menuPicked("filemenu",pick)&&pick>=0&&pick<int(menuActions_.size()))menuActions_[pick]();
}
void App::drawFileMenu(R anchor){
    menuActions_.clear();std::vector<std::string> items;
    auto add=[&](std::string label,std::function<void()> fn){items.push_back(std::move(label));menuActions_.push_back(std::move(fn));};
    add(u8"打开场景文件…",[this]{openScene();});add(u8"保存场景文件…（Ctrl+S）",[this]{saveSceneFile();});
    add(u8"打开结果文件夹…（Ctrl+O）",[this]{pickResults();});
    if(session_->readOnly())add(u8"返回场景编辑",[this]{backToEditing();});
    add(u8"在资源管理器中打开输出目录",[this]{auto p=form_->outputRoot;std::error_code ec;fs::create_directories(p,ec);openInExplorer(p);});
    auto recent=recentResults(form_->outputRoot,6);
    for(auto&r:recent)add(u8"最近结果：  "+r.filename().u8string(),[this,r]{openResults(r);});
    ui_.openMenu(anchor,"filemenu",items);
}
// ---------------------------------------------------------------- dock
void App::drawDock(){
    auto&g=gfx();g.fill(dockR_,col::panel);g.fill({dockR_.x,dockR_.y,dockR_.w,1},col::line);occluders_.push_back(dockR_);
    if(tab_==Tab::analysis)drawTimeline(dockR_);else if(tab_==Tab::run)drawRunStrip(dockR_);else drawSceneStrip(dockR_);
}
void App::drawSceneStrip(R d){
    auto&g=gfx();std::string s;
    if(sv_.config){auto&c=sv_.config->source;s=std::string(c.terrain.type==TerrainType::flat?u8"平地":u8"参数化山坡");s+=sfmt(u8"  ·  N=%llu  ·  挡板：%s  ·  障碍：%s",(unsigned long long)c.count,sv_.config->barrier.enabled?(sv_.config->barrier.elastic?u8"弹性":u8"刚性"):u8"无",c.obstacles.enabled?sfmt(u8"%d 块",c.obstacles.count).c_str():u8"无");}
    g.textVCenter(s,{d.x+16,d.y,d.w*.6f,d.h},12,col::dim);
    g.textRight(u8"左键旋转 · 右键平移 · 滚轮缩放 · 1–4 切换视角",d.right()-16,d.y+9,12,col::faint);
}
void App::drawRunStrip(R d){
    auto&g=gfx();bool run=running();bool busyOrDone=run||sv_.result||sv_.state==SessionState::completed;
    ui_.pushId("runstrip");
    R inner=d.inset(16,12,16,10);
    if(!busyOrDone){g.textVCenter(u8"尚未运行。在左侧设置批次后点击“开始批量”，或先运行单次试验观察石块。",inner,13,col::dim);ui_.popId();return;}
    double total=sv_.config?double(sv_.config->source.count):1;double frac=total>0?std::clamp(double(sv_.processed)/total,0.,1.):0;
    if(sv_.state==SessionState::completed||sv_.state==SessionState::failed)frac=sv_.result?1.0:frac;
    std::string head=sessionStateZh(sv_);
    g.text(head,inner.x,inner.y,15,col::text,true);
    std::string stats=sfmt(u8"已处理 %llu / %llu    成功 %llu    失败 %llu    用时 %.1f s",(unsigned long long)sv_.processed,(unsigned long long)total,(unsigned long long)sv_.completed,(unsigned long long)sv_.failures,sv_.wallSeconds);
    g.textRight(stats,inner.right(),inner.y+2,13,col::dim);
    ui_.progress({inner.x,inner.y+28,inner.w,10},frac,sv_.state==SessionState::failed?col::danger:col::accent);
    if(run&&sv_.processed>2&&sv_.wallSeconds>0.5){double eta=sv_.wallSeconds*(total-double(sv_.processed))/double(sv_.processed);g.text(sfmt(u8"预计剩余 %.0f s",eta),inner.x,inner.y+44,12,col::faint);}
    else if(!sv_.error.empty())g.text(g.fit(sv_.error,inner.w,12),inner.x,inner.y+44,12,col::warn);
    ui_.popId();
}
// ---------------------------------------------------------------- demo mode
void App::drawDemo(){
    drawViewport();auto&g=gfx();R v=viewR_;
    // title + state
    g.text(u8"滚石模拟",28,22,26,col::text,true);std::string st=sessionStateZh(sv_);g.text(st,30,58,15,stateColor(sv_,false),true);
    // KPI stack
    struct K{std::string label,value;};std::vector<K> ks;std::shared_ptr<const PhysicsScene> sc;Snapshot s=shownSnapshot(sc);
    auto an=replay_.analysis();
    if(running()||an){ks.push_back({u8"试验",sfmt(u8"#%llu / %llu",(unsigned long long)(replay_.hasAnalysis()&&!replay_.following()?replay_.selected():std::max<std::uint64_t>(1,sv_.trialId)),(unsigned long long)(sv_.config?sv_.config->source.count:1))});}
    ks.push_back({u8"时间",sfmt("%.2f s",s.time)});ks.push_back({u8"速度",sfmt("%.1f m/s",norm(s.velocity))});
    if(sv_.config&&sv_.config->barrier.enabled&&an&&an->contact){auto&c=*an->contact;if(c.known()>0)ks.push_back({u8"首次触板",sfmt(u8"%llu / %llu",(unsigned long long)c.hitTrials(),(unsigned long long)c.known())});}
    float kw=210,kh=62;float y=22;
    for(auto&k:ks){R box{v.right()-kw-24,y,kw,kh};occluders_.push_back(box);g.fill(box,alpha(col::panel,.8f),10);g.stroke(box,alpha(col::line,.9f),1,10);g.text(k.label,box.x+16,box.y+8,12,col::faint);g.text(k.value,box.x+16,box.y+26,24,col::text,true);y+=kh+10;}
    // controls pill
    float pw=620,ph=58;R pill{v.cx()-pw/2,v.bottom()-ph-26,pw,ph};occluders_.push_back(pill);g.fill(pill,alpha(col::panel,.9f),29);g.stroke(pill,alpha(col::line,.9f),1,29);
    ui_.pushId("demo");float x=pill.x+16;
    auto btn=[&](Icon ic,const std::string&label,Variant var,bool en,float w)->bool{R r{x,pill.y+9,w,40};x+=w+8;return ui_.iconButton(r,ic,en,{},false,label,var);};
    bool run=running();
    if(!run){
        if(btn(Icon::play,u8"运行单次",Variant::primary,!session_->readOnly(),122))startSingle();
        if(btn(Icon::chart,u8"批量计算",Variant::secondary,!session_->readOnly(),122))startBatch();
    }else{
        if(btn(paused()?Icon::play:Icon::pause,paused()?u8"继续":u8"暂停",Variant::secondary,true,100))pauseResume();
        if(btn(Icon::stop,u8"取消",Variant::danger,sv_.state!=SessionState::cancelling,100))cancelRun();
    }
    if(replay_.hasAnalysis()){
        if(btn(replay_.playing()?Icon::pause:Icon::play,replay_.playing()?u8"暂停回放":u8"回放",Variant::secondary,true,112))replay_.setPlaying(!replay_.playing());
        if(btn(Icon::target,u8"首次触板",Variant::secondary,replay_.event()!=nullptr,116))replay_.jumpToFirstContact();
    }
    ui_.popId();
    R exit{v.right()-120,v.bottom()-52,100,32};occluders_.push_back(exit);
    ui_.pushId("demoexit");if(ui_.button(exit,u8"退出 Esc",Variant::ghost))setDemo(false);ui_.popId();
}
// ---------------------------------------------------------------- overlays
void App::drawModal(){
    auto&g=gfx();g.fill({0,0,W_,H_},rgba(0,150));float w=440,h=190;R box{(W_-w)/2,(H_-h)/2,w,h};
    g.fill(box,col::panel2,12);g.stroke(box,col::lineStrong,1,12);g.text(modal_.title,box.x+24,box.y+20,18,col::text,true);
    g.wrapped(modal_.body,{box.x+24,box.y+58,w-48,80},14,col::dim);
    ui_.pushId("modal");
    if(ui_.button({box.right()-24-170,box.bottom()-56,170,36},modal_.ok,Variant::primary)){auto fn=modal_.onOk;modal_.open=false;if(fn)fn();}
    if(ui_.button({box.right()-24-170-92,box.bottom()-56,80,36},u8"返回"))modal_.open=false;
    ui_.popId();
    if(IsKeyPressed(KEY_ESCAPE))modal_.open=false;
}
void App::drawToasts(){
    auto&g=gfx();toasts_.erase(std::remove_if(toasts_.begin(),toasts_.end(),[&](const Toast&t){return t.until<now_;}),toasts_.end());
    float y=H_-(demo_?110:(tab_==Tab::analysis?144:tab_==Tab::run?104:54));
    for(int i=int(toasts_.size())-1;i>=0;--i){auto&t=toasts_[size_t(i)];float w=std::min(W_-40,g.textWidth(t.text,13)+44);R box{viewR_.cx()-w/2,y-34,w,32};
        g.fill(box,rgba(0x1f2a36,240),16);g.stroke(box,alpha(t.color,.7f),1,16);g.circle(box.x+16,box.cy(),4,t.color);g.text(g.fit(t.text,w-44,13),box.x+30,box.y+7,13,col::text);y-=38;}
}
void App::drawHelp(){
    auto&g=gfx();g.fill({0,0,W_,H_},rgba(0,170));float w=std::min(760.f,W_-60),h=std::min(560.f,H_-60);R box{(W_-w)/2,(H_-h)/2,w,h};
    g.fill(box,col::panel2,14);g.stroke(box,col::lineStrong,1,14);g.text(u8"使用指南",box.x+28,box.y+20,20,col::text,true);
    ui_.pushId("help");if(ui_.iconButton({box.right()-52,box.y+14,34,34},Icon::close,true,u8"关闭（Esc）"))help_=false;
    R view=box.inset(28,64,22,20);ui_.beginScroll(view,helpScroll_,"hs");float y=view.y-helpScroll_.y;
    auto sec=[&](const std::string&t){g.text(t,view.x,y,14,col::accent,true);y+=26;};
    auto kv=[&](const std::string&k,const std::string&v){g.text(k,view.x,y,13,col::text,true);y+=g.wrapped(v,{view.x+170,y,view.w-186,400},13,col::dim)+8;};
    sec(u8"三步工作流");kv(u8"① 场景",u8"在左侧调整地形、石块、挡板、障碍等参数。改动先标为“待应用”，点击底部“应用”后才生效；无效值会在字段旁说明原因。");
    kv(u8"② 运行",u8"“运行单次”用预览石块实时观察一次；“开始批量”按批次数 N 和批次种子做蒙特卡洛，可暂停、取消；结果写入输出目录。");
    kv(u8"③ 分析",u8"查看统计与热图（路径频率、首次触板、坡脚分布），逐次试验回放，拖动时间轴、跳到首次触板瞬间。");
    y+=6;sec(u8"鼠标与键盘");
    kv(u8"三维视图",u8"左键拖动旋转，右键或中键拖动平移，滚轮缩放；1 透视 · 2 俯视 · 3 侧视 · 4 跟随石块。右上角的导航轴：点轴球把视角对齐到该轴方向，在圆盘上拖动旋转视角（x 沿坡向下，y 横向，z 向上，与输出文件一致）。");
    kv("Tab / Shift+Tab",u8"切换 场景 / 运行 / 分析");kv("Ctrl+Enter",u8"应用待修改的配置");kv("R",u8"场景页：换一块预览石块");
    kv(u8"空格",u8"分析页：播放/暂停回放；运行页：暂停/继续计算");kv(u8"← →  ↑ ↓",u8"逐帧前进后退；上一个/下一个试验");kv("E",u8"跳到首次触板瞬间");
    kv("Ctrl+S / Ctrl+O",u8"保存场景 / 打开结果文件夹");kv("F5  F1  F12",u8"演示模式 / 本帮助 / 保存当前画面截图");kv(u8"Ctrl + 加减 / 0",u8"放大、缩小、还原界面");
    y+=6;sec(u8"怎样读热图");
    kv(u8"频率",u8"某格被 H 次试验经过，频率 = H / N。批次未完成时只给出下界与上界，不当作概率。");
    kv(u8"条件概率",u8"“触板的试验中”落在该格的比例 H / B，只有当所有试验的触板状态都已确认时才可用。");
    kv(u8"首次触板",u8"石块第一次与挡板接触的位置；弹性挡板会真实地位移和回弹。");
    ui_.endScroll(view,helpScroll_,y+helpScroll_.y-view.y);ui_.popId();
    if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_F1))help_=false;
}
}
