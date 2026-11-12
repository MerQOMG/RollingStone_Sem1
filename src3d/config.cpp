#include "config.h"
#include "hash.h"
#include <Windows.h>
#include <atomic>
#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <iomanip>
#include <locale>

namespace rs3d {
const std::array<const char*,propertyCount> propertyNames{"x","y","vx","vy","vz","omega_x","omega_y","omega_z","mu","e","Cd","c_rr"};
namespace {
void require(bool b,const std::string&s){if(!b)throw std::runtime_error(s);}
std::string trim(std::string s){auto begin=s.find_first_not_of(" \t\r\n"),end=s.find_last_not_of(" \t\r\n");return begin==s.npos?std::string{}:s.substr(begin,end-begin+1);}
double number(const std::string&s){std::istringstream in(s);in.imbue(std::locale::classic());double value;in>>value;require(bool(in)&&std::isfinite(value),"invalid finite number: "+s);in>>std::ws;require(in.eof(),"trailing number content: "+s);return value;}
std::uint64_t integer(const std::string&s){std::uint64_t v=0;auto result=std::from_chars(s.data(),s.data()+s.size(),v);require(result.ec==std::errc{}&&result.ptr==s.data()+s.size(),"invalid unsigned integer: "+s);return v;}
int smallInteger(const std::string&s){auto v=integer(s);require(v<=1000000,"integer exceeds resource range: "+s);return int(v);}
bool boolean(const std::string&s){require(s=="true"||s=="false","expected true/false: "+s);return s=="true";}
Range range(const std::string&s){auto comma=s.find(',');if(comma==s.npos){double v=number(s);return {v,v};}return {number(trim(s.substr(0,comma))),number(trim(s.substr(comma+1)))};}
std::string rangeString(Range r){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<r.lower;if(r.upper!=r.lower)out<<','<<r.upper;return out.str();}
}
std::string sceneText(const SceneDraft&s,bool output){
    std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<std::boolalpha;
    o<<"schema="<<((s.sourceSchema==1&&!s.obstacles.enabled)?1:sceneSchema)<<"\n[geometry]\nk_min="<<s.geometry.kMin<<"\nk_max="<<s.geometry.kMax<<"\nr="<<s.geometry.radius<<"\nV="<<s.geometry.volume;
    const auto&t=s.terrain;o<<"\n[terrain]\ntype="<<(t.type==TerrainType::flat?"flat":"parameterized")<<"\nheight="<<t.topHeight<<"\nsteep_degrees="<<t.steepDegrees<<"\nbench_start="<<t.benchStart<<"\nbench_length="<<t.benchLength<<"\nbench_degrees="<<t.benchDegrees<<"\ntoe_length="<<t.toeLength<<"\nwidth="<<t.width<<"\ncross_slope_degrees="<<t.crossSlopeDegrees<<"\ngroove_amplitude="<<t.grooveAmplitude<<"\ncell_size="<<t.cellSize<<"\nflat_length="<<t.flatLength<<"\nflat_height="<<t.flatHeight<<"\nflat_slope_x="<<t.flatSlopeX<<"\nflip_diagonals="<<t.flipDiagonals;
    o<<"\n[material]\ndensity="<<s.density<<"\nair_density="<<s.airDensity;
    o<<"\n[initial]\ngap="<<s.releaseGap<<"\norientation="<<(s.stableFaceRelease?"stable_face":"sampled");for(int i=0;i<propertyCount;++i)o<<'\n'<<propertyNames[i]<<'='<<rangeString(s.properties[i]);
    const auto&b=s.barrier;o<<"\n[barrier]\nenabled="<<b.enabled<<"\ncenter_x=";if(s.barrierAutoX)o<<"auto";else o<<b.center.x;
    o<<"\ncenter_y="<<b.center.y<<"\ncenter_z=";if(s.barrierAutoZ)o<<"auto";else o<<b.center.z;
    o<<"\nwidth="<<b.width<<"\nheight="<<b.height<<"\nthickness="<<b.thickness<<"\nyaw_degrees="<<b.yawDegrees<<"\nmu="<<b.friction<<"\ne="<<b.restitution;
    // Preserve legacy schema=1 physical hashes when the rigid extension is default.
    if(output||b.elastic||b.mass!=2000||b.stiffness!=500000||b.damping!=30000||b.initialDisplacement!=0||b.initialVelocity!=0)
        o<<"\nmode="<<(b.elastic?"elastic":"rigid")<<"\nmass="<<b.mass<<"\nstiffness="<<b.stiffness<<"\ndamping="<<b.damping<<"\ninitial_displacement="<<b.initialDisplacement<<"\ninitial_velocity="<<b.initialVelocity;
    if(output||s.sourceSchema>=2||s.obstacles.enabled){const auto&a=s.obstacles;
        o<<"\n[obstacles]\nenabled="<<a.enabled<<"\ncount="<<a.count<<"\nseed="<<a.seed<<"\nsize_mode="<<(a.relativeToRadius?"relative_to_r":"absolute_m")<<"\nlength="<<rangeString({a.lengthMin,a.lengthMax})<<"\nwidth="<<rangeString({a.widthMin,a.widthMax})<<"\nexposed_height="<<rangeString({a.heightMin,a.heightMax})<<"\nautomatic_region="<<a.automaticRegion<<"\nx="<<rangeString({a.xMin,a.xMax})<<"\ny="<<rangeString({a.yMin,a.yMax})<<"\nmin_spacing="<<a.minSpacing<<"\nembed_depth="<<a.embedDepth<<"\nmax_total_height="<<a.maxTotalHeight<<"\nmu="<<a.friction<<"\ne="<<a.restitution;}
    o<<"\n[statistics]\nheight_reference="<<(s.heightReference==HeightReference::toeCenter?"toe_center":"toe_min")<<"\nautomatic_box="<<s.automaticGrid;
    for(int i=0;i<3;++i)o<<"\nlower_"<<"xyz"[i]<<'='<<s.grid.lower[i]<<"\nupper_"<<"xyz"[i]<<'='<<s.grid.upper[i]<<"\ncell_"<<"xyz"[i]<<'='<<s.grid.cell[i];
    o<<"\ncollector_x=";if(s.automaticCollector)o<<"auto";else o<<s.collectorX;
    o<<"\n[run]\ncount="<<s.count<<"\nseed="<<s.masterSeed<<"\nmax_time="<<s.maxTime<<"\ndt=0.002\ncollision_steps=4\n";
    if(output)o<<"[preview]\nseed="<<s.previewSeed<<"\n[output]\nroot="<<s.outputRoot.u8string()<<"\ntrajectory_stride="<<s.trajectoryStride<<"\n[history]\nenabled="<<s.history.enabled<<"\ninterval="<<(s.history.interval?rangeString({s.history.interval,s.history.interval}):"auto")<<"\ncache_mib="<<s.history.cacheMiB<<"\ndisplay_vertices="<<s.history.displayVertices<<"\n[analysis]\nroute_cell="<<s.routeCell<<"\ncontact_cell="<<s.contactCell<<'\n';return o.str();
}
std::shared_ptr<const RunConfig> freezeConfig(const SceneDraft&s,std::shared_ptr<const ObstacleSet> savedObstacles){
    validateGeometrySettings(s.geometry);require(s.geometry.kMin>=10,"production k must be at least 10");require(s.count>=1&&s.count<=1000000,"run count outside [1,1000000]");
    for(double v:{s.density,s.airDensity,s.releaseGap,s.maxTime})require(std::isfinite(v),"non-finite scene parameter");
    require(s.density>0&&s.airDensity>=0&&s.releaseGap>=0&&s.maxTime>0&&s.maxTime<=60,"invalid density/air/gap/time limit");
    require(s.trajectoryStride>=0&&s.trajectoryStride<=1000000,"invalid trajectory stride");require(!s.outputRoot.empty(),"empty output root");
    require(s.outputRoot.u8string().find_first_of("\r\n") == std::string::npos,"newline in output path");
    for(int i=0;i<propertyCount;++i){auto r=s.properties[i];require(std::isfinite(r.lower)&&std::isfinite(r.upper)&&r.lower<=r.upper,"invalid range: "+std::string(propertyNames[i]));
        if(i>=frictionValue)require(r.lower>=0,"negative material coefficient");if(i==restitutionValue)require(r.upper<=1,"restitution above one");
        if(i>=velocityX&&i<=omegaZ)require(std::max(std::abs(r.lower),std::abs(r.upper))<5000,"initial velocity component exceeds solver range");}
    auto config=std::make_shared<RunConfig>();config->source=s;config->terrain=std::make_shared<TerrainMesh>(buildTerrain(s.terrain));auto&t=*config->terrain;
    if(s.stableFaceRelease)require(s.terrain.type==TerrainType::flat&&s.terrain.flatSlopeX==0&&s.terrain.crossSlopeDegrees==0&&s.terrain.grooveAmplitude==0,"stable_face fixture requires a horizontal plane");
    require(s.properties[releaseX].lower>=t.xMin&&s.properties[releaseX].upper<=t.xEnd&&s.properties[releaseY].lower>=t.yMin&&s.properties[releaseY].upper<=t.yMax,"release x/y range outside terrain");
    validateBarrierSettings(s.barrier);config->barrier=s.barrier;auto&b=config->barrier;
    for(double v:{b.center.x,b.center.y,b.center.z,b.width,b.height,b.thickness,b.yawDegrees,b.friction,b.restitution})require(std::isfinite(v),"non-finite barrier parameter");
    require(b.width>0&&b.height>0&&b.thickness>0&&b.friction>=0&&b.restitution>=0&&b.restitution<=1,"invalid barrier dimensions/material");
    if(s.barrierAutoX)b.center.x=t.xEnd-10;
    if(s.barrierAutoZ){if(b.enabled)b=groundedBarrier(t,b);else b.center.z=t.height(std::clamp(b.center.x,t.xMin,t.xEnd),std::clamp(b.center.y,t.yMin,t.yMax))+b.height/2;}
    config->obstacles=freezeObstacles(s.obstacles,t,s.geometry.radius,s.properties[releaseX].lower,s.properties[releaseX].upper,s.properties[releaseY].lower,s.properties[releaseY].upper,b,std::move(savedObstacles));
    ContentHash environment;environment.text(t.contentHash);environment.text(config->obstacles->hash);environment.word(b.enabled);environment.word(b.elastic);for(double value:{b.center.x,b.center.y,b.center.z,b.width,b.height,b.thickness,b.yawDegrees,b.friction,b.restitution,b.mass,b.stiffness,b.damping,b.initialDisplacement,b.initialVelocity})environment.number(value);config->environmentHash="fnv1a64:"+environment.hex();
    config->zReference=s.heightReference==HeightReference::toeCenter?t.toeCenter():t.toeMin();
    config->grid=s.grid;if(s.automaticGrid){config->grid.lower={std::max(t.xMin,t.xEnd-40),t.yMin,config->zReference};config->grid.upper={t.xEnd,t.yMax,config->zReference+10};}
    config->collectorX=s.automaticCollector?t.xEnd-10:s.collectorX;Statistics validate(config->grid,config->collectorX,s.count);
    config->history=historyBudget(s.history,s.count,s.maxTime);require(std::isfinite(s.routeCell)&&s.routeCell>0&&std::isfinite(s.contactCell)&&s.contactCell>0,"invalid analysis cell size");
    Grid3D route({{t.xMin,t.yMin,0},{t.xEnd,t.yMax,1},{s.routeCell,s.routeCell,1}}),contact({{0,0,0},{b.width,b.height,1},{s.contactCell,s.contactCell,1}});
    ContentHash recording;recording.word(s.history.enabled);recording.word(config->history.stride);recording.word(motionVersion);recording.text(motionRule);config->recordingHash="fnv1a64:"+recording.hex();
    ContentHash hash;hash.text(sceneText(s,false));hash.text(t.contentHash);hash.number(b.center.x);hash.number(b.center.y);hash.number(b.center.z);config->hash="fnv1a64:"+hash.hex();return config;
}
SceneDraft parseScene(const std::string&utf8,bool validate){
    require(utf8.size()<=1024*1024&&utf8.find('\0')==utf8.npos,"scene too large or contains NUL");
    require(!utf8.empty()&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),int(utf8.size()),nullptr,0)>0,"invalid UTF-8 scene");
    std::string text=utf8;if(text.compare(0,3,"\xEF\xBB\xBF")==0)text.erase(0,3);
    std::map<std::string,std::string>fields;std::istringstream input(text);std::string line,section;int lineNumber=0;
    while(std::getline(input,line)){++lineNumber;line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['){require(line.back()==']'&&line.size()>2,"invalid INI section line "+std::to_string(lineNumber));section=trim(line.substr(1,line.size()-2));continue;}
        auto equal=line.find('=');require(equal!=line.npos,"missing '=' line "+std::to_string(lineNumber));auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));require(!key.empty()&&!value.empty(),"empty INI key/value");if(!section.empty())key=section+'.'+key;
        require(fields.emplace(key,value).second,"duplicate INI key: "+key);
    }
    auto take=[&](const std::string&key,auto consume){auto it=fields.find(key);if(it!=fields.end()){try{consume(it->second);}catch(const std::exception&e){throw std::runtime_error(key+": "+e.what());}fields.erase(it);}};
    require(fields.count("schema"),"missing scene schema");auto schema=integer(fields.at("schema"));require(schema==1||schema==sceneSchema,"unsupported scene schema");fields.erase("schema");SceneDraft s;s.sourceSchema=int(schema);
    if(schema>=2)for(auto name:{"enabled","count","seed","size_mode","length","width","exposed_height","automatic_region","x","y","min_spacing","embed_depth","max_total_height","mu","e"})require(fields.count(std::string("obstacles.")+name)!=0,std::string("missing schema 2 obstacle field: ")+name);
    auto num=[&](const char*key,double&v){take(key,[&](const std::string&x){v=number(x);});};
    auto flag=[&](const char*key,bool&v){take(key,[&](const std::string&x){v=boolean(x);});};
    take("geometry.k_min",[&](auto&x){s.geometry.kMin=smallInteger(x);});take("geometry.k_max",[&](auto&x){s.geometry.kMax=smallInteger(x);});num("geometry.r",s.geometry.radius);num("geometry.V",s.geometry.volume);
    auto&t=s.terrain;take("terrain.type",[&](auto&x){require(x=="parameterized"||x=="flat","unknown terrain type");t.type=x=="flat"?TerrainType::flat:TerrainType::parameterized;});
    num("terrain.height",t.topHeight);num("terrain.steep_degrees",t.steepDegrees);num("terrain.bench_start",t.benchStart);num("terrain.bench_length",t.benchLength);num("terrain.bench_degrees",t.benchDegrees);num("terrain.toe_length",t.toeLength);num("terrain.width",t.width);num("terrain.cross_slope_degrees",t.crossSlopeDegrees);num("terrain.groove_amplitude",t.grooveAmplitude);num("terrain.cell_size",t.cellSize);num("terrain.flat_length",t.flatLength);num("terrain.flat_height",t.flatHeight);num("terrain.flat_slope_x",t.flatSlopeX);flag("terrain.flip_diagonals",t.flipDiagonals);
    num("material.density",s.density);num("material.air_density",s.airDensity);num("initial.gap",s.releaseGap);take("initial.orientation",[&](auto&x){require(x=="sampled"||x=="stable_face","orientation must be sampled/stable_face");s.stableFaceRelease=x=="stable_face";});for(int i=0;i<propertyCount;++i)take(std::string("initial.")+propertyNames[i],[&](auto&x){s.properties[i]=range(x);});
    auto&b=s.barrier;flag("barrier.enabled",b.enabled);take("barrier.center_x",[&](auto&x){s.barrierAutoX=x=="auto";if(!s.barrierAutoX)b.center.x=number(x);});num("barrier.center_y",b.center.y);take("barrier.center_z",[&](auto&x){s.barrierAutoZ=x=="auto";if(!s.barrierAutoZ)b.center.z=number(x);});
    num("barrier.width",b.width);num("barrier.height",b.height);num("barrier.thickness",b.thickness);num("barrier.yaw_degrees",b.yawDegrees);num("barrier.mu",b.friction);num("barrier.e",b.restitution);
    take("barrier.mode",[&](auto&x){require(x=="rigid"||x=="elastic","barrier.mode must be rigid/elastic");b.elastic=x=="elastic";});
    num("barrier.mass",b.mass);num("barrier.stiffness",b.stiffness);num("barrier.damping",b.damping);num("barrier.initial_displacement",b.initialDisplacement);num("barrier.initial_velocity",b.initialVelocity);
    auto&a=s.obstacles;flag("obstacles.enabled",a.enabled);flag("obstacles.automatic_region",a.automaticRegion);take("obstacles.count",[&](auto&x){a.count=smallInteger(x);});take("obstacles.seed",[&](auto&x){a.seed=integer(x);});
    take("obstacles.size_mode",[&](auto&x){require(x=="relative_to_r"||x=="absolute_m","obstacle size_mode must be relative_to_r/absolute_m");a.relativeToRadius=x=="relative_to_r";});
    auto interval=[&](const char*key,double&low,double&high){take(key,[&](auto&x){auto r=range(x);low=r.lower;high=r.upper;});};
    interval("obstacles.length",a.lengthMin,a.lengthMax);interval("obstacles.width",a.widthMin,a.widthMax);interval("obstacles.exposed_height",a.heightMin,a.heightMax);interval("obstacles.x",a.xMin,a.xMax);interval("obstacles.y",a.yMin,a.yMax);
    num("obstacles.min_spacing",a.minSpacing);num("obstacles.embed_depth",a.embedDepth);num("obstacles.max_total_height",a.maxTotalHeight);num("obstacles.mu",a.friction);num("obstacles.e",a.restitution);
    take("statistics.height_reference",[&](auto&x){require(x=="toe_center"||x=="toe_min","unknown height reference");s.heightReference=x=="toe_center"?HeightReference::toeCenter:HeightReference::toeMin;});flag("statistics.automatic_box",s.automaticGrid);
    for(int i=0;i<3;++i){std::string axis(1,"xyz"[i]);take("statistics.lower_"+axis,[&](auto&x){s.grid.lower[i]=number(x);});take("statistics.upper_"+axis,[&](auto&x){s.grid.upper[i]=number(x);});take("statistics.cell_"+axis,[&](auto&x){s.grid.cell[i]=number(x);});}
    take("statistics.collector_x",[&](auto&x){s.automaticCollector=x=="auto";if(!s.automaticCollector)s.collectorX=number(x);});
    take("run.count",[&](auto&x){s.count=integer(x);});take("run.seed",[&](auto&x){s.masterSeed=integer(x);});num("run.max_time",s.maxTime);take("run.dt",[&](auto&x){require(number(x)==.002,"first release fixes dt=0.002");});take("run.collision_steps",[&](auto&x){require(integer(x)==4,"first release fixes 4 collision substeps");});
    take("output.root",[&](auto&x){s.outputRoot=std::filesystem::u8path(x);});take("output.trajectory_stride",[&](auto&x){s.trajectoryStride=smallInteger(x);});
    take("preview.seed",[&](auto&x){s.previewSeed=integer(x);});
    flag("history.enabled",s.history.enabled);take("history.interval",[&](auto&x){s.history.interval=x=="auto"?0:number(x);});take("history.cache_mib",[&](auto&x){s.history.cacheMiB=integer(x);});take("history.display_vertices",[&](auto&x){s.history.displayVertices=size_t(integer(x));});num("analysis.route_cell",s.routeCell);num("analysis.contact_cell",s.contactCell);
    require(fields.empty(),fields.empty()?"":"unknown INI key: "+fields.begin()->first);if(validate)freezeConfig(s);return s;
}
SceneDraft loadScene(const std::filesystem::path&path,bool validate){std::ifstream file(path,std::ios::binary);require(bool(file),"cannot open scene: "+path.u8string());std::string text((std::istreambuf_iterator<char>(file)),{});require(!file.bad(),"scene read failed");return parseScene(text,validate);}
void saveScene(const std::filesystem::path&path,const SceneDraft&s){
    freezeConfig(s);static std::atomic<unsigned>sequence{0};auto temporary=path;temporary+=L".tmp_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(++sequence);
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);require(bool(file),"cannot write scene temporary file");file<<sceneText(s);file.flush();file.close();require(bool(file),"scene write/close failed");
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){auto error=GetLastError();std::error_code ignored;std::filesystem::remove(temporary,ignored);throw std::runtime_error("scene atomic replace failed, Win32="+std::to_string(error));}
}
TrialInput sampleTrialInput(const RunConfig&c,std::uint64_t id){
    require(id>=1&&id<=c.source.count,"trial_id outside batch");TrialInput out;out.id=id;out.seed=trialSeed(c.source.masterSeed,id);out.kSeed=mix(out.seed^kDomain);out.propertySeed=mix(out.seed^propertyDomain);Random rng(out.propertySeed);
    for(int i=0;i<propertyCount;++i)out.values[i]=c.source.properties[i].sample(rng);return out;
}
PhysicsScene makePhysicsScene(const RunConfig&c,const TrialInput&i,const Polyhedron&p){
    Quaternion rotation;
    if(c.source.stableFaceRelease){
        // The face nearest the COM has the COM projection inside its supporting polygon:
        // the COM-centered inscribed sphere touches it there. Rotate that normal down.
        double closest=1e300;Vec3 normal;
        for(auto face:p.faces){auto a=p.vertices[face.a],n=unit(cross(p.vertices[face.b]-a,p.vertices[face.c]-a));double distance=dot(n,a-p.center);if(distance<closest){closest=distance;normal=n;}}
        Vec3 axis=cross(normal,{0,0,-1});double cosine=-normal.z;
        if(norm(axis)<1e-12)rotation=cosine>0?Quaternion{}:Quaternion::fromAxisAngle({1,0,0},pi);
        else rotation=Quaternion::fromAxisAngle(axis,std::acos(std::clamp(cosine,-1.0,1.0)));
    }
    PhysicsScene scene;scene.shape=p;scene.radius=c.source.geometry.radius;scene.terrain=c.terrain;scene.obstacles=c.obstacles;scene.barrier=c.barrier;scene.initial=supportedRelease(p,*c.terrain,i.values[releaseX],i.values[releaseY],c.source.releaseGap,rotation);
    scene.initial.velocity={i.values[velocityX],i.values[velocityY],i.values[velocityZ]};scene.initial.angularVelocity={i.values[omegaX],i.values[omegaY],i.values[omegaZ]};
    scene.dynamics.density=c.source.density;scene.dynamics.airDensity=c.source.airDensity;scene.dynamics.friction=i.values[frictionValue];scene.dynamics.restitution=i.values[restitutionValue];scene.dynamics.dragCoefficient=i.values[dragValue];scene.dynamics.rollingCoefficient=i.values[rollingValue];return scene;
}
}
