#include "obstacles.h"
#include "physics.h"
#include "random.h"
#include "hash.h"
#include <sstream>
namespace rs3d {
ObstacleSettings legacyObstacleSettings(){ObstacleSettings s;s.wholeTerrain=false;s.relativeToRadius=true;s.count=40;s.lengthMin=s.widthMin=.30;s.lengthMax=s.widthMax=.80;s.heightMin=.10;s.heightMax=.40;s.minSpacing=.20;s.maxTotalHeight=2;return s;}
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
double pointSegment(Vec3 p,Vec3 a,Vec3 b){p.z=a.z=b.z=0;auto d=b-a;double t=norm2(d)>0?std::clamp(dot(p-a,d)/norm2(d),0.,1.):0;return norm(p-a-d*t);}
bool separated(const Footprint&a,const Footprint&b){
    for(int i=0;i<4;++i){auto d=a[(i+1)%4]-a[i];Vec3 n{-d.y,d.x,0};double loA=1e300,hiA=-1e300,loB=1e300,hiB=-1e300;
        for(auto p:a){double v=dot(n,p);loA=std::min(loA,v);hiA=std::max(hiA,v);}for(auto p:b){double v=dot(n,p);loB=std::min(loB,v);hiB=std::max(hiB,v);}if(hiA<loB||hiB<loA)return true;}
    return false;
}
void hashVector(ContentHash&h,Vec3 v){h.number(v.x);h.number(v.y);h.number(v.z);}
}
Footprint rectangleFootprint(double x,double y,double length,double width,double yaw){
    auto r=axisRotation({0,0,1},yaw*pi/180);Footprint out{{{-length/2,-width/2,0},{length/2,-width/2,0},{length/2,width/2,0},{-length/2,width/2,0}}};for(auto&p:out)p=Vec3{x,y,0}+r*p;return out;
}
Footprint Obstacle::footprint()const{return rectangleFootprint(center.x,center.y,dimensions.x,dimensions.y,yawDegrees);}
double footprintDistance(const Footprint&a,const Footprint&b){
    if(!separated(a,b)&&!separated(b,a))return 0;
    double distance=1e300;for(int i=0;i<4;++i)for(int j=0;j<4;++j){distance=std::min(distance,pointSegment(a[i],b[j],b[(j+1)%4]));distance=std::min(distance,pointSegment(b[i],a[j],a[(j+1)%4]));}return distance;
}
void validateObstacleSettings(const ObstacleSettings&s){
    require(s.count>=0&&s.count<=500,"obstacles.count outside [0,500]");
    for(double v:{s.lengthMin,s.lengthMax,s.widthMin,s.widthMax,s.heightMin,s.heightMax,s.xMin,s.xMax,s.yMin,s.yMax,s.minSpacing,s.embedDepth,s.maxTotalHeight,s.friction,s.restitution})require(std::isfinite(v),"non-finite obstacle setting");
    require(s.lengthMin>0&&s.lengthMin<=s.lengthMax&&s.widthMin>0&&s.widthMin<=s.widthMax&&s.heightMin>0&&s.heightMin<=s.heightMax,"invalid obstacle size range");
    require(s.minSpacing>=0&&s.embedDepth>=0&&s.maxTotalHeight>0&&s.friction>=0&&s.restitution>=0&&s.restitution<=1,"invalid obstacle spacing/depth/material/height limit");
    if(!s.automaticRegion)require(s.xMin<s.xMax&&s.yMin<s.yMax,"empty custom obstacle region");
}
void validateObstacle(const Obstacle&o){
    require(o.id>=firstObstacleObjectId&&finite(o.center)&&finite(o.dimensions)&&std::isfinite(o.yawDegrees+o.exposedHeight+o.friction+o.restitution),"nonfinite/invalid obstacle");
    require(o.dimensions.x>0&&o.dimensions.y>0&&o.dimensions.z>0&&o.dimensions.x<1e5&&o.dimensions.y<1e5&&o.dimensions.z<1e5&&o.exposedHeight>0&&o.friction>=0&&o.restitution>=0&&o.restitution<=1,"invalid obstacle dimensions/material");
    require(o.geometry.vertices.size()==8&&o.geometry.faces.size()==12,"obstacle must have eight vertices and twelve triangles");validatePolyhedron(o.geometry);o.rotation.matrix();
}
std::string obstacleContentHash(const ObstacleSet&s){ContentHash h;h.text(s.algorithm);h.text(s.inputHash);h.word(s.boxes.size());
    for(const auto&o:s.boxes){h.word(o.id);hashVector(h,o.center);hashVector(h,o.dimensions);h.number(o.yawDegrees);h.number(o.rotation.w);h.number(o.rotation.x);h.number(o.rotation.y);h.number(o.rotation.z);h.number(o.exposedHeight);h.number(o.friction);h.number(o.restitution);for(auto p:o.geometry.vertices)hashVector(h,p);for(auto f:o.geometry.faces){h.word(f.a);h.word(f.b);h.word(f.c);}}
    return "fnv1a64:"+h.hex();
}
std::shared_ptr<const ObstacleSet> freezeObstacles(const ObstacleSettings&s,const TerrainMesh&t,double radius,double rx0,double rx1,double ry0,double ry1,const BarrierSettings&b,std::shared_ptr<const ObstacleSet> saved){
    validateObstacleSettings(s);require(std::isfinite(radius)&&radius>0,"invalid obstacle radius scale");auto out=std::make_shared<ObstacleSet>();double scale=s.relativeToRadius?radius:1.;
    out->algorithm=s.wholeTerrain?obstacleAlgorithm:legacyObstacleAlgorithm;out->actualMaxTotalHeight=s.maxTotalHeight*scale;
    auto release=rectangleFootprint((rx0+rx1)/2,(ry0+ry1)/2,rx1-rx0+2*(radius+.5),ry1-ry0+2*(radius+.5),0);
    auto bc=initialBarrierCenter(b);auto reserve=rectangleFootprint(bc.x,bc.y,b.thickness+2,b.width+2,b.yawDegrees);
    out->xMin=s.automaticRegion?t.xMin+5:s.xMin;out->xMax=s.automaticRegion?t.xEnd-5:s.xMax;
    out->yMin=s.automaticRegion?std::max(ry0-3*radius,t.yMin+1):s.yMin;out->yMax=s.automaticRegion?std::min(ry1+3*radius,t.yMax-1):s.yMax;
    if(s.automaticRegion&&b.enabled)for(auto p:reserve)out->xMax=std::min(out->xMax,p.x);
    if(s.wholeTerrain&&s.automaticRegion){
        // Whole surface, independent of release position/radius and barrier x.
        // Only actual release and barrier footprints remain excluded below.
        double mx=std::min(1.,(t.xEnd-t.xMin)*.025),my=std::min(1.,(t.yMax-t.yMin)*.025);
        out->xMin=t.xMin+mx;out->xMax=t.xEnd-mx;out->yMin=t.yMin+my;out->yMax=t.yMax-my;
    }
    ContentHash input;input.text(out->algorithm);input.text(t.contentHash);input.word(s.seed);input.word(s.enabled);input.word(s.count);input.word(s.relativeToRadius);input.word(s.automaticRegion);
    for(double v:{radius,s.lengthMin,s.lengthMax,s.widthMin,s.widthMax,s.heightMin,s.heightMax,s.minSpacing,s.embedDepth,s.maxTotalHeight,s.friction,s.restitution,out->xMin,out->xMax,out->yMin,out->yMax})input.number(v);
    for(auto p:release)hashVector(input,p);input.word(b.enabled);if(b.enabled)for(auto p:reserve)hashVector(input,p);out->inputHash="fnv1a64:"+input.hex();
    if(saved){require(saved->inputHash==out->inputHash,"saved obstacle environment inputs differ");require(saved->boxes.size()==size_t(s.enabled?s.count:0),"saved obstacle count mismatch");require(obstacleContentHash(*saved)==saved->hash,"saved obstacle hash mismatch");return saved;}
    if(!s.enabled||s.count==0){out->hash=obstacleContentHash(*out);return out;}
    require(out->xMin<out->xMax&&out->yMin<out->yMax,"empty obstacle placement corridor");
    Random rng(mix(s.seed^obstacleDomain));auto sample=[&](double low,double high){return low+(high-low)*rng.uniform();};
    for(int target=0;target<s.count;++target){bool accepted=false;
        // v2 fixes each ID's size draws independently of placement rejection,
        // so editing the rock cannot resample rubble dimensions via clearance.
        Random sizes(mix(s.seed^obstacleDomain^0x53ULL^mix(std::uint64_t(target))));
        Random positions(mix(s.seed^obstacleDomain^mix(std::uint64_t(target))));
        double fixedLength=(s.lengthMin+(s.lengthMax-s.lengthMin)*sizes.uniform())*scale;
        double fixedWidth=(s.widthMin+(s.widthMax-s.widthMin)*sizes.uniform())*scale;
        double fixedHeight=(s.heightMin+(s.heightMax-s.heightMin)*sizes.uniform())*scale;
        double fixedYaw=360*sizes.uniform();
        for(int attempt=0;attempt<200;++attempt){++out->candidates;
            double x,y,yaw,length,width,height;
            if(s.wholeTerrain){
                double u=positions.uniform();
                // Spread automatic placement along the slope. A fully reserved
                // strip can fall back to the full surface without reducing count.
                if(s.automaticRegion&&attempt<100)u=(target+u)/s.count;
                x=out->xMin+(out->xMax-out->xMin)*u;y=out->yMin+(out->yMax-out->yMin)*positions.uniform();yaw=fixedYaw;
                length=fixedLength;width=fixedWidth;height=fixedHeight;
            }else{
                // Preserve v1 draw order, including rejected candidates.
                x=sample(out->xMin,out->xMax);y=sample(out->yMin,out->yMax);yaw=sample(0,360);
                length=sample(s.lengthMin,s.lengthMax)*scale;width=sample(s.widthMin,s.widthMax)*scale;height=sample(s.heightMin,s.heightMax)*scale;
            }
            auto foot=rectangleFootprint(x,y,length,width,yaw);bool inside=true;for(auto p:foot)inside=inside&&p.x>=std::max(out->xMin,t.xMin)&&p.x<=std::min(out->xMax,t.xEnd)&&p.y>=std::max(out->yMin,t.yMin)&&p.y<=std::min(out->yMax,t.yMax);
            if(!inside){++out->rejected["boundary"];continue;}
            if(footprintDistance(foot,release)<=1e-10||(b.enabled&&footprintDistance(foot,reserve)<=1e-10)){++out->rejected["reserved"];continue;}
            bool spaced=true;for(const auto&o:out->boxes)if(footprintDistance(foot,o.footprint())<s.minSpacing+1e-10){spaced=false;break;}
            if(!spaced){++out->rejected["spacing"];continue;}
            auto z=terrainHeightRange(t,{foot.begin(),foot.end()});double base=z.first-s.embedDepth,top=z.second+height;
            if(top-base>out->actualMaxTotalHeight){++out->rejected["total_height"];continue;}
            Obstacle o;o.id=firstObstacleObjectId+target;o.center={x,y,(base+top)/2};o.dimensions={length,width,top-base};o.exposedHeight=height;o.yawDegrees=yaw;o.rotation=Quaternion::fromAxisAngle({0,0,1},yaw*pi/180);o.friction=s.friction;o.restitution=s.restitution;
            std::vector<Vec3>v;for(int a:{-1,1})for(int c:{-1,1})for(int d:{-1,1})v.push_back({a*length/2,c*width/2,d*(top-base)/2});o.geometry=convexPolyhedron(v);validateObstacle(o);out->boxes.push_back(std::move(o));accepted=true;break;
        }
        if(!accepted){std::ostringstream message;message<<"obstacle layout failed: accepted="<<out->boxes.size()<<" requested="<<s.count<<" seed="<<s.seed<<" candidates="<<out->candidates;for(auto&reason:out->rejected)message<<' '<<reason.first<<'='<<reason.second;throw std::runtime_error(message.str());}
    }
    out->hash=obstacleContentHash(*out);return out;
}
}
