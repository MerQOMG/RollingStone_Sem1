#include "ui/viz.h"
#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
namespace rs3d::gui::viz {
namespace {
const Vector3 kLight=Vector3Normalize({.55f,-.35f,.75f}); // towards the sun; x is downslope, z is up
const Vector3 kFog{.105f,.185f,.23f};
const char*kTerrainVs=R"(#version 330
in vec3 vertexPosition;in vec3 vertexNormal;in vec4 vertexColor;
uniform mat4 mvp;uniform mat4 matModel;
out vec3 wPos;out vec3 wN;out float ao;
void main(){vec4 w=matModel*vec4(vertexPosition,1.0);wPos=w.xyz;wN=normalize(vertexNormal);ao=vertexColor.a;gl_Position=mvp*vec4(vertexPosition,1.0);}
)";
const char*kTerrainFs=R"(#version 330
in vec3 wPos;in vec3 wN;in float ao;
uniform vec3 viewPos;uniform vec3 fogColor;uniform vec2 fogRange;uniform vec2 hRange;uniform vec3 lightDir;uniform vec4 rock;uniform float contourOn;
uniform sampler2D shadowMap;uniform mat4 lightVP;uniform vec4 shadowP; // x on, y texel size in metres, z depth bias, w filter radius in uv
uniform sampler2D rockD;uniform sampler2D rockN;uniform sampler2D rockR;uniform sampler2D grdD;uniform sampler2D grdN;uniform sampler2D grdR;
uniform float texMode;                 // 0 procedural, 1 photo-scan textures
uniform vec4 texA;                     // rock tiles per metre, ground tiles per metre, rock brightness, ground brightness
uniform vec4 texB;                     // rock saturation, ground saturation, height-tint mix, specular
uniform vec3 texTint;
out vec4 finalColor;
float h12(vec2 p){vec3 q=fract(vec3(p.xyx)*.1031);q+=dot(q,q.yzx+33.33);return fract((q.x+q.y)*q.z);}
vec2 h22(vec2 p){vec3 q=fract(vec3(p.xyx)*vec3(.1031,.1030,.0973));q+=dot(q,q.yzx+33.33);return fract((q.xx+q.yz)*q.zy);}
// value noise with analytic derivatives: (value, d/dx, d/dy)
vec3 vn(vec2 p){
 vec2 i=floor(p),f=fract(p);
 vec2 u=f*f*f*(f*(f*6.-15.)+10.),du=30.*f*f*(f*(f-2.)+1.);
 float a=h12(i),b=h12(i+vec2(1,0)),c=h12(i+vec2(0,1)),d=h12(i+vec2(1,1));
 float k1=b-a,k2=c-a,k4=a-b-c+d;
 return vec3(a+k1*u.x+k2*u.y+k4*u.x*u.y,du.x*(k1+k4*u.y),du.y*(k2+k4*u.x));}
// one projection plane: multi-scale fractal noise, erosion rills and joint cracks
struct Pl{float lo,mid,grain,crack,rill;vec2 gT,gC,gR;};
float vis(float px,float freq){return 1.-smoothstep(.10,.28,px*freq);} // an octave fades out when it nears the pixel size
Pl plane(vec2 p,float px,vec2 st)
{
 Pl o;o.gT=vec2(0.);o.gC=vec2(0.);o.gR=vec2(0.);o.rill=0.;
 vec3 n0=vn(p*.07+vec2(3.1,9.7));o.lo=n0.x;
 vec3 n1=vn(p*.31+vec2(17.,5.));float v1=vis(px,.31);
 vec3 n2=vn(p*.83+vec2(41.,23.));float v2=vis(px,.83);
 vec3 n3=vn(p*2.9+vec2(7.,61.));float v3=vis(px,2.9);
 vec3 n4=vn(p*8.3+vec2(29.,13.));float v4=vis(px,8.3);
 o.mid=mix(.5,n1.x*.6+n2.x*.4,v1);o.grain=mix(.5,n3.x*.55+n4.x*.45,v3);
 o.gT=.08*n0.yz+.24*n1.yz*v1+.26*n2.yz*v2+.30*n3.yz*v3+.22*n4.yz*v4;
 // erosion rills: soft ridges stretched along the fall line, widened with the pixel size so they never alias
 float fm=max(st.x,st.y),vr=vis(px,fm*.8);
 if(vr>.01)
 {
  vec3 r1=vn(p*st+vec2(5.5,1.7));float d=abs(r1.x-.5)*2.;float w=max(.16,2.2*px*fm);
  float k=(1.-smoothstep(0.,w,d))*smoothstep(.45,.66,vn(p*vec2(.11,.17)+vec2(2.2,8.8)).x);o.rill=k*min(1.,.16/w)*vr;
  o.gR=-sign(r1.x-.5)*r1.yz*st*.30*k*vr;
 }
 // joints: thin ridges of a stretched noise
 vec3 c1=vn(p*vec2(.42,1.1)+vec2(11.,3.));float rid=1.-abs(2.*c1.x-1.);
 o.crack=smoothstep(.945,.992,rid)*vis(px,1.2);
 o.gC=-(2.*c1.x-1.>0.?1.:-1.)*c1.yz*vec2(.42,1.1)*.55*smoothstep(.85,.992,rid)*vis(px,1.2);
 return o;
}
float lineAt(float z,float step,float fw){
 float d=abs(fract(z/step+.5)-.5)*step;
 float l=1.-smoothstep(0.,fw*1.25,d);
 l*=1.-smoothstep(.18,.45,fw/step); // fade when lines crowd together
 return fw<.0015?0.:l;}
float shadowAt(vec3 p,vec3 N)
{
 if(shadowP.x<.5)return 1.;
 float nl=clamp(dot(N,lightDir),0.,1.);
 vec3 q=p+N*shadowP.y*(1.4+2.*(1.-nl));
 vec4 lp=lightVP*vec4(q,1.);vec3 c=lp.xyz/lp.w*.5+.5;
 if(c.x<0.||c.x>1.||c.y<0.||c.y>1.||c.z>1.)return 1.;
 float ref=c.z-shadowP.z;
 float a=h12(gl_FragCoord.xy)*6.2831853;vec2 cs=vec2(cos(a),sin(a));mat2 R=mat2(cs.x,cs.y,-cs.y,cs.x);
 const vec2 pd[12]=vec2[](vec2(-.326,-.406),vec2(-.840,-.074),vec2(-.696,.457),vec2(-.203,.621),vec2(.962,-.195),vec2(.473,-.480),vec2(.519,.767),vec2(.185,-.893),vec2(.507,.064),vec2(.896,.412),vec2(-.322,-.933),vec2(-.792,-.598));
 float s=0.;
 for(int i=0;i<12;++i)s+=step(ref,texture(shadowMap,c.xy+R*pd[i]*shadowP.w).r);
 return s/12.;
}
// photo textures: triplanar, stochastic offsets (two taps blended by a low-frequency index) to hide the repeat
vec3 tRnd(float i){return fract(sin(vec3(i*127.1+1.7,i*311.7+9.3,i*74.7+3.1))*43758.5453);}
mat2 tRot(float a){float c=cos(a),s=sin(a);return mat2(c,s,-s,c);}
void photo(sampler2D D,sampler2D Nm,sampler2D Rm,float scale,float nk,vec3 N,vec3 aw,float lowMix,out vec3 col,out vec3 wn,out vec2 ra)
{
 col=vec3(0.);wn=vec3(0.);ra=vec2(0.);
 for(int k=0;k<3;++k)
 {
  float w=k==0?aw.z:(k==1?aw.x:aw.y);if(w<.02)continue;
  vec2 uv=k==0?wPos.xy:(k==1?wPos.zy:wPos.xz);uv*=scale;
  vec2 dx=dFdx(uv),dy=dFdy(uv);
  float q=vn(uv*.23+vec2(5.3,1.9)).x*7.;
  float i=floor(q),wb=smoothstep(.25,.75,fract(q));
  vec3 ra0=tRnd(i),rb0=tRnd(i+1.);
  mat2 Ra=tRot(ra0.z*6.2832),Rb=tRot(rb0.z*6.2832);
  vec2 ua=Ra*uv+ra0.xy,ub=Rb*uv+rb0.xy;
  col+=w*mix(textureGrad(D,ua,Ra*dx,Ra*dy).rgb,textureGrad(D,ub,Rb*dx,Rb*dy).rgb,wb);
  vec3 na=textureGrad(Nm,ua,Ra*dx,Ra*dy).xyz*2.-1.,nb=textureGrad(Nm,ub,Rb*dx,Rb*dy).xyz*2.-1.;
  na.xy=transpose(Ra)*na.xy;nb.xy=transpose(Rb)*nb.xy;       // rotate the tangent normals back into the world frame
  vec3 n=mix(na,nb,wb);n.xy*=nk;n.z=max(n.z,.05);
  ra+=w*mix(textureGrad(Rm,ua,Ra*dx,Ra*dy).rg,textureGrad(Rm,ub,Rb*dx,Rb*dy).rg,wb);
  vec3 t;
  if(k==0){t=vec3(n.xy+N.xy,abs(n.z)*N.z);wn+=w*t.xyz;}
  else if(k==1){t=vec3(n.xy+N.zy,abs(n.z)*N.x);wn+=w*t.zyx;}
  else{t=vec3(n.xy+N.xz,abs(n.z)*N.y);wn+=w*t.xzy;}
 }
}
vec3 grade(vec3 c,float bright,float sat){float l=dot(c,vec3(.299,.587,.114));return mix(vec3(l),c,sat)*bright*texTint;}
void main(){
 vec3 N=normalize(wN);
 float px=max(length(dFdx(wPos)),length(dFdy(wPos)))+1e-4;
 vec3 aw=pow(abs(N),vec3(6.));aw/=aw.x+aw.y+aw.z;
 float lo=0.,mid=0.,grain=0.,crack=0.,rill=0.;vec3 Gt=vec3(0.),Gc=vec3(0.),Gr=vec3(0.);
 if(texMode>.5)
 {
  lo=vn(wPos.xy*.07+vec2(3.1,9.7)).x;mid=vn(wPos.xy*.31+vec2(17.,5.)).x;grain=.5;
 }
 else
 {
  if(aw.z>.02){Pl o=plane(wPos.xy,px,vec2(.12,1.3));float w=aw.z;lo+=w*o.lo;mid+=w*o.mid;grain+=w*o.grain;crack+=w*o.crack;rill+=w*o.rill;Gt+=w*vec3(o.gT,0.);Gc+=w*vec3(o.gC,0.);Gr+=w*vec3(o.gR,0.);}
  if(aw.x>.02){Pl o=plane(wPos.yz,px,vec2(1.3,.12));float w=aw.x;lo+=w*o.lo;mid+=w*o.mid;grain+=w*o.grain;crack+=w*o.crack;rill+=w*o.rill;Gt+=w*vec3(0.,o.gT);Gc+=w*vec3(0.,o.gC);Gr+=w*vec3(0.,o.gR);}
  if(aw.y>.02){Pl o=plane(wPos.xz,px,vec2(.2,.2));float w=aw.y;lo+=w*o.lo;mid+=w*o.mid;grain+=w*o.grain;crack+=w*o.crack;rill+=w*o.rill;Gt+=w*vec3(o.gT.x,0.,o.gT.y);Gc+=w*vec3(o.gC.x,0.,o.gC.y);Gr+=w*vec3(o.gR.x,0.,o.gR.y);}
 }
 float h=clamp((wPos.z-hRange.x)/max(hRange.y-hRange.x,1.),0.,1.);
 float slope=1.-clamp(N.z,0.,1.);
 float rockM=texMode>.5?smoothstep(.12,.36,slope+(lo-.5)*.20+(mid-.5)*.10):smoothstep(.27,.56,slope+(lo-.5)*.35+(mid-.5)*.14);   // steep ground turns to bedrock, broken up by noise
 // base: hypsometric tint, then soil texture
 vec3 lowC=vec3(.080,.290,.335),midC=vec3(.165,.335,.345),highC=vec3(.46,.43,.34);
 vec3 baseC=mix(mix(lowC,midC,smoothstep(0.,.45,h)),highC,smoothstep(.5,1.,h));
 vec3 alb=baseC;
 alb*=.92+.16*lo;alb*=.90+.20*mid;alb*=.84+.32*grain;alb*=1.-.11*rill*(1.-rockM);
 // bedrock: bedding planes (slightly dipping), thin laminations, dark joints
 float s=(wPos.z+.10*wPos.x)*1.2+(lo-.5)*1.8+(mid-.5)*.7;
 float band=mix(.5,smoothstep(.28,.72,sin(s*6.2832)*.5+.5),vis(px,1.7));
 float lam=sin((wPos.z+.10*wPos.x)*11.+(mid-.5)*5.+lo*6.)*.5+.5;
 vec3 rockC=mix(vec3(.27,.28,.28),vec3(.46,.43,.39),band);
 rockC*=.92+.08*lam*vis(px,1.8);rockC*=.80+.40*mid;rockC=mix(rockC,rockC*vec3(1.08,1.0,.9),smoothstep(.4,.7,grain));
 rockC*=1.-.42*crack;
 alb=mix(alb,rockC,rockM);
 // normal: fractal bump and joints
 vec3 G=Gt*(.6+.5*(1.-rockM))+Gc*rockM*1.2+Gr*(1.-rockM);
 G-=N*dot(G,N);
 vec3 Nb=normalize(N-G);
 float mao=1.-.55*crack*rockM-.12*rill*(1.-rockM);
 float specK=0.;
 if(texMode>.5)
 {
  float lowMix=.2+.6*smoothstep(.3,.7,vn(wPos.xy*.043+vec2(8.1,2.3)).x);
  vec3 rc=vec3(0.),gc=vec3(0.),rn=N,gn=N;vec2 rr=vec2(.8,1.),gr=vec2(.8,1.);
  if(rockM>.01)photo(rockD,rockN,rockR,texA.x,1.,N,aw,lowMix,rc,rn,rr);
  if(rockM<.99)photo(grdD,grdN,grdR,texA.y,.85,N,aw,1.-lowMix,gc,gn,gr);
  vec3 tex=mix(grade(gc,texA.w,texB.y),grade(rc,texA.z,texB.x),rockM);
  float bl=max(dot(baseC,vec3(.299,.587,.114)),.05);
  tex*=mix(vec3(1.),baseC/bl,texB.z);
  alb=tex*(.92+.16*lo);
  vec3 wn=normalize(mix(normalize(gn),normalize(rn),rockM));
  Nb=normalize(mix(N,wn,.92));
  vec2 ra=mix(gr,rr,rockM);
  mao=mix(1.,ra.y,.85);specK=(1.-ra.x)*texB.w;
 }
 float diff=max(dot(Nb,lightDir),0.);
 vec3 skyA=vec3(.42,.54,.66),gndA=vec3(.17,.19,.19);
 vec3 amb=mix(gndA,skyA,Nb.z*.5+.5);
 // real shadow from the shadow map; a faint ellipse keeps a little contact darkening under the rock
 float sh=shadowAt(wPos,N);
 float cs=0.;
 if(rock.w>0.){
  vec2 ld=normalize(lightDir.xy+vec2(1e-4));
  float hg=max(rock.z-wPos.z,0.);
  vec2 off=-lightDir.xy/max(lightDir.z,.25)*min(hg,rock.w*3.);
  vec2 dd=wPos.xy-rock.xy-off;
  float al=dot(dd,ld),pe=dd.x*-ld.y+dd.y*ld.x;
  float r=rock.w*1.15;
  cs=exp(-(al*al/(r*r*2.6)+pe*pe/(r*r*1.1))*1.3)*(1.-smoothstep(0.,rock.w*7.,hg)*.75);
  cs*=shadowP.x>.5?.42:1.;}
 vec3 key=vec3(.97,.95,.90)*1.38;
 vec3 V=normalize(viewPos-wPos);
 vec3 col=alb*(amb*(.80+.20*sh)*ao*mao*(1.-.62*cs)+key*diff*ao*mao*sh*(1.-.9*cs));
 col+=key*pow(max(dot(Nb,normalize(lightDir+V)),0.),18.)*specK*diff*sh*(1.-.9*cs);
 col+=skyA*pow(1.-max(dot(N,V),0.),4.)*.10*smoothstep(.0,.5,N.z);
 if(contourOn>.5){
  float fw=fwidth(wPos.z);
  float lm=lineAt(wPos.z,5.,fw),lM=lineAt(wPos.z,25.,fw);
  col=mix(col,vec3(.55,.86,.92),lm*.16);
  col=mix(col,vec3(.40,.82,.92),lM*.42);}
 float f=smoothstep(fogRange.x,fogRange.y,distance(wPos,viewPos));
 col=mix(col,fogColor,f);
 finalColor=vec4(col,1.);}
)";
const char*kSolidVs=R"(#version 330
in vec3 vertexPosition;in vec4 vertexColor;
uniform mat4 mvp;uniform mat4 matModel;
out vec3 wPos;out vec4 vc;
void main(){vec4 w=matModel*vec4(vertexPosition,1.0);wPos=w.xyz;vc=vertexColor;gl_Position=mvp*vec4(vertexPosition,1.0);}
)";
const char*kSolidFs=R"(#version 330
in vec3 wPos;in vec4 vc;
uniform vec3 viewPos;uniform vec3 fogColor;uniform vec2 fogRange;
out vec4 finalColor;
void main(){float f=smoothstep(fogRange.x,fogRange.y,distance(wPos,viewPos));finalColor=vec4(mix(vc.rgb,fogColor,f),vc.a);}
)";
const char*kGroundFs=R"(#version 330
in vec3 wPos;in vec4 vc;
uniform vec3 viewPos;uniform vec3 fogColor;uniform vec2 fogRange;uniform vec3 gridCenter;
out vec4 finalColor;
void main(){
 if(viewPos.z<wPos.z)discard;
 vec2 p=wPos.xy;
 vec2 g1=abs(fract(p/10.-.5)-.5)/max(fwidth(p/10.),vec2(1e-5));float l1=1.-min(min(g1.x,g1.y),1.);
 vec2 g2=abs(fract(p/100.-.5)-.5)/max(fwidth(p/100.),vec2(1e-5));float l2=1.-min(min(g2.x,g2.y),1.);
 float d=length(p-gridCenter.xy);
 float fade=1.-smoothstep(100.,820.,d);
 vec3 col=vec3(.050,.082,.105)+vec3(.045,.105,.125)*(l1*.30+l2*.60)*fade;
 col+=vec3(.02,.05,.06)*(1.-smoothstep(0.,180.,d));
 float f=smoothstep(fogRange.x,fogRange.y,distance(wPos,viewPos));
 finalColor=vec4(mix(col,fogColor,f),1.);}
)";
const char*kRibbonVs=R"(#version 330
in vec3 vertexPosition;in vec2 vertexTexCoord;in vec2 vertexTexCoord2;in vec3 vertexNormal;in vec4 vertexColor;
uniform mat4 mvp;uniform vec2 viewport;uniform float widthPx;
out vec4 vc;out float vt;out float vs;
void main(){
 vec4 a=mvp*vec4(vertexPosition,1.0);vec4 b=mvp*vec4(vertexNormal,1.0);
 float aw=max(a.w,1e-3),bw=max(b.w,1e-3);
 vec2 sa=a.xy/aw*viewport*.5,sb=b.xy/bw*viewport*.5;
 vec2 d=sb-sa;float l=length(d);d=l>1e-4?d/l:vec2(1.,0.);
 vec2 nrm=vec2(-d.y,d.x);
 vec2 off=nrm*vertexTexCoord.x*widthPx*.5/(viewport*.5);
 gl_Position=vec4(a.xy+off*a.w,a.z,a.w);
 vc=vertexColor;vt=vertexTexCoord.y;vs=vertexTexCoord2.x;}
)";
const char*kRibbonFs=R"(#version 330
in vec4 vc;in float vt;in float vs;
uniform float cutTime;uniform float whiten;uniform float alphaMul;
out vec4 finalColor;
void main(){
 float edge=1.-smoothstep(.55,1.,abs(vs));
 vec3 c=mix(vc.rgb,vec3(1.),whiten)+vec3(.16)*(1.-abs(vs));
 float a=vc.a*edge*alphaMul;
 if(cutTime>=0.&&vt>cutTime)a*=.26;
 finalColor=vec4(c,a);}
)";
const char*kSkyVs=R"(#version 330
in vec3 vertexPosition;uniform mat4 mvp;
void main(){gl_Position=mvp*vec4(vertexPosition,1.0);}
)";
const char*kSkyFs=R"(#version 330
out vec4 finalColor;
uniform vec4 vp;uniform vec3 camR;uniform vec3 camU;uniform vec3 camF;uniform vec2 tanHalf;uniform vec3 lightDir;
float hash(vec2 p){return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453);}
void main(){
 vec2 uv=(gl_FragCoord.xy-vp.xy)/vp.zw*2.-1.;
 vec3 ray=normalize(camF+camR*uv.x*tanHalf.x+camU*uv.y*tanHalf.y);
 float e=ray.z;
 vec3 zen=vec3(.043,.067,.094),hor=vec3(.125,.215,.265),gnd=vec3(.092,.165,.205);
 vec3 c=mix(hor,zen,smoothstep(0.,.6,pow(max(e,0.),.55)));
 c=mix(c,gnd,1.-smoothstep(-.4,0.,e));
 c+=vec3(.03,.075,.085)*exp(-abs(e)*12.);
 float s=max(dot(ray,lightDir),0.);
 c+=vec3(.55,.40,.24)*pow(s,28.)*.28+vec3(.30,.24,.17)*pow(s,5.)*.07;
 c+=(hash(gl_FragCoord.xy)-.5)/200.;
 finalColor=vec4(c,1.);}
)";
Shader load(const char*vs,const char*fs){
    Shader s=LoadShaderFromMemory(vs,fs);
    if(s.id==0||s.id==rlGetShaderIdDefault())return Shader{};
    return s;
}
Material material(Shader s){Material m=LoadMaterialDefault();m.shader=s;return m;}
Programs P;
FrameParams last;
unsigned shFbo=0,shTex=0;int shSize=2048;bool shTried=false,shOk=false;Matrix shVP{};float shTexel=.05f,shBias=.001f;Material depthMat{};bool depthMatOk=false;
struct TexSet
{
    bool on=false;std::string name;Texture2D t[6]{};int count=0;
    float rockScale=.33f,groundScale=.42f,rockBright=.6f,groundBright=.6f,rockSat=.6f,groundSat=.6f,hypso=.35f,spec=.1f;Vector3 tint{.82f,.93f,1.f};
};
TexSet TX;std::string wantedTex;
std::filesystem::path findDir(const std::string&rel) // like fonts: next to the exe, the working directory, then up to 5 parents of both
{
    namespace fs=std::filesystem;
    std::vector<fs::path> roots;std::error_code ec;
    for(fs::path base:{fs::path(GetApplicationDirectory()),fs::current_path(ec)}){fs::path p=base;for(int i=0;i<6;++i){roots.push_back(p);if(!p.has_parent_path()||p.parent_path()==p)break;p=p.parent_path();}}
    for(auto&r:roots){auto c=r/rel;if(fs::exists(c,ec))return c;}
    return {};
}
std::filesystem::path texRoot() // candidate packs (build/tex_candidates), development only
{
    namespace fs=std::filesystem;
    if(const char*e=std::getenv("RS3D_TEX_ROOT"))return fs::path(e);
    for(const char*rel:{"build/tex_candidates","tex_candidates","assets/terrain"}){auto p=findDir(rel);std::error_code ec;if(!p.empty()&&fs::exists(p/"cands",ec))return p;}
    return {};
}
Texture2D loadTex(const std::filesystem::path&p)
{
    Texture2D t{};Image im=LoadImage(p.string().c_str());if(!im.data)return t;
    t=LoadTextureFromImage(im);UnloadImage(im);if(!t.id)return t;
    GenTextureMipmaps(&t);SetTextureWrap(t,TEXTURE_WRAP_REPEAT);SetTextureFilter(t,TEXTURE_FILTER_TRILINEAR);SetTextureFilter(t,TEXTURE_FILTER_ANISOTROPIC_16X);
    return t;
}
void unloadTex(){for(int i=0;i<TX.count;++i)if(TX.t[i].id)UnloadTexture(TX.t[i]);TX=TexSet{};}
bool readCfg(const std::filesystem::path&p,std::map<std::string,std::string>&kv)
{
    std::ifstream f(p);if(!f)return false;
    std::string line;
    while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto e=line.find('=');if(e==std::string::npos||line[0]=='#')continue;kv[line.substr(0,e)]=line.substr(e+1);}
    return true;
}
bool buildSet(const std::string&name,std::map<std::string,std::string>&kv,const std::filesystem::path files[6]) // any failure leaves the procedural look in place
{
    auto num=[&](const char*k,float d){auto it=kv.find(k);return it==kv.end()?d:std::stof(it->second);};
    TexSet n;n.name=name;n.rockScale=1.f/num("rockTile",3.f);n.groundScale=1.f/num("groundTile",2.4f);n.rockBright=num("rockBright",.6f);n.groundBright=num("groundBright",.6f);
    n.rockSat=num("rockSat",.6f);n.groundSat=num("groundSat",.6f);n.hypso=num("hypso",.35f);n.spec=num("spec",.1f);
    if(kv.count("tint")){std::istringstream is(kv["tint"]);is>>n.tint.x>>n.tint.y>>n.tint.z;}
    for(int i=0;i<6;++i)
    {
        Texture2D t=loadTex(files[i]);
        if(!t.id){for(int j=0;j<n.count;++j)UnloadTexture(n.t[j]);return false;}
        n.t[n.count++]=t;
    }
    n.on=true;TX=n;return true;
}
bool loadDefaultSet() // assets/textures/terrain: rock_{d,n,r}.jpg, ground_{d,n,r}.jpg, terrain.cfg
{
    namespace fs=std::filesystem;
    try
    {
        fs::path dir=findDir("assets/textures/terrain");if(dir.empty())return false;
        std::map<std::string,std::string> kv;if(!readCfg(dir/"terrain.cfg",kv))return false;
        const char*names[6]{"rock_d.jpg","rock_n.jpg","rock_r.jpg","ground_d.jpg","ground_n.jpg","ground_r.jpg"};fs::path files[6];
        for(int i=0;i<6;++i)files[i]=dir/names[i];
        return buildSet("default",kv,files);
    }
    catch(const std::exception&){return false;}
}
bool loadTexSet(const std::string&name) // candidate pack: cands/NAME.cfg + mats/<id>/{d,n,r}.jpg
{
    namespace fs=std::filesystem;
    try
    {
        fs::path root=texRoot();if(root.empty())return false;
        std::map<std::string,std::string> kv;if(!readCfg(root/"cands"/(name+".cfg"),kv))return false;
        const char*kinds[3]{"d.jpg","n.jpg","r.jpg"};const std::string mats[2]{kv.at("rock"),kv.at("ground")};fs::path files[6];
        for(int m=0;m<2;++m)for(int k=0;k<3;++k)files[m*3+k]=root/"mats"/mats[m]/kinds[k];
        return buildSet(name,kv,files);
    }
    catch(const std::exception&){return false;}
}
void setV3(Shader s,const char*n,Vector3 v){int l=GetShaderLocation(s,n);if(l>=0)SetShaderValue(s,l,&v,SHADER_UNIFORM_VEC3);}
void setV2(Shader s,const char*n,Vector2 v){int l=GetShaderLocation(s,n);if(l>=0)SetShaderValue(s,l,&v,SHADER_UNIFORM_VEC2);}
void setV4(Shader s,const char*n,Vector4 v){int l=GetShaderLocation(s,n);if(l>=0)SetShaderValue(s,l,&v,SHADER_UNIFORM_VEC4);}
void setF(Shader s,const char*n,float v){int l=GetShaderLocation(s,n);if(l>=0)SetShaderValue(s,l,&v,SHADER_UNIFORM_FLOAT);}
}
Programs& programs()
{
    if(P.tried)return P;
    P.tried=true;
    if(std::getenv("RS3D_NO_VIZ")){TraceLog(LOG_INFO,"viz disabled by RS3D_NO_VIZ");return P;} // diagnostic switch to test the plain fallback
    P.terrain=load(kTerrainVs,kTerrainFs);P.solid=load(kSolidVs,kSolidFs);P.ground=load(kSolidVs,kGroundFs);P.ribbon=load(kRibbonVs,kRibbonFs);P.sky=load(kSkyVs,kSkyFs);
    P.ok=P.terrain.id&&P.solid.id&&P.ground.id&&P.ribbon.id&&P.sky.id;
    if(!P.ok){TraceLog(LOG_WARNING,"viz shaders unavailable, using the plain 3D look");return P;}
    {
        std::string want=wantedTex;if(want.empty())if(const char*e=std::getenv("RS3D_TERRAIN_TEX"))want=e;
        if(want=="procedural"||want=="none"||want=="off")TraceLog(LOG_INFO,"viz terrain: procedural (requested)");
        else
        {
            bool ok=want.empty()||want=="default"?loadDefaultSet():loadTexSet(want);
            if(ok)TraceLog(LOG_INFO,"viz terrain texture: %s",TX.name.c_str());
            else TraceLog(LOG_WARNING,"viz terrain texture '%s' unavailable, using the procedural ground",want.empty()?"default":want.c_str());
        }
    }
    P.terrainMat=material(P.terrain);P.solidMat=material(P.solid);P.groundMat=material(P.ground);P.ribbonMat=material(P.ribbon);
    return P;
}
void setTerrainTexture(const std::string&name){wantedTex=name;}
bool terrainTextureOn(){return TX.on;}
const std::string& terrainTextureName(){return TX.name;}
void release()
{
    unloadTex();
    if(shOk){rlUnloadFramebuffer(shFbo);rlUnloadTexture(shTex);shOk=false;}
    if(depthMatOk){RL_FREE(depthMat.maps);depthMat.maps=nullptr;depthMatOk=false;}
    shTried=false;shFbo=shTex=0;
    if(!P.tried||!P.ok){P={};return;}
    for(Shader*s:{&P.terrain,&P.solid,&P.ribbon,&P.sky,&P.ground})UnloadShader(*s);
    for(Material*m:{&P.terrainMat,&P.solidMat,&P.ribbonMat,&P.groundMat}){RL_FREE(m->maps);m->maps=nullptr;}
    P={};
}
void beginFrame(const FrameParams&f)
{
    auto&p=programs();if(!p.ok)return;
    last=f;Vector2 fog{f.distance*2.4f,f.distance*7.5f};
    for(Shader s:{p.terrain,p.solid,p.ground}){setV3(s,"viewPos",f.viewPos);setV3(s,"fogColor",kFog);setV2(s,"fogRange",fog);}
    setV2(p.terrain,"hRange",f.heightRange);setV3(p.terrain,"lightDir",kLight);setV4(p.terrain,"rock",f.rock);setF(p.terrain,"contourOn",f.contours?1.f:0.f);
    setV3(p.ground,"gridCenter",f.gridCenter);
    {
        int slot=10;Vector4 sp{shOk&&f.shadow?1.f:0.f,shTexel,shBias,2.4f/float(shSize)};
        int l=GetShaderLocation(p.terrain,"shadowMap");if(l>=0)SetShaderValue(p.terrain,l,&slot,SHADER_UNIFORM_INT);
        l=GetShaderLocation(p.terrain,"lightVP");if(l>=0)SetShaderValueMatrix(p.terrain,l,shVP);
        setV4(p.terrain,"shadowP",sp);
        static const char*names[6]{"rockD","rockN","rockR","grdD","grdN","grdR"};static const int slots[6]{5,6,7,8,9,11};
        for(int i=0;i<6;++i){int sl=GetShaderLocation(p.terrain,names[i]);if(sl>=0)SetShaderValue(p.terrain,sl,&slots[i],SHADER_UNIFORM_INT);}
        setF(p.terrain,"texMode",TX.on?1.f:0.f);setV4(p.terrain,"texA",{TX.rockScale,TX.groundScale,TX.rockBright,TX.groundBright});setV4(p.terrain,"texB",{TX.rockSat,TX.groundSat,TX.hypso,TX.spec});setV3(p.terrain,"texTint",TX.tint);
    }
}
Vector3 sunDir(){return kLight;}
bool shadowReady()
{
    if(shTried)return shOk;
    shTried=true;if(!programs().ok||std::getenv("RS3D_NO_SHADOW"))return false; // RS3D_NO_SHADOW: diagnostic switch
    shFbo=rlLoadFramebuffer();if(!shFbo)return false;
    shTex=rlLoadTextureDepth(shSize,shSize,false);
    if(!shTex){rlUnloadFramebuffer(shFbo);shFbo=0;return false;}
    rlFramebufferAttach(shFbo,shTex,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_TEXTURE2D,0);
    if(!rlFramebufferComplete(shFbo)){rlUnloadFramebuffer(shFbo);rlUnloadTexture(shTex);shFbo=shTex=0;TraceLog(LOG_WARNING,"viz shadow map unavailable, shadows off");return false;}
    depthMat=LoadMaterialDefault();depthMatOk=true;shOk=true;return true;
}
void shadowBegin(const Matrix&view,float l,float r,float b,float t,float n,float f)
{
    rlDrawRenderBatchActive();EndScissorMode();
    rlEnableFramebuffer(shFbo);rlViewport(0,0,shSize,shSize);rlDisableScissorTest();rlClearScreenBuffers();
    rlEnableDepthTest();rlEnableDepthMask();rlDisableBackfaceCulling();
    rlMatrixMode(RL_PROJECTION);rlPushMatrix();rlLoadIdentity();rlOrtho(l,r,b,t,n,f);
    rlMatrixMode(RL_MODELVIEW);rlLoadIdentity();rlMultMatrixf(MatrixToFloat(view));
    shVP=MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection());shTexel=(r-l)/float(shSize);shBias=.08f/std::max(f-n,1.f);
}
void drawDepthMesh(const Mesh&m){if(shOk)DrawMesh(m,depthMat,MatrixIdentity());}
void shadowEnd(int fbW,int fbH)
{
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);rlPopMatrix();rlMatrixMode(RL_MODELVIEW);rlLoadIdentity();
    rlDisableFramebuffer();rlViewport(0,0,fbW,fbH);rlDisableDepthTest();
}
void bindShadow()
{
    if(shOk){rlActiveTextureSlot(10);rlEnableTexture(shTex);}
    if(TX.on){static const int slots[6]{5,6,7,8,9,11};for(int i=0;i<6;++i){rlActiveTextureSlot(slots[i]);rlEnableTexture(TX.t[i].id);}}
    rlActiveTextureSlot(0);
}
void drawSky(int x,int yTop,int w,int h,int fbH,Vector3 r,Vector3 u,Vector3 f,float tanV,float aspect)
{
    auto&p=programs();if(!p.ok)return;
    Vector4 vp{float(x),float(fbH-(yTop+h)),float(w),float(h)};
    setV4(p.sky,"vp",vp);setV3(p.sky,"camR",r);setV3(p.sky,"camU",u);setV3(p.sky,"camF",f);setV2(p.sky,"tanHalf",{tanV*aspect,tanV});setV3(p.sky,"lightDir",kLight);
    BeginShaderMode(p.sky);DrawRectangle(x,yTop,w,h,WHITE);EndShaderMode();
}
void Ribbon::clear(){if(has_)UnloadMesh(mesh_);mesh_={};has_=false;}
void Ribbon::build(const std::vector<Vector3>&p,const std::vector<float>&time,Color base)
{
    clear();size_t n=p.size();if(n<2||time.size()!=n)return;
    std::vector<float> speed(n-1);float smax=1e-3f;
    for(size_t i=0;i+1<n;++i){speed[i]=Vector3Distance(p[i],p[i+1])/std::max(time[i+1]-time[i],1e-3f);smax=std::max(smax,speed[i]);}
    size_t segs=n-1,vc=segs*6;
    mesh_={};mesh_.vertexCount=int(vc);mesh_.triangleCount=int(segs*2);
    mesh_.vertices=static_cast<float*>(MemAlloc(unsigned(vc*3*sizeof(float))));mesh_.normals=static_cast<float*>(MemAlloc(unsigned(vc*3*sizeof(float))));
    mesh_.texcoords=static_cast<float*>(MemAlloc(unsigned(vc*2*sizeof(float))));mesh_.texcoords2=static_cast<float*>(MemAlloc(unsigned(vc*2*sizeof(float))));
    mesh_.colors=static_cast<unsigned char*>(MemAlloc(unsigned(vc*4)));
    size_t k=0;
    auto put=[&](Vector3 pos,Vector3 other,float sign,float side,float t,Color c){
        mesh_.vertices[3*k]=pos.x;mesh_.vertices[3*k+1]=pos.y;mesh_.vertices[3*k+2]=pos.z;
        mesh_.normals[3*k]=other.x;mesh_.normals[3*k+1]=other.y;mesh_.normals[3*k+2]=other.z;
        mesh_.texcoords[2*k]=sign;mesh_.texcoords[2*k+1]=t;mesh_.texcoords2[2*k]=side;mesh_.texcoords2[2*k+1]=0;
        mesh_.colors[4*k]=c.r;mesh_.colors[4*k+1]=c.g;mesh_.colors[4*k+2]=c.b;mesh_.colors[4*k+3]=c.a;++k;};
    auto shade=[&](size_t i,float s){float t=float(i)/float(n-1);float b=.78f+.42f*std::min(s/smax,1.f);
        auto ch=[&](unsigned char v){return (unsigned char)std::clamp(v*b,0.f,255.f);};
        return Color{ch(base.r),ch(base.g),ch(base.b),(unsigned char)std::clamp((.34f+.66f*t)*float(base.a),0.f,255.f)};};
    for(size_t i=0;i<segs;++i)
    {
        Color ca=shade(i,speed[i]),cb=shade(i+1,speed[i]);Vector3 a=p[i],b=p[i+1];
        put(a,b,1,1,time[i],ca);put(a,b,-1,-1,time[i],ca);put(b,a,-1,1,time[i+1],cb);   // b-vertices see the segment reversed
        put(a,b,-1,-1,time[i],ca);put(b,a,1,-1,time[i+1],cb);put(b,a,-1,1,time[i+1],cb);
    }
    UploadMesh(&mesh_,false);has_=true;
}
void Ribbon::draw(float widthPx,float whiten,float cutTime,int vpW,int vpH,float alphaMul)
{
    auto&p=programs();if(!has_||!p.ok)return;
    setV2(p.ribbon,"viewport",{float(vpW),float(vpH)});setF(p.ribbon,"widthPx",widthPx);setF(p.ribbon,"whiten",whiten);setF(p.ribbon,"cutTime",cutTime);setF(p.ribbon,"alphaMul",alphaMul);
    DrawMesh(mesh_,p.ribbonMat,MatrixIdentity());
}
void glowDisc(Vector3 c,float r,Vector3 right,Vector3 up,Color in,int seg)
{
    rlBegin(RL_TRIANGLES);
    for(int i=0;i<seg;++i)
    {
        float a0=2*PI*i/seg,a1=2*PI*(i+1)/seg;
        Vector3 p0=Vector3Add(c,Vector3Add(Vector3Scale(right,std::cos(a0)*r),Vector3Scale(up,std::sin(a0)*r)));
        Vector3 p1=Vector3Add(c,Vector3Add(Vector3Scale(right,std::cos(a1)*r),Vector3Scale(up,std::sin(a1)*r)));
        rlColor4ub(in.r,in.g,in.b,in.a);rlVertex3f(c.x,c.y,c.z);
        rlColor4ub(in.r,in.g,in.b,0);rlVertex3f(p1.x,p1.y,p1.z);rlVertex3f(p0.x,p0.y,p0.z);
    }
    rlEnd();
}
void ring(Vector3 c,float r,float w,Vector3 right,Vector3 up,Color col,int seg)
{
    rlBegin(RL_TRIANGLES);rlColor4ub(col.r,col.g,col.b,col.a);
    float ri=std::max(0.f,r-w*.5f),ro=r+w*.5f;
    for(int i=0;i<seg;++i)
    {
        float a0=2*PI*i/seg,a1=2*PI*(i+1)/seg;
        auto pt=[&](float a,float rad){return Vector3Add(c,Vector3Add(Vector3Scale(right,std::cos(a)*rad),Vector3Scale(up,std::sin(a)*rad)));};
        Vector3 i0=pt(a0,ri),o0=pt(a0,ro),i1=pt(a1,ri),o1=pt(a1,ro);
        rlVertex3f(i0.x,i0.y,i0.z);rlVertex3f(o0.x,o0.y,o0.z);rlVertex3f(o1.x,o1.y,o1.z);
        rlVertex3f(i0.x,i0.y,i0.z);rlVertex3f(o1.x,o1.y,o1.z);rlVertex3f(i1.x,i1.y,i1.z);
    }
    rlEnd();
}
}
