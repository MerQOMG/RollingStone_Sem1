// Left inspector: scene editing form, run controls, apply bar.
#include "ui/app_impl.h"
#include <algorithm>
namespace rs3d::gui {
namespace fs=std::filesystem;
static const float kPad=16;
void App::drawLeft(){
    auto&g=gfx();R body=leftR_;g.fill(body,col::panel);g.fill({body.right()-1,body.y,1,body.h},col::line);occluders_.push_back(body);
    if(tab_==Tab::analysis){drawAnalysisTab(body);return;}
    bool ro=session_->readOnly();
    if(formDirty_){form_->validate();formDirty_=false;}
    std::string msg;
    if(!ro){
        if(!form_->applyError().empty())msg=form_->applyError();else if(!form_->formError().empty())msg=form_->formError();else if(form_->errorCount()>0)msg=sfmt(u8"有 %d 处输入需要修正（已在字段下方标出）。",form_->errorCount());
    }
    bool showBar=!ro&&(form_->pendingTotal()>0||!msg.empty());
    float barH=0;if(showBar){barH=64;if(!msg.empty())barH+=g.wrapped(msg,{0,0,body.w-32,500},12,col::danger,false,false)+8;}
    R bar=barH>0?body.cutBottom(barH):R{};
    if(tab_==Tab::scene)drawSceneTab(body);else drawRunTab(body);
    if(showBar){drawApplyBar(bar);if(!msg.empty()){}}
    // message is drawn inside drawApplyBar via member copy
}
void App::drawApplyBar(R bar){
    auto&g=gfx();g.fill(bar,col::panel2);g.fill({bar.x,bar.y,bar.w,1},col::lineStrong);
    std::string msg;if(!form_->applyError().empty())msg=form_->applyError();else if(!form_->formError().empty())msg=form_->formError();else if(form_->errorCount()>0)msg=sfmt(u8"有 %d 处输入需要修正（已在字段下方标出）。",form_->errorCount());
    float y=bar.y+10;if(!msg.empty()){y+=g.wrapped(msg,{bar.x+kPad,y,bar.w-2*kPad,500},12,col::danger)+8;}
    int n=form_->pendingTotal();bool can=form_->valid()&&!running()&&n>0;
    g.text(running()?u8"运行中，配置已锁定":(n>0?sfmt(u8"%d 项修改待应用",n):std::string(u8"所有修改已应用")),bar.x+kPad,y+9,13,running()?col::warn:(n>0?col::amber:col::dim),true);
    ui_.pushId("applybar");
    if(ui_.button({bar.right()-kPad-132,y,132,34},u8"应用",Variant::primary,can,u8"应用修改（Ctrl+Enter）"))applyEdits();
    if(ui_.button({bar.right()-kPad-132-70,y,62,34},u8"还原",Variant::secondary,n>0&&!running())){form_->revert();formDirty_=true;rangeMode_.clear();}
    ui_.popId();
}
float App::drawField(Col&c,const FieldDef&d,bool en){
    auto&g=gfx();Val&v=form_->value(d.key);bool pend=form_->pending(d.key);const std::string&err=form_->error(d.key);
    const float rowH=30;float labelW=std::min(150.f,c.w*.42f);R row{c.x,c.y,c.w,rowH};R lab{row.x,row.y,labelW,rowH};R ctl{row.x+labelW+6,row.y,row.w-labelW-6,rowH};
    float start=c.y;bool changed=false;
    if(pend)g.fill({row.x-9,row.y+7,3,rowH-14},col::amber,1.5f);
    g.textVCenter(g.fit(d.label,labelW-6,13),lab,13,en?(pend?col::amber:col::dim):col::faint);
    if(!d.help.empty())ui_.tip(lab,d.help);
    ui_.pushId(d.key);
    FieldOpt fo;fo.unit=d.unit;fo.edited=pend;fo.enabled=en;fo.error=err;
    float extra=0;
    switch(d.kind){
    case Kind::number:case Kind::integer:case Kind::count:{fo.integer=d.kind!=Kind::number;changed=ui_.field(ctl,"f",v.a,fo);break;}
    case Kind::flag:{bool b=v.a=="true";if(ui_.toggle({ctl.right()-44,row.y+4,44,22},"t",b,en,d.help)){v.a=b?"true":"false";changed=true;}break;}
    case Kind::choice:{int sel=0;for(size_t i=0;i<d.tokens.size();++i)if(d.tokens[i]==v.a)sel=int(i);if(ui_.segmented({ctl.x,row.y+1,ctl.w,28},"s",d.labels,sel,en)){v.a=d.tokens[size_t(sel)];changed=true;}break;}
    case Kind::autoNumber:{
        bool automatic=v.a=="auto";int sel=automatic?0:1;R seg{ctl.x,row.y+1,100,28};
        if(ui_.segmented(seg,"m",{u8"自动",u8"手动"},sel,en)){v.a=sel==0?"auto":form_->resolvedText(d.key);changed=true;automatic=sel==0;}
        R f{seg.right()+6,row.y,ctl.right()-seg.right()-6,rowH};
        if(automatic){g.textVCenter(g.fit("= "+form_->resolvedText(d.key)+(d.unit.empty()?"":" "+d.unit),f.w-4,12),f,12,col::faint,false,4);}
        else{fo.numeric=true;changed|=ui_.field(f,"f",v.a,fo);}
        break;}
    case Kind::range:{
        bool rm=rangeMode_.count(d.key)>0||v.a!=v.b;if(v.a!=v.b)rangeMode_.insert(d.key);
        if(!rm){R f{ctl.x,row.y,ctl.w-58,rowH};if(ui_.field(f,"f",v.a,fo)){v.b=v.a;changed=true;}
            if(ui_.button({ctl.right()-52,row.y+2,52,26},u8"区间",Variant::ghost,en,u8"改为随机区间（每次试验在区间内抽样）")){rangeMode_.insert(d.key);}}
        else{
            if(ui_.button({ctl.right()-64,row.y+2,64,26},u8"固定值",Variant::ghost,en,u8"改回单一固定值")){rangeMode_.erase(d.key);v.b=v.a;changed=true;}
            float hw=(c.w-26)/2;R fa{row.x,row.y+rowH+2,hw,rowH},fb{row.x+hw+26,row.y+rowH+2,hw,rowH};g.textCenter(u8"至",{fa.right(),fa.y,26,fa.h},12,col::faint);
            FieldOpt fa_=fo;fa_.error="";changed|=ui_.field(fa,"a",v.a,fa_);changed|=ui_.field(fb,"b",v.b,fa_);extra=rowH+2;
        }
        break;}
    }
    ui_.popId();
    if(changed){formDirty_=true;form_->clearApplyError();}
    float h=rowH+extra;
    if(!err.empty()){float eh=g.wrapped(err,{ctl.x,start+h,ctl.w,300},11,col::danger);h+=eh+2;if(d.kind==Kind::range&&extra>0)g.wrapped(err,{row.x,start+h-eh-2,c.w,300},11,col::danger,false,false);}
    c.y+=h+6;return h+6;
}
void App::drawSpecimen(Col&c){
    auto&g=gfx();R card=c.row(104,10);g.fill(card,col::panel2,10);g.stroke(card,col::line,1,10);
    g.text(u8"预览石块",card.x+14,card.y+10,14,col::text,true);
    ui_.pushId("spec");
    if(ui_.iconButton({card.right()-112,card.y+8,100,28},Icon::dice,!running()&&!session_->readOnly(),u8"随机换一块石头（R）：只改变预览种子，不影响批次种子",false,u8"换一块",Variant::secondary))newSpecimen();
    ui_.popId();
    if(sv_.scene&&!sv_.scene->shape.vertices.empty()){
        auto&sh=sv_.scene->shape;
        g.text(sfmt(u8"顶点 %d    体积 %.2f m³    外包半径 %.2f m",int(sh.vertices.size()),sh.volume,sv_.scene->radius),card.x+14,card.y+46,12,col::dim);
        g.text(sfmt(u8"内切半径 %.2f m    扁度 τ %.2f    细长比 %.2f",sh.innerRadius,sh.tau,sh.slenderness),card.x+14,card.y+66,12,col::dim);
        g.text(sfmt(u8"预览种子 %llu",(unsigned long long)session_->draft().previewSeed),card.x+14,card.y+84,11,col::faint);
    }else g.text(u8"（场景尚未生成）",card.x+14,card.y+46,12,col::faint);
}
void App::drawSceneTab(R view){
    auto&g=gfx();ui_.beginScroll(view,sceneScroll_,"scene");
    Col c{view.x+kPad,view.y+14-sceneScroll_.y,view.w-2*kPad-8,view.y-sceneScroll_.y};
    if(session_->readOnly()){
        R card=c.row(150,10);g.fill(card,col::panel2,10);g.stroke(card,col::info,1,10);
        g.text(u8"正在查看已保存的结果",card.x+14,card.y+12,14,col::info,true);
        g.wrapped(u8"结果文件夹是只读的：场景参数不可修改，也不能在此重新运行。要修改参数并重新计算，请返回场景编辑。",{card.x+14,card.y+38,card.w-28,70},12,col::dim);
        ui_.pushId("ro");if(ui_.button({card.x+14,card.bottom()-44,160,32},u8"返回场景编辑",Variant::primary,!running()))backToEditing();ui_.popId();
        if(sv_.config){auto&s=sv_.config->source;c.space(4);g.text(sfmt(u8"批次数 N = %llu    批次种子 %llu",(unsigned long long)s.count,(unsigned long long)s.masterSeed),c.x,c.y,13,col::dim);c.space(26);
            g.text(sfmt(u8"挡板：%s    障碍：%s",sv_.config->barrier.enabled?(sv_.config->barrier.elastic?u8"弹性":u8"刚性"):u8"无",s.obstacles.enabled?u8"有":u8"无"),c.x,c.y,13,col::dim);c.space(26);}
        ui_.endScroll(view,sceneScroll_,c.used()+16);return;
    }
    drawSpecimen(c);
    bool en=!running();
    if(!en){g.text(u8"运行中，参数暂时锁定。",c.x,c.y,12,col::warn);c.space(22);}
    for(auto&sec:sectionDefs()){
        auto it=open_.find(sec.id);if(it==open_.end())it=open_.emplace(sec.id,sec.id=="terrain").first;
        int pc=form_->pendingCount(sec.id);R hr=c.row(36,4);
        ui_.header(hr,sec.title,it->second,pc>0?sfmt(u8"%d 项待应用",pc):std::string());
        if(!it->second)continue;
        g.text(g.fit(sec.blurb,c.w,11),c.x+4,c.y,11,col::faint);c.space(22);
        std::string group="~";
        for(auto&d:fieldDefs()){
            if(d.section!=sec.id||!form_->visible(d))continue;
            if(d.group!=group&&!d.group.empty()){group=d.group;c.space(4);g.text(d.group,c.x,c.y,12,col::accent,true);c.space(22);}
            drawField(c,d,en);
        }
        if(sec.id=="output"){
            outputText_=form_->outputRoot.u8string();float lw=std::min(150.f,c.w*.42f);R row{c.x,c.y,c.w,30};g.textVCenter(u8"输出目录",{row.x,row.y,lw,30},13,form_->pendingOutput()?col::amber:col::dim);
            FieldOpt fo;fo.numeric=false;fo.enabled=en;fo.edited=form_->pendingOutput();
            ui_.pushId("outdir");if(ui_.field({row.x+lw+6,row.y,row.w-lw-6-40,30},"f",outputText_,fo)){form_->outputRoot=fs::u8path(outputText_);form_->clearApplyError();}
            if(ui_.iconButton({row.right()-34,row.y,34,30},Icon::folder,en,u8"选择输出目录")){auto p=pickFolder(form_->outputRoot,nativeWindowHandle(),L"选择输出目录");if(!p.empty())form_->outputRoot=p;}
            ui_.popId();c.space(36);
        }
        c.space(6);
    }
    ui_.endScroll(view,sceneScroll_,c.used()+20);
}
void App::drawRunControls(Col&c,bool){
    auto&g=gfx();bool run=running();
    R card=c.row(run?96:0,10);if(!run)return;
    g.fill(card,col::panel2,10);g.stroke(card,col::accent,1,10);
    g.text(sessionStateZh(sv_),card.x+14,card.y+10,15,col::accent,true);
    if(sv_.config)g.text(sfmt(u8"已处理 %llu / %llu",(unsigned long long)sv_.processed,(unsigned long long)sv_.config->source.count),card.x+14,card.y+34,12,col::dim);
    ui_.pushId("rc");
    if(ui_.iconButton({card.x+14,card.y+54,card.w*.5f-20,32},paused()?Icon::play:Icon::pause,true,{},false,paused()?u8"继续":u8"暂停",Variant::secondary))pauseResume();
    if(ui_.iconButton({card.cx()+6,card.y+54,card.w*.5f-20,32},Icon::stop,sv_.state!=SessionState::cancelling,{},false,u8"取消",Variant::danger))cancelRun();
    ui_.popId();
}
void App::drawBatchSummary(Col&c){
    auto&g=gfx();if(!sv_.result||running())return;auto&r=*sv_.result;
    R hr=c.row(30,2);g.text(u8"上一批结果",hr.x,hr.y+4,14,col::text,true);
    auto kv=[&](const std::string&k,const std::string&v,Color vc=col::text){R r1=c.row(22,0);g.text(k,r1.x,r1.y+2,12,col::dim);g.textRight(v,r1.right(),r1.y+2,13,vc,true);};
    Color sc=r.status==BatchStatus::complete?col::ok:(r.status==BatchStatus::cancelled?col::warn:col::danger);
    kv(u8"状态",batchStatusZh(r.status),sc);
    kv(u8"计划 / 已完成",sfmt("%llu / %llu",(unsigned long long)r.config->source.count,(unsigned long long)r.statistics.completed()));
    kv(u8"失败 / 超时",sfmt("%llu / %llu",(unsigned long long)r.failures,(unsigned long long)r.timeouts),r.failures||r.timeouts?col::warn:col::text);
    std::uint64_t cross=0;for(auto h:r.statistics.collectorHits())cross+=h;
    bool complete=r.status==BatchStatus::complete;auto p=r.statistics.probability(cross,complete);
    kv(sfmt(u8"越过收集截面 x=%.1f m",r.statistics.collectorX()),sfmt(u8"%llu 次",(unsigned long long)cross));
    if(p.value)kv(u8"越过频率",formatPercent(*p.value));else kv(u8"越过频率区间",formatPercent(p.lower)+" – "+formatPercent(p.upper));
    kv(u8"未越过 / 区域外",sfmt("%llu / %llu",(unsigned long long)r.statistics.collectorNotCrossed(),(unsigned long long)r.statistics.collectorOutside()));
    kv(u8"总用时",sfmt("%.1f s",r.timing.wall));
    if(!r.error.empty()){c.space(4);float h=g.wrapped(r.error,{c.x,c.y,c.w,300},11,col::warn);c.space(h+4);}
    if(!r.directory.empty()){c.space(4);std::string d=r.directory.u8string();g.text(g.fit(u8"保存于 "+d,c.w,11),c.x,c.y,11,col::faint);c.space(22);
        ui_.pushId("sum");if(ui_.iconButton({c.x,c.y,c.w*.5f-4,32},Icon::folder,true,{},false,u8"打开文件夹",Variant::secondary))openInExplorer(r.directory);
        if(ui_.iconButton({c.x+c.w*.5f+4,c.y,c.w*.5f-4,32},Icon::chart,true,{},false,u8"查看分析",Variant::primary))setTab(Tab::analysis);ui_.popId();c.space(42);}
}
void App::drawRunTab(R view){
    auto&g=gfx();ui_.beginScroll(view,runScroll_,"run");
    Col c{view.x+kPad,view.y+14-runScroll_.y,view.w-2*kPad-8,view.y-runScroll_.y};
    if(session_->readOnly()){
        g.wrapped(u8"当前打开的是只读结果，无法在此运行。请在“场景”页返回编辑后再运行。",{c.x,c.y,c.w,60},13,col::dim);c.space(70);
        ui_.pushId("rob");if(ui_.button(c.row(36,8),u8"返回场景编辑",Variant::primary,!running()))backToEditing();ui_.popId();
        ui_.endScroll(view,runScroll_,c.used()+16);return;
    }
    bool en=!running();
    drawRunControls(c,false);
    g.text(u8"单次试验",c.x,c.y,15,col::text,true);c.space(26);
    float h=g.wrapped(u8"用当前“预览石块”从释放点运行一次，实时观察运动。不消耗批次种子。",{c.x,c.y,c.w,60},12,col::dim);c.space(h+8);
    ui_.pushId("single");if(ui_.iconButton(c.row(38,16),Icon::play,en,u8"运行一次预览石块",false,u8"运行单次试验",Variant::secondary))startSingle();ui_.popId();
    g.fill({c.x,c.y,c.w,1},col::line);c.space(14);
    g.text(u8"批量蒙特卡洛",c.x,c.y,15,col::text,true);c.space(26);
    h=g.wrapped(u8"按批次数 N 与批次种子生成 N 块不同的石头逐一计算；结果可重复。可随时暂停或取消。",{c.x,c.y,c.w,60},12,col::dim);c.space(h+8);
    for(auto&d:fieldDefs())if(d.section=="run"&&form_->visible(d))drawField(c,d,en);
    {R r=c.row(28,6);g.textVCenter(u8"实时预览",{r.x,r.y,std::min(150.f,c.w*.42f),28},13,col::dim);ui_.tip({r.x,r.y,std::min(150.f,c.w*.42f),28},u8"批量计算时是否先实时播放前两次试验。选择“后台”则全速计算。");
        float lw=std::min(150.f,c.w*.42f)+6;ui_.pushId("pv");ui_.segmented({r.x+lw,r.y,r.w-lw,28},"mode",{u8"前两次",u8"后台全速"},previewMode_,en);ui_.popId();}
    c.space(6);
    std::string label=form_->pendingTotal()>0?u8"应用并开始批量计算":u8"开始批量计算";
    ui_.pushId("batch");if(ui_.iconButton(c.row(42,16),Icon::chart,en,u8"以批次数 N 开始蒙特卡洛",false,label,Variant::primary))startBatch();ui_.popId();
    g.fill({c.x,c.y,c.w,1},col::line);c.space(14);
    drawBatchSummary(c);
    ui_.endScroll(view,runScroll_,c.used()+20);
}
}
