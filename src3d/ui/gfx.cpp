#include "ui/gfx.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace rs3d::gui {
namespace {
std::vector<unsigned char> readAll(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);if(!in)return {};return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),{});}
bool isCjk(int c){return c>=0x2E80;}
}
Gfx&gfx(){static Gfx g;return g;}
int decodeUtf8(const char*s,int&n){
    unsigned char c=(unsigned char)s[0];
    if(c<0x80){n=1;return c;}
    if((c>>5)==6&&(s[1]&0xC0)==0x80){n=2;return ((c&31)<<6)|(s[1]&63);}
    if((c>>4)==14&&(s[1]&0xC0)==0x80&&(s[2]&0xC0)==0x80){n=3;return ((c&15)<<12)|((s[1]&63)<<6)|(s[2]&63);}
    if((c>>3)==30&&(s[1]&0xC0)==0x80&&(s[2]&0xC0)==0x80&&(s[3]&0xC0)==0x80){n=4;return ((c&7)<<18)|((s[1]&63)<<12)|((s[2]&63)<<6)|(s[3]&63);}
    n=1;return '?';
}
std::vector<int> codepointsOf(const std::string&s){std::vector<int> out;for(size_t i=0;i<s.size();){int n=1;out.push_back(decodeUtf8(s.c_str()+i,n));i+=n;}return out;}
std::string encodeUtf8(const std::vector<int>&cps,size_t a,size_t b){
    std::string o;for(size_t k=a;k<b&&k<cps.size();++k){int cp=cps[k];
        if(cp<0x80)o+=char(cp);
        else if(cp<0x800){o+=char(0xC0|(cp>>6));o+=char(0x80|(cp&63));}
        else if(cp<0x10000){o+=char(0xE0|(cp>>12));o+=char(0x80|((cp>>6)&63));o+=char(0x80|(cp&63));}
        else{o+=char(0xF0|(cp>>18));o+=char(0x80|((cp>>12)&63));o+=char(0x80|((cp>>6)&63));o+=char(0x80|(cp&63));}}
    return o;
}
bool Gfx::init(const std::filesystem::path&regular,const std::filesystem::path&bold){
    regular_=readAll(regular);bold_=readAll(bold);
    if(regular_.empty())return false;
    if(bold_.empty())bold_=regular_;
    for(int c=32;c<127;++c)codepoints_.insert(c);
    for(int c:{0xB0,0xB7,0xD7,0x3BC,0x3C9,0x3C3,0x2192,0x2190,0x2191,0x2193,0x2026,0x2014,0x2013,0x3001,0x3002,0xFF0C,0xFF1A,0xFF08,0xFF09,0xFF1B,0xFF01,0xFF1F,0x201C,0x201D,0x2022,0x25CF,0x2248,0x2264,0x2265,0xB1})codepoints_.insert(c);
    return true;
}
void Gfx::shutdown(){for(auto&s:slots_)if(s.second.loaded)UnloadFont(s.second.font);slots_.clear();}
void Gfx::setScale(float s){
    s=std::clamp(s,.5f,4.f);if(std::abs(s-scale_)<1e-4f)return;scale_=s;
    for(auto&e:slots_)if(e.second.loaded){UnloadFont(e.second.font);e.second.loaded=false;}
}
void Gfx::loadSlot(int size,bool bold,Slot&slot){
    int dev=std::max(6,int(std::lround(size*scale_)));
    std::vector<int> cps(codepoints_.begin(),codepoints_.end());
    auto&bytes=bold?bold_:regular_;
    slot.font=LoadFontFromMemory(".ttf",bytes.data(),int(bytes.size()),dev,cps.data(),int(cps.size()));
    if(slot.font.texture.id==0)throw std::runtime_error("interface font failed to load");
    SetTextureFilter(slot.font.texture,TEXTURE_FILTER_BILINEAR);
    slot.have=codepoints_;slot.loaded=true;
}
const Font&Gfx::font(int size,bool bold){
    auto&slot=slots_[{size,bold}];if(!slot.loaded)loadSlot(size,bold,slot);return slot.font;
}
void Gfx::flushFonts(){
    if(!dirty_)return;dirty_=false;
    for(auto&e:slots_)if(e.second.loaded&&e.second.have.size()!=codepoints_.size()){UnloadFont(e.second.font);e.second.loaded=false;}
}
static void ensure(std::set<int>&all,bool&dirty,const std::string&s){
    for(size_t i=0;i<s.size();){unsigned char c=(unsigned char)s[i];if(c<0x80){++i;continue;}int n=1;int cp=decodeUtf8(s.c_str()+i,n);i+=n;if(all.insert(cp).second)dirty=true;}
}
float Gfx::textWidth(const std::string&s,int size,bool bold){
    ensure(codepoints_,dirty_,s);auto&f=font(size,bold);
    return MeasureTextEx(f,s.c_str(),float(f.baseSize),0).x/scale_;
}
void Gfx::text(const std::string&s,float x,float y,int size,Color c,bool bold){
    if(s.empty())return;
    ensure(codepoints_,dirty_,s);auto&f=font(size,bold);
    DrawTextEx(f,s.c_str(),{std::floor(x*scale_),std::floor(y*scale_)},float(f.baseSize),0,c);
}
void Gfx::textRight(const std::string&s,float right,float y,int size,Color c,bool bold){text(s,right-textWidth(s,size,bold),y,size,c,bold);}
void Gfx::textCenter(const std::string&s,R r,int size,Color c,bool bold){
    float w=textWidth(s,size,bold);text(s,r.x+(r.w-w)*.5f,r.y+(r.h-size*1.2f)*.5f,size,c,bold);
}
void Gfx::textVCenter(const std::string&s,R r,int size,Color c,bool bold,float padLeft){text(s,r.x+padLeft,r.y+(r.h-size*1.2f)*.5f,size,c,bold);}
std::string Gfx::fit(const std::string&s,float maxWidth,int size,bool bold){
    if(textWidth(s,size,bold)<=maxWidth)return s;
    auto cps=codepointsOf(s);
    for(size_t keep=cps.size();keep-->0;){
        auto out=encodeUtf8(cps,0,keep)+u8"…";
        if(textWidth(out,size,bold)<=maxWidth)return out;
    }
    return u8"…";
}
float Gfx::wrapped(const std::string&s,R r,int size,Color c,bool bold,bool draw){
    float lh=lineHeight(size),y=r.y;std::string line;
    auto cps=codepointsOf(s);size_t i=0;
    auto flush=[&](){if(draw&&!line.empty())text(line,r.x,y,size,c,bold);y+=lh;line.clear();};
    while(i<cps.size()){
        if(cps[i]=='\n'){flush();++i;continue;}
        size_t j=i+1;
        if(!isCjk(cps[i])&&cps[i]!=' ')while(j<cps.size()&&!isCjk(cps[j])&&cps[j]!=' '&&cps[j]!='\n')++j;
        auto unit=encodeUtf8(cps,i,j);
        if(!line.empty()&&textWidth(line+unit,size,bold)>r.w){flush();if(unit==" "){i=j;continue;}}
        line+=unit;i=j;
    }
    if(!line.empty())flush();
    return y-r.y;
}
void Gfx::applyClip(){
    EndScissorMode();
    if(!clips_.empty()){auto c=clips_.back();BeginScissorMode(int(std::floor(c.x*scale_)),int(std::floor(c.y*scale_)),std::max(0,int(std::ceil(c.w*scale_))),std::max(0,int(std::ceil(c.h*scale_))));}
}
void Gfx::pushClip(R r){
    if(!clips_.empty())r=r.intersect(clips_.back());
    clips_.push_back(r);applyClip();
}
void Gfx::popClip(){if(!clips_.empty())clips_.pop_back();applyClip();}
static Rectangle dev(R r,float s){return {std::floor(r.x*s),std::floor(r.y*s),std::max(0.f,std::ceil(r.w*s)),std::max(0.f,std::ceil(r.h*s))};}
void Gfx::fill(R r,Color c,float radius){
    if(c.a==0||r.w<=0||r.h<=0)return;
    auto d=dev(r,scale_);
    if(radius<=0.5f){DrawRectangleRec(d,c);return;}
    float round=std::clamp(radius*scale_/(std::min(d.width,d.height)*.5f),0.f,1.f);DrawRectangleRounded(d,round,8,c);
}
void Gfx::stroke(R r,Color c,float thickness,float radius){
    if(c.a==0||r.w<=0||r.h<=0)return;
    auto d=dev(r,scale_);float t=std::max(1.f,std::round(thickness*scale_));
    if(radius<=0.5f){DrawRectangleLinesEx(d,t,c);return;}
    float round=std::clamp(radius*scale_/(std::min(d.width,d.height)*.5f),0.f,1.f);DrawRectangleRoundedLinesEx(d,round,8,t,c);
}
void Gfx::line(float x0,float y0,float x1,float y1,Color c,float thickness){DrawLineEx({x0*scale_,y0*scale_},{x1*scale_,y1*scale_},std::max(1.f,thickness*scale_),c);}
void Gfx::circle(float x,float y,float radius,Color c){DrawCircleV({x*scale_,y*scale_},radius*scale_,c);}
void Gfx::ring(float x,float y,float radius,float thickness,Color c){DrawRing({x*scale_,y*scale_},std::max(0.f,(radius-thickness)*scale_),radius*scale_,0,360,36,c);}
void Gfx::tri(float x0,float y0,float x1,float y1,float x2,float y2,Color c){
    float cross=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);
    if(cross>0)DrawTriangle({x0*scale_,y0*scale_},{x2*scale_,y2*scale_},{x1*scale_,y1*scale_},c);
    else DrawTriangle({x0*scale_,y0*scale_},{x1*scale_,y1*scale_},{x2*scale_,y2*scale_},c);
}
void Gfx::gradientV(R r,Color top,Color bottom){auto d=dev(r,scale_);DrawRectangleGradientV(int(d.x),int(d.y),int(d.width),int(d.height),top,bottom);}
}
