#ifndef ROCK_PARAMETER_EDITOR_H
#define ROCK_PARAMETER_EDITOR_H

class ParameterEditor {
    HWND window=nullptr,owner=nullptr,derived=nullptr,points=nullptr;
    HFONT font=nullptr,titleFont=nullptr,labelFont=nullptr; HBRUSH white=nullptr,editBrush=nullptr;
    ProjectSettings draft; int page=0; bool closed=false,accepted=false,building=false;
    std::vector<HWND> pageControls; std::vector<std::pair<int,HWND>> inputs;
    HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id,bool temporary=true) {
        HWND c=CreateWindowExW(cls==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,cls,text,
            WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandle(nullptr),nullptr);
        SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE); if(temporary) pageControls.push_back(c); return c;
    }
    void label(const std::wstring& text,int x,int y,int w,int h) { HWND c=control(L"STATIC",text.c_str(),0,x,y,w,h,0); SendMessageW(c,WM_SETFONT,(WPARAM)labelFont,TRUE); }
    static std::wstring number(double value) { wchar_t b[80]; std::swprintf(b,80,L"%.10g",value); return b; }
    HWND inputFor(int index) const { for(const auto& p:inputs) if(p.first==index) return p.second; return nullptr; }
    void readPage() {
        for(const auto& input:inputs) {
            const auto& f=parameterFields()[input.first]; double value;
            if(f.choices) value=(double)SendMessageW(input.second,CB_GETCURSEL,0,0);
            else {
                wchar_t text[128]; GetWindowTextW(input.second,text,128); wchar_t* end;
                errno=0; value=std::wcstod(text,&end);
                if(end==text || errno==ERANGE) { SetFocus(input.second); throw std::runtime_error(std::string("请输入有效数值：")+f.key); }
                while(*end==L' ' || *end==L'\t') ++end;
                if(*end || !std::isfinite(value)) { SetFocus(input.second); throw std::runtime_error(std::string("数值格式无效：")+f.key); }
            }
            draft.*(f.member)=value;
        }
        if(points) {
            int length=GetWindowTextLengthW(points); std::vector<wchar_t> text(length+1);
            GetWindowTextW(points,text.data(),length+1); draft.terrainPoints=projectUtf8(text.data());
        }
    }
    void updateState() {
        if(building) return;
        try { readPage(); } catch(...) { return; }
        for(const auto& input:inputs) {
            const auto& f=parameterFields()[input.first]; bool enable=true;
            if(f.member==&ProjectSettings::radius && draft.shapeMode==1) enable=false;
            if(f.page==2 && f.member!=&ProjectSettings::terrainMode) enable=draft.terrainMode==1;
            if(f.member==&ProjectSettings::barrierX) enable=draft.barrierAuto==0;
            if(f.member==&ProjectSettings::collectorX) enable=draft.collectorAuto==0;
            if(f.member==&ProjectSettings::batchXMin || f.member==&ProjectSettings::batchXMax) enable=draft.batchRange!=0;
            EnableWindow(input.second,enable);
            HWND minus=GetDlgItem(window,10000+input.first*2),plus=GetDlgItem(window,10001+input.first*2);
            if(minus) EnableWindow(minus,enable); if(plus) EnableWindow(plus,enable);
        }
        if(points) EnableWindow(points,draft.terrainMode==2);
        wchar_t text[240]; double radius=draft.radius;
        if(draft.shapeMode==1 && draft.sidesMin>=3 && draft.area>0) radius=sqrt(draft.area/maxPolygonArea((int)draft.sidesMin,1));
        std::swprintf(text,240,L"派生量：质量 %.2f kg  ·  半径约 %.3f m\n应用后重新开始，已有导出文件保留。",draft.density*draft.area*draft.thickness,radius);
        if(derived) SetWindowTextW(derived,text);
    }
    void buildPage() {
        building=true; for(HWND c:pageControls) DestroyWindow(c); pageControls.clear(); inputs.clear(); points=nullptr;
        std::vector<int> indexes;
        for(size_t i=0;i<parameterFields().size();++i) if(parameterFields()[i].page==page) indexes.push_back((int)i);
        int rows=((int)indexes.size()+1)/2;
        for(size_t j=0;j<indexes.size();++j) {
            int index=indexes[j],col=(int)j/rows,row=(int)j%rows,x=244+col*366,y=119+row*58;
            const auto& f=parameterFields()[index]; label(std::wstring(f.label)+L"  ["+number(f.lo)+L"~"+number(f.hi)+L"]",x,y,345,21);
            if(f.choices) {
                HWND c=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,x,y+23,338,190,1000+index);
                std::wistringstream stream(f.choices); std::wstring choice;
                while(std::getline(stream,choice,L'|')) SendMessageW(c,CB_ADDSTRING,0,(LPARAM)choice.c_str());
                SendMessageW(c,CB_SETCURSEL,(WPARAM)(int)(draft.*(f.member)),0); inputs.push_back({index,c});
            } else {
                HWND c=control(L"EDIT",number(draft.*(f.member)).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,x,y+23,246,28,1000+index);
                inputs.push_back({index,c});
                control(L"BUTTON",L"−",BS_OWNERDRAW|WS_TABSTOP,x+254,y+23,38,28,10000+index*2);
                control(L"BUTTON",L"+",BS_OWNERDRAW|WS_TABSTOP,x+300,y+23,38,28,10001+index*2);
            }
        }
        if(page==2) {
            label(L"自定义地形控制点：每行 x z（米），x 严格递增；仅在“自定义控制点”模式启用。",244,388,706,25);
            std::string raw=draft.terrainPoints; std::string multiline;
            for(char c:raw) { if(c==';') multiline+="\r\n"; else multiline+=c; }
            points=control(L"EDIT",projectWide(multiline).c_str(),ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,244,420,704,218,1900);
            SendMessageW(points,EM_SETLIMITTEXT,12000,0);
        }
        building=false; updateState(); InvalidateRect(window,nullptr,TRUE);
        for(int i=0;i<6;++i) InvalidateRect(GetDlgItem(window,200+i),nullptr,TRUE);
    }
    void stepValue(int id) {
        int index=(id-10000)/2; const auto& f=parameterFields()[index]; HWND edit=inputFor(index); if(!edit) return;
        wchar_t text[128]; GetWindowTextW(edit,text,128); double value=std::wcstod(text,nullptr),step=1;
        if(!f.integer) {
            step=0.1;
            if(f.hi<=5) step=0.01;
            if(f.member==&ProjectSettings::timeStep) step=0.00025;
            if(f.member==&ProjectSettings::density) step=100;
            if(f.member==&ProjectSettings::barrierMass) step=100;
            if(f.member==&ProjectSettings::barrierStiffness) step=10000;
            if(f.member==&ProjectSettings::barrierDamping) step=1000;
            if(f.member==&ProjectSettings::initialSpeed || f.member==&ProjectSettings::batchSpeed) step=0.5;
            if(f.member==&ProjectSettings::initialAngle || f.member==&ProjectSettings::steepAngle || f.member==&ProjectSettings::benchAngle) step=1;
        }
        value=max(f.lo,min(f.hi,value+((id-10000)%2?step:-step)));
        SetWindowTextW(edit,number(value).c_str()); updateState();
    }
    LRESULT message(UINT m,WPARAM w,LPARAM l) {
        if(m==WM_COMMAND) {
            int id=LOWORD(w),code=HIWORD(w);
            try {
                if(id>=200 && id<206 && code==BN_CLICKED) { readPage(); page=id-200; buildPage(); }
                else if(id>=10000 && id<11000 && code==BN_CLICKED) stepValue(id);
                else if(id==IDOK && code==BN_CLICKED) { readPage(); validateProjectSettings(draft); accepted=true; closed=true; DestroyWindow(window); }
                else if(id==IDCANCEL && code==BN_CLICKED) { closed=true; DestroyWindow(window); }
                else if((id>=1000 && id<1200) && (code==EN_CHANGE || code==CBN_SELCHANGE)) updateState();
            } catch(const std::exception& e) { MessageBoxW(window,projectWide(e.what()).c_str(),L"请检查参数",MB_OK|MB_ICONWARNING); }
            return 0;
        }
        if(m==WM_CLOSE) { closed=true; DestroyWindow(window); return 0; }
        if(m==WM_CTLCOLORSTATIC || m==WM_CTLCOLOREDIT || m==WM_CTLCOLORLISTBOX) {
            HDC dc=(HDC)w; SetTextColor(dc,RGB(36,48,65)); SetBkColor(dc,RGB(255,255,255)); return (LRESULT)white;
        }
        if(m==WM_DRAWITEM) {
            auto item=(DRAWITEMSTRUCT*)l; RECT r=item->rcItem; int id=(int)item->CtlID;
            bool selected=(id>=200 && id<206 && id-200==page),primary=id==IDOK;
            COLORREF color=(selected||primary)?RGB(13,148,136):(id>=200 && id<206?RGB(31,47,68):RGB(232,238,245));
            if(item->itemState&ODS_SELECTED) color=RGB(15,118,110);
            HBRUSH b=CreateSolidBrush(color); FillRect(item->hDC,&r,b); DeleteObject(b);
            SetBkMode(item->hDC,TRANSPARENT); SetTextColor(item->hDC,(selected||primary||(id>=200 && id<206))?RGB(255,255,255):RGB(40,55,75));
            SelectObject(item->hDC,font); wchar_t text[160]; GetWindowTextW(item->hwndItem,text,160);
            DrawTextW(item->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            if(item->itemState&ODS_FOCUS) { InflateRect(&r,-3,-3); DrawFocusRect(item->hDC,&r); }
            return TRUE;
        }
        if(m==WM_PAINT) {
            PAINTSTRUCT ps; HDC dc=BeginPaint(window,&ps); RECT r; GetClientRect(window,&r); FillRect(dc,&r,white);
            HBRUSH nav=CreateSolidBrush(RGB(18,33,53)); RECT left={0,0,216,r.bottom}; FillRect(dc,&left,nav); DeleteObject(nav);
            SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,titleFont);
            TextOutW(dc,24,34,L"参数中心",4); SelectObject(dc,font); SetTextColor(dc,RGB(148,170,193));
            TextOutW(dc,24,72,L"PARAMETER STUDIO",16);
            SetTextColor(dc,RGB(26,42,61)); SelectObject(dc,titleFont);
            const wchar_t* title=parameterPageName(page); TextOutW(dc,244,28,title,(int)wcslen(title));
            SelectObject(dc,font); SetTextColor(dc,RGB(108,123,141));
            const wchar_t* hints[]={L"近圆模式由面积推算半径，保持质量；原随机块石模式可分别给定半径与面积。",
                L"这些值也是批量正态分布的地表参数均值；摩擦与滚动阻力具有不同物理作用。",
                L"先选择地形来源。六参数只影响参数化山坡；原始地形保持课程数据。",
                L"等效水平弹簧阻尼挡板；接触恢复系数固定为0，回弹由弹簧产生。",
                L"本页设置批量试验与概率口径。CV默认0.1；近圆半径由抽样面积推算。",
                L"倍速增加执行步数，不放大时间步；缩放、拖尾与曲线窗口只改变显示。"};
            RECT hint={244,66,955,112}; DrawTextW(dc,hints[page],-1,&hint,DT_WORDBREAK);
            EndPaint(window,&ps); return 0;
        }
        return DefWindowProcW(window,m,w,l);
    }
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l) {
        ParameterEditor* self=(ParameterEditor*)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(m==WM_NCCREATE) { self=(ParameterEditor*)((CREATESTRUCT*)l)->lpCreateParams; self->window=h; SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self); }
        return self?self->message(m,w,l):DefWindowProcW(h,m,w,l);
    }
