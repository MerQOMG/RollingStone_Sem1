#include "motion_archive.h"
#include <fstream>
#include <map>
#include <iostream>
using namespace rs3d;
namespace {
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&e){rejected=true;std::cout<<"expected rejection: "<<e.what()<<'\n';}check(rejected,"corrupt archive accepted");}
}
int main(){try{
    auto path=std::filesystem::temp_directory_path()/"rs3d_motion_test";std::filesystem::create_directories(path);std::map<std::string,std::ofstream> files;
    auto write=[&](const std::string&name,const std::string&bytes){auto&f=files[name];if(!f.is_open())f.open(path/name,std::ios::binary|std::ios::trunc);f.write(bytes.data(),bytes.size());check(bool(f),"test writer failed");};
    auto flush=[&](const std::string&name){files.at(name).flush();};MotionArchiveWriter writer(std::make_shared<ArchiveLocation>(path),4,write,flush);
    MotionRecorder recorder(writer,1,5,true);Snapshot s;recorder.sample(0,s);for(int i=1;i<=1500;++i){s.time=i*.002;s.center={double(i),2,3};s.shapeOrigin={i+.1,2.2,3.3};s.velocity={4,5,6};s.angularVelocity={7,8,9};s.rotation=Quaternion::fromAxisAngle({0,0,1},i*.001);s.barrier.enabled=true;s.barrier.elastic=true;s.barrier.center={10+i*.01,2,3};s.barrier.rotation=Quaternion::fromAxisAngle({0,1,0},i*.001);s.barrier.supportForce=i*10;s.contacts.barrier=i>=17&&i<24;recorder.sample(i,s);}s.reason=StopReason::cancelled;recorder.finish(1500,s);
    MotionRecorder zero(writer,2,5,true);Snapshot z;zero.sample(0,z);z.reason=StopReason::cancelled;zero.finish(0,z);writer.status(3,"geometry_failure","no initial state");for(auto&f:files)f.second.close();
    auto directory=readMotionDirectory(path,4);check(directory->trials.size()==4&&directory->trials[0]->blocks.size()==2&&directory->trials[1]->frames==1&&directory->trials[2]->frames==0&&directory->trials[3]->status=="not_started","all-trial directory lost entries");
    std::vector<MotionFrame> frames;for(const auto&b:directory->trials[0]->blocks){auto block=readMotionBlock(*directory,b);frames.insert(frames.end(),block.begin(),block.end());}check(frames.front().step==0&&frames.back().step==1500&&frames.back().snapshot.reason==StopReason::cancelled,"initial/final frame lost");for(auto step:{16,17,23,24})check(std::find_if(frames.begin(),frames.end(),[&](const auto&f){return f.step==unsigned(step);})!=frames.end(),"contact transition bracket lost");for(const auto&f:frames)if(f.step){check(f.snapshot.center.x==double(f.step)&&f.snapshot.shapeOrigin.x==f.step+.1&&f.snapshot.barrier.center.x==10+f.step*.01&&f.snapshot.barrier.supportForce==f.step*10,"encoded values changed");}
    auto b=directory->trials[0]->blocks[0];auto bad=b;bad.offset++;rejects([&]{readMotionBlock(*directory,bad);});bad=b;bad.checksum++;rejects([&]{readMotionBlock(*directory,bad);});
    auto size=std::filesystem::file_size(path/"motion.bin");std::filesystem::resize_file(path/"motion.bin",size-1);rejects([&]{readMotionDirectory(path,4);});auto recovered=recoverMotionDirectory(path,4);check(recovered->diagnostic&&recovered->trials[0]->frames==frames.size()&&recovered->trials[1]->frames==0,"valid raw blocks not recoverable before truncation");
    auto legacy=path/"legacy";std::filesystem::create_directories(legacy);{std::ofstream f(legacy/"trajectory.csv");f<<"trial_id,time,com_x,com_y,com_z,origin_x,origin_y,origin_z,qw,qx,qy,qz,vx,vy,vz,omega_x,omega_y,omega_z,reason\n1,0,1,2,3,1.1,2.1,3.1,1,0,0,0,1,2,3,4,5,6,running\n1,0.01,2,3,4,2.1,3.1,4.1,1,0,0,0,2,3,4,5,6,7,timeout\n";}
    auto old=readLegacyMotionDirectory(legacy,3);check(old->legacy&&!old->legacyBarrier&&old->trials[0]->frames==2&&old->trials[1]->frames==0,"legacy missing-history handling");auto oldFrames=readMotionBlock(*old,old->trials[0]->blocks[0]);check(oldFrames.size()==2&&oldFrames[1].snapshot.center.x==2&&oldFrames[1].snapshot.shapeOrigin.x==2.1&&oldFrames[1].snapshot.reason==StopReason::timeout,"legacy CSV source state mismatch");std::cout<<"legacy CSV actual frames and missing barrier history PASS\n";
    auto small=historyBudget({},100,60),large=historyBudget({},1000,60);check(small.interval==.01&&large.interval==.02,"auto recording interval");HistorySettings explicitInterval;explicitInterval.interval=.0101;check(historyBudget(explicitInterval,100,60).interval==.012,"interval not rounded up to dt multiple");
    std::cout<<"R1/R2 source equality, contact brackets, cancellation, zero duration, all-trial directory and corruption PASS frames="<<frames.size()<<" frame_bytes="<<motionFrameBytes<<" auto_1000_nominal="<<large.nominalBytes<<" maximum="<<large.maximumBytes<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
