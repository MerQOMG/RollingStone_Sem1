#include "ui/viewport3d.h"
#include <rlgl.h>
#include <cmath>
#include <set>
namespace rs3d::gui {
void nativeDrawLines(int count);
namespace {
Shader lineShader{};int mvpLoc=-1,colorLoc=-1;
void ensureLineShader(){
    if(lineShader.id)return;
    lineShader=LoadShaderFromMemory("#version 330\nlayout(location=0) in vec3 vertexPosition;uniform mat4 mvp;void main(){gl_Position=mvp*vec4(vertexPosition,1.0);}",
        "#version 330\nuniform vec4 color;out vec4 finalColor;void main(){finalColor=color;}");
    mvpLoc=GetShaderLocation(lineShader,"mvp");colorLoc=GetShaderLocation(lineShader,"color");
}
Vector3 sub(Vector3 a,Vector3 b){return Vector3Subtract(a,b);}
const Vector3 lightDir=Vector3Normalize({.55f,-.35f,.75f}); // same sun as the shaders
float lightAt(Vector3 n){float d=std::max(0.f,Vector3DotProduct(n,lightDir));return .36f+.64f*d;}
Color lit(Color base,float k,unsigned char a=255){auto f=[&](unsigned char c){return (unsigned char)std::clamp(c*k,0.f,255.f);};return {f(base.r),f(base.g),f(base.b),a};}
Color hypso(double t){
    Color a=rgba(0x2d474a),b=rgba(0x5b6b58),c=rgba(0x9a947a);
    return t<.5?mix(a,b,float(t*2)):mix(b,c,float(t*2-1));
}
Mesh makeMesh(const std::vector<Vector3>&verts,const std::vector<Color>&colors){
    Mesh m{};m.vertexCount=int(verts.size());m.triangleCount=m.vertexCount/3;
    m.vertices=static_cast<float*>(MemAlloc(unsigned(m.vertexCount*3*sizeof(float))));m.colors=static_cast<unsigned char*>(MemAlloc(unsigned(m.vertexCount*4)));
    for(int i=0;i<m.vertexCount;++i){m.vertices[3*i]=verts[i].x;m.vertices[3*i+1]=verts[i].y;m.vertices[3*i+2]=verts[i].z;m.colors[4*i]=colors[i].r;m.colors[4*i+1]=colors[i].g;m.colors[4*i+2]=colors[i].b;m.colors[4*i+3]=colors[i].a;}
    UploadMesh(&m,false);return m;
}
Mesh makeMeshN(const std::vector<Vector3>&verts,const std::vector<Vector3>&norms,const std::vector<Color>&colors){
    Mesh m{};m.vertexCount=int(verts.size());m.triangleCount=m.vertexCount/3;
    m.vertices=static_cast<float*>(MemAlloc(unsigned(m.vertexCount*3*sizeof(float))));m.normals=static_cast<float*>(MemAlloc(unsigned(m.vertexCount*3*sizeof(float))));m.colors=static_cast<unsigned char*>(MemAlloc(unsigned(m.vertexCount*4)));
    for(int i=0;i<m.vertexCount;++i){m.vertices[3*i]=verts[i].x;m.vertices[3*i+1]=verts[i].y;m.vertices[3*i+2]=verts[i].z;m.normals[3*i]=norms[i].x;m.normals[3*i+1]=norms[i].y;m.normals[3*i+2]=norms[i].z;m.colors[4*i]=colors[i].r;m.colors[4*i+1]=colors[i].g;m.colors[4*i+2]=colors[i].b;m.colors[4*i+3]=colors[i].a;}
    UploadMesh(&m,false);return m;
}
void unloadModel(Model&m,bool&has){if(has){if(m.materialCount>0&&m.materials){m.materials[0].shader.id=rlGetShaderIdDefault();m.materials[0].shader.locs=rlGetShaderLocsDefault();}UnloadModel(m);}m={};has=false;} // never let raylib free our shared programs
Matrix poseMatrix(const Quaternion&q,Vec3 p){
    auto r=q.matrix();Matrix m{};
    m.m0=float(r(0,0));m.m4=float(r(0,1));m.m8=float(r(0,2));m.m12=float(p.x);
    m.m1=float(r(1,0));m.m5=float(r(1,1));m.m9=float(r(1,2));m.m13=float(p.y);
    m.m2=float(r(2,0));m.m6=float(r(2,1));m.m10=float(r(2,2));m.m14=float(p.z);
    m.m3=0;m.m7=0;m.m11=0;m.m15=1;return m;
}
void collectEdges(const std::vector<Vector3>&v,const std::vector<Face>&faces,std::vector<Vector3>&normals,std::vector<std::pair<int,int>>&edges,Vector3 inside){
    normals.clear();edges.clear();
    for(auto&f:faces){Vector3 n=Vector3CrossProduct(sub(v[f.b],v[f.a]),sub(v[f.c],v[f.a]));float l=Vector3Length(n);n=l>0?Vector3Scale(n,1/l):Vector3{0,0,1};
        Vector3 cen=Vector3Scale(Vector3Add(Vector3Add(v[f.a],v[f.b]),v[f.c]),1.f/3);if(Vector3DotProduct(n,sub(cen,inside))<0)n=Vector3Negate(n);normals.push_back(n);}
    std::map<std::pair<int,int>,std::vector<int>> owner;
    for(size_t i=0;i<faces.size();++i){int ids[3]{faces[i].a,faces[i].b,faces[i].c};for(int k=0;k<3;++k){int a=ids[k],b=ids[(k+1)%3];owner[{std::min(a,b),std::max(a,b)}].push_back(int(i));}}
    for(auto&e:owner){bool show=e.second.size()!=2||Vector3DotProduct(normals[e.second[0]],normals[e.second[1]])<.9995f;if(show)edges.push_back(e.first);}
}
}
Vector3 toV3(Vec3 v){return {float(v.x),float(v.y),float(v.z)};}
Color heatRamp(double t,unsigned char a){
    t=std::clamp(t,0.,1.);
    static const Color stops[]{rgba(0x120a2e),rgba(0x4b1a78),rgba(0x9a2b7c),rgba(0xe5526a),rgba(0xfd9f5b),rgba(0xfcf2b0)};
    double f=t*5;int i=std::min(4,int(f));Color c=mix(stops[i],stops[i+1],float(f-i));c.a=a;return c;
}
Color stopColor(StopReason r,unsigned char a){
    Color c=rgba(0x9aa7b5);
    switch(r){case StopReason::resting:c=rgba(0x3cb7cb);break;case StopReason::downstream:c=rgba(0x78aefc);break;case StopReason::upstream:c=rgba(0xb99cf5);break;case StopReason::lateral:c=rgba(0xf2b64c);break;
    case StopReason::timeout:c=rgba(0xf06464);break;case StopReason::geometryFailure:case StopReason::numericFailure:c=rgba(0xff4d8d);break;case StopReason::cancelled:c=rgba(0x6f7c8a);break;default:break;}
    c.a=a;return c;
}
void LineBuffer::clear(){if(vbo_)rlUnloadVertexBuffer(vbo_);if(vao_)rlUnloadVertexArray(vao_);vao_=vbo_=0;count_=0;}
void LineBuffer::releaseShared(){if(lineShader.id)UnloadShader(lineShader);lineShader={};}
void LineBuffer::set(const std::vector<Vector3>&pts){
    clear();if(pts.size()<2)return;ensureLineShader();
    count_=int(pts.size()&~size_t(1));vao_=rlLoadVertexArray();rlEnableVertexArray(vao_);
    vbo_=rlLoadVertexBuffer(pts.data(),int(count_*sizeof(Vector3)),false);
    rlSetVertexAttribute(0,3,RL_FLOAT,false,int(sizeof(Vector3)),0);rlEnableVertexAttribute(0);rlDisableVertexArray();rlDisableVertexBuffer();
}
void LineBuffer::draw(Color c)const{
    if(!count_||!vao_)return;
    rlDrawRenderBatchActive();rlEnableShader(lineShader.id);
    rlSetUniformMatrix(mvpLoc,MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));
    float col4[4]{c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f};rlSetUniform(colorLoc,col4,RL_SHADER_UNIFORM_VEC4,1);
    rlEnableVertexArray(vao_);nativeDrawLines(count_);rlDisableVertexArray();rlDisableShader();
}
Camera3D OrbitView::camera()const{
    Vector3 d{distance*std::cos(pitch)*std::cos(yaw),distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch)};
    return {Vector3Add(target,d),target,{0,0,1},45,CAMERA_PERSPECTIVE};
}
Viewport3D::~Viewport3D(){release();}
void Viewport3D::release(){
    unloadModel(terrain_,hasTerrain_);unloadModel(obstacles_,hasObstacles_);unloadModel(skirt_,hasSkirt_);unloadModel(ground_,hasGround_);unloadHeat();ribbons_.clear();
    contourMinor_.clear();contourMajor_.clear();obstacleEdges_.clear();trails_.clear();scene_.reset();
}
void Viewport3D::unloadHeat(){unloadModel(heat_,hasHeat_);}
void Viewport3D::buildTerrain(const TerrainMesh&t){
    fancy_=viz::programs().ok;
    if(fancy_){buildTerrainFancy(t);return;}
    unloadModel(terrain_,hasTerrain_);
    double zmin=1e300,zmax=-1e300;for(auto&v:t.vertices){zmin=std::min(zmin,v.z);zmax=std::max(zmax,v.z);}
    std::vector<Vector3> verts;std::vector<Color> cols;verts.reserve(t.faces.size()*3);cols.reserve(t.faces.size()*3);
    std::vector<Vector3> minor,major;
    const double step=5;
    for(auto&f:t.faces){
        Vector3 p[3]{toV3(t.vertices[f.a]),toV3(t.vertices[f.b]),toV3(t.vertices[f.c])};
        Vector3 n=Vector3CrossProduct(sub(p[1],p[0]),sub(p[2],p[0]));float l=Vector3Length(n);n=l>0?Vector3Scale(n,1/l):Vector3{0,0,1};if(n.z<0)n=Vector3Negate(n);
        double zc=(p[0].z+p[1].z+p[2].z)/3;double h=zmax>zmin?(zc-zmin)/(zmax-zmin):0;Color c=lit(hypso(h),lightAt(n));
        for(int k=0;k<3;++k){verts.push_back(p[k]);cols.push_back(c);}
        float lo=std::min({p[0].z,p[1].z,p[2].z}),hi=std::max({p[0].z,p[1].z,p[2].z});
        for(double L=std::ceil(lo/step)*step;L<=hi&&hi>lo;L+=step){
            Vector3 hit[2];int nh=0;
            for(int k=0;k<3&&nh<2;++k){Vector3 a=p[k],b=p[(k+1)%3];if((a.z-L)*(b.z-L)<0||(a.z==L&&b.z!=L)){double u=(L-a.z)/(b.z-a.z);if(u<0||u>1)continue;Vector3 q{float(a.x+(b.x-a.x)*u),float(a.y+(b.y-a.y)*u),float(L+.04)};hit[nh++]=q;}}
            if(nh==2){bool isMajor=std::fmod(std::round(L),25.0)==0;auto&dst=isMajor?major:minor;dst.push_back(hit[0]);dst.push_back(hit[1]);}
        }
    }
    if(!verts.empty()){terrain_=LoadModelFromMesh(makeMesh(verts,cols));hasTerrain_=true;}
    contourMinor_.set(minor);contourMajor_.set(major);
}
void Viewport3D::buildTerrainFancy(const TerrainMesh&t)
{
    unloadModel(terrain_,hasTerrain_);unloadModel(skirt_,hasSkirt_);unloadModel(ground_,hasGround_);contourMinor_.clear();contourMajor_.clear();
    double zmin=1e300,zmax=-1e300;for(auto&v:t.vertices){zmin=std::min(zmin,v.z);zmax=std::max(zmax,v.z);}
    hRange_={float(zmin),float(zmax)};bbMin_={float(t.xMin),float(t.yMin),float(zmin)};bbMax_={float(t.xEnd),float(t.yMax),float(zmax)};
    // smooth vertex normals (area weighted) and a cheap concavity AO per vertex
    std::vector<Vector3> vn(t.vertices.size(),Vector3{0,0,0});
    for(auto&f:t.faces){Vector3 a=toV3(t.vertices[f.a]),b=toV3(t.vertices[f.b]),c=toV3(t.vertices[f.c]);Vector3 n=Vector3CrossProduct(sub(b,a),sub(c,a));if(n.z<0)n=Vector3Negate(n);for(int id:{f.a,f.b,f.c})vn[id]=Vector3Add(vn[id],n);}
    for(auto&n:vn){float l=Vector3Length(n);n=l>0?Vector3Scale(n,1/l):Vector3{0,0,1};}
    std::vector<float> ao(t.vertices.size(),1.f);
    if(t.vertices.size()<250000)for(size_t i=0;i<t.vertices.size();++i)
    {
        const auto&v=t.vertices[i];double sum=0;const double r=2.5;
        for(int k=0;k<8;++k){double a=k*PI/4;sum+=t.height(std::clamp(v.x+std::cos(a)*r,t.xMin,t.xEnd),std::clamp(v.y+std::sin(a)*r,t.yMin,t.yMax));}
        double c=(sum/8-v.z)/r;ao[i]=float(std::clamp(1-c*1.6,.55,1.0));
    }
    std::vector<Vector3> verts,norms;std::vector<Color> cols;verts.reserve(t.faces.size()*3);norms.reserve(t.faces.size()*3);cols.reserve(t.faces.size()*3);
    for(auto&f:t.faces)for(int id:{f.a,f.b,f.c}){verts.push_back(toV3(t.vertices[id]));norms.push_back(vn[id]);cols.push_back({255,255,255,(unsigned char)std::clamp(ao[id]*255.f,0.f,255.f)});}
    if(!verts.empty()){terrain_=LoadModelFromMesh(makeMeshN(verts,norms,cols));terrain_.materials[0].shader=viz::programs().terrain;hasTerrain_=true;}
    // skirt: the hillside stands on a stratified base instead of floating in the void
    double depth=std::max(6.0,.12*(zmax-zmin));double zb=zmin-depth;groundZ_=float(zb);
    double cx=(t.xMin+t.xEnd)/2,cy=(t.yMin+t.yMax)/2;groundCentre_={float(cx),float(cy),float(zb)};
    std::map<std::pair<int,int>,int> cnt;for(auto&f:t.faces){int ids[3]{f.a,f.b,f.c};for(int k=0;k<3;++k){int a=ids[k],b=ids[(k+1)%3];++cnt[{std::min(a,b),std::max(a,b)}];}}
    std::vector<Vector3> sv;std::vector<Color> sc;const int bands=7;
    for(auto&e:cnt){
        if(e.second!=1)continue;Vector3 a=toV3(t.vertices[e.first.first]),b=toV3(t.vertices[e.first.second]);
        Vector3 d={b.x-a.x,b.y-a.y,0};Vector3 n={d.y,-d.x,0};float l=Vector3Length(n);if(l<1e-6f)continue;n=Vector3Scale(n,1/l);
        Vector3 mid={(a.x+b.x)/2-float(cx),(a.y+b.y)/2-float(cy),0};if(Vector3DotProduct(n,mid)<0)n=Vector3Negate(n);
        float k=.52f+.48f*std::max(0.f,Vector3DotProduct(n,lightDir));
        for(int i=0;i<bands;++i){
            float f0=float(i)/bands,f1=float(i+1)/bands;
            Vector3 a0={a.x,a.y,float(a.z+(zb-a.z)*f0)},a1={a.x,a.y,float(a.z+(zb-a.z)*f1)},b0={b.x,b.y,float(b.z+(zb-b.z)*f0)},b1={b.x,b.y,float(b.z+(zb-b.z)*f1)};
            Color top=rgba(0x3b4a47),bot=rgba(0x151e24);float fm=(f0+f1)*.5f;Color c=mix(top,bot,fm);float strata=(i%2)?.9f:1.04f;
            auto sh=[&](unsigned char v){return (unsigned char)std::clamp(v*k*strata,0.f,255.f);};c={sh(c.r),sh(c.g),sh(c.b),255};
            for(Vector3 p:{a0,b0,a1,b0,b1,a1}){sv.push_back(p);sc.push_back(c);}
        }
    }
    if(!sv.empty()){skirt_=LoadModelFromMesh(makeMesh(sv,sc));skirt_.materials[0].shader=viz::programs().solid;hasSkirt_=true;}
    float R=3000;std::vector<Vector3> gv{{float(cx-R),float(cy-R),float(zb)},{float(cx+R),float(cy-R),float(zb)},{float(cx+R),float(cy+R),float(zb)},{float(cx-R),float(cy-R),float(zb)},{float(cx+R),float(cy+R),float(zb)},{float(cx-R),float(cy+R),float(zb)}};
    std::vector<Color> gc(6,WHITE);ground_=LoadModelFromMesh(makeMesh(gv,gc));ground_.materials[0].shader=viz::programs().ground;hasGround_=true;
}
void Viewport3D::buildObstacles(const ObstacleSet*set){
    unloadModel(obstacles_,hasObstacles_);obstacleEdges_.clear();
    if(!set||set->boxes.empty())return;
    std::vector<Vector3> verts;std::vector<Color> cols;std::vector<Vector3> edges;
    for(auto&o:set->boxes){
        auto&g=o.geometry;if(g.faces.empty())continue;auto m=poseMatrix(o.rotation,o.center);
        std::vector<Vector3> w;for(auto&v:g.vertices)w.push_back(Vector3Transform(toV3(v),m));
        Vector3 inside=Vector3Transform({0,0,0},m);std::vector<Vector3> normals;std::vector<std::pair<int,int>> es;collectEdges(w,g.faces,normals,es,inside);
        float zlo=1e30f,zhi=-1e30f;for(auto&p:w){zlo=std::min(zlo,p.z);zhi=std::max(zhi,p.z);}
        for(size_t i=0;i<g.faces.size();++i){auto&f=g.faces[i];Color c=lit(rgba(0x8aa3b6),lightAt(normals[i]));for(int id:{f.a,f.b,f.c}){float ao=.6f+.4f*std::clamp((w[id].z-zlo)/std::max(zhi-zlo,.02f),0.f,1.f);Color cc=lit(c,ao);verts.push_back(w[id]);cols.push_back(cc);}} // base darkened like a contact shadow
        for(auto&e:es){edges.push_back(w[e.first]);edges.push_back(w[e.second]);}
    }
    if(!verts.empty()){obstacles_=LoadModelFromMesh(makeMesh(verts,cols));if(viz::ok())obstacles_.materials[0].shader=viz::programs().solid;hasObstacles_=true;}
    obstacleEdges_.set(edges);
}
void Viewport3D::buildRock(const PhysicsScene&s){
    rockLocal_.clear();rockFaces_.clear();rockEdges_.clear();rockNormals_.clear();
    if(s.shape.faces.empty())return;
    for(auto&v:s.shape.vertices)rockLocal_.push_back(toV3(v-s.shape.center));
    rockFaces_=s.shape.faces;collectEdges(rockLocal_,rockFaces_,rockNormals_,rockEdges_,{0,0,0});
}
void Viewport3D::setScene(const std::shared_ptr<const PhysicsScene>&scene){
    if(scene==scene_)return;
    if(!scene){release();return;}
    bool first=!scene_;
    if(scene->terrain&&scene->terrain->contentHash!=terrainHash_){buildTerrain(*scene->terrain);terrainHash_=scene->terrain->contentHash;clearHeat();if(first||true)frameScene();}
    std::string oh=scene->obstacles?scene->obstacles->hash:"none";
    if(oh!=obstacleHash_){buildObstacles(scene->obstacles.get());obstacleHash_=oh;}
    if(!scene_||scene->shape.vertices.size()!=scene_->shape.vertices.size()||(!scene->shape.vertices.empty()&&(scene->shape.vertices[0].x!=scene_->shape.vertices[0].x||scene->shape.volume!=scene_->shape.volume)))buildRock(*scene);
    else if(rockLocal_.empty())buildRock(*scene);
    scene_=scene;
}
void Viewport3D::setTrajectories(const std::vector<std::shared_ptr<const DisplayTrajectory>>&list){
    std::set<std::uint64_t> keep;
    for(auto&t:list){
        if(!t||t->points.size()<2)continue;keep.insert(t->trialId);
        auto&slot=trails_[t->trialId];
        if(slot.first!=t){std::vector<Vector3> pts;pts.reserve(t->points.size()*2);for(size_t i=1;i<t->points.size();++i){pts.push_back(toV3(t->points[i-1].center));pts.push_back(toV3(t->points[i].center));}
            slot.second=std::make_unique<LineBuffer>();slot.second->set(pts);slot.first=t;
            if(viz::ok()){std::vector<Vector3> cp;std::vector<float> tm;for(auto&q:t->points){cp.push_back(toV3(q.center));tm.push_back(float(q.time));}auto rb=std::make_unique<viz::Ribbon>();rb->build(cp,tm,stopColor(t->status));ribbons_[t->trialId]=std::move(rb);}}
    }
    for(auto it=trails_.begin();it!=trails_.end();){if(!keep.count(it->first)){ribbons_.erase(it->first);it=trails_.erase(it);}else ++it;}
}
void Viewport3D::setHeat(const HeatSpec&spec){
    if(!spec.valid||!spec.terrain||!spec.hits){clearHeat();return;}
    if(spec.key==heatKey_&&hasHeat_)return;
    unloadHeat();heatKey_=spec.key;
    std::vector<Vector3> verts;std::vector<Color> cols;
    const auto&g=spec.grid;
    for(int xi=0;xi<g.size(0);++xi)for(int yi=0;yi<g.size(1);++yi){
        auto id=g.id(xi,yi,0);if(id>=spec.hits->size())continue;auto h=(*spec.hits)[id];if(!h)continue;
        double t=spec.denominator>0?double(h)/spec.denominator:0;Color c=heatRamp(.1+.9*std::clamp(t,0.,1.),215);
        auto tris=terrainCellTriangles(*spec.terrain,g.lines(0)[xi],g.lines(0)[xi+1],g.lines(1)[yi],g.lines(1)[yi+1]);
        for(auto&p:tris){verts.push_back({float(p.x),float(p.y),float(p.z+.06)});cols.push_back(c);}
    }
    verts.resize(verts.size()/3*3);cols.resize(verts.size());
    if(verts.empty())return;
    heat_=LoadModelFromMesh(makeMesh(verts,cols));if(viz::ok())heat_.materials[0].shader=viz::programs().solid;hasHeat_=true;
}
void Viewport3D::frameScene(){
    if(!scene_||!scene_->terrain){return;}
    auto&t=*scene_->terrain;double zmin=1e300,zmax=-1e300;for(auto&v:t.vertices){zmin=std::min(zmin,v.z);zmax=std::max(zmax,v.z);}
    double sx=t.xEnd-t.xMin,sy=t.yMax-t.yMin,sz=zmax-zmin;
    view.target={float((t.xMin+t.xEnd)/2),float((t.yMin+t.yMax)/2),float((zmin+zmax)/2)};
    double r=.5*std::sqrt(sx*sx+sy*sy+sz*sz);view.distance=float(std::max(8.0,r*2.1));view.yaw=-1.0f;view.pitch=.55f;goal_=view;animating_=false;preset=ViewPreset::perspective;
}
void Viewport3D::setPreset(ViewPreset p){
    preset=p;if(!scene_||!scene_->terrain)return;
    auto&t=*scene_->terrain;double zmin=1e300,zmax=-1e300;for(auto&v:t.vertices){zmin=std::min(zmin,v.z);zmax=std::max(zmax,v.z);}
    double sx=t.xEnd-t.xMin,sy=t.yMax-t.yMin,sz=zmax-zmin;
    OrbitView g=view;double tanHalf=std::tan(22.5*DEG2RAD);double aspect=1.6;
    g.target={float((t.xMin+t.xEnd)/2),float((t.yMin+t.yMax)/2),float((zmin+zmax)/2)};
    if(p==ViewPreset::perspective){double r=.5*std::sqrt(sx*sx+sy*sy+sz*sz);g.distance=float(std::max(8.0,r*2.1));g.yaw=-1.0f;g.pitch=.55f;}
    else if(p==ViewPreset::top){g.yaw=-PI/2;g.pitch=PI/2-.001f;g.distance=float(std::max(sx/aspect,sy)/2/tanHalf*1.15);}
    else if(p==ViewPreset::side){g.yaw=-PI/2;g.pitch=0;g.distance=float(std::max(sx/aspect,sz)/2/tanHalf*1.2);}
    else{g.distance=std::min(view.distance,40.f);g.pitch=.4f;}
    if(p==ViewPreset::follow)g.target=view.target;
    goal_=g;animating_=true;
}
bool Viewport3D::updateCamera(const Input&in,R rect,bool overUi,bool typing,bool modal,float dt,const Vector3*follow){
    bool inside=rect.contains(in.mx,in.my)&&!overUi;
    bool pressed=in.pressed||in.rpressed||IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE),down=in.down||in.rdown||in.mdown;
    router_.update(in.focused,pressed,down,inside,typing,modal);
    bool allowed=router_.cameraAllowed(in.focused,down,inside,typing,modal);
    Vector2 d=GetMouseDelta();float s=gfx().scale();d.x/=s;d.y/=s;
    bool dragging=false;
    if(allowed&&down){
        dragging=true;
        bool pan=in.rdown||in.mdown||(in.down&&in.shift);
        if(pan){
            Vector3 right{std::sin(view.yaw),-std::cos(view.yaw),0};
            Vector3 up=Vector3Normalize(Vector3CrossProduct(right,sub(view.camera().position,view.target)));
            view.target=Vector3Add(view.target,Vector3Scale(Vector3Add(Vector3Scale(right,-d.x),Vector3Scale(up,d.y)),view.distance*.0016f));
            if(preset==ViewPreset::follow)preset=ViewPreset::perspective;
        }else{view.yaw-=d.x*.0075f;view.pitch=std::clamp(view.pitch+d.y*.0075f,-1.55f,1.5698f);}
        animating_=false;
    }
    if(allowed&&inside&&in.wheel!=0){view.distance=std::clamp(view.distance*std::exp(-in.wheel*.12f),1.f,900.f);animating_=false;}
    if(animating_){
        float k=1-std::exp(-dt*9);auto lerpAngle=[&](float a,float b){float diff=std::remainder(b-a,2*PI);return a+diff*k;};
        view.yaw=lerpAngle(view.yaw,goal_.yaw);view.pitch+=(goal_.pitch-view.pitch)*k;view.distance+=(goal_.distance-view.distance)*k;view.target=Vector3Lerp(view.target,goal_.target,k);
        if(std::abs(view.distance-goal_.distance)<.05f&&std::abs(view.pitch-goal_.pitch)<.002f&&Vector3Distance(view.target,goal_.target)<.05f&&std::abs(std::remainder(goal_.yaw-view.yaw,2*PI))<.002f){view=goal_;animating_=false;}
    }
    if(preset==ViewPreset::follow&&follow){float k=1-std::exp(-dt*8);view.target=Vector3Lerp(view.target,*follow,k);}
    return dragging;
}
Vector2 Viewport3D::project(Vector3 p,R rect)const{
    auto cam=view.camera();float s=gfx().scale();int w=int(std::ceil(rect.w*s)),h=int(std::ceil(rect.h*s));
    Vector3 f=Vector3Normalize(sub(cam.target,cam.position));if(Vector3DotProduct(f,sub(p,cam.position))<=0.01f)return {-99999,-99999};
    Vector2 sp=GetWorldToScreenEx(p,cam,w,h);return {rect.x+sp.x/s,rect.y+sp.y/s};
}
namespace {
const Color axisX=rgba(0xe5534b),axisY=rgba(0x6fcf5f),axisZ=rgba(0x4f8df5);
struct GzItem {Vector3 d;Color c;const char*pos;const char*neg;bool positive;};
}
void Viewport3D::alignView(int item)
{
    OrbitView g=view;
    switch(item) // camera sits on the chosen side of the target and looks back along the axis
    {
    case 0:g.yaw=0;g.pitch=0;break;
    case 1:g.yaw=PI;g.pitch=0;break;
    case 2:g.yaw=PI/2;g.pitch=0;break;
    case 3:g.yaw=-PI/2;g.pitch=0;break;
    case 4:g.yaw=-PI/2;g.pitch=1.5698f;break; // top: x to the right, y up
    default:g.yaw=PI/2;g.pitch=-1.55f;break; // bottom
    }
    goal_=g;animating_=true;if(preset!=ViewPreset::follow)preset=ViewPreset::perspective;
}
bool Viewport3D::gizmo(R area,const Input&in,bool uiFree)
{
    auto&g=gfx();float s=g.scale();auto cam=view.camera();
    Vector3 f=Vector3Normalize(sub(cam.target,cam.position)),right=Vector3Normalize(Vector3CrossProduct(f,cam.up)),up=Vector3CrossProduct(right,f);
    float cx=area.cx(),cy=area.y+62,discR=58,len=36;
    static const GzItem items[6]{{{1,0,0},axisX,"X","-X",true},{{-1,0,0},axisX,"X","-X",false},{{0,1,0},axisY,"Y","-Y",true},{{0,-1,0},axisY,"Y","-Y",false},{{0,0,1},axisZ,"Z","-Z",true},{{0,0,-1},axisZ,"Z","-Z",false}};
    struct P {int i;float x,y,depth,r;};P ps[6];
    gzCenters_.assign(6,{});
    for(int i=0;i<6;++i){auto&it=items[i];ps[i]={i,cx+Vector3DotProduct(it.d,right)*len,cy-Vector3DotProduct(it.d,up)*len,Vector3DotProduct(it.d,f),it.positive?10.f:7.5f};gzCenters_[i]={ps[i].x,ps[i].y};}
    float dd=std::hypot(in.mx-cx,in.my-cy);bool onDisc=uiFree&&in.focused&&dd<=discR;
    int hov=-1;float bestDepth=1e9f; // nearest to the viewer wins
    if(onDisc)for(auto&p:ps){if(std::hypot(in.mx-p.x,in.my-p.y)<=p.r+2&&p.depth<bestDepth){bestDepth=p.depth;hov=p.i;}}
    gzHover_=hov;
    if(!in.focused)gzOwns_=false;
    if(in.pressed&&onDisc){gzOwns_=true;gzPress_=hov;gzStart_={in.mx,in.my};gzDrag_=false;}
    if(gzOwns_&&in.down)
    {
        if(!gzDrag_&&std::hypot(in.mx-gzStart_.x,in.my-gzStart_.y)>4)gzDrag_=true;
        if(gzDrag_){Vector2 d=GetMouseDelta();d.x/=s;d.y/=s;view.yaw-=d.x*.0075f;view.pitch=std::clamp(view.pitch+d.y*.0075f,-1.55f,1.5698f);animating_=false;}
    }
    if(gzOwns_&&!in.down)
    {
        if(!gzDrag_&&gzPress_>=0&&gzPress_==hov)alignView(gzPress_);
        gzOwns_=false;
    }
    bool active=onDisc||gzOwns_;
    g.circle(cx,cy,discR,alpha(rgba(0xaab6c4),active?.20f:.07f));
    g.ring(cx,cy,discR,1,alpha(rgba(0xaab6c4),active?.35f:.14f));
    std::sort(ps,ps+6,[](const P&a,const P&b){return a.depth>b.depth;}); // far to near
    for(auto&p:ps)
    {
        auto&it=items[p.i];bool hot=p.i==hov;
        if(it.positive)g.line(cx,cy,p.x,p.y,alpha(it.c,.9f),2);
        if(it.positive)
        {
            Color fill=hot?mix(it.c,rgba(0xffffff),.45f):it.c;g.circle(p.x,p.y,p.r,fill);if(hot)g.ring(p.x,p.y,p.r+1,1.5f,rgba(0xffffff));
            g.textCenter(it.pos,{p.x-10,p.y-10,20,20},12,rgba(0x0b1016),true);
        }
        else
        {
            Color fill=hot?alpha(mix(it.c,rgba(0xffffff),.3f),.85f):alpha(it.c,.38f);g.circle(p.x,p.y,p.r,fill);g.ring(p.x,p.y,p.r,1.3f,alpha(it.c,hot?1.f:.75f));
            if(hot)g.textCenter(it.neg,{p.x-12,p.y-10,24,20},11,rgba(0x0b1016),true);
        }
    }
    if(hov>=0)
    {
        static const char*names[6]{u8"从 +X（坡下）看",u8"从 −X（坡上）看",u8"从 +Y 侧看",u8"从 −Y 侧看",u8"俯视（+Z）",u8"仰视（−Z）"};
        g.textCenter(names[hov],{area.x,cy+discR+4,area.w,16},12,col::dim);
    }
    return active;
}
namespace {
const Color bandCol=rgba(0x6fe3b5);
// A constant-pixel-width line drawn as a camera-facing quad.
void screenLine(Vector3 a,Vector3 b,float px,Color c,const Camera3D&cam,int vh)
{
    Vector3 m=Vector3Scale(Vector3Add(a,b),.5f),d=Vector3Subtract(b,a),toCam=Vector3Subtract(cam.position,m);
    Vector3 sd=Vector3CrossProduct(d,toCam);float l=Vector3Length(sd);if(l<1e-6f)return;
    float wpp=Vector3Length(toCam)*2.f*std::tan(cam.fovy*.5f*DEG2RAD)/std::max(1,vh);sd=Vector3Scale(sd,.5f*px*wpp/l);
    Vector3 p0=Vector3Add(a,sd),p1=Vector3Subtract(a,sd),p2=Vector3Subtract(b,sd),p3=Vector3Add(b,sd);
    rlBegin(RL_TRIANGLES);rlColor4ub(c.r,c.g,c.b,c.a);
    rlVertex3f(p0.x,p0.y,p0.z);rlVertex3f(p1.x,p1.y,p1.z);rlVertex3f(p2.x,p2.y,p2.z);
    rlVertex3f(p0.x,p0.y,p0.z);rlVertex3f(p2.x,p2.y,p2.z);rlVertex3f(p3.x,p3.y,p3.z);
    rlEnd();
}
// A vertical wall quad with an alpha gradient (clear at the bottom, stronger at the top rim).
void wall(Vector3 a,Vector3 b,float z0,float z1,Color c,unsigned char aBot,unsigned char aTop)
{
    Vector3 p[4]{{a.x,a.y,z0},{b.x,b.y,z0},{b.x,b.y,z1},{a.x,a.y,z1}};unsigned char al[4]{aBot,aBot,aTop,aTop};
    rlBegin(RL_TRIANGLES);
    for(int i:{0,1,2,0,2,3}){rlColor4ub(c.r,c.g,c.b,al[i]);rlVertex3f(p[i].x,p[i].y,p[i].z);}
    rlEnd();
}
void drawStatBox(const StatBox&sb,const Camera3D&cam,int vh,float s)
{
    float x0=float(sb.lower.x),x1=float(sb.upper.x),y0=float(sb.lower.y),y1=float(sb.upper.y),z0=float(sb.lower.z),z1=float(sb.upper.z);
    Color c=bandCol;
    wall({x0,y0,0},{x1,y0,0},z0,z1,c,3,34);wall({x0,y1,0},{x1,y1,0},z0,z1,c,3,34);wall({x1,y0,0},{x1,y1,0},z0,z1,c,3,40);wall({x0,y0,0},{x0,y1,0},z0,z1,c,3,26);
    rlBegin(RL_TRIANGLES);rlColor4ub(c.r,c.g,c.b,22);
    for(Vector3 p:{Vector3{x0,y0,z1},Vector3{x1,y0,z1},Vector3{x1,y1,z1},Vector3{x0,y0,z1},Vector3{x1,y1,z1},Vector3{x0,y1,z1}})rlVertex3f(p.x,p.y,p.z);
    rlEnd();
    // collector section: the plane whose crossings are counted
    double cx=sb.collectorX;
    if(cx>=x0&&cx<=x1){Color k=rgba(0xf2b64c);wall({float(cx),y0,0},{float(cx),y1,0},z0,z1,k,2,22);
        Vector3 q[4]{{float(cx),y0,z0},{float(cx),y1,z0},{float(cx),y1,z1},{float(cx),y0,z1}};for(int i=0;i<4;++i)screenLine(q[i],q[(i+1)%4],1.1f*s,{k.r,k.g,k.b,150},cam,vh);}
    Vector3 q[8]{{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    for(int i=0;i<4;++i){screenLine(q[i],q[(i+1)%4],1.2f*s,{c.r,c.g,c.b,120},cam,vh);screenLine(q[4+i],q[4+(i+1)%4],1.8f*s,{c.r,c.g,c.b,235},cam,vh);screenLine(q[i],q[4+i],1.3f*s,{c.r,c.g,c.b,150},cam,vh);}
}
}
void Viewport3D::render(R rect,const Snapshot&pose,const DrawOptions&opt){
    auto&g=gfx();float s=g.scale();
    g.gradientV(rect,col::bgTop,col::bgHorizon);
    if(!scene_||rect.w<10||rect.h<10)return;
    int fbW=GetRenderWidth(),fbH=GetRenderHeight();
    int vx=int(std::floor(rect.x*s)),vw=int(std::ceil(rect.w*s)),vh=int(std::ceil(rect.h*s)),vyTop=int(std::floor(rect.y*s)),vy=fbH-(vyTop+vh);
    bool shadowOn=false;
    if(fancy_&&viz::ok()&&hasTerrain_&&viz::shadowReady())
    {
        Vector3 mn=bbMin_,mx=bbMax_;mx.z+=12; // headroom for rock, barrier and obstacles
        Vector3 ctr=Vector3Scale(Vector3Add(mn,mx),.5f);float rad=Vector3Length(sub(mx,mn))*.5f;
        Matrix lv=MatrixLookAt(Vector3Add(ctr,Vector3Scale(viz::sunDir(),rad*1.5f)),ctr,{0,0,1});
        float l=1e30f,r=-1e30f,bt=1e30f,tp=-1e30f,zn=1e30f,zf=-1e30f;
        for(int i=0;i<8;++i){Vector3 q=Vector3Transform({(i&1)?mx.x:mn.x,(i&2)?mx.y:mn.y,(i&4)?mx.z:mn.z},lv);l=std::min(l,q.x);r=std::max(r,q.x);bt=std::min(bt,q.y);tp=std::max(tp,q.y);zn=std::min(zn,-q.z);zf=std::max(zf,-q.z);}
        viz::shadowBegin(lv,l,r,bt,tp,zn-5,zf+5);
        if(hasObstacles_)viz::drawDepthMesh(obstacles_.meshes[0]);
        if(opt.showRock&&!rockLocal_.empty())
        {
            auto m=poseMatrix(pose.rotation,pose.shapeOrigin);std::vector<Vector3> w;w.reserve(rockLocal_.size());for(auto&v:rockLocal_)w.push_back(Vector3Transform(v,m));
            for(auto&f:rockFaces_)DrawTriangle3D(w[f.a],w[f.b],w[f.c],WHITE);
        }
        auto&bs=scene_->barrier;
        if(opt.showBarrier&&bs.enabled)
        {
            Vec3 bc=pose.barrier.enabled?pose.barrier.center:initialBarrierCenter(bs);Quaternion br=pose.barrier.enabled?pose.barrier.rotation:Quaternion::fromAxisAngle({0,0,1},bs.yawDegrees*pi/180);
            auto m=poseMatrix(br,bc);float hx=float(bs.thickness/2),hy=float(bs.width/2),hz=float(bs.height/2);
            Vector3 loc[8]{{-hx,-hy,-hz},{hx,-hy,-hz},{hx,hy,-hz},{-hx,hy,-hz},{-hx,-hy,hz},{hx,-hy,hz},{hx,hy,hz},{-hx,hy,hz}};Vector3 w[8];for(int i=0;i<8;++i)w[i]=Vector3Transform(loc[i],m);
            static const int quads[6][4]{{0,1,2,3},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
            for(auto&q:quads){DrawTriangle3D(w[q[0]],w[q[1]],w[q[2]],WHITE);DrawTriangle3D(w[q[0]],w[q[2]],w[q[3]],WHITE);}
        }
        viz::shadowEnd(fbW,fbH);shadowOn=true;
    }
    g.pushClip(rect);
    bool fancy=fancy_&&viz::ok();
    if(fancy)
    {
        auto c0=view.camera();Vector3 f0=Vector3Normalize(sub(c0.target,c0.position)),r0=Vector3Normalize(Vector3CrossProduct(f0,c0.up)),u0=Vector3CrossProduct(r0,f0);
        viz::drawSky(vx,vyTop,vw,vh,fbH,r0,u0,f0,std::tan(c0.fovy*.5f*DEG2RAD),float(vw)/std::max(1,vh));
    }
    rlDrawRenderBatchActive();rlViewport(vx,vy,vw,vh);
    rlMatrixMode(RL_PROJECTION);rlPushMatrix();rlLoadIdentity();
    auto cam=view.camera();double nearP=std::max(.1,view.distance*.02),farP=std::max(600.0,view.distance*12.0),aspect=double(vw)/std::max(1,vh);
    double top=nearP*std::tan(cam.fovy*.5*DEG2RAD),right=top*aspect;rlFrustum(-right,right,-top,top,nearP,farP);
    rlMatrixMode(RL_MODELVIEW);rlLoadIdentity();Matrix mv=MatrixLookAt(cam.position,cam.target,cam.up);rlMultMatrixf(MatrixToFloat(mv));
    try
    {
        rlEnableDepthTest();rlDisableBackfaceCulling();
        Vector3 camF=Vector3Normalize(sub(cam.target,cam.position)),camR=Vector3Normalize(Vector3CrossProduct(camF,cam.up)),camU=Vector3CrossProduct(camR,camF);
        if(fancy)
        {
            viz::FrameParams fp;fp.viewPos=cam.position;fp.distance=view.distance;fp.heightRange=hRange_;fp.gridCenter=groundCentre_;fp.contours=opt.showContours;fp.shadow=shadowOn;
            fp.rock={float(pose.center.x),float(pose.center.y),float(pose.center.z),opt.showRock?float(std::max(.3,scene_->radius)):0.f};
            viz::beginFrame(fp);
            if(hasGround_)DrawModel(ground_,{0,0,0},1,WHITE);
            if(hasSkirt_)DrawModel(skirt_,{0,0,0},1,WHITE);
            if(hasTerrain_){viz::bindShadow();DrawModel(terrain_,{0,0,0},1,WHITE);}
        }
        else
        {
            if(hasTerrain_)DrawModel(terrain_,{0,0,0},1,WHITE);
            if(opt.showContours){contourMinor_.draw({255,255,255,26});contourMajor_.draw({255,255,255,58});}
        }
        if(hasObstacles_){DrawModel(obstacles_,{0,0,0},1,WHITE);obstacleEdges_.draw({205,222,240,120});}
        // rock
        if(opt.showRock&&!rockLocal_.empty()){
            auto m=poseMatrix(pose.rotation,pose.shapeOrigin);std::vector<Vector3> w;w.reserve(rockLocal_.size());for(auto&v:rockLocal_)w.push_back(Vector3Transform(v,m));
            Matrix rot=m;rot.m12=rot.m13=rot.m14=0;
            Vector3 vdir=Vector3Normalize(sub(cam.position,toV3(pose.center)));
            for(size_t i=0;i<rockFaces_.size();++i){auto&f=rockFaces_[i];Vector3 n=Vector3Transform(rockNormals_[i],rot);float rim=std::pow(1.f-std::max(0.f,Vector3DotProduct(n,vdir)),3.f);Color c=lit(mix(rgba(0xff8a12),rgba(0xffcf7a),rim*.80f),.62f+.20f*n.z+.85f*std::max(0.f,Vector3DotProduct(n,lightDir)));c.r=(unsigned char)std::min(255,c.r+34);c.g=(unsigned char)std::min(255,c.g+12);DrawTriangle3D(w[f.a],w[f.b],w[f.c],c);}
            for(auto&e:rockEdges_)DrawLine3D(w[e.first],w[e.second],rgba(0x2b1606,210));
            Vector3 c=toV3(pose.center);const auto*tm=scene_->terrain.get();
            if(tm&&pose.center.x>=tm->xMin&&pose.center.x<=tm->xEnd&&pose.center.y>=tm->yMin&&pose.center.y<=tm->yMax){ // the rock may roll past the mesh edge
                double gz=tm->height(pose.center.x,pose.center.y);
                DrawLine3D(c,{c.x,c.y,float(gz+.05)},rgba(0xf2b64c,110));
            }
        }
        rlDrawRenderBatchActive();rlDisableDepthMask();
        if(opt.showHeat&&hasHeat_)DrawModel(heat_,{0,0,0},1,WHITE);
        if(opt.showStatBox&&opt.statBox.valid){rlDrawRenderBatchActive();drawStatBox(opt.statBox,cam,vh,s);rlDrawRenderBatchActive();}
        // barrier
        auto&b=scene_->barrier;
        if(opt.showBarrier&&b.enabled){
            Vec3 bc=pose.barrier.enabled?pose.barrier.center:initialBarrierCenter(b);Quaternion br=pose.barrier.enabled?pose.barrier.rotation:Quaternion::fromAxisAngle({0,0,1},b.yawDegrees*pi/180);
            auto m=poseMatrix(br,bc);float hx=float(b.thickness/2),hy=float(b.width/2),hz=float(b.height/2);
            Vector3 loc[8]{{-hx,-hy,-hz},{hx,-hy,-hz},{hx,hy,-hz},{-hx,hy,-hz},{-hx,-hy,hz},{hx,-hy,hz},{hx,hy,hz},{-hx,hy,hz}};Vector3 w[8];for(int i=0;i<8;++i)w[i]=Vector3Transform(loc[i],m);
            static const int quads[6][4]{{0,1,2,3},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};static const Vector3 nrm[6]{{0,0,-1},{0,0,1},{0,-1,0},{1,0,0},{0,1,0},{-1,0,0}};
            Matrix rot=m;rot.m12=rot.m13=rot.m14=0;
            for(int q=0;q<6;++q){Vector3 n=Vector3Transform(nrm[q],rot);Color c=lit(pose.barrier.touched?rgba(0xa9cdfd):rgba(0x78aefc),lightAt(n),pose.barrier.touched?150:118);
                DrawTriangle3D(w[quads[q][0]],w[quads[q][1]],w[quads[q][2]],c);DrawTriangle3D(w[quads[q][0]],w[quads[q][2]],w[quads[q][3]],c);}
            static const int edges[12][2]{{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
            for(auto&e:edges)DrawLine3D(w[e[0]],w[e[1]],rgba(0xbcd8ff,230));
            if(opt.restBarrierGhost&&b.elastic){
                auto m0=poseMatrix(Quaternion::fromAxisAngle({0,0,1},b.yawDegrees*pi/180),initialBarrierCenter(b));Vector3 w0[8];for(int i=0;i<8;++i)w0[i]=Vector3Transform(loc[i],m0);
                for(auto&e:edges)DrawLine3D(w0[e[0]],w0[e[1]],rgba(0xf2b64c,120));
            }
        }
        // trajectories
        if(opt.showTrails&&fancy&&!ribbons_.empty())
        {
            rlDrawRenderBatchActive();
            float am=std::clamp(opt.trailAlpha*1.25f,.15f,1.f);
            for(auto&kv:ribbons_){if(kv.first==opt.selectedTrial)continue;kv.second->draw(2.2f*s,0.f,-1.f,vw,vh,am);}
            auto sel=ribbons_.find(opt.selectedTrial);if(sel!=ribbons_.end())sel->second->draw(4.6f*s,.8f,float(opt.selectedTime),vw,vh,1.f);
        }
        else if(opt.showTrails){
            for(auto&kv:trails_){if(kv.first==opt.selectedTrial||!kv.second.second)continue;auto&t=kv.second.first;Color c=stopColor(t->status,(unsigned char)std::clamp(opt.trailAlpha*255.f,10.f,255.f));kv.second.second->draw(c);}
            auto it=trails_.find(opt.selectedTrial);
            if(it!=trails_.end()&&it->second.first){
                auto&pts=it->second.first->points;Vector3 off[3]{{0,0,0},{.035f*view.distance*.03f,0,0},{0,.035f*view.distance*.03f,0}};
                for(size_t i=1;i<pts.size();++i){bool done=opt.selectedTime<0||pts[i].time<=opt.selectedTime;Color c=done?rgba(0xffffff,245):rgba(0xffffff,70);
                    for(int k=0;k<(done?3:1);++k)DrawLine3D(Vector3Add(toV3(pts[i-1].center),off[k]),Vector3Add(toV3(pts[i].center),off[k]),c);}
            }
        }
        if(fancy)
        {
            BeginBlendMode(BLEND_ADDITIVE);double tt=GetTime();
            for(auto&p:opt.contactPoints)
            {
                Vector3 c=toV3(p);float base=std::max(.5f,float(scene_->radius)*.6f);
                for(int k=0;k<2;++k){float ph=std::fmod(float(tt)*.9f+k*.5f,1.f);viz::ring(c,base*(.3f+ph*2.4f),base*.12f,camR,camU,{255,214,110,(unsigned char)(220*(1-ph))});}
                viz::glowDisc(c,base*1.1f,camR,camU,{255,200,90,130});
            }
            EndBlendMode();
            for(auto&p:opt.contactPoints)DrawSphere(toV3(p),float(std::max(.05,scene_->radius*.045)),rgba(0xfff1b8));
        }
        else
        for(auto&p:opt.contactPoints){DrawSphere(toV3(p),float(std::max(.05,scene_->radius*.045)),rgba(0xff4d4d));DrawSphereWires(toV3(p),float(std::max(.09,scene_->radius*.08)),4,8,rgba(0xffe066));}
        rlDrawRenderBatchActive();rlEnableDepthMask();
    }
    catch(const std::exception&){} // a failing 3D helper must never end the program; the matrix state is restored below
    rlDrawRenderBatchActive();rlMatrixMode(RL_PROJECTION);rlPopMatrix();rlMatrixMode(RL_MODELVIEW);rlLoadIdentity();rlDisableDepthTest();rlViewport(0,0,fbW,fbH);
    // 2D annotations
    if(opt.showStatBox&&opt.statBox.valid)
    {
        auto&sb=opt.statBox;float H=float(sb.upper.z-sb.lower.z);char txt[80];
        if(std::abs(H-10.f)<.05f)std::snprintf(txt,sizeof(txt),u8"坡脚 10 m 统计带");else std::snprintf(txt,sizeof(txt),u8"坡脚统计带（高 %.1f m）",H);
        auto pill=[&](const char*t,Vector2 at,Color c,bool centre){
            if(at.x<rect.x+8||at.x>rect.right()-8||at.y<rect.y+8||at.y>rect.bottom()-8)return;
            float w=g.textWidth(t,12)+16,x=centre?at.x-w*.5f:at.x;R b{x,at.y-11,w,22};
            g.fill(b,alpha(col::panel,.82f),6);g.stroke(b,alpha(c,.75f),1,6);g.text(t,b.x+8,b.y+4,12,c);};
        float xm=float((sb.lower.x+sb.upper.x)/2);
        pill(txt,project({xm,float(sb.lower.y),float(sb.upper.z)},rect),bandCol,true);
        Vector2 pz0=project({float(sb.upper.x),float(sb.lower.y),float(sb.lower.z)},rect),pz1=project({float(sb.upper.x),float(sb.lower.y),float(sb.upper.z)},rect);
        char t0[24],t1[24];std::snprintf(t0,sizeof(t0),"0 m");std::snprintf(t1,sizeof(t1),"+%.0f m",H);
        if(pz0.x>-9999&&pz1.x>-9999&&std::abs(pz0.y-pz1.y)>26){g.text(t0,pz0.x+8,pz0.y-7,11,alpha(bandCol,.95f));g.text(t1,pz1.x+8,pz1.y-7,11,alpha(bandCol,.95f));}
        if(sb.collectorX>=sb.lower.x&&sb.collectorX<=sb.upper.x)pill(u8"收集截面",project({float(sb.collectorX),float(sb.upper.y),float(sb.upper.z)},rect),rgba(0xf2b64c),true);
    }
    if(opt.showRuler&&scene_->terrain){
        auto&t=*scene_->terrain;auto cam2=view.camera();(void)cam2;
        double span=t.xEnd-t.xMin;double step=span>150?25:10;
        for(double x=std::ceil(t.xMin/step)*step;x<=t.xEnd;x+=step){
            Vector3 p{float(x),float(t.yMin),float(t.height(x,t.yMin))};Vector2 sp=project(p,rect);if(sp.x<rect.x+8||sp.x>rect.right()-8||sp.y<rect.y+8||sp.y>rect.bottom()-8)continue;
            g.line(sp.x,sp.y,sp.x,sp.y+5,alpha(col::dim,.8f),1);
            char label[24];std::snprintf(label,sizeof(label),"%.0f m",x);g.text(label,sp.x-g.textWidth(label,11)*.5f,sp.y+6,11,alpha(col::dim,.9f));
        }
    }
    g.popClip();
}
}
