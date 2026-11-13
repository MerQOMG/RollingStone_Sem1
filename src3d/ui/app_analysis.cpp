// Analysis tab: statistics, heat maps, trial list and the replay timeline.
#include "ui/app_impl.h"
#include <algorithm>
#include <cmath>
namespace rs3d::gui {
static const float kPad=16;
static Color levelColor(Level l){return l==Level::ok?col::ok:(l==Level::warn?col::warn:(l==Level::error?col::danger:col::info));}
void App::drawMapPlot(R r){
    auto&g=gfx();const MapData&m=map_;g.fill(r,col::field,8);
    const auto&gx=m.grid.lines(0);const auto&gy=m.grid.lines(1);
    if(gx.size()<2||gy.size()<2||!m.hits){g.textCenter(u8"无数据",r,13,col::faint);return;}
    double x0=gx.front(),x1=gx.back(),y0=gy.front(),y1=gy.back();double wx=std::max(1e-9,x1-x0),wy=std::max(1e-9,y1-y0);
    R in=r.inset(26,8,10,22);double s=std::min(in.w/wx,in.h/wy);float pw=float(wx*s),ph=float(wy*s);R pr{in.x+(in.w-pw)/2,in.y+(in.h-ph)/2,pw,ph};
    g.fill(pr,rgba(0x0a0e13),2);
    auto sx=[&](double x){return pr.x+float((x-x0)/wx*pw);};auto sy=[&](double y){return pr.bottom()-float((y-y0)/wy*ph);};
    int nx=m.grid.size(0),ny=m.grid.size(1);
    if(m.hits->size()==m.grid.count()){
        for(int xi=0;xi<nx;++xi)for(int yi=0;yi<ny;++yi){
            auto h=(*m.hits)[m.grid.id(xi,yi,0)];if(!h)continue;double t=m.denominator>0?double(h)/m.denominator:0;
            Color c=heatRamp(.1+.9*std::clamp(t,0.,1.),m.usable?255:150);
            float ax=sx(gx[size_t(xi)]),bx=sx(gx[size_t(xi)+1]),ay=sy(gy[size_t(yi)+1]),by=sy(gy[size_t(yi)]);g.fill({ax,ay,std::max(1.f,bx-ax),std::max(1.f,by-ay)},c);
        }
    }
    g.stroke(pr,col::lineStrong,1);
    // barrier / terrain extent hints: axes labels
    const char*xl=u8"x (m)";std::string yl=m.kind==MapKind::toeXZ?"z":(m.kind==MapKind::contact?u8"高":"y");if(m.kind==MapKind::contact)xl=u8"宽向 (m)";
    g.text(sfmt("%.0f",x0),pr.x-2,r.bottom()-18,10,col::faint);g.textRight(sfmt("%.0f",x1),pr.right()+2,r.bottom()-18,10,col::faint);g.textCenter(xl,{pr.x,r.bottom()-20,pr.w,16},10,col::faint);
    g.textRight(sfmt("%.0f",y1),pr.x-4,pr.y-2,10,col::faint);g.textRight(sfmt("%.0f",y0),pr.x-4,pr.bottom()-12,10,col::faint);g.text(yl,r.x+6,pr.cy()-7,10,col::faint);
    // hover + selection
    hoverCell_.reset();
    ui_.pushId("plot");
    if(ui_.hovered(pr)&&m.hits->size()==m.grid.count()){
        double wxp=x0+(ui_.in.mx-pr.x)/pw*wx,wyp=y0+(pr.bottom()-ui_.in.my)/ph*wy;auto xi=m.grid.index(wxp,0),yi=m.grid.index(wyp,1);
        if(xi&&yi){hoverCell_=m.grid.id(*xi,*yi,0);ui_.setCursor(MOUSE_CURSOR_POINTING_HAND);int ix=*xi,iy=*yi;
            g.stroke({sx(gx[size_t(ix)]),sy(gy[size_t(iy)+1]),sx(gx[size_t(ix)+1])-sx(gx[size_t(ix)]),sy(gy[size_t(iy)])-sy(gy[size_t(iy)+1])},col::text,1.5f);
            if(ui_.in.pressed){auto cur=replay_.cell();if(cur&&*cur==*hoverCell_)replay_.setCell(std::nullopt,mapKind_);else replay_.setCell(hoverCell_,mapKind_);}}
    }
    if(auto sel=replay_.cell()){if(*sel<m.grid.count()){auto ix=m.grid.indices(*sel);g.stroke({sx(gx[size_t(ix[0])]),sy(gy[size_t(ix[1])+1]),sx(gx[size_t(ix[0])+1])-sx(gx[size_t(ix[0])]),sy(gy[size_t(ix[1])])-sy(gy[size_t(ix[1])+1])},col::amber,2);}}
    ui_.popId();
}
void App::drawStopLegend(Col&c){
    auto&g=gfx();auto a=replay_.analysis();if(!a||!a->directory)return;std::map<std::string,int> counts;for(auto&t:a->directory->trials)++counts[t->status];
    float x=c.x,y=c.y;
    for(auto&kv:counts){
        std::string label=stopReasonZh(kv.first)+" "+std::to_string(kv.second);float w=g.textWidth(label,12)+30;if(x+w>c.x+c.w){x=c.x;y+=24;}
        StopReason sr=StopReason::running;for(StopReason r:{StopReason::resting,StopReason::downstream,StopReason::upstream,StopReason::lateral,StopReason::timeout,StopReason::geometryFailure,StopReason::numericFailure,StopReason::cancelled})if(kv.first==stopReasonName(r))sr=r;
        g.fill({x,y,w,20},alpha(stopColor(sr),.14f),10);g.circle(x+11,y+10,4,stopColor(sr));g.text(label,x+22,y+3,12,col::text);x+=w+6;
    }
    c.y=y+30;
}
void App::drawTrialList(Col&c,float height){
    auto&g=gfx();auto a=replay_.analysis();if(!a||!a->directory)return;
    auto ids=replay_.listed();R box{c.x,c.y,c.w,height};g.fill(box,col::field,8);g.stroke(box,col::line,1,8);
    std::string title=replay_.cell()?sfmt(u8"所选格内的试验（%d）",int(ids.size())):sfmt(u8"全部试验（%d）",int(ids.size()));
    g.text(title,box.x+12,box.y+8,12,col::dim,true);
    ui_.pushId("tl");
    if(replay_.cell()&&ui_.button({box.right()-88,box.y+4,80,24},u8"清除选择",Variant::ghost))replay_.setCell(std::nullopt,mapKind_);
    R view{box.x+2,box.y+34,box.w-4,box.h-38};const float rh=28;trialScroll_.content=ids.size()*rh;
    ui_.beginScroll(view,trialScroll_,"rows");
    int first=std::max(0,int(trialScroll_.y/rh)),last=std::min<int>(int(ids.size()),first+int(view.h/rh)+2);
    for(int i=first;i<last;++i){
        auto id=ids[size_t(i)];R row{view.x,view.y+i*rh-trialScroll_.y,view.w-10,rh};bool sel=id==replay_.selected()&&!replay_.following();
        auto&t=a->directory->trials[size_t(id-1)];bool hv=ui_.hovered(row);
        if(sel)g.fill(row,alpha(col::accent,.18f),4);else if(hv)g.fill(row,alpha(col::text,.05f),4);
        StopReason sr=StopReason::running;for(StopReason r:{StopReason::resting,StopReason::downstream,StopReason::upstream,StopReason::lateral,StopReason::timeout,StopReason::geometryFailure,StopReason::numericFailure,StopReason::cancelled})if(t->status==stopReasonName(r))sr=r;
        g.circle(row.x+14,row.cy(),4,stopColor(sr));g.text(sfmt("#%llu",(unsigned long long)id),row.x+28,row.y+6,12,sel?col::text:col::dim,true);
        g.text(stopReasonZh(t->status),row.x+86,row.y+6,12,col::dim);
        std::string right=t->frames?sfmt("%.1f s",t->lastTime):u8"无历史";g.textRight(right,row.right()-8,row.y+6,11,col::faint);
        if(hv&&ui_.in.pressed&&view.contains(ui_.in.mx,ui_.in.my))replay_.select(id);
    }
    ui_.endScroll(view,trialScroll_,ids.size()*rh);
    ui_.popId();c.y+=height+8;
}
void App::drawAnalysisTab(R body){
    auto&g=gfx();ui_.beginScroll(body,anaScroll_,"ana");
    Col c{body.x+kPad,body.y+14-anaScroll_.y,body.w-2*kPad-8,body.y-anaScroll_.y};
    ui_.pushId("anah");
    R head=c.row(30,8);std::string src=session_->readOnly()&&sv_.archive?u8"历史结果 · "+sv_.archive->directory.filename().u8string():(running()?u8"当前批次（计算中，数据实时更新）":u8"当前批次");
    g.textVCenter(g.fit(src,head.w-100,14,true),head,14,col::text,true);
    if(ui_.button({head.right()-88,head.y,88,30},u8"打开结果…",Variant::secondary,true,u8"打开已保存的结果文件夹（Ctrl+O）"))pickResults();
    ui_.popId();
    if(session_->readOnly()&&!sv_.error.empty()){float h=g.wrapped(sv_.error,{c.x,c.y,c.w,200},11,col::warn);c.space(h+8);}
    auto a=replay_.analysis();
    if(!a||!sv_.config){
        R card=c.row(250,10);g.fill(card,col::panel2,10);g.stroke(card,col::line,1,10);drawIcon(Icon::chart,{card.cx()-24,card.y+22,48,48},col::faint);
        g.textCenter(u8"还没有可分析的数据",{card.x,card.y+80,card.w,24},16,col::text,true);
        g.wrapped(u8"在“运行”页完成一次批量计算，或打开此前保存的结果文件夹（其中包含 manifest.ini）。",{card.x+20,card.y+112,card.w-40,60},13,col::dim);
        ui_.pushId("empty");if(ui_.button({card.x+20,card.bottom()-56,card.w*.5f-26,36},u8"去运行",Variant::primary))setTab(Tab::run);
        if(ui_.button({card.cx()+6,card.bottom()-56,card.w*.5f-26,36},u8"打开结果…",Variant::secondary))pickResults();ui_.popId();
        ui_.endScroll(body,anaScroll_,c.used()+16);return;
    }
    const RunConfig&cfg=*sv_.config;
    // KPI grid
    std::map<std::string,int> counts;if(a->directory)for(auto&t:a->directory->trials)++counts[t->status];
    struct K{std::string label,value;Color color;};std::vector<K> ks;
    ks.push_back({u8"计划试验",std::to_string(cfg.source.count),col::text});ks.push_back({u8"已记录",std::to_string(a->directory?a->directory->trials.size():0),col::text});ks.push_back({u8"正常结束",std::to_string(a->normalTrials),col::ok});
    if(cfg.barrier.enabled&&a->contact){auto&ct=*a->contact;ks.push_back({u8"触板",std::to_string(ct.hitTrials()),col::info});ks.push_back({u8"未触板",std::to_string(ct.missTrials()),col::text});ks.push_back({u8"未确认",std::to_string(ct.unknown()),ct.unknown()?col::warn:col::faint});}
    else{ks.push_back({u8"静止",std::to_string(counts["resting"]),col::text});ks.push_back({u8"越过下游",std::to_string(counts["downstream"]),col::text});ks.push_back({u8"超时/失败",std::to_string(counts["timeout"]+counts["numeric_failure"]+counts["geometry_failure"]),counts["timeout"]?col::warn:col::faint});}
    {float kw=(c.w-16)/3,kh=56;for(size_t i=0;i<ks.size();++i){R k{c.x+(i%3)*(kw+8),c.y+(i/3)*(kh+8),kw,kh};g.fill(k,col::panel2,8);g.text(ks[i].label,k.x+10,k.y+7,11,col::faint);g.text(ks[i].value,k.x+10,k.y+23,22,ks[i].color,true);}c.space(2*56+8+14);}
    // map selection
    g.text(u8"热图",c.x,c.y,15,col::text,true);c.space(26);
    ui_.pushId("map");
    int kindSel=int(mapKind_);if(ui_.segmented(c.row(30,8),"kind",{u8"路径频率",u8"首次触板",u8"坡脚 xy",u8"坡脚 xz"},kindSel,true,{u8"每个格子被多少次试验经过",u8"石块第一次碰到挡板的位置",u8"石块在坡脚统计盒内的水平分布",u8"石块在坡脚统计盒内的竖向分布"})){mapKind_=MapKind(kindSel);hoverCell_.reset();replay_.setCell(std::nullopt,mapKind_);if(mapKind_!=MapKind::contact&&metric_==Metric::conditional)metric_=Metric::frequency;}
    int metSel=int(metric_);std::vector<std::string> mets{u8"次数",u8"频率"};if(mapKind_==MapKind::contact)mets.push_back(u8"条件概率");
    if(ui_.segmented(c.row(28,8),"metric",mets,metSel,true,{u8"命中次数 H",u8"H / N",u8"H / B：仅在已触板试验内的分布"}))metric_=Metric(metSel);
    ui_.popId();
    map_=buildMap(cfg,*a,mapKind_,metric_,!replay_.error().empty());mapAnalysis_=a;
    float aspect=1;{auto&gx=map_.grid.lines(0);auto&gy=map_.grid.lines(1);if(gx.size()>1&&gy.size()>1)aspect=float((gy.back()-gy.front())/std::max(1e-9,gx.back()-gx.front()));}
    float ph=std::clamp(c.w*aspect+30,150.f,260.f);R plot=c.row(ph,6);drawMapPlot(plot);
    // colour bar
    {R bar{plot.x+26,c.y,plot.w-36,8};for(int i=0;i<40;++i)g.fill({bar.x+bar.w*i/40,bar.y,bar.w/40+1,bar.h},heatRamp(.1+.9*i/39.0));c.space(12);g.text(g.fit(map_.legend,c.w,11),c.x,c.y,11,col::faint);c.space(20);}
    // status
    {Color lc=levelColor(map_.level);R st{c.x,c.y,c.w,0};float h=g.wrapped(map_.detail,{c.x+16,c.y+22,c.w-16,200},11,col::dim,false,false)+26;st.h=h;g.fill({c.x,c.y+2,3,h-4},lc,1.5f);g.text(g.fit(map_.status,c.w-14,12),c.x+14,c.y+2,12,lc,true);g.wrapped(map_.detail,{c.x+14,c.y+22,c.w-16,200},11,col::dim);c.space(h+4);}
    if(hoverCell_){std::string rd=cellReadout(map_,*a,*hoverCell_);float h=g.wrapped(rd,{c.x,c.y,c.w,100},12,col::text,false,false);g.wrapped(rd,{c.x,c.y,c.w,100},12,col::text);c.space(h+8);}
    else{g.text(u8"悬停格子查看数值，点击格子筛选试验。",c.x,c.y,11,col::faint);c.space(24);}
    g.fill({c.x,c.y,c.w,1},col::line);c.space(12);
    // display options
    g.text(u8"三维视图显示",c.x,c.y,15,col::text,true);c.space(26);
    ui_.pushId("disp");
    auto toggleRow=[&](const char*label,const char*key,bool&v){R r=c.row(26,4);g.textVCenter(label,r,13,col::dim);ui_.toggle({r.right()-44,r.y+2,44,22},key,v);};
    toggleRow(u8"轨迹线",  "tr",showTrails_);toggleRow(u8"热图贴在地形上","hm",showHeat_);toggleRow(u8"石块","rk",showRock_);toggleRow(u8"坡脚统计带","bd",showBand_);
    {R r=c.row(26,4);g.textVCenter(u8"轨迹不透明度",{r.x,r.y,130,26},13,col::dim);ui_.slider({r.x+130,r.y,r.w-130,26},"ta",trailAlpha_,.1,1.0);}
    ui_.popId();
    c.space(8);drawStopLegend(c);
    g.fill({c.x,c.y,c.w,1},col::line);c.space(12);
    ui_.pushId("trials");
    {R r=c.row(30,8);g.textVCenter(u8"转到试验 #",{r.x,r.y,92,30},13,col::dim);FieldOpt fo;fo.integer=true;fo.unit="";
     if(ui_.field({r.x+92,r.y,r.w-92-60,30},"goto",gotoText_,fo)){}
     if(ui_.button({r.right()-52,r.y,52,30},u8"转到",Variant::secondary)||(IsKeyPressed(KEY_ENTER)&&ui_.typing()&&false)){char*e=nullptr;auto v=std::strtoull(gotoText_.c_str(),&e,10);if(v>0)replay_.select(v);}}
    ui_.popId();
    drawTrialList(c,260);
    ui_.endScroll(body,anaScroll_,c.used()+16);
}
// ---------------------------------------------------------------- timeline
void App::drawTimeline(R d){
    auto&g=gfx();R in=d.inset(16,10,16,8);
    if(!replay_.hasAnalysis()){g.textVCenter(u8"没有可回放的数据。完成一次运行或打开历史结果后，这里会出现时间轴。",in,13,col::dim);return;}
    auto t=replay_.trial();bool frames=t&&t->frames;
    ui_.pushId("tlbar");float x=in.x;const float bh=34;
    auto ib=[&](Icon ic,const std::string&tip,bool en,bool primary=false,float w=38){R r{x,in.y,w,bh};x+=w+4;return ui_.iconButton(r,ic,en,tip,false,{},primary?Variant::primary:Variant::secondary);};
    if(ib(Icon::toStart,u8"回到起点",frames))replay_.seek(t->firstTime);
    if(ib(Icon::stepBack,u8"上一帧（←）",frames))replay_.stepFrame(-1);
    if(ib(replay_.playing()?Icon::pause:Icon::play,replay_.playing()?u8"暂停（空格）":u8"播放（空格）",frames,true,48))replay_.setPlaying(!replay_.playing());
    if(ib(Icon::stepFwd,u8"下一帧（→）",frames))replay_.stepFrame(1);
    if(ib(Icon::toEnd,u8"跳到终点",frames))replay_.seek(t->lastTime);
    x+=10;
    {static const double speeds[]{.25,.5,1,2,4};int sel=2;for(int i=0;i<5;++i)if(std::abs(replay_.speed()-speeds[i])<1e-6)sel=i;
     if(ui_.segmented({x,in.y+3,230,28},"speed",{"0.25×","0.5×","1×","2×","4×"},sel,true))replay_.setSpeed(speeds[sel]);x+=240;}
    {auto e=replay_.event();bool has=e&&e->status==FirstContactStatus::hit;if(ui_.iconButton({x,in.y,126,bh},Icon::target,has,has?u8"跳到石块第一次碰到挡板的瞬间（E）":u8"这次试验没有触板",false,u8"首次触板",Variant::secondary))replay_.jumpToFirstContact();x+=134;
     bool pc=replay_.pauseAtContact();g.textVCenter(u8"触板时停顿",{x,in.y,84,bh},12,col::dim);if(ui_.toggle({x+84,in.y+6,40,22},"pc",pc,true,u8"回放经过首次触板时自动停顿片刻"))replay_.setPauseAtContact(pc);x+=134;}
    if(replay_.followAvailable()){bool f=replay_.following();R r{in.right()-120,in.y,120,bh};if(ui_.iconButton(r,Icon::present,true,u8"跟随正在计算的试验",f,u8"实时跟随",Variant::secondary))replay_.setFollow(!f);}
    ui_.popId();
    // caption
    std::string cap=sfmt(u8"试验 #%llu",(unsigned long long)replay_.selected());
    if(t){cap+=u8"  ·  "+stopReasonZh(t->status);if(frames)cap+=sfmt("  ·  t = %.3f / %.3f s",replay_.cursor(),t->lastTime);else cap+=u8"  ·  此试验没有逐帧历史（仅轨迹线）";}
    if(replay_.sample()&&replay_.sampleReady()&&replay_.sample()->interpolated)cap+=u8"  ·  插值帧";
    if(replay_.following())cap=u8"实时跟随中  ·  "+cap;
    if(!replay_.error().empty())cap=u8"读取出错："+replay_.error();
    if(replay_.barrierHistoryMissing())cap+=u8"  ·  无挡板历史";
    g.text(g.fit(cap,in.w,12),in.x,in.y+bh+8,12,replay_.error().empty()?col::dim:col::danger);
    // scrubber
    R track{in.x,in.y+bh+34,in.w,22};float ty=track.cy();g.fill({track.x,ty-3,track.w,6},col::field,3);
    if(frames&&t->lastTime>t->firstTime){
        double span=t->lastTime-t->firstTime;auto px=[&](double time){return track.x+float((time-t->firstTime)/span*track.w);};
        float cx=px(std::clamp(replay_.cursor(),t->firstTime,t->lastTime));g.fill({track.x,ty-3,cx-track.x,6},alpha(col::accent,.8f),3);
        if(auto e=replay_.event();e&&e->status==FirstContactStatus::hit){float ex=px((e->timeLower+e->timeUpper)/2);g.tri(ex,ty-11,ex+6,ty-5,ex-6,ty-5,col::amber);g.fill({ex-1,ty-5,2,10},alpha(col::amber,.8f));}
        bool inside=track.inset(-4).contains(ui_.in.mx,ui_.in.my)&&!ui_.anyPopup()&&!modal_.open&&!help_;
        bool seek=updateTimelineCapture(timelineCapture_,ui_.in.focused,true,ui_.in.pressed,ui_.in.down,inside);
        if(inside)ui_.setCursor(MOUSE_CURSOR_POINTING_HAND);
        if(seek){double f=std::clamp((ui_.in.mx-track.x)/track.w,0.f,1.f);replay_.seek(t->firstTime+f*span,true);}
        g.circle(cx,ty,8,col::text);g.circle(cx,ty,5,col::accent);
        g.text(sfmt("%.1f s",t->firstTime),track.x,track.bottom()+3,10,col::faint);g.textRight(sfmt("%.1f s",t->lastTime),track.right(),track.bottom()+3,10,col::faint);
    }else g.textVCenter(u8"此试验没有可拖动的时间轴",{track.x,track.y,track.w,track.h},12,col::faint,false,4);
}
}