public:
    bool show(HWND parent,ProjectSettings& settings,int initialPage) {
        draft=settings; page=initialPage; owner=parent;
        font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        titleFont=CreateFontW(-25,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        labelFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        white=CreateSolidBrush(RGB(255,255,255)); editBrush=CreateSolidBrush(RGB(248,250,253));
        WNDCLASSW cls={}; cls.lpfnWndProc=procedure; cls.hInstance=GetModuleHandle(nullptr); cls.lpszClassName=L"RockParameterEditorV2";
        cls.hCursor=LoadCursor(nullptr,IDC_ARROW); RegisterClassW(&cls);
        RECT rect={0,0,1000,770}; AdjustWindowRect(&rect,WS_CAPTION|WS_SYSMENU,FALSE);
        int width=rect.right-rect.left,height=rect.bottom-rect.top;
        window=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,L"参数中心 · 山区落石实验室",WS_CAPTION|WS_SYSMENU,
            max(0,(GetSystemMetrics(SM_CXSCREEN)-width)/2),max(0,(GetSystemMetrics(SM_CYSCREEN)-height)/2),width,height,owner,nullptr,cls.hInstance,this);
        if(!window) { DeleteObject(font);DeleteObject(titleFont);DeleteObject(labelFont);DeleteObject(white);DeleteObject(editBrush); throw std::runtime_error("无法打开参数编辑窗口。"); }
        for(int i=0;i<6;++i) control(L"BUTTON",parameterPageName(i),BS_OWNERDRAW|WS_TABSTOP,20,128+i*68,176,48,200+i,false);
        control(L"BUTTON",L"应用并重新开始",BS_OWNERDRAW|WS_TABSTOP,696,716,156,36,IDOK,false);
        control(L"BUTTON",L"取消",BS_OWNERDRAW|WS_TABSTOP,868,716,96,36,IDCANCEL,false);
        derived=control(L"STATIC",L"",0,244,710,435,48,1980,false);
        buildPage(); EnableWindow(owner,FALSE); ShowWindow(window,SW_SHOW); SetForegroundWindow(window);
        MSG msg;
        while(!closed) {
            int rc=GetMessageW(&msg,nullptr,0,0); if(rc<=0) { closed=true; break; }
            if(!IsDialogMessageW(window,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        if(IsWindow(window)) DestroyWindow(window); EnableWindow(owner,TRUE); SetForegroundWindow(owner);
        DeleteObject(font);DeleteObject(titleFont);DeleteObject(labelFont);DeleteObject(white);DeleteObject(editBrush);
        if(accepted) settings=draft; return accepted;
    }
};
#endif
