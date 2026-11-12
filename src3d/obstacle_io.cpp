#include "obstacle_io.h"
#include "archive_csv.h"
#include <sstream>
#include <iomanip>
#include <locale>
namespace rs3d {
namespace {
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::ostringstream row(){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);return s;}
double number(const CsvRow&r,const char*k){return archiveNumber(r.at(k));}
}
void writeObstacleFiles(const ObstacleSet&s,const std::function<void(const std::string&,const std::string&)>&put){
    put("obstacles.csv","obstacle_id,center_x,center_y,center_z,qw,qx,qy,qz,yaw_degrees,length,width,total_height,exposed_height,mu,e\n");
    put("obstacle_vertices.csv","obstacle_id,vertex_id,x,y,z\n");put("obstacle_faces.csv","obstacle_id,face_id,a,b,c\n");
    for(const auto&o:s.boxes){auto r=row();r<<o.id<<','<<o.center.x<<','<<o.center.y<<','<<o.center.z<<','<<o.rotation.w<<','<<o.rotation.x<<','<<o.rotation.y<<','<<o.rotation.z<<','<<o.yawDegrees<<','<<o.dimensions.x<<','<<o.dimensions.y<<','<<o.dimensions.z<<','<<o.exposedHeight<<','<<o.friction<<','<<o.restitution<<'\n';put("obstacles.csv",r.str());
        for(size_t i=0;i<o.geometry.vertices.size();++i){auto p=o.geometry.vertices[i];auto v=row();v<<o.id<<','<<i<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';put("obstacle_vertices.csv",v.str());}
        for(size_t i=0;i<o.geometry.faces.size();++i){auto f=o.geometry.faces[i];auto v=row();v<<o.id<<','<<i<<','<<f.a<<','<<f.b<<','<<f.c<<'\n';put("obstacle_faces.csv",v.str());}}
}
std::shared_ptr<const ObstacleSet> readObstacleFiles(const std::filesystem::path&directory,const std::string&inputHash,const std::string&expectedHash,const std::string&algorithm){
    require(algorithm==obstacleAlgorithm||algorithm==legacyObstacleAlgorithm,"unsupported obstacle algorithm");
    auto s=std::make_shared<ObstacleSet>();s->algorithm=algorithm;s->inputHash=inputHash;
    readArchiveCsv(directory/L"obstacles.csv",500,[&](const CsvRow&r){Obstacle o;auto id=archiveInteger(r.at("obstacle_id"));require(id==firstObstacleObjectId+s->boxes.size(),"obstacle ID/order mismatch");o.id=int(id);
        o.center={number(r,"center_x"),number(r,"center_y"),number(r,"center_z")};o.rotation={number(r,"qw"),number(r,"qx"),number(r,"qy"),number(r,"qz")};o.yawDegrees=number(r,"yaw_degrees");o.dimensions={number(r,"length"),number(r,"width"),number(r,"total_height")};o.exposedHeight=number(r,"exposed_height");o.friction=number(r,"mu");o.restitution=number(r,"e");s->boxes.push_back(std::move(o));});
    auto selected=[&](const CsvRow&r)->Obstacle&{auto id=archiveInteger(r.at("obstacle_id"));require(id>=firstObstacleObjectId&&id-firstObstacleObjectId<s->boxes.size(),"unknown obstacle geometry ID");return s->boxes[size_t(id-firstObstacleObjectId)];};
    readArchiveCsv(directory/L"obstacle_vertices.csv",4000,[&](const CsvRow&r){auto&o=selected(r);require(archiveInteger(r.at("vertex_id"))==o.geometry.vertices.size()&&o.geometry.vertices.size()<8,"obstacle vertex order/count");o.geometry.vertices.push_back({number(r,"x"),number(r,"y"),number(r,"z")});});
    readArchiveCsv(directory/L"obstacle_faces.csv",6000,[&](const CsvRow&r){auto&o=selected(r);require(archiveInteger(r.at("face_id"))==o.geometry.faces.size()&&o.geometry.faces.size()<12,"obstacle face order/count");auto a=archiveInteger(r.at("a")),b=archiveInteger(r.at("b")),c=archiveInteger(r.at("c"));require(a<8&&b<8&&c<8,"obstacle face index");o.geometry.faces.push_back({int(a),int(b),int(c)});});
    for(auto&o:s->boxes){o.geometry=polyhedronFromMesh(o.geometry.vertices,o.geometry.faces);validateObstacle(o);
        require(norm(o.geometry.center)<1e-10&&std::abs(o.geometry.volume-o.dimensions.x*o.dimensions.y*o.dimensions.z)<1e-10*o.geometry.volume,"obstacle solid mismatch");
        require(frobenius(o.rotation.matrix()-axisRotation({0,0,1},o.yawDegrees*pi/180))<1e-12,"obstacle yaw/quaternion mismatch");
        for(auto v:o.geometry.vertices)for(int a=0;a<3;++a)require(std::abs(std::abs(v[a])-o.dimensions[a]/2)<1e-12,"obstacle box vertex mismatch");}
    s->hash=obstacleContentHash(*s);require(s->hash==expectedHash,"obstacle archive hash mismatch");return s;
}
}
