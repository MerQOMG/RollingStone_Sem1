#ifndef ROCK_DASHBOARD_H
#define ROCK_DASHBOARD_H

namespace RockUI {
const COLORREF navy=RGB(17,32,52),muted=RGB(116,132,151),ink=RGB(29,45,65),teal=RGB(13,148,136),paper=RGB(241,245,249);
std::atomic<bool> closeRequested(false);
WNDPROC previousWindowProcedure=nullptr;
LRESULT CALLBACK closeProcedure(HWND h,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_CLOSE) { closeRequested=true; return 0; }
    return CallWindowProcW(previousWindowProcedure,h,message,w,l);
}
struct Button { RECT rect; int action; std::wstring text; bool primary,enabled; };
struct HeatCell { double x1,x2,z1,z2,p; };
void text(int x,int y,const std::wstring& value,int size=16,COLORREF color=ink,int width=0) {
    setbkmode(TRANSPARENT); settextstyle((int)(size*1.18),0,L"Microsoft YaHei UI"); settextcolor(color);
    std::wstring shown=value;
    if(width>0) { while(!shown.empty() && textwidth((shown+L"…").c_str())>width) shown.pop_back(); if(shown!=value) shown+=L"…"; }
    outtextxy(x,y,shown.c_str());
}
std::wstring value(double v,int decimals=2) { wchar_t b[100]; std::swprintf(b,100,L"%.*f",decimals,v); return b; }
void box(int x,int y,int w,int h,COLORREF color,int radius=14) {
    setfillcolor(color); setlinecolor(color); solidroundrect(x,y,x+w,y+h,radius,radius);
}
std::vector<HeatCell> readHeatmap(const std::string& directory) {
    std::ifstream in(directory+"/route_probability_grid.csv"); std::vector<HeatCell> cells; std::string line;
    std::getline(in,line);
    while(std::getline(in,line)) {
        std::replace(line.begin(),line.end(),',',' '); std::istringstream row(line);
        double x1,x2,z1,z2,cx,cz,ground,hits,p;
        if(row>>x1>>x2>>z1>>z2>>cx>>cz>>ground>>hits>>p) cells.push_back({x1,x2,z1,z2,p});
    } return cells;
}
bool chooseSettingsPath(HWND owner,bool save,std::wstring& path) {
    wchar_t filename[1024]=L"rockfall_settings.ini"; OPENFILENAMEW dialog={}; dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=owner; dialog.lpstrFile=filename; dialog.nMaxFile=1024;
    dialog.lpstrFilter=L"落石参数方案 (*.ini)\0*.ini\0所有文件\0*.*\0\0"; dialog.lpstrDefExt=L"ini";
    dialog.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST)|OFN_NOCHANGEDIR;
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog)) { path=filename; return true; } return false;
}
}

