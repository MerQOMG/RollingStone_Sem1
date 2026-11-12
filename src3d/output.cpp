#include "output.h"
#include "build_version.h"
#include "hash.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <locale>
#include <charconv>
#include <type_traits>
namespace rs3d {
namespace {
std::ostringstream stream(){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);return s;}
std::string csv(const std::string&s){std::string out="\"";for(char c:s){if(c=='\"')out+='\"';out+=c=='\n'||c=='\r'?' ':c;}return out+'\"';}
void vec(std::ostream&o,Vec3 v){o<<v.x<<','<<v.y<<','<<v.z;}
void quat(std::ostream&o,Quaternion q){o<<q.w<<','<<q.x<<','<<q.y<<','<<q.z;}
void state(std::ostream&o,const PhysicsSnapshot&s){o<<s.time<<',';vec(o,s.center);o<<',';vec(o,s.shapeOrigin);o<<',';quat(o,s.rotation);o<<',';vec(o,s.velocity);o<<',';vec(o,s.angularVelocity);}
std::string clean(std::string s){for(char&c:s)if(c=='\n'||c=='\r')c=' ';return s;}
template<class T>void number(std::string&row,T value){
    char buffer[64];std::to_chars_result converted;
    if constexpr(std::is_floating_point_v<T>)converted=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::general,17);
    else converted=std::to_chars(buffer,buffer+sizeof(buffer),value);
    if(converted.ec!=std::errc{})throw OutputFailure("CSV number formatting failed");
    row.append(buffer,converted.ptr);row+=',';
}
}
RunOutput::RunOutput(const RunConfig&c,std::uint64_t failAfter):failAfter_(failAfter){
    std::error_code error;std::filesystem::create_directories(c.source.outputRoot,error);if(error)throw OutputFailure("cannot create output root: "+c.source.outputRoot.u8string()+" code="+std::to_string(error.value()));
    static std::atomic<unsigned>sequence{0};SYSTEMTIME now;GetLocalTime(&now);auto name=stream();name<<"run_"<<now.wYear<<std::setfill('0')<<std::setw(2)<<now.wMonth<<std::setw(2)<<now.wDay<<'_'<<std::setw(2)<<now.wHour<<std::setw(2)<<now.wMinute<<std::setw(2)<<now.wSecond<<'_'<<GetCurrentProcessId()<<'_';
    for(int attempt=0;attempt<100;++attempt){final_=c.source.outputRoot/std::filesystem::u8path(name.str()+std::to_string(++sequence));pending_=final_;pending_+=L".pending";
        if(std::filesystem::exists(final_))continue;if(std::filesystem::create_directory(pending_,error))return;if(error)throw OutputFailure("cannot create run directory code="+std::to_string(error.value()));}
    throw OutputFailure("cannot allocate unique run directory");
}
void RunOutput::put(const std::string&name,const std::string&text){
    auto it=files_.find(name);if(it==files_.end()){auto&f=files_[name];f.open(pending_/std::filesystem::u8path(name),std::ios::binary|std::ios::trunc);if(!f)throw OutputFailure("cannot open output file: "+name);it=files_.find(name);}
    auto&file=it->second;if(failAfter_&&written_+text.size()>failAfter_)file.setstate(std::ios::badbit);
    file.write(text.data(),std::streamsize(text.size()));if(!file)throw OutputFailure("write failed: "+name);written_+=text.size();
}
void RunOutput::closeFiles(){for(auto&item:files_){item.second.flush();item.second.close();if(!item.second)throw OutputFailure("flush/close failed: "+item.first);}files_.clear();}
void RunOutput::initialize(const RunConfig&c){
    put("INCOMPLETE.txt","Output transaction is in progress. No formal probabilities are valid until the directory is committed.\n");
    put("scene.ini",sceneText(c.source));
    put("terrain_vertices.csv","vertex_id,x,y,z\n");for(size_t i=0;i<c.terrain->vertices.size();++i){auto o=stream();o<<i<<',';vec(o,c.terrain->vertices[i]);o<<'\n';put("terrain_vertices.csv",o.str());}
    put("terrain_faces.csv","face_id,a,b,c\n");for(size_t i=0;i<c.terrain->faces.size();++i){auto f=c.terrain->faces[i];auto o=stream();o<<i<<','<<f.a<<','<<f.b<<','<<f.c<<'\n';put("terrain_faces.csv",o.str());}
    put("shape_vertices.csv","trial_id,vertex_id,x,y,z\n");put("shape_faces.csv","trial_id,face_id,a,b,c\n");
    put("trajectory.csv","trial_id,time,com_x,com_y,com_z,origin_x,origin_y,origin_z,qw,qx,qy,qz,vx,vy,vz,omega_x,omega_y,omega_z,reason\n");
    put("attempt_rejections.csv","trial_id,attempt,reason\n");
    put("trials.csv","trial_id,trial_seed,k_seed,property_seed,attempt_seed,k,r,requested_V,measured_V,mass,canonical_com_x,canonical_com_y,canonical_com_z,inertia_xx,inertia_xy,inertia_xz,inertia_yx,inertia_yy,inertia_yz,inertia_zx,inertia_zy,inertia_zz,inner_radius,tau,slenderness,double_fallback,attempts,root_iterations,engine_volume_error,engine_mass_error,engine_com_error,engine_inertia_error,engine_vertex_error,engine_inner_radius,has_shape,has_initial,initial_origin_x,initial_origin_y,initial_origin_z,initial_qw,initial_qx,initial_qy,initial_qz,x,y,vx,vy,vz,omega_x,omega_y,omega_z,mu,e,Cd,c_rr,first_com_x,first_com_y,first_com_z,final_time,final_com_x,final_com_y,final_com_z,final_origin_x,final_origin_y,final_origin_z,final_qw,final_qx,final_qy,final_qz,final_vx,final_vy,final_vz,final_omega_x,final_omega_y,final_omega_z,reason,error,slop,ccd_threshold,ccd_max_penetration,collision_steps,velocity_iterations,position_iterations,updates,internal_steps,generation_seconds,physics_seconds,statistics_seconds,trajectory_shape_output_seconds\n");
}
void RunOutput::shape(std::uint64_t id,const Polyhedron&p){
    for(size_t i=0;i<p.vertices.size();++i){auto o=stream();o<<id<<','<<i<<',';vec(o,p.vertices[i]);o<<'\n';put("shape_vertices.csv",o.str());}
    for(size_t i=0;i<p.faces.size();++i){auto f=p.faces[i];auto o=stream();o<<id<<','<<i<<','<<f.a<<','<<f.b<<','<<f.c<<'\n';put("shape_faces.csv",o.str());}
}
void RunOutput::trajectory(std::uint64_t id,const Snapshot&s){auto o=stream();o<<id<<',';state(o,s);o<<','<<stopReasonName(s.reason)<<'\n';put("trajectory.csv",o.str());}
void RunOutput::trial(const TrialRecord&r){
    auto o=stream();const auto&d=r.shapeDiagnostics;const auto&p=r.physics;
    o<<r.input.id<<','<<r.input.seed<<','<<r.input.kSeed<<','<<r.input.propertySeed<<','<<d.attemptSeed<<','<<d.k<<','<<r.radius<<','<<r.requestedVolume<<','<<r.volume<<','<<p.mass<<',';vec(o,r.canonicalCenter);for(double v:r.canonicalInertia.a)o<<','<<v;
    o<<','<<r.innerRadius<<','<<r.tau<<','<<r.slenderness<<','<<r.doubleFallback<<','<<d.attempts<<','<<d.rootIterations<<','<<d.engineVolumeError<<','<<d.engineMassError<<','<<d.engineCenterError<<','<<d.engineInertiaError<<','<<d.engineVertexError<<','<<d.engineInnerRadius<<','<<r.hasShape<<','<<r.hasInitialState<<',';
    vec(o,r.initial.shapeOrigin);o<<',';quat(o,r.initial.rotation);for(double v:r.input.values)o<<','<<v;o<<',';vec(o,r.first.center);o<<',';state(o,r.last);
    o<<','<<stopReasonName(r.last.reason)<<','<<csv(r.last.error)<<','<<p.slop<<','<<p.linearCastThreshold<<','<<p.linearCastMaxPenetration<<','<<p.collisionSteps<<','<<p.velocityIterations<<','<<p.positionIterations<<','<<p.updates<<','<<p.internalSteps<<','<<r.timing.generation<<','<<r.timing.physics<<','<<r.timing.statistics<<','<<r.timing.output<<'\n';put("trials.csv",o.str());
    for(size_t i=0;i<d.rejections.size();++i){auto row=stream();row<<r.input.id<<','<<i<<','<<csv(d.rejections[i])<<'\n';put("attempt_rejections.csv",row.str());}
}
void RunOutput::finish(BatchResult&r){
    auto begin=std::chrono::steady_clock::now();const auto&s=r.statistics;const auto&g=s.grid();bool complete=r.status==BatchStatus::complete;
    const std::string common="hit_trials,N,C,probability,probability_lower,probability_upper,complete\n";
    // Dense probability tables include zero-hit cells. Locale-independent
    // to_chars avoids per-number iostream/locale overhead during cancellation.
    // General/17 digits preserves the existing round-trip double CSV format;
    // retain every row and put call, including the late-write failure seams.
    std::string row;row.reserve(512);
    auto suffix=[&](std::uint64_t hits){auto p=s.probability(hits,complete);number(row,hits);number(row,s.requested());number(row,s.completed());if(p.value)number(row,*p.value);else row+=',';number(row,p.lower);number(row,p.upper);row+=complete?'1':'0';row+='\n';};
    put("voxel_probability.csv","x_lower,x_upper,y_lower,y_upper,z_lower,z_upper,"+common);
    for(size_t id=0;id<g.count();++id){auto index=g.indices(id);row.clear();for(int a=0;a<3;++a){number(row,g.lines(a)[index[a]]);number(row,g.lines(a)[index[a]+1]);}suffix(s.voxelHits()[id]);put("voxel_probability.csv",row);}
    auto projection=[&](const char*name,int first,int second,const std::vector<std::uint64_t>&hits){std::string a(1,"xyz"[first]),b(1,"xyz"[second]);put(name,a+"_lower,"+a+"_upper,"+b+"_lower,"+b+"_upper,"+common);
        for(int i=0;i<g.size(first);++i)for(int j=0;j<g.size(second);++j){row.clear();number(row,g.lines(first)[i]);number(row,g.lines(first)[i+1]);number(row,g.lines(second)[j]);number(row,g.lines(second)[j+1]);suffix(hits[size_t(i)*g.size(second)+j]);put(name,row);}};
    projection("projection_xy.csv",0,1,s.xyHits());projection("projection_xz.csv",0,2,s.xzHits());projection("collector_yz.csv",1,2,s.collectorHits());
    put("collector_summary.csv","N,C,first_crossing_not_observed,first_crossing_outside_yz,complete\n"+std::to_string(s.requested())+','+std::to_string(s.completed())+','+std::to_string(s.collectorNotCrossed())+','+std::to_string(s.collectorOutside())+','+std::to_string(complete)+'\n');
    closeFiles();double beforeManifest=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();r.timing.output+=beforeManifest;
    const auto&c=*r.config;auto m=stream();m<<"schema=1\nstatus="<<batchStatusName(r.status)<<"\ncomplete="<<complete<<"\nN="<<s.requested()<<"\nC="<<s.completed()<<"\nfailures="<<r.failures<<"\ntimeouts="<<r.timeouts<<"\ncancelled_trials="<<r.cancelledTrials<<"\nunstarted="<<r.unstarted()<<"\nrun_config_hash="<<c.hash<<"\nterrain_hash="<<c.terrain->contentHash<<"\nseed_scheme="<<seedScheme<<"\nsampler="<<samplerVersion<<"\nradius_definition=max_vertex_distance_from_solid_com\nvertices_may_be_inside_bound=true\nmaster_seed="<<c.source.masterSeed<<"\nsource_head="<<RS3D_SOURCE_HEAD<<"\nsource_tree_sha256="<<RS3D_SOURCE_HASH<<"\nbuild="<<RS3D_BUILD_MODE<<"\ncompiler="<<_MSC_FULL_VER<<"\nJolt=0373ec0dd762e4bc2f6acdb08371ee84fa23c6db\nraylib=c1ab645ca298a2801097931d1079b10ff7eb9df8\nraygui=25c8c65a6e5f0f4d4b564a0343861898c6f2778b\ncoordinate=x_downslope_y_cross_z_up\nunits=m_s_kg\nconvex_radius=0\ndt=0.002\ncollision_steps=4\nmanifold_reduction=0\nrestitution_velocity_threshold=0.15\nmaterial_combination=static_surface\nshape_transform=rotation*(vertex-canonical_com)+shape_origin\nheight_reference="<<(c.source.heightReference==HeightReference::toeCenter?"toe_center":"toe_min")<<"\nz_reference="<<c.zReference<<"\ncollector_x="<<c.collectorX;
    for(int axis=0;axis<3;++axis)m<<"\ngrid_lower_"<<"xyz"[axis]<<'='<<c.grid.lower[axis]<<"\ngrid_upper_"<<"xyz"[axis]<<'='<<c.grid.upper[axis]<<"\ngrid_cell_"<<"xyz"[axis]<<'='<<c.grid.cell[axis]<<"\nbarrier_center_"<<"xyz"[axis]<<'='<<c.barrier.center[axis];
    std::map<int,std::uint64_t>attempts;std::uint64_t firstReject=0,totalReject=0,geometryFailures=0;for(auto&t:r.trials){++attempts[t.shapeDiagnostics.attempts];firstReject+=!t.shapeDiagnostics.rejections.empty();totalReject+=t.shapeDiagnostics.rejections.size();geometryFailures+=t.last.reason==StopReason::geometryFailure;}
    m<<"\nfirst_attempt_rejected_trials="<<firstReject<<"\ntotal_rejected_attempts="<<totalReject<<"\nfinal_geometry_failures="<<geometryFailures<<"\nfirst_rejection_rate_denominator=started_trials\nstarted_trials="<<r.trials.size()<<"\nfirst_rejection_rate="<<(r.trials.empty()?0:double(firstReject)/r.trials.size())<<"\nfinal_geometry_failure_rate="<<(r.trials.empty()?0:double(geometryFailures)/r.trials.size());
    m<<"\nupdates="<<r.updates<<"\ninternal_steps="<<r.internalSteps<<"\ngeneration_seconds="<<r.timing.generation<<"\nphysics_seconds="<<r.timing.physics<<"\nstatistics_seconds="<<r.timing.statistics<<"\noutput_seconds_before_manifest="<<r.timing.output<<"\nwall_seconds_before_commit="<<r.timing.wall<<"\nerror="<<clean(r.error)<<"\n[attempt_histogram]\n";for(auto a:attempts)m<<a.first<<'='<<a.second<<'\n';
    m<<"[request_root]\n";std::istringstream requested(sceneText(c.source));std::string line;while(std::getline(requested,line)){if(!line.empty()&&line.front()=='[')line="[request."+line.substr(1);m<<line<<'\n';}
    m<<"[commit_timing]\nwall_seconds_before_manifest="<<r.timing.wall+std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"\n";
    put("manifest.ini",m.str());put("FILES_COMPLETE.txt","scene.ini\nmanifest.ini\ntrials.csv\nshape_vertices.csv\nshape_faces.csv\nterrain_vertices.csv\nterrain_faces.csv\nvoxel_probability.csv\ncollector_yz.csv\ncollector_summary.csv\nprojection_xy.csv\nprojection_xz.csv\ntrajectory.csv\nattempt_rejections.csv\n");closeFiles();
    std::error_code ec;std::filesystem::remove(pending_/L"INCOMPLETE.txt",ec);if(ec)throw OutputFailure("cannot clear transaction marker");
    if(!MoveFileExW(pending_.c_str(),final_.c_str(),MOVEFILE_WRITE_THROUGH))throw OutputFailure("cannot commit output directory, Win32="+std::to_string(GetLastError()));r.directory=final_;
    r.timing.output+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()-beforeManifest;
}
void RunOutput::diagnose(BatchResult&r)noexcept{
    try{for(auto&f:files_)f.second.close();files_.clear();std::ofstream marker(pending_/L"INCOMPLETE.txt",std::ios::binary);marker<<"io_error: "<<clean(r.error)<<"\nNo formal probabilities are valid in a .pending directory.\n";marker.close();
        std::ofstream manifest(pending_/L"manifest.ini",std::ios::binary|std::ios::trunc);manifest<<"schema=1\nstatus=io_error\ncomplete=0\nN="<<r.config->source.count<<"\nC="<<r.statistics.completed()<<"\nstarted_trials="<<r.trials.size()<<"\nunstarted="<<r.unstarted()<<"\nerror="<<clean(r.error)<<'\n';manifest.close();
        // Probability files might have been partly written with complete=1 before a late I/O failure.
        // Truncate them to headers; raw normal-trial records remain for diagnosis/recomputation.
        for(auto name:{L"voxel_probability.csv",L"collector_yz.csv",L"projection_xy.csv",L"projection_xz.csv"}){auto path=pending_/name;if(std::filesystem::exists(path)){std::ofstream blank(path,std::ios::binary|std::ios::trunc);blank<<"io_error_no_formal_probability\n";}}
    }catch(...){/* A failed disk may reject diagnostic writes too; .pending remains authoritative. */}
}
}
