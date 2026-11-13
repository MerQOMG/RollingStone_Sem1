#include "ui/widgets.h"
#include <cmath>
#include <cstring>
namespace rs3d::gui {
namespace {
std::uint32_t fnv(std::uint32_t h,const std::string&s){for(unsigned char c:s){h^=c;h*=16777619u;}return h;}
}
void drawIcon(Icon icon,R r,Color c){
    auto&g=gfx();float cx=r.cx(),cy=r.cy(),s=std::min(r.w,r.h)*.5f*0.62f;
    switch(icon){
    case Icon::play:g.tri(cx-s*.7f,cy-s,cx-s*.7f,cy+s,cx+s,cy,c);break;
    case Icon::pause:g.fill({cx-s*.8f,cy-s,s*.55f,s*2},c,1);g.fill({cx+s*.25f,cy-s,s*.55f,s*2},c,1);break;
    case Icon::stop:g.fill({cx-s*.8f,cy-s*.8f,s*1.6f,s*1.6f},c,2);break;
    case Icon::toStart:g.fill({cx-s,cy-s,s*.3f,s*2},c);g.tri(cx+s,cy-s,cx+s,cy+s,cx-s*.55f,cy,c);break;
    case Icon::toEnd:g.fill({cx+s*.7f,cy-s,s*.3f,s*2},c);g.tri(cx-s,cy-s,cx-s,cy+s,cx+s*.55f,cy,c);break;
    case Icon::stepBack:g.tri(cx+s*.8f,cy-s,cx+s*.8f,cy+s,cx-s*.6f,cy,c);break;
    case Icon::stepFwd:g.tri(cx-s*.8f,cy-s,cx-s*.8f,cy+s,cx+s*.6f,cy,c);break;
    case Icon::folder:g.fill({cx-s,cy-s*.55f,s*2,s*1.45f},c,2);g.fill({cx-s,cy-s*.9f,s*.9f,s*.55f},c,1);break;
    case Icon::save:g.stroke({cx-s*.9f,cy-s*.9f,s*1.8f,s*1.8f},c,1.6f,2);g.fill({cx-s*.45f,cy-s*.9f,s*.9f,s*.7f},c);g.fill({cx-s*.5f,cy+s*.1f,s,s*.7f},c,1);break;
    case Icon::dice:g.stroke({cx-s*.9f,cy-s*.9f,s*1.8f,s*1.8f},c,1.6f,3);for(auto p:{std::pair<float,float>{-.4f,-.4f},{.4f,.4f},{0,0},{-.4f,.4f},{.4f,-.4f}})g.circle(cx+p.first*s,cy+p.second*s,s*.14f,c);break;
    case Icon::reset:g.ring(cx,cy,s*.9f,1.7f,c);g.tri(cx+s*.2f,cy-s*1.15f,cx+s*.2f,cy-s*.35f,cx+s*.95f,cy-s*.75f,c);break;
    case Icon::present:g.stroke({cx-s,cy-s*.75f,s*2,s*1.35f},c,1.6f,2);g.line(cx,cy+s*.6f,cx,cy+s*1.0f,c,1.6f);g.line(cx-s*.5f,cy+s,cx+s*.5f,cy+s,c,1.6f);break;
    case Icon::help:g.ring(cx,cy,s,1.6f,c);g.text("?",cx-3,cy-8,13,c,true);break;
    case Icon::close:g.line(cx-s*.7f,cy-s*.7f,cx+s*.7f,cy+s*.7f,c,1.8f);g.line(cx-s*.7f,cy+s*.7f,cx+s*.7f,cy-s*.7f,c,1.8f);break;
    case Icon::chevronDown:g.line(cx-s*.7f,cy-s*.3f,cx,cy+s*.4f,c,1.8f);g.line(cx,cy+s*.4f,cx+s*.7f,cy-s*.3f,c,1.8f);break;
    case Icon::chevronRight:g.line(cx-s*.3f,cy-s*.7f,cx+s*.4f,cy,c,1.8f);g.line(cx+s*.4f,cy,cx-s*.3f,cy+s*.7f,c,1.8f);break;
    case Icon::check:g.line(cx-s*.8f,cy,cx-s*.2f,cy+s*.6f,c,2);g.line(cx-s*.2f,cy+s*.6f,cx+s*.8f,cy-s*.6f,c,2);break;
    case Icon::warning:g.tri(cx,cy-s,cx-s,cy+s*.85f,cx+s,cy+s*.85f,c);g.fill({cx-1,cy-s*.35f,2,s*.7f},col::panel);g.circle(cx,cy+s*.5f,1.2f,col::panel);break;
    case Icon::camera:g.stroke({cx-s,cy-s*.6f,s*2,s*1.4f},c,1.6f,3);g.ring(cx,cy+s*.1f,s*.45f,1.5f,c);break;
    case Icon::target:g.ring(cx,cy,s,1.5f,c);g.circle(cx,cy,s*.3f,c);break;
    case Icon::chart:g.fill({cx-s,cy,s*.5f,s},c);g.fill({cx-s*.25f,cy-s*.6f,s*.5f,s*1.6f},c);g.fill({cx+s*.5f,cy-s,s*.5f,s*2},c);break;
    case Icon::cube:g.stroke({cx-s*.8f,cy-s*.5f,s*1.4f,s*1.4f},c,1.5f,1);g.line(cx-s*.8f,cy-s*.5f,cx-s*.3f,cy-s,c,1.5f);g.line(cx+s*.6f,cy-s*.5f,cx+s*1.0f,cy-s,c,1.5f);g.line(cx-s*.3f,cy-s,cx+s*1.0f,cy-s,c,1.5f);g.line(cx+s*1.0f,cy-s,cx+s*1.0f,cy+s*.3f,c,1.5f);g.line(cx+s*.6f,cy+s*.9f,cx+s*1.0f,cy+s*.3f,c,1.5f);break;
    case Icon::apply:g.line(cx-s*.8f,cy,cx-s*.2f,cy+s*.6f,c,2.2f);g.line(cx-s*.2f,cy+s*.6f,cx+s*.8f,cy-s*.6f,c,2.2f);break;
    case Icon::undo:g.line(cx-s*.7f,cy-s*.3f,cx+s*.2f,cy-s*.3f,c,1.8f);g.line(cx+s*.2f,cy-s*.3f,cx+s*.8f,cy+s*.1f,c,1.8f);g.line(cx+s*.8f,cy+s*.1f,cx+s*.2f,cy+s*.7f,c,1.8f);g.line(cx-s*.7f,cy-s*.3f,cx-s*.2f,cy-s*.8f,c,1.8f);g.line(cx-s*.7f,cy-s*.3f,cx-s*.2f,cy+s*.2f,c,1.8f);break;
    case Icon::plus:g.line(cx-s*.8f,cy,cx+s*.8f,cy,c,1.8f);g.line(cx,cy-s*.8f,cx,cy+s*.8f,c,1.8f);break;
    }
}
std::uint32_t Ui::id(const std::string&label)const{return fnv(idStack_.back(),label);}
bool Ui::hoverRaw(R r)const{
    if(!in.focused)return false;
    if(menuBlock_&&!inOverlay_)return false;
    return r.contains(in.mx,in.my)&&gfx().clip().contains(in.mx,in.my);
}
void Ui::begin(float w,float h){
    float s=gfx().scale();Vector2 m=GetMousePosition();in.mx=m.x/s;in.my=m.y/s;
    in.pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);in.released=IsMouseButtonReleased(MOUSE_BUTTON_LEFT);in.down=IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.rpressed=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);in.rdown=IsMouseButtonDown(MOUSE_BUTTON_RIGHT);in.mdown=IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    in.wheel=GetMouseWheelMove();in.focused=IsWindowFocused();
    in.ctrl=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);in.shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
    winW_=w;winH_=h;
    chars_.clear();for(int c=GetCharPressed();c>0;c=GetCharPressed())chars_.push_back(c);
    if(!hoverSeen_)hoverId_=0;hoverSeen_=false;tipText_.clear();
    wheelUsed_=false;fieldClicked_=false;cursor_=MOUSE_CURSOR_DEFAULT;blocked_=false;menuBlock_=menuOpen_;
    prevOrder_.swap(order_);order_.clear();
    if(!in.down&&!in.pressed&&!in.released)active_=0; // keep the id on the release frame so interact() can fire the click
    if(!in.focused){active_=0;}
}
void Ui::end(){
    if(in.pressed&&!fieldClicked_&&focus_&&!menuOpen_)blur();
    inOverlay_=true;
    if(menuOpen_)drawMenu();
    for(auto&f:overlays_)f();overlays_.clear();
    if(!tipText_.empty()&&!menuOpen_){
        auto&g=gfx();float w=std::min(320.f,g.textWidth(tipText_,12)+20);R body{0,0,w,0};float h=g.wrapped(tipText_,{0,0,w-20,1000},12,col::text,false,false)+12;
        float x=std::min(tipX_+12,winW_-w-6),y=tipY_+20;if(y+h>winH_-4)y=tipY_-h-8;
        R box{std::max(4.f,x),y,w,h};g.fill({box.x+1,box.y+2,box.w,box.h},rgba(0,90),6);g.fill(box,rgba(0x2a3441),6);g.stroke(box,col::lineStrong,1,6);g.wrapped(tipText_,{box.x+10,box.y+6,w-20,h},12,col::text);
    }
    inOverlay_=false;
    SetMouseCursor(cursor_);
    if(in.released)active_=0;
}
void Ui::markHover(std::uint32_t id,const std::string&tip,R){
    if(hoverId_!=id){hoverId_=id;hoverSince_=GetTime();}
    hoverSeen_=true;
    if(!tip.empty()&&GetTime()-hoverSince_>.45&&!active_){tipText_=tip;tipX_=in.mx;tipY_=in.my;}
}
bool Ui::interact(std::uint32_t id,R r,bool enabled,bool&hover,bool&held,const std::string&tip){
    hover=enabled&&hovered(r)&&(active_==0||active_==id);
    if(trace&&enabled)trace(id,r,0);
    if(hover){markHover(id,tip,r);setCursor(MOUSE_CURSOR_POINTING_HAND);if(in.pressed){active_=id;if(trace)trace(id,r,1);}}
    else if(!enabled&&hovered(r)&&!tip.empty())markHover(id,tip,r);
    held=active_==id&&in.down;
    bool clicked=false;
    if(active_==id&&in.released){clicked=hover;active_=0;if(clicked&&trace)trace(id,r,2);}
    return clicked;
}
bool Ui::button(R r,const std::string&label,Variant v,bool enabled,const std::string&tip){
    auto&g=gfx();auto key=id(label+"#btn");bool hover,held;bool clicked=interact(key,r,enabled,hover,held,tip);
    Color fill=col::panel3,border=col::line,text=col::text;
    switch(v){
    case Variant::primary:fill=hover?col::accentHi:col::accent;border=fill;text=rgba(0x04191d);if(held)fill=mix(col::accent,col::bgTop,.15f);break;
    case Variant::secondary:fill=hover?rgba(0x2b3746):col::panel3;border=hover?col::lineStrong:col::line;if(held)fill=rgba(0x182029);break;
    case Variant::ghost:fill=hover?col::panel3:alpha(col::panel3,0);border=alpha(col::line,0);text=hover?col::text:col::dim;break;
    case Variant::danger:fill=hover?rgba(0x5e2428):col::dangerLo;border=col::danger;text=rgba(0xffb3b3);break;
    }
    if(!enabled){fill=v==Variant::ghost?alpha(col::panel3,0):col::panel2;border=col::line;text=col::faint;if(v==Variant::primary){fill=col::panel3;border=col::line;}}
    g.fill(r,fill,6);if(border.a)g.stroke(r,border,1,6);
    g.textCenter(g.fit(label,r.w-10,13,true),r,13,text,true);
    return clicked&&enabled;
}
bool Ui::iconButton(R r,Icon icon,bool enabled,const std::string&tip,bool active,const std::string&label,Variant v){
    auto&g=gfx();auto key=id(tip+label+std::to_string(int(icon))+"#ib");bool hover,held;bool clicked=interact(key,r,enabled,hover,held,tip);
    Color fill=active?col::accentLo:(hover?col::panel3:alpha(col::panel3,0)),fg=active?col::accentHi:(hover?col::text:col::dim);
    if(v==Variant::primary){fill=hover?col::accentHi:col::accent;fg=rgba(0x04191d);}
    if(v==Variant::secondary){fill=hover?rgba(0x2b3746):col::panel3;}
    if(v==Variant::danger){fill=hover?rgba(0x5e2428):col::dangerLo;fg=rgba(0xffb3b3);}
    if(held&&v!=Variant::primary)fill=rgba(0x182029);
    if(!enabled){fg=col::faint;if(v!=Variant::ghost)fill=col::panel2;}
    g.fill(r,fill,6);
    if(active)g.stroke(r,col::accent,1,6);else if(v==Variant::secondary&&enabled)g.stroke(r,hover?col::lineStrong:col::line,1,6);
    if(label.empty())drawIcon(icon,r,fg);
    else{float tw=g.textWidth(label,13,true);float span=std::min(r.h*.5f,18.f);float total=span+9+tw;float x0=r.x+(r.w-total)*.5f;drawIcon(icon,{x0+span*.5f-r.h*.5f,r.y,r.h,r.h},fg);g.text(label,x0+span+9,r.y+(r.h-15.6f)*.5f,13,fg,true);}
    return clicked&&enabled;
}
bool Ui::toggle(R r,const std::string&k,bool&value,bool enabled,const std::string&tip){
    auto&g=gfx();auto key=id(k+"#tg");bool hover,held;bool clicked=interact(key,r,enabled,hover,held,tip);
    float tw=40,th=22;R track{r.right()-tw,r.cy()-th*.5f,tw,th};
    Color back=!enabled?col::panel3:(value?col::accent:rgba(0x39465a));if(hover&&enabled)back=value?col::accentHi:rgba(0x47566c);
    g.fill(track,back,th*.5f);float kx=value?track.right()-th*.5f:track.x+th*.5f;g.circle(kx,track.cy(),th*.5f-3,enabled?rgba(0xf4f8fb):col::faint);
    if(clicked&&enabled){value=!value;return true;}
    return false;
}
bool Ui::segmented(R r,const std::string&k,const std::vector<std::string>&labels,int&sel,bool enabled,const std::vector<std::string>&tips){
    auto&g=gfx();if(labels.empty())return false;
    g.fill(r,col::field,7);g.stroke(r,col::line,1,7);
    float w=r.w/labels.size();bool changed=false;
    for(size_t i=0;i<labels.size();++i){
        R cell{r.x+w*i,r.y,w,r.h};R in2=cell.inset(2);auto key=id(k+std::to_string(i)+"#seg");bool hover,held;
        bool click=interact(key,in2,enabled,hover,held,i<tips.size()?tips[i]:std::string{});
        bool on=int(i)==sel;
        Color fill=on?(enabled?col::accent:col::lineStrong):(hover?col::panel3:alpha(col::panel3,0));Color fg=on?rgba(0x04191d):(enabled?(hover?col::text:col::dim):col::faint);
        g.fill(in2,fill,5);g.textCenter(g.fit(labels[i],in2.w-6,13,on),in2,13,fg,on);
        if(click&&enabled&&!on){sel=int(i);changed=true;}
    }
    return changed;
}
bool Ui::field(R r,const std::string&key,std::string&value,const FieldOpt&o){
    auto&g=gfx();auto fid=id(key+"#fld");order_.push_back(fid);
    bool hover=o.enabled&&hovered(r);bool changed=false;
    if(hover){setCursor(MOUSE_CURSOR_IBEAM);markHover(fid,"",r);}
    auto cps=codepointsOf(value);
    if(focus_==fid&&edit_.id!=fid){edit_={fid,value,int(cps.size()),true};}
    if(hover&&in.pressed&&!fieldClicked_){
        fieldClicked_=true;
        if(focus_!=fid){focus_=fid;edit_={fid,value,int(cps.size()),true};}
        else{ // place caret
            float tx=in.mx-(r.x+8);int best=int(cps.size());
            for(int i=0;i<=int(cps.size());++i){if(g.textWidth(encodeUtf8(cps,0,i),14)>=tx){best=i;break;}}
            edit_.cursor=best;edit_.selectAll=false;
        }
    }
    bool editing=focus_==fid&&o.enabled;
    if(editing&&in.focused){
        int cur=std::clamp(edit_.cursor,0,int(cps.size()));
        auto allowed=[&](int c){if(c<32)return false;if(o.integer)return c>='0'&&c<='9';if(o.numeric)return (c>='0'&&c<='9')||c=='+'||c=='-'||c=='.'||c=='e'||c=='E';return true;};
        auto insert=[&](int c){if(!allowed(c)||cps.size()>=120)return;if(edit_.selectAll){cps.clear();cur=0;edit_.selectAll=false;}cps.insert(cps.begin()+cur,c);++cur;changed=true;};
        for(int c:chars_)insert(c);
        auto rep=[&](int k){return IsKeyPressed(k)||IsKeyPressedRepeat(k);};
        if(rep(KEY_BACKSPACE)){if(edit_.selectAll){cps.clear();cur=0;edit_.selectAll=false;changed=true;}else if(cur>0){cps.erase(cps.begin()+cur-1);--cur;changed=true;}}
        if(rep(KEY_DELETE)){if(edit_.selectAll){cps.clear();cur=0;edit_.selectAll=false;changed=true;}else if(cur<int(cps.size())){cps.erase(cps.begin()+cur);changed=true;}}
        if(rep(KEY_LEFT)){edit_.selectAll=false;cur=std::max(0,cur-1);}
        if(rep(KEY_RIGHT)){edit_.selectAll=false;cur=std::min(int(cps.size()),cur+1);}
        if(IsKeyPressed(KEY_HOME)){edit_.selectAll=false;cur=0;}
        if(IsKeyPressed(KEY_END)){edit_.selectAll=false;cur=int(cps.size());}
        if(in.ctrl&&IsKeyPressed(KEY_A))edit_.selectAll=true;
        if(in.ctrl&&IsKeyPressed(KEY_C))SetClipboardText(value.c_str());
        if(in.ctrl&&IsKeyPressed(KEY_X)){SetClipboardText(value.c_str());cps.clear();cur=0;changed=true;edit_.selectAll=false;}
        if(in.ctrl&&IsKeyPressed(KEY_V)){const char*clip=GetClipboardText();if(clip){for(int c:codepointsOf(clip)){if(c=='\n'||c=='\r')break;insert(c);}}}
        edit_.cursor=cur;
        if(changed)value=encodeUtf8(cps,0,cps.size());
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_ENTER))blur();
        else if(IsKeyPressed(KEY_ESCAPE)){if(value!=edit_.original){value=edit_.original;changed=true;}blur();}
        else if(IsKeyPressed(KEY_TAB)){
            auto&ord=prevOrder_;auto it=std::find(ord.begin(),ord.end(),fid);
            if(it!=ord.end()&&ord.size()>1){size_t i=size_t(it-ord.begin());i=in.shift?(i+ord.size()-1)%ord.size():(i+1)%ord.size();focus_=ord[i];edit_={};}
        }
        cps=codepointsOf(value);
    }
    editing=focus_==fid&&o.enabled;
    Color border=!o.error.empty()?col::danger:(editing?col::accent:(hover?col::lineStrong:col::line));
    g.fill(r,o.enabled?col::field:col::panel2,5);g.stroke(r,border,editing||!o.error.empty()?1.5f:1,5);
    if(o.edited&&o.error.empty())g.fill({r.x+1,r.y+5,3,r.h-10},col::amber,1.5f);
    float padL=10,padR=8;float unitW=o.unit.empty()?0:g.textWidth(o.unit,12)+6;
    R textR{r.x+padL,r.y,std::max(1.f,r.w-padL-padR-unitW),r.h};
    if(!o.unit.empty())g.textRight(o.unit,r.right()-padR,r.y+(r.h-14.4f)*.5f,12,col::dim);
    g.pushClip(textR.inset(0,1,0,1));
    Color fg=o.enabled?col::text:col::faint;
    if(value.empty()&&!editing&&!o.placeholder.empty())g.textVCenter(o.placeholder,textR,14,col::faint);
    else{
        float tw=g.textWidth(value,14);float shift=0;
        if(editing){float cx=g.textWidth(encodeUtf8(cps,0,std::clamp(edit_.cursor,0,int(cps.size()))),14);if(cx>textR.w-6)shift=cx-(textR.w-6);}
        float x0=o.center?textR.x+(textR.w-tw)*.5f:textR.x-shift;
        if(editing&&edit_.selectAll&&!value.empty())g.fill({x0-1,r.y+5,tw+2,r.h-10},alpha(col::accent,.35f),2);
        g.textVCenter(value,{x0,r.y,textR.w+shift,r.h},14,fg);
        if(editing&&!edit_.selectAll&&std::fmod(GetTime(),1.0)<.62){float cx=x0+g.textWidth(encodeUtf8(cps,0,std::clamp(edit_.cursor,0,int(cps.size()))),14);g.fill({cx,r.y+6,1.5f,r.h-12},col::accentHi);}
    }
    g.popClip();
    return changed;
}
bool Ui::slider(R r,const std::string&key,double&v,double lo,double hi,bool enabled){
    auto&g=gfx();auto sid=id(key+"#sl");bool hover,held;interact(sid,r,enabled,hover,held,"");
    R track{r.x+6,r.cy()-2,r.w-12,4};g.fill(track,col::field,2);double f=hi>lo?std::clamp((v-lo)/(hi-lo),0.,1.):0;
    g.fill({track.x,track.y,track.w*float(f),track.h},enabled?col::accent:col::lineStrong,2);
    float kx=track.x+track.w*float(f);g.circle(kx,r.cy(),hover||held?7.5f:6.5f,enabled?rgba(0xf4f8fb):col::faint);
    bool changed=false;
    if(active_==sid&&in.down){double nv=lo+(hi-lo)*std::clamp((in.mx-track.x)/track.w,0.f,1.f);if(nv!=v){v=nv;changed=true;}}
    return changed;
}
bool Ui::header(R r,const std::string&label,bool&open,const std::string&badge,Color badgeColor){
    auto&g=gfx();auto hid=id(label+"#hd");bool hover,held;bool clicked=interact(hid,r,true,hover,held,"");
    g.fill(r,hover?col::panel3:col::panel2,6);
    drawIcon(open?Icon::chevronDown:Icon::chevronRight,{r.x+4,r.y,22,r.h},hover?col::text:col::dim);
    float right=r.right()-10;
    if(!badge.empty()){float bw=g.textWidth(badge,11,true)+14;R b{right-bw,r.cy()-9,bw,18};g.fill(b,alpha(badgeColor,.18f),9);g.textCenter(badge,b,11,badgeColor,true);right-=bw+6;}
    g.text(g.fit(label,right-r.x-30,14,true),r.x+28,r.y+(r.h-16.8f)*.5f,14,col::text,true);
    if(clicked)open=!open;
    return clicked;
}
void Ui::progress(R r,double f,Color fill,Color back){auto&g=gfx();g.fill(r,back,r.h*.5f);f=std::clamp(f,0.,1.);if(f>0)g.fill({r.x,r.y,std::max(r.h,float(r.w*f)),r.h},fill,r.h*.5f);}
void Ui::chip(R r,const std::string&text,Color fg,Color bg,int size,bool dot){
    auto&g=gfx();g.fill(r,bg,r.h*.5f);float x=r.x;if(dot){g.circle(r.x+r.h*.5f+1,r.cy(),3.5f,fg);x+=r.h*.5f+6;}
    g.text(g.fit(text,r.right()-x-8,size,true),x+(dot?0:(r.w-g.textWidth(text,size,true))*.5f-(dot?0:0)),r.y+(r.h-size*1.2f)*.5f,size,fg,true);
}
void Ui::openMenu(R anchor,const std::string&key,const std::vector<std::string>&items){menuOpen_=true;menuId_=id(key);menuAnchor_=anchor;menuItems_=items;menuChoice_=-1;}
bool Ui::menuPicked(const std::string&key,int&choice){if(menuChoice_>=0&&menuChoiceFor_==id(key)){choice=menuChoice_;menuChoice_=-1;menuChoiceFor_=0;return true;}return false;}
void Ui::drawMenu(){
    auto&g=gfx();float w=0;for(auto&s:menuItems_)w=std::max(w,g.textWidth(s,13)+32);w=std::max(w,menuAnchor_.w);
    float rowH=32,h=rowH*menuItems_.size()+8;R box{std::min(menuAnchor_.x,winW_-w-6),menuAnchor_.bottom()+4,w,h};if(box.bottom()>winH_-4)box.y=menuAnchor_.y-h-4;
    g.fill({box.x+1,box.y+3,box.w,box.h},rgba(0,110),8);g.fill(box,rgba(0x1d2631),8);g.stroke(box,col::lineStrong,1,8);
    bool inside=box.contains(in.mx,in.my);
    for(size_t i=0;i<menuItems_.size();++i){R row{box.x+4,box.y+4+rowH*i,box.w-8,rowH};bool hv=row.contains(in.mx,in.my);if(hv){g.fill(row,col::accentLo,5);setCursor(MOUSE_CURSOR_POINTING_HAND);}
        g.textVCenter(menuItems_[i],row,13,hv?col::text:col::dim,false,10);
        if(hv&&in.pressed){menuChoice_=int(i);menuChoiceFor_=menuId_;menuOpen_=false;}}
    if(in.pressed&&!inside)menuOpen_=false;
    if(IsKeyPressed(KEY_ESCAPE))menuOpen_=false;
}
void Ui::beginScroll(R view,ScrollState&s,const std::string&key){
    pushId(key);
    if(hovered(view)&&wheelAvailable()&&in.wheel!=0){s.y-=in.wheel*54;consumeWheel();}
    gfx().pushClip(view);
}
void Ui::endScroll(R view,ScrollState&s,float contentHeight){
    auto&g=gfx();g.popClip();s.content=contentHeight;
    float maxY=std::max(0.f,contentHeight-view.h);s.y=std::clamp(s.y,0.f,maxY);
    if(maxY>0){
        float trackH=view.h-8;float thumbH=std::max(28.f,trackH*view.h/contentHeight);float thumbY=view.y+4+(trackH-thumbH)*(s.y/maxY);
        R thumb{view.right()-8,thumbY,6,thumbH};R grab{view.right()-14,thumbY,14,thumbH};
        auto sid=id("#sbar");bool hv=hovered(grab);
        if(hv&&in.pressed&&active_==0){active_=sid;s.drag=true;s.dragOffset=in.my-thumbY;}
        if(s.drag&&active_==sid&&in.down){float ny=in.my-s.dragOffset-(view.y+4);s.y=std::clamp(ny/(trackH-thumbH),0.f,1.f)*maxY;}
        else s.drag=false;
        g.fill(thumb,hv||s.drag?col::lineStrong:col::line,3);
    }
    popId();
}
}
