#include "terrain.h"
#include "hash.h"
#include <limits>
namespace rs3d {
namespace {
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<double> lines(double begin,double end,double step,const std::vector<double>& exact){
    std::vector<double> result=exact;result.push_back(begin);result.push_back(end);
    const double tolerance=1e-11*std::max(1.0,std::max(std::abs(begin),std::abs(end)));
    for(double x=begin+step;x<end;x+=step){bool duplicate=false;for(double p:exact)if(std::abs(x-p)<=tolerance){duplicate=true;break;}if(!duplicate&&end-x>tolerance)result.push_back(x);}
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end(),[&](double a,double b){return std::abs(a-b)<=tolerance;}),result.end());return result;
}
size_t cell(const std::vector<double>& line,double x){auto it=std::upper_bound(line.begin(),line.end(),x);return std::min(line.size()-2,size_t(it-line.begin()-(it==line.begin()?0:1)));}
double xyCross(Vec3 a,Vec3 b){return a.x*b.y-a.y*b.x;}
std::vector<Vec3> clipTriangle(std::vector<Vec3> poly,Vec3 a,Vec3 b,Vec3 c){
    if(xyCross(b-a,c-a)<0)std::swap(b,c);
    for(auto edge:{std::pair<Vec3,Vec3>{a,b},{b,c},{c,a}}){
        if(poly.empty())break;std::vector<Vec3> out;Vec3 before=poly.back();
        double db=xyCross(edge.second-edge.first,before-edge.first);
        for(auto point:poly){double dp=xyCross(edge.second-edge.first,point-edge.first);bool pin=dp>=-1e-12,bin=db>=-1e-12;
            if(pin!=bin){double t=db/(db-dp);out.push_back(before+(point-before)*t);}if(pin)out.push_back(point);before=point;db=dp;}
        poly=std::move(out);
    }return poly;
}
double planeZ(Vec3 a,Vec3 normal,double x,double y){return a.z-(normal.x*(x-a.x)+normal.y*(y-a.y))/normal.z;}
}
double profileHeight(const std::vector<Vec3>& profile,double x){
    if(profile.size()<2||x<profile.front().x-1e-9||x>profile.back().x+1e-9)throw std::runtime_error("x outside terrain profile");
    for(size_t i=1;i<profile.size();++i)if(x<=profile[i].x+1e-12){auto a=profile[i-1],b=profile[i];return a.z+(b.z-a.z)*std::clamp((x-a.x)/(b.x-a.x),0.0,1.0);}
    return profile.back().z;
}
TerrainMesh buildTerrain(const TerrainSettings&s){
    for(double value:{s.topHeight,s.steepDegrees,s.benchStart,s.benchLength,s.benchDegrees,s.toeLength,s.width,s.crossSlopeDegrees,s.grooveAmplitude,s.cellSize,s.flatLength,s.flatHeight,s.flatSlopeX})require(std::isfinite(value),"non-finite terrain parameter");
    require(s.width>0&&s.cellSize>0&&std::abs(s.crossSlopeDegrees)<89&&s.grooveAmplitude>=0,"invalid terrain width/cell/cross-slope/groove");
    TerrainMesh t;t.yMin=-s.width/2;t.yMax=s.width/2;
    if(s.type==TerrainType::flat){require(s.flatLength>0,"flat length must be positive");t.xEnd=s.flatLength;t.xFoot=0;t.profile={{0,0,s.flatHeight},{s.flatLength,0,s.flatHeight+s.flatSlopeX*s.flatLength}};}
    else {
        require(s.topHeight>=5&&s.topHeight<=60,"top height outside [5,60] m");
        require(s.steepDegrees>=15&&s.steepDegrees<=70,"steep angle outside [15,70] degrees");
        require(s.benchDegrees>=0&&s.benchDegrees<s.steepDegrees,"invalid bench angle");
        require(s.benchStart>=0&&s.benchLength>=0&&s.toeLength>=5,"invalid bench/toe length");
        double steep=std::tan(s.steepDegrees*pi/180),bench=std::tan(s.benchDegrees*pi/180);
        double z1=s.topHeight-s.benchStart*steep,x2=s.benchStart+s.benchLength,z2=z1-s.benchLength*bench;
        require(z2>=1,"bench ends less than 1 m above toe");t.xFoot=x2+z2/steep;t.xEnd=t.xFoot+s.toeLength;
        require(t.xEnd<=120,"parameterized terrain longer than 120 m");t.profile={{0,0,s.topHeight}};
        if(s.benchStart>0)t.profile.push_back({s.benchStart,0,z1});if(s.benchLength>0)t.profile.push_back({x2,0,z2});
        t.profile.push_back({t.xFoot,0,0});t.profile.push_back({t.xEnd,0,0});
    }
    require(t.xEnd/s.cellSize<=1000&&s.width/s.cellSize<=1000,"terrain grid too fine");
    std::vector<double> breaks;for(auto p:t.profile)breaks.push_back(p.x);
    t.xLines=lines(0,t.xEnd,s.cellSize,breaks);t.yLines=lines(t.yMin,t.yMax,s.cellSize,{0});
    size_t nx=t.xLines.size(),ny=t.yLines.size();require((nx-1)*(ny-1)*2<=76800,"terrain exceeds validated display limit of 76800 triangles");
    for(double x:t.xLines)for(double y:t.yLines)t.vertices.push_back({x,y,profileHeight(t.profile,x)+std::tan(s.crossSlopeDegrees*pi/180)*y+s.grooveAmplitude*(y/(s.width/2))*(y/(s.width/2))});
    for(size_t i=0;i+1<nx;++i)for(size_t j=0;j+1<ny;++j){int a=int(i*ny+j),b=int((i+1)*ny+j),c=b+1,d=a+1;
        if(s.flipDiagonals){t.faces.push_back({a,b,d});t.faces.push_back({b,c,d});}else{t.faces.push_back({a,b,c});t.faces.push_back({a,c,d});}}
    ContentHash hash;hash.word(t.vertices.size());for(auto p:t.vertices){hash.number(p.x);hash.number(p.y);hash.number(p.z);}for(auto f:t.faces){hash.word(f.a);hash.word(f.b);hash.word(f.c);}t.contentHash="fnv1a64:"+hash.hex();return t;
}
double TerrainMesh::height(double x,double y)const{
    require(x>=xMin-1e-9&&x<=xEnd+1e-9&&y>=yMin-1e-9&&y<=yMax+1e-9,"point outside terrain mesh");
    size_t i=cell(xLines,x),j=cell(yLines,y),ny=yLines.size();
    double u=std::clamp((x-xLines[i])/(xLines[i+1]-xLines[i]),0.0,1.0),v=std::clamp((y-yLines[j])/(yLines[j+1]-yLines[j]),0.0,1.0);
    // Heights are sums of an x profile and a y profile, so either diagonal has the same plane.
    return vertices[i*ny+j].z+(vertices[(i+1)*ny+j].z-vertices[i*ny+j].z)*u+(vertices[i*ny+j+1].z-vertices[i*ny+j].z)*v;
}
double TerrainMesh::toeMin()const{double low=std::numeric_limits<double>::infinity();for(double y:yLines)low=std::min(low,height(xEnd,y));return low;}
std::vector<Vec3> terrainCellTriangles(const TerrainMesh&t,double x0,double x1,double y0,double y1){
    std::vector<Vec3> result;double xmin=std::max(x0,t.xMin),xmax=std::min(x1,t.xEnd),ymin=std::max(y0,t.yMin),ymax=std::min(y1,t.yMax);if(xmin>=xmax||ymin>=ymax)return result;
    const std::vector<Vec3> cellPolygon{{xmin,ymin,0},{xmax,ymin,0},{xmax,ymax,0},{xmin,ymax,0}};
    auto visit=[&](size_t id){auto f=t.faces[id];auto a=t.vertices[f.a],b=t.vertices[f.b],c=t.vertices[f.c];if(std::max({a.x,b.x,c.x})<xmin||std::min({a.x,b.x,c.x})>xmax||std::max({a.y,b.y,c.y})<ymin||std::min({a.y,b.y,c.y})>ymax)return;auto normal=cross(b-a,c-a);if(std::abs(normal.z)<1e-20)return;auto polygon=clipTriangle(cellPolygon,a,b,c);for(auto&p:polygon)p.z=planeZ(a,normal,p.x,p.y);for(size_t i=2;i<polygon.size();++i)if(norm2(cross(polygon[i-1]-polygon[0],polygon[i]-polygon[0]))>1e-20){result.push_back(polygon[0]);result.push_back(polygon[i-1]);result.push_back(polygon[i]);}};
    if(t.xLines.size()>=2&&t.yLines.size()>=2&&t.faces.size()==2*(t.xLines.size()-1)*(t.yLines.size()-1)){for(size_t i=cell(t.xLines,xmin);i<=cell(t.xLines,xmax);++i)for(size_t j=cell(t.yLines,ymin);j<=cell(t.yLines,ymax);++j){auto id=2*(i*(t.yLines.size()-1)+j);visit(id);visit(id+1);}}else for(size_t i=0;i<t.faces.size();++i)visit(i);return result;
}
std::pair<double,double> terrainHeightRange(const TerrainMesh&t,const std::vector<Vec3>& polygon){
    require(polygon.size()>=3,"terrain footprint needs at least three vertices");
    double xmin=1e300,xmax=-1e300,ymin=1e300,ymax=-1e300,area=0;
    for(size_t i=0;i<polygon.size();++i){auto p=polygon[i];require(finite(p)&&p.x>=t.xMin&&p.x<=t.xEnd&&p.y>=t.yMin&&p.y<=t.yMax,"terrain footprint outside mesh");xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);ymin=std::min(ymin,p.y);ymax=std::max(ymax,p.y);area+=xyCross(p,polygon[(i+1)%polygon.size()]);}
    require(std::abs(area)>1e-16,"zero-area footprint");
    double low=1e300,high=-1e300;
    auto visit=[&](size_t index){auto f=t.faces[index];auto a=t.vertices[f.a],b=t.vertices[f.b],c=t.vertices[f.c];
        if(std::max({a.x,b.x,c.x})<xmin||std::min({a.x,b.x,c.x})>xmax||std::max({a.y,b.y,c.y})<ymin||std::min({a.y,b.y,c.y})>ymax)return;
        auto n=cross(b-a,c-a);if(std::abs(n.z)<1e-20)return;
        for(auto p:clipTriangle(polygon,a,b,c)){auto z=planeZ(a,n,p.x,p.y);low=std::min(low,z);high=std::max(high,z);}};
    if(t.xLines.size()>=2&&t.yLines.size()>=2&&t.faces.size()==2*(t.xLines.size()-1)*(t.yLines.size()-1)){
        for(size_t i=cell(t.xLines,xmin);i<=cell(t.xLines,xmax);++i)for(size_t j=cell(t.yLines,ymin);j<=cell(t.yLines,ymax);++j){auto f=2*(i*(t.yLines.size()-1)+j);visit(f);visit(f+1);}
    }else for(size_t i=0;i<t.faces.size();++i)visit(i);
    require(low<=high&&std::isfinite(low)&&std::isfinite(high),"no terrain under footprint");return {low,high};
}
double supportHeight(const Polyhedron&p,const TerrainMesh&t,double x,double y,const Mat3&rotation){
    std::vector<Vec3> v;for(auto point:p.vertices){auto q=rotation*(point-p.center)+Vec3{x,y,0};require(q.x>=t.xMin&&q.x<=t.xEnd&&q.y>=t.yMin&&q.y<=t.yMax,"release shape footprint crosses terrain boundary");v.push_back(q);}
    double result=-std::numeric_limits<double>::infinity();size_t ny=t.yLines.size();
    for(auto f:p.faces){Vec3 a=v[f.a],b=v[f.b],c=v[f.c],n=cross(b-a,c-a);if(n.z>=-1e-14*norm(n))continue;
        double xmin=std::min({a.x,b.x,c.x}),xmax=std::max({a.x,b.x,c.x}),ymin=std::min({a.y,b.y,c.y}),ymax=std::max({a.y,b.y,c.y});
        for(size_t i=cell(t.xLines,xmin);i<=cell(t.xLines,xmax);++i)for(size_t j=cell(t.yLines,ymin);j<=cell(t.yLines,ymax);++j){
            size_t faceIndex=2*(i*(ny-1)+j);
            for(size_t fidx=faceIndex;fidx<faceIndex+2;++fidx){auto ground=t.faces[fidx];Vec3 ga=t.vertices[ground.a],gb=t.vertices[ground.b],gc=t.vertices[ground.c],gn=cross(gb-ga,gc-ga);
                for(auto q:clipTriangle({a,b,c},ga,gb,gc))result=std::max(result,planeZ(ga,gn,q.x,q.y)-planeZ(a,n,q.x,q.y));}
        }
    }
    require(std::isfinite(result),"release support has no overlapping surface");return result;
}
}
