#pragma once
// 2D drawing layer of the new RollingStone3D front end: logical pixels in, device pixels out.
#include <raylib.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace rs3d::gui {
struct R { // logical-pixel rectangle
    float x=0,y=0,w=0,h=0;
    float right()const{return x+w;}
    float bottom()const{return y+h;}
    float cx()const{return x+w*.5f;}
    float cy()const{return y+h*.5f;}
    bool contains(float px,float py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}
    R inset(float d)const{return {x+d,y+d,std::max(0.f,w-2*d),std::max(0.f,h-2*d)};}
    R inset(float l,float t,float r,float b)const{return {x+l,y+t,std::max(0.f,w-l-r),std::max(0.f,h-t-b)};}
    R cutLeft(float d){d=std::min(d,w);R out{x,y,d,h};x+=d;w-=d;return out;}
    R cutRight(float d){d=std::min(d,w);R out{x+w-d,y,d,h};w-=d;return out;}
    R cutTop(float d){d=std::min(d,h);R out{x,y,w,d};y+=d;h-=d;return out;}
    R cutBottom(float d){d=std::min(d,h);R out{x,y+h-d,w,d};h-=d;return out;}
    R intersect(const R&o)const{float l=std::max(x,o.x),t=std::max(y,o.y),r=std::min(right(),o.right()),b=std::min(bottom(),o.bottom());return {l,t,std::max(0.f,r-l),std::max(0.f,b-t)};}
};
inline Color rgba(unsigned hex,unsigned char a=255){return {(unsigned char)(hex>>16),(unsigned char)(hex>>8),(unsigned char)hex,a};}
inline Color alpha(Color c,float a){c.a=(unsigned char)std::clamp(a*255.f,0.f,255.f);return c;}
inline Color mix(Color a,Color b,float t){t=std::clamp(t,0.f,1.f);auto f=[&](unsigned char p,unsigned char q){return (unsigned char)(p+(q-p)*t+.5f);};return {f(a.r,b.r),f(a.g,b.g),f(a.b,b.b),f(a.a,b.a)};}
// Visual identity: calm slate surfaces, one teal accent, amber reserved for "pending / the rock".
namespace col {
inline const Color bgTop=rgba(0x0b1016),bgHorizon=rgba(0x1c2733),panel=rgba(0x131921),panel2=rgba(0x19212b),panel3=rgba(0x212b37),field=rgba(0x0d1218);
inline const Color line=rgba(0x2a3441),lineStrong=rgba(0x3b4959),text=rgba(0xe8edf3),dim=rgba(0xa3afbd),faint=rgba(0x6f7c8a);
inline const Color accent=rgba(0x3cb7cb),accentHi=rgba(0x66d3e4),accentLo=rgba(0x14464f),amber=rgba(0xf2b64c),amberLo=rgba(0x4a3910);
inline const Color ok=rgba(0x4cc38a),warn=rgba(0xf2b64c),danger=rgba(0xf06464),info=rgba(0x78aefc),dangerLo=rgba(0x4a1c1f),okLo=rgba(0x143826);
}
class Gfx {
    struct Slot {Font font{};std::set<int> have;bool loaded=false;};
    std::map<std::pair<int,bool>,Slot> slots_;
    std::set<int> codepoints_;
    std::vector<unsigned char> regular_,bold_;
    std::vector<R> clips_;
    bool dirty_=false;
    float scale_=1;
    void loadSlot(int size,bool bold,Slot&slot);
public:
    bool init(const std::filesystem::path&regular,const std::filesystem::path&bold);
    void shutdown();
    float scale()const{return scale_;}
    void setScale(float s);
    float px(float v)const{return v*scale_;}
    // Character coverage grows on demand; call once per frame after drawing.
    void flushFonts();
    bool fontsDirty()const{return dirty_;}
    const Font&font(int size,bool bold);
    float textWidth(const std::string&s,int size,bool bold=false);
    float lineHeight(int size)const{return size*1.35f;}
    void text(const std::string&s,float x,float y,int size,Color c,bool bold=false);
    void textRight(const std::string&s,float right,float y,int size,Color c,bool bold=false);
    void textCenter(const std::string&s,R r,int size,Color c,bool bold=false);
    void textVCenter(const std::string&s,R r,int size,Color c,bool bold=false,float padLeft=0);
    std::string fit(const std::string&s,float maxWidth,int size,bool bold=false);
    // Greedy wrapping, CJK breaks between any two characters. Returns the height used.
    float wrapped(const std::string&s,R r,int size,Color c,bool bold=false,bool draw=true);
    void fill(R r,Color c,float radius=0);
    void stroke(R r,Color c,float thickness=1,float radius=0);
    void line(float x0,float y0,float x1,float y1,Color c,float thickness=1);
    void circle(float x,float y,float radius,Color c);
    void ring(float x,float y,float radius,float thickness,Color c);
    void tri(float x0,float y0,float x1,float y1,float x2,float y2,Color c);
    void gradientV(R r,Color top,Color bottom);
    void pushClip(R r);
    void popClip();
    R clip()const{return clips_.empty()?R{0,0,1e9f,1e9f}:clips_.back();}
    void applyClip();
};
Gfx&gfx();
// UTF-8 helpers shared by the text layer and the editors.
int decodeUtf8(const char*s,int&consumed);
std::vector<int> codepointsOf(const std::string&s);
std::string encodeUtf8(const std::vector<int>&cps,size_t begin,size_t end);
}