int runDashboard(bool preview=false) {
    using namespace RockUI;
    const int width=1360,height=840;
    initgraph(width,height); SetWindowTextW(GetHWnd(),L"山区落石实验室 | Rockfall Studio"); BeginBatchDraw();
    closeRequested=false;
    previousWindowProcedure=(WNDPROC)SetWindowLongPtrW(GetHWnd(),GWLP_WNDPROC,(LONG_PTR)closeProcedure);
    ProjectSettings settings; HillSlope hill; std::unique_ptr<RollingStone> stone; ElasticBarrier barrier;
    std::vector<TrialSample> samples; std::vector<Button> buttons; std::vector<HeatCell> heatmap;
    double simTime=0,stable=0,budget=0; bool paused=true,ended=false,exported=false,heatmapView=false,running=true;
    std::string endReason="running",lastDirectory; std::wstring status=L"准备就绪 · 点击左侧参数按钮设置实验，再开始模拟。";
    std::atomic<bool> workerDone(false),cancelBatch(false); std::atomic<int> progressDone(0);
    std::thread worker; ConfiguredBatchReport batchReport; std::string workerError; bool busy=false;
    POINT mouse={-1,-1};
    auto resetTrial=[&]() {
        validateProjectSettings(settings); applyProjectGlobals(settings); HillSlope nextHill=projectHill(settings);
        std::mt19937 rng((unsigned int)settings.seed); PolygonShape shape=projectShape(settings,rng);
        std::unique_ptr<RollingStone> next(new RollingStone(projectStone(settings,nextHill,shape,settings.releaseX,settings.initialSpeed)));
        ElasticBarrier nextBarrier; nextBarrier.reset(nextHill,projectBarrier(settings,nextHill));
        hill=nextHill; stone=std::move(next); barrier=nextBarrier;
        simTime=stable=budget=0; paused=true; ended=false; exported=false; endReason="running";
        samples.clear(); recordSample(samples,0,*stone,barrier); heatmapView=false; heatmap.clear();
    };
    auto saveTrial=[&]() {
        std::string directory=uniqueProjectRun("single");
        exportTrial(directory+"/trial",samples,*stone,barrier,hill,endReason.c_str(),ended && endReason=="settled");
        saveProjectSettings(settings,projectWide(directory+"/settings.ini")); lastDirectory=directory;
        status=L"已导出轨迹、汇总、参数方案："+projectWide(directory);
    };
    try { resetTrial(); }
    catch(const std::exception& e) { MessageBoxW(GetHWnd(),projectWide(e.what()).c_str(),L"初始化失败",MB_OK); EndBatchDraw();closegraph();return 1; }
    auto action=[&](int id) {
        if(busy && id!=103 && id!=104) return;
        try {
            if(id>=0 && id<6) {
                paused=true; ProjectSettings candidate=settings;
                ParameterEditor editor;
                if(editor.show(GetHWnd(),candidate,id)) {
                    ProjectSettings old=settings; settings=candidate;
                    try { resetTrial(); } catch(...) { settings=old; applyProjectGlobals(old); throw; }
                    status=L"参数已应用 · 本次模拟已重置。点击“开始模拟”运行。";
                }
            } else if(id==100) { if(ended) resetTrial(); paused=!paused; }
            else if(id==101) { resetTrial(); status=L"已重置为同一随机种子、同一块石头。"; }
            else if(id==102) saveTrial();
            else if(id==103) {
                if(busy) { cancelBatch=true; status=L"正在取消批量试验；已完成样本会保留。"; return; }
                validateProjectSettings(settings); paused=true; budget=0; std::string directory=uniqueProjectRun("batch");
                ProjectSettings snapshot=settings; workerDone=false; cancelBatch=false; progressDone=0; workerError.clear(); busy=true;
                lastDirectory=directory; status=L"正在执行批量试验，界面参数暂时锁定；再次点击可取消。";
                worker=std::thread([&,snapshot,directory]() {
                    try { batchReport=runConfiguredBatch(snapshot,directory,[&](int done,int) { progressDone=done; return !cancelBatch.load(); }); }
                    catch(const std::exception& e) { workerError=e.what(); }
                    workerDone=true;
                });
            } else if(id==104) {
                if(lastDirectory.empty()) { CreateDirectoryA("results",nullptr); lastDirectory="results"; }
                ShellExecuteW(GetHWnd(),L"open",projectWide(lastDirectory).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            } else if(id==105) { if(!heatmap.empty()) heatmapView=!heatmapView; }
            else if(id==200) {
                settings=ProjectSettings(); settings.shapeMode=1; settings.sidesMin=settings.sidesMax=30;
                settings.releaseX=2; settings.initialSpeed=3; settings.friction=0.25; settings.rolling=0.005;
                settings.batchRange=0; settings.batchSpeed=3; resetTrial();
                status=L"近圆滚落实验 · 保持面积与质量，半径自动匹配；原始地形不变。";
            } else if(id==201) { settings=ProjectSettings(); resetTrial(); status=L"已恢复课程基准；批量参数保留课程下部坡面统计场景。"; }
            else if(id==202) {
                paused=true; std::wstring path;
                if(chooseSettingsPath(GetHWnd(),true,path)) { saveProjectSettings(settings,path); status=L"参数方案已保存。"; }
            } else if(id==203) {
                paused=true; std::wstring path;
                if(chooseSettingsPath(GetHWnd(),false,path)) {
                    ProjectSettings candidate=loadProjectSettings(path),old=settings; settings=candidate;
                    try { resetTrial(); } catch(...) { settings=old; applyProjectGlobals(old); throw; }
                    status=L"参数方案已载入并应用。";
                }
            } else if(id==204) {
                settings.barrierEnabled=settings.barrierEnabled?0:1; resetTrial(); status=L"已切换挡板，同一块石头从相同初始状态重跑。";
            } else if(id==205) {
                settings.seed=settings.seed>=4294967295.0?1:settings.seed+1; resetTrial(); status=L"已增加随机种子，生成新石头。";
            }
        } catch(const std::exception& e) {
            paused=true; status=L"参数或运行错误："+projectWide(e.what());
            MessageBoxW(GetHWnd(),projectWide(e.what()).c_str(),L"请检查实验设置",MB_OK|MB_ICONWARNING);
        }
    };
    auto button=[&](int x,int y,int w,int h,const std::wstring& label,int id,bool primary=false,bool dark=false) {
        bool enabled=!busy || id==103 || id==104;
        bool hover=mouse.x>=x && mouse.x<=x+w && mouse.y>=y && mouse.y<=y+h;
        COLORREF fill=primary?teal:(dark?RGB(31,49,71):RGB(227,235,243));
        if(hover && enabled) fill=primary?RGB(15,118,110):(dark?RGB(43,65,89):RGB(213,227,239));
        if(!enabled) fill=dark?RGB(27,40,57):RGB(220,226,232);
        box(x,y,w,h,fill,10); int tw;
        settextstyle((int)(15*1.18),0,L"Microsoft YaHei UI"); tw=textwidth(label.c_str());
        text(x+(w-tw)/2,y+(h-19)/2,label,15,enabled?(primary||dark?WHITE:ink):muted);
        buttons.push_back(Button{RECT{x,y,x+w,y+h},id,label,primary,enabled});
    };
    while(running && !closeRequested.load()) {
        ExMessage event;
        while(peekmessage(&event,EX_MOUSE|EX_CHAR)) {
            if(event.message==WM_MOUSEMOVE) { mouse.x=event.x; mouse.y=event.y; }
            else if(event.message==WM_LBUTTONUP) {
                // action 可能触发重绘或弹窗，先复制编号，避免迭代时引用失效。
                int id=-1;
                for(const auto& b:buttons) if(b.enabled && event.x>=b.rect.left && event.x<=b.rect.right && event.y>=b.rect.top && event.y<=b.rect.bottom) { id=b.action;break; }
                if(id>=0) action(id);
            } else if(event.message==WM_CHAR) {
                int ch=(int)event.ch;
                if(ch==27) running=false;
                else if(ch==32) action(100);
                else if(ch=='r'||ch=='R') action(205);
                else if(ch=='g'||ch=='G') action(0);
                else if(ch=='m'||ch=='M') action(1);
                else if(ch=='t'||ch=='T') action(2);
                else if(ch=='b'||ch=='B') action(3);
                else if(ch=='q'||ch=='Q') action(4);
                else if(ch=='p'||ch=='P') action(5);
                else if(ch=='v'||ch=='V') action(204);
                else if(ch=='e'||ch=='E') action(102);
            }
        }
        if(busy && workerDone.load()) {
            worker.join(); busy=false;
            if(workerError.empty()) {
                status=(batchReport.cancelled?L"已取消 · ":L"批量完成 · ")+std::to_wstring(batchReport.completed)+L"次，失败 "+
                    std::to_wstring(batchReport.failed)+L"次，到达收集器范围 "+std::to_wstring(batchReport.collected)+L"次。";
                heatmap=readHeatmap(lastDirectory); heatmapView=!heatmap.empty();
            } else { status=L"批量运行失败："+projectWide(workerError); }
            applyProjectGlobals(settings);
        }
        if(!paused && !ended && !busy) {
            budget+=0.016*settings.playback;
            while(budget+1e-12>=settings.timeStep && !ended) {
                double dt=min(settings.timeStep,settings.maxTime-simTime);
                try { stone->update(hill,dt,&barrier); simTime+=dt; budget-=settings.timeStep; recordSample(samples,simTime,*stone,barrier); }
                catch(const std::exception& e) { ended=true; paused=true;endReason="solver_error"; status=projectWide(e.what()); break; }
                stable=(!stone->isMoving() && barrier.settled())?stable+dt:0;
                if(stable>=settings.settleWait) { ended=true;endReason="settled"; }
                else if(simTime>=settings.maxTime-1e-10) { ended=true;endReason="time_limit"; }
                else if(stone->getX()>=max(projectEndX(hill),barrier.enabled()?barrier.faceX()+stone->getShape().extent+1:projectEndX(hill))) { ended=true;endReason="downstream_boundary"; }
            }
            if(ended && !exported) {
                paused=true; exported=true;
                try { saveTrial(); } catch(const std::exception& e) { status=projectWide(e.what()); }
            }
        }
        buttons.clear(); setbkcolor(paper); cleardevice();
        setfillcolor(navy); solidrectangle(0,0,width,80); solidrectangle(0,80,224,height);
        box(24,22,34,34,teal,9); text(33,24,L"R",25,WHITE); text(72,20,L"山区落石实验室",24,WHITE);
        text(73,49,L"ROCKFALL STUDIO  /  实验工作台",12,RGB(160,184,206));
        button(812,22,120,36,ended?L"重新运行":(paused?L"开始模拟":L"暂停模拟"),100,true);
        button(944,22,96,36,L"重置",101,false,true); button(1052,22,116,36,L"导出 CSV",102,false,true);
        button(1180,22,156,36,busy?L"取消批量试验":L"运行批量试验",103,true);
        text(24,108,L"实验参数",13,RGB(132,155,181));
        text(24,134,L"修改参数，无需编辑代码",13,RGB(192,207,222));
        for(int i=0;i<6;++i) button(18,178+i*51,188,40,std::to_wstring(i+1)+L"   "+parameterPageName(i),i,false,true);
        text(24,505,L"预设场景",13,RGB(132,155,181));
        button(18,535,188,39,L"近圆滚落实验",200,true);
        button(18,584,188,39,L"恢复课程基准",201,false,true);
        button(18,661,89,36,L"保存方案",202,false,true); button(117,661,89,36,L"载入方案",203,false,true);
        button(18,710,188,37,L"打开结果文件夹",104,false,true);
        text(24,780,L"SI 单位  ·  固定物理步长",12,RGB(132,155,181));
        text(24,802,L"SPACE 暂停  /  ESC 退出",12,RGB(132,155,181));
        const wchar_t* cardLabels[]={L"当前速度",L"滚石动能",L"水平位置",L"模拟时间"};
        double values[]={stone->getSpeed(),stone->getEnergy()/1000,stone->getX(),simTime};
        const wchar_t* units[]={L"m/s",L"kJ",L"m",L"s"};
        for(int i=0;i<4;++i) {
            int x=248+i*276; box(x,102,260,95,WHITE); box(x+16,122,4,51,i==1?RGB(221,153,65):teal,4);
            text(x+30,117,cardLabels[i],14,muted); text(x+30,141,value(values[i],2),29,ink); text(x+190,157,units[i],14,muted);
        }
        box(248,216,1088,388,WHITE); text(270,230,heatmapView?L"坡脚路线概率":L"地形与运动轨迹",18);
        text(270,257,heatmapView?L"颜色深浅表示经过概率 p_all；每次试验在同一格最多计数一次。":L"水平距离 x / m   ·   高程 z / m",12,muted);
        button(915,228,134,29,heatmapView?L"切回运动轨迹":L"查看概率网格",105);
        double maxZ=0,minZ=0; const auto& ground=hill.getPoints();
        for(size_t i=0;i<ground.size();++i) maxZ=max(maxZ,ground[i].z);
        maxZ+=max(4.0,settings.releaseLift+stone->getShape().extent*2);
        double scale=min(755.0/(projectEndX(hill)-projectStartX(hill)+10),270.0/(maxZ-minZ+3))*settings.zoom;
        auto px=[&](double x) { return 284+(int)((x-projectStartX(hill)+2)*scale); };
        auto py=[&](double z) { return 574-(int)((z-minZ)*scale); };
        HRGN clip=CreateRectRgn(270,278,1050,588); HDC canvas=GetImageHDC(); SelectClipRgn(canvas,clip);
        setlinecolor(RGB(228,235,242)); setlinestyle(PS_SOLID,1);
        for(int x=0;x<=140;x+=10) line(px(x),278,px(x),578);
        for(int z=0;z<=90;z+=10) line(270,py(z),1050,py(z));
        if(heatmapView) for(const auto& c:heatmap) if(c.p>0) {
            double strength=sqrt(min(1.0,c.p)); COLORREF color=RGB((int)(225-212*strength),(int)(244-96*strength),(int)(239-103*strength));
            double z1=c.z1,z2=c.z2;
            if(settings.heightReference) { z1+=hill.getHeight((c.x1+c.x2)/2);z2+=hill.getHeight((c.x1+c.x2)/2); }
            setfillcolor(color); solidrectangle(px(c.x1),py(z2),px(c.x2)+1,py(z1)+1);
        }
        std::vector<int> earth; earth.push_back(px(ground[0].x)-20);earth.push_back(py(-5));
        earth.push_back(px(ground[0].x)-20);earth.push_back(py(ground[0].z));
        for(size_t i=0;i<ground.size();++i) { earth.push_back(px(ground[i].x));earth.push_back(py(ground[i].z)); }
        earth.push_back(px(projectEndX(hill))+30);earth.push_back(py(ground[ground.size()-1].z));
        earth.push_back(px(projectEndX(hill))+30);earth.push_back(py(-5));
        setfillcolor(RGB(221,230,226)); setlinecolor(RGB(221,230,226)); fillpoly((int)earth.size()/2,earth.data());
        setlinecolor(RGB(77,105,94));setlinestyle(PS_SOLID,3);
        for(size_t i=1;i<ground.size();++i) line(px(ground[i-1].x),py(ground[i-1].z),px(ground[i].x),py(ground[i].z));
        double collector=projectCollectorX(settings,hill),base=hill.getHeight(collector);
        setlinecolor(RGB(213,159,66));setlinestyle(PS_DASH,1);
        line(px(collector),py(settings.heightReference?base+settings.analysisHeight:settings.analysisHeight),px(collector),py(base));
        if(!heatmapView && settings.trailLimit>0) {
            setlinecolor(teal);setlinestyle(PS_SOLID,2);
            size_t first=samples.size()>(size_t)settings.trailLimit?samples.size()-(size_t)settings.trailLimit:1;
            for(size_t i=max((size_t)1,first);i<samples.size();++i) line(px(samples[i-1].x),py(samples[i-1].z),px(samples[i].x),py(samples[i].z));
        }
        if(barrier.enabled()) {
            setlinecolor(RGB(54,104,169));setlinestyle(PS_SOLID,4);
            line(px(barrier.faceX()),py(barrier.baseZ()),px(barrier.faceX()),py(barrier.topZ()));
        }
        if(!heatmapView) {
            auto vertices=stone->worldVertices(); std::vector<int> coords;
            for(const auto& p:vertices) { coords.push_back(px(p.x));coords.push_back(py(p.z)); }
            setfillcolor(RGB(67,82,104));setlinecolor(RGB(29,45,65));setlinestyle(PS_SOLID,1);
            fillpoly((int)vertices.size(),coords.data()); setfillcolor(RGB(249,190,76));solidcircle(px(stone->getX()),py(stone->getZ()),2);
        }
        SelectClipRgn(canvas,nullptr);DeleteObject(clip);setlinestyle(PS_SOLID,1);
        for(int x=0;x<=120;x+=20) if(px(x)>276 && px(x)<1040) text(px(x)-8,580,std::to_wstring(x),11,muted);
        text(1080,280,L"场景快照",14,muted);
        text(1080,309,settings.shapeMode?L"近圆凸多边形":L"随机凸多边形",16);
        text(1080,340,L"边数  "+std::to_wstring(stone->getSides())+L"     半径 "+value(stone->getRadius(),2)+L" m",13,muted);
        text(1080,367,L"质量  "+value(stone->getMass(),0)+L" kg",14);
        text(1080,394,L"断面  x = "+value(collector,1)+L" m",14);
        text(1080,421,L"种子  "+value(settings.seed,0),13,muted);
        if(heatmapView) {
            for(int i=0;i<100;++i) { double s=sqrt(i/99.0);setfillcolor(RGB((int)(225-212*s),(int)(244-96*s),(int)(239-103*s)));solidrectangle(1080+i*2,454,1082+i*2,462); }
            text(1080,466,L"0",10,muted);text(1265,466,L"1",10,muted);
        }
        const auto& shape=stone->getShape(); double extent=max(0.01,shape.extent); std::vector<int> previewXY;
        for(const auto& p:shape.local) { Point q=rotate2(p,stone->getAngle());previewXY.push_back(1185+(int)(q.x*40/extent));previewXY.push_back(500-(int)(q.z*40/extent)); }
        setfillcolor(RGB(99,116,139));setlinecolor(RGB(60,78,100));fillpoly((int)shape.local.size(),previewXY.data());
        text(1110,549,L"形状预览 · 放大显示",12,muted);
        box(248,622,700,172,WHITE);text(270,635,L"速度—时间",16);text(822,639,L"m/s",12,muted);
        int gx=292,gy=665,gw=625,gh=100;double t0=max(0.0,simTime-settings.chartWindow),t1=max(settings.chartWindow,simTime),ymax=5;
        auto firstSample=std::lower_bound(samples.begin(),samples.end(),t0,[](const TrialSample& a,double t){return a.t<t;});
        for(auto it=firstSample;it!=samples.end();++it) ymax=max(ymax,sqrt(it->vx*it->vx+it->vz*it->vz)); ymax*=1.1;
        setlinecolor(RGB(228,235,242));
        for(int i=0;i<4;++i) { int y=gy+i*gh/3;line(gx,y,gx+gw,y);text(258,y-6,value(ymax*(3-i)/3,0),10,muted); }
        int stride=max(1,(int)(samples.end()-firstSample)/600);bool hasPrevious=false;int lastX=0,lastY=0;
        setlinecolor(teal);setlinestyle(PS_SOLID,2);
        for(auto it=firstSample;it<samples.end();it+=min(stride,(int)(samples.end()-it))) {
            int x=gx+(int)((it->t-t0)/(t1-t0)*gw),y=gy+gh-(int)(sqrt(it->vx*it->vx+it->vz*it->vz)/ymax*gh);
            if(hasPrevious) line(lastX,lastY,x,y);lastX=x;lastY=y;hasPrevious=true;
        }
        setlinestyle(PS_SOLID,1); text(gx,773,value(t0,1)+L" s",10,muted);text(gx+gw-45,773,value(t1,1)+L" s",10,muted);
        box(968,622,368,172,WHITE);text(990,636,L"弹性挡板 · 数值结果",16);
        text(990,668,L"最大位移",13,muted);text(1140,665,value(barrier.maxDisplacement(),4)+L" m",18,teal);
        text(990,700,L"支座力",13,muted);text(1140,697,value(barrier.supportForce()/1000,2)+L" kN",18);
        text(990,733,L"阻尼耗能",13,muted);text(1140,730,value(barrier.dampingEnergy()/1000,2)+L" kJ",18);
        std::wstring state=busy?L"批量运行 "+std::to_wstring(progressDone.load())+L" / "+value(settings.batchCount,0):
            (ended?L"已结束":(paused?L"已暂停":L"正在模拟"));
        box(248,806,94,23,busy?RGB(224,239,251):RGB(222,242,235),8);text(257,809,state,11,teal,82);
        text(355,809,status,12,muted,975);
        if(busy) { double p=min(1.0,(double)progressDone.load()/settings.batchCount);box(248,798,1088,3,RGB(218,230,239),2);if(p>0)box(248,798,(int)(1088*p),3,teal,2); }
        FlushBatchDraw();
        if(preview) { CreateDirectoryA("docs",nullptr); saveimage(L"docs/ui_dashboard.png"); running=false; }
        Sleep(16);
    }
    cancelBatch=true; if(worker.joinable()) worker.join();
    SetWindowLongPtrW(GetHWnd(),GWLP_WNDPROC,(LONG_PTR)previousWindowProcedure);
    EndBatchDraw();closegraph();return 0;
}
#endif
