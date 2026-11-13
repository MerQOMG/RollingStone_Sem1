#pragma once
// Immediate-mode widget layer (buttons, fields, segmented controls, scroll views, menus).
#include "ui/gfx.h"
#include <functional>
#include <string>
#include <vector>
namespace rs3d::gui {
enum class Variant { primary, secondary, ghost, danger };
enum class Icon { play, pause, stop, toStart, toEnd, stepBack, stepFwd, folder, save, dice, reset, present, help, close, chevronDown, chevronRight, check, warning, camera, target, chart, cube, apply, undo, plus };
void drawIcon(Icon icon,R r,Color c);
struct Input {
    float mx=0,my=0,wheel=0;
    bool pressed=false,released=false,down=false,rpressed=false,rdown=false,mdown=false,focused=true,ctrl=false,shift=false;
};
struct FieldOpt {
    std::string unit,error,placeholder;
    bool edited=false,enabled=true,numeric=true,integer=false,center=false;
};
struct ScrollState {float y=0,content=0;bool drag=false;float dragOffset=0;};
class Ui {
    struct Edit {std::uint32_t id=0;std::string original;int cursor=0;bool selectAll=false;};
    std::vector<std::uint32_t> idStack_{2166136261u};
    std::uint32_t active_=0,hoverId_=0,focus_=0,lastFocus_=0;
    double hoverSince_=0;bool hoverSeen_=false;
    std::string tipText_;float tipX_=0,tipY_=0;
    Edit edit_;
    bool fieldClicked_=false,wheelUsed_=false,inOverlay_=false,blocked_=false;
    std::vector<std::uint32_t> order_,prevOrder_;
    std::vector<std::function<void()>> overlays_;
    int cursor_=MOUSE_CURSOR_DEFAULT;
    // popup menu state
    std::uint32_t menuId_=0;bool menuOpen_=false;R menuAnchor_{};std::vector<std::string> menuItems_;int menuChoice_=-1;std::uint32_t menuChoiceFor_=0;
    float winW_=0,winH_=0;bool menuBlock_=false;std::vector<int> chars_;
    bool hoverRaw(R r)const;
    void markHover(std::uint32_t id,const std::string&tip,R r);
    bool interact(std::uint32_t id,R r,bool enabled,bool&hover,bool&held,const std::string&tip);
    void drawMenu();
public:
    std::function<void(std::uint32_t,R,int)> trace; // optional input probe: kind 0 seen, 1 press, 2 click
    Input in;
    void begin(float logicalW,float logicalH);
    void end();
    std::uint32_t id(const std::string&label)const;
    void pushId(const std::string&s){idStack_.push_back(id(s));}
    void popId(){if(idStack_.size()>1)idStack_.pop_back();}
    void setBlocked(bool b){blocked_=b;}
    bool hovered(R r)const{return !blocked_&&hoverRaw(r);}
    bool typing()const{return focus_!=0;}
    void blur(){focus_=0;edit_={};}
    bool wheelAvailable()const{return !wheelUsed_;}
    void consumeWheel(){wheelUsed_=true;}
    void setCursor(int c){cursor_=c;}
    bool anyPopup()const{return menuOpen_;}
    void overlay(std::function<void()> f){overlays_.push_back(std::move(f));}
    // widgets
    bool button(R r,const std::string&label,Variant v=Variant::secondary,bool enabled=true,const std::string&tip={});
    bool iconButton(R r,Icon icon,bool enabled=true,const std::string&tip={},bool active=false,const std::string&label={},Variant v=Variant::ghost);
    bool toggle(R r,const std::string&key,bool&value,bool enabled=true,const std::string&tip={});
    bool segmented(R r,const std::string&key,const std::vector<std::string>&labels,int&selected,bool enabled=true,const std::vector<std::string>&tips={});
    bool field(R r,const std::string&key,std::string&value,const FieldOpt&opt={});
    bool slider(R r,const std::string&key,double&value,double lo,double hi,bool enabled=true);
    bool header(R r,const std::string&label,bool&open,const std::string&badge={},Color badgeColor=col::amber);
    void progress(R r,double fraction,Color fill=col::accent,Color back=col::field);
    void chip(R r,const std::string&text,Color fg,Color bg,int size=12,bool dot=false);
    void openMenu(R anchor,const std::string&key,const std::vector<std::string>&items);
    bool menuPicked(const std::string&key,int&choice);
    void beginScroll(R view,ScrollState&s,const std::string&key);
    void endScroll(R view,ScrollState&s,float contentHeight);
    void tip(R r,const std::string&text){if(hovered(r))markHover(id("tip#"+text),text,r);}
    bool keyboardActive()const{return focus_!=0;}
    bool pointerFree()const{return !active_;}
};
}
