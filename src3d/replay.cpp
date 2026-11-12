#include "output.h"
#include "hash.h"
#include <charconv>
#include <sstream>
#include <locale>
namespace rs3d {
namespace {
void require(bool ok,const std::string&message){if(!ok)throw std::runtime_error("replay: "+message);}
std::vector<std::string> split(const std::string&line){
    std::vector<std::string>cells;std::string cell;bool quoted=false,closed=false;
    for(size_t i=0;i<line.size();++i){char c=line[i];if(quoted){if(c=='"'){if(i+1<line.size()&&line[i+1]=='"'){cell+='"';++i;}else{quoted=false;closed=true;}}else cell+=c;}
        else if(c==','){cells.push_back(cell);cell.clear();closed=false;}else if(c=='"'){require(cell.empty()&&!closed,"invalid CSV quote");quoted=true;}else{require(!closed,"content after closing CSV quote");cell+=c;}}
    require(!quoted,"unterminated CSV quote");cells.push_back(cell);return cells;
}
using Row=std::map<std::string,std::string>;
void rows(const std::filesystem::path&path,const std::function<void(const Row&)>&consume){
    std::ifstream file(path,std::ios::binary);require(bool(file),"missing "+path.u8string());std::string line;require(bool(std::getline(file,line)),"empty CSV");if(!line.empty()&&line.back()=='\r')line.pop_back();auto header=split(line);
    while(std::getline(file,line)){require(line.size()<1024*1024,"CSV row exceeds limit");if(!line.empty()&&line.back()=='\r')line.pop_back();auto cells=split(line);require(cells.size()==header.size(),"CSV field count mismatch: "+path.filename().u8string());Row row;for(size_t i=0;i<header.size();++i)require(row.emplace(header[i],cells[i]).second,"duplicate CSV header");consume(row);}require(!file.bad(),"CSV read error");
}
std::uint64_t integer(const std::string&s){std::uint64_t n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n);require(r.ec==std::errc{}&&r.ptr==s.data()+s.size(),"invalid integer");return n;}
double number(const std::string&s){std::istringstream in(s);in.imbue(std::locale::classic());double n=0;in>>n;require(bool(in)&&std::isfinite(n),"invalid finite number");in>>std::ws;require(in.eof(),"trailing number content");return n;}
double get(const Row&r,const std::string&k){return number(r.at(k));}
Vec3 vector(const Row&r,const std::string&prefix){return {get(r,prefix+"x"),get(r,prefix+"y"),get(r,prefix+"z")};}
Quaternion rotation(const Row&r,const std::string&prefix){return {get(r,prefix+"qw"),get(r,prefix+"qx"),get(r,prefix+"qy"),get(r,prefix+"qz")};}
Row manifest(const std::filesystem::path&path){std::ifstream in(path,std::ios::binary);require(bool(in),"missing manifest");Row out;std::string line;while(std::getline(in,line)){if(!line.empty()&&line[0]=='[')break;auto eq=line.find('=');if(eq!=line.npos)require(out.emplace(line.substr(0,eq),line.substr(eq+1)).second,"duplicate manifest key");}return out;}
Face face(const Row&r,size_t vertices){std::uint64_t a=integer(r.at("a")),b=integer(r.at("b")),c=integer(r.at("c"));require(a<vertices&&b<vertices&&c<vertices,"face index out of range");return {int(a),int(b),int(c)};}
}
bool matchesReplay(const Snapshot&a,const ReplayInput&r){
    const auto&e=r.expected;auto close=[](double x,double y){return std::isfinite(x)&&std::abs(x-y)<=1e-8*std::max(1.0,std::abs(y));};
    auto rotationClose=[&](Quaternion x,Quaternion y){return close(x.w,y.w)&&close(x.x,y.x)&&close(x.y,y.y)&&close(x.z,y.z);};
    if(a.reason!=e.reason||std::abs(a.time-e.time)>1e-10||norm(a.center-e.center)>1e-8||norm(a.shapeOrigin-e.shapeOrigin)>1e-8
       ||norm(a.velocity-e.velocity)>1e-8||norm(a.angularVelocity-e.angularVelocity)>1e-8||!rotationClose(a.rotation,e.rotation))return false;
    if(!r.hasBarrierState)return true;
    const auto&b=a.barrier;const auto&c=e.barrier;
    if(b.enabled!=c.enabled||b.elastic!=c.elastic||b.touched!=c.touched||b.settled!=c.settled||norm(b.center-c.center)>1e-8||!rotationClose(b.rotation,c.rotation))return false;
    const double observed[]{b.displacement,b.velocity,b.supportForce,b.contactForce,b.kineticEnergy,b.springEnergy,b.dampingEnergy,b.supportWorkResidual,b.contactImpulse,b.maxDisplacement,b.peakSupportForce,b.peakContactForce};
    const double expected[]{c.displacement,c.velocity,c.supportForce,c.contactForce,c.kineticEnergy,c.springEnergy,c.dampingEnergy,c.supportWorkResidual,c.contactImpulse,c.maxDisplacement,c.peakSupportForce,c.peakContactForce};
    for(size_t i=0;i<std::size(observed);++i)if(!close(observed[i],expected[i]))return false;return true;
}
ReplayInput loadReplay(const std::filesystem::path&directory,std::uint64_t id){
    require(!std::filesystem::exists(directory/L"INCOMPLETE.txt")&&std::filesystem::exists(directory/L"FILES_COMPLETE.txt"),"uncommitted output transaction");auto m=manifest(directory/L"manifest.ini");require(m.at("status")!="io_error","I/O-failed transaction");auto config=freezeConfig(loadScene(directory/L"scene.ini"));require(config->hash==m.at("run_config_hash"),"configuration hash mismatch");
    Row selected;rows(directory/L"trials.csv",[&](const Row&r){if(integer(r.at("trial_id"))==id){require(selected.empty(),"duplicate trial");selected=r;}});require(!selected.empty()&&selected.at("has_initial")=="1"&&selected.at("has_shape")=="1","trial has no replayable initial state");
    auto terrain=std::make_shared<TerrainMesh>(*config->terrain);terrain->vertices.clear();terrain->faces.clear();
    rows(directory/L"terrain_vertices.csv",[&](const Row&r){require(integer(r.at("vertex_id"))==terrain->vertices.size(),"terrain vertex order");require(terrain->vertices.size()<100000,"terrain vertex limit");terrain->vertices.push_back(vector(r,""));});
    rows(directory/L"terrain_faces.csv",[&](const Row&r){require(integer(r.at("face_id"))==terrain->faces.size(),"terrain face order");require(terrain->faces.size()<76800,"terrain face limit");terrain->faces.push_back(face(r,terrain->vertices.size()));});
    require(terrain->vertices.size()==config->terrain->vertices.size()&&terrain->faces.size()==config->terrain->faces.size(),"terrain topology mismatch");ContentHash hash;hash.word(terrain->vertices.size());for(auto v:terrain->vertices){hash.number(v.x);hash.number(v.y);hash.number(v.z);}for(auto f:terrain->faces){hash.word(f.a);hash.word(f.b);hash.word(f.c);}terrain->contentHash="fnv1a64:"+hash.hex();require(terrain->contentHash==m.at("terrain_hash"),"actual terrain CSV hash mismatch");
    std::vector<Vec3>vertices;std::vector<Face>faces;
    rows(directory/L"shape_vertices.csv",[&](const Row&r){if(integer(r.at("trial_id"))==id){require(integer(r.at("vertex_id"))==vertices.size()&&vertices.size()<100,"shape vertex order/count");vertices.push_back(vector(r,""));}});
    rows(directory/L"shape_faces.csv",[&](const Row&r){if(integer(r.at("trial_id"))==id){require(integer(r.at("face_id"))==faces.size()&&faces.size()<196,"shape face order/count");faces.push_back(face(r,vertices.size()));}});
    ReplayInput out;auto&scene=out.scene;scene.shape=polyhedronFromMesh(vertices,faces);require(vertices.size()==integer(selected.at("k")),"shape k mismatch");
    require(std::abs(scene.shape.volume-get(selected,"measured_V"))<=1e-12*scene.shape.volume,"shape volume mismatch");require(norm(scene.shape.center-vector(selected,"canonical_com_"))<=1e-12*get(selected,"r"),"shape COM mismatch");
    const std::array<const char*,9>inertiaNames{"xx","xy","xz","yx","yy","yz","zx","zy","zz"};Mat3 expectedInertia;for(int i=0;i<9;++i)expectedInertia.a[i]=get(selected,std::string("inertia_")+inertiaNames[i]);require(frobenius(scene.shape.inertia-expectedInertia)<=1e-12*frobenius(expectedInertia),"shape inertia mismatch");
    scene.shape.tau=get(selected,"tau");scene.radius=get(selected,"r");scene.terrain=terrain;scene.barrier=config->barrier;scene.barrier.center=vector(m,"barrier_center_");scene.initial.shapeOrigin=vector(selected,"initial_origin_");scene.initial.rotation=rotation(selected,"initial_");
    scene.initial.velocity={get(selected,"vx"),get(selected,"vy"),get(selected,"vz")};scene.initial.angularVelocity=vector(selected,"omega_");
    scene.dynamics.density=config->source.density;scene.dynamics.airDensity=config->source.airDensity;scene.dynamics.friction=get(selected,"mu");scene.dynamics.restitution=get(selected,"e");scene.dynamics.dragCoefficient=get(selected,"Cd");scene.dynamics.rollingCoefficient=get(selected,"c_rr");
    auto reduction=m.find("manifold_reduction");require(reduction==m.end()||reduction->second=="0"||reduction->second=="1","invalid manifold reduction setting");
    scene.dynamics.manifoldReduction=reduction==m.end()||reduction->second=="1";
    out.settings.timeLimit=config->source.maxTime;out.grid=config->grid;out.collectorX=config->collectorX;out.input.id=id;out.input.seed=integer(selected.at("trial_seed"));out.input.kSeed=integer(selected.at("k_seed"));out.input.propertySeed=integer(selected.at("property_seed"));for(int i=0;i<propertyCount;++i)out.input.values[i]=get(selected,propertyNames[i]);
    out.expected.time=get(selected,"final_time");out.expected.center=vector(selected,"final_com_");out.expected.shapeOrigin=vector(selected,"final_origin_");out.expected.rotation=rotation(selected,"final_");out.expected.velocity={get(selected,"final_vx"),get(selected,"final_vy"),get(selected,"final_vz")};out.expected.angularVelocity=vector(selected,"final_omega_");
    if(std::filesystem::exists(directory/L"barrier_trials.csv")){
        rows(directory/L"barrier_trials.csv",[&](const Row&r){if(integer(r.at("trial_id"))!=id)return;
            require(!out.hasBarrierState&&r.at("has_state")=="1","duplicate/missing barrier replay state");out.hasBarrierState=true;auto&b=out.expected.barrier;
            b.enabled=integer(r.at("enabled"))!=0;b.elastic=integer(r.at("elastic"))!=0;b.touched=integer(r.at("touched"))!=0;b.settled=integer(r.at("settled"))!=0;b.center=vector(r,"center_");b.rotation=rotation(r,"");
            b.displacement=get(r,"displacement_m");b.velocity=get(r,"velocity_m_s");b.dampingEnergy=get(r,"damper_loss_J");b.supportWorkResidual=get(r,"support_work_residual_J");
            b.maxDisplacement=get(r,"max_abs_displacement_m");b.contactImpulse=get(r,"contact_impulse_Ns");
            b.supportForce=get(r,"support_force_N");b.contactForce=get(r,"contact_force_step_average_N");b.kineticEnergy=get(r,"kinetic_J");b.springEnergy=get(r,"spring_J");
            b.peakSupportForce=get(r,"peak_abs_support_N");b.peakContactForce=get(r,"peak_abs_contact_substep_average_N");
            require(get(r,"time")==out.expected.time&&r.at("reason")==selected.at("reason"),"barrier trial time/reason mismatch");
        });require(out.hasBarrierState,"missing barrier trial row");
    }
    bool known=false;for(auto reason:{StopReason::resting,StopReason::downstream,StopReason::upstream,StopReason::lateral,StopReason::timeout,StopReason::numericFailure,StopReason::cancelled})if(selected.at("reason")==stopReasonName(reason)){out.expected.reason=reason;known=true;}require(known,"unknown stop reason");validatePhysicsScene(scene);return out;
}
}
