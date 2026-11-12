#include "playback.h"
#include "result_archive.h"
#include <chrono>
namespace rs3d {
namespace {
Quaternion normalized(Quaternion q){double n=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);if(!std::isfinite(n)||n<=0)throw std::runtime_error("invalid playback quaternion");return {q.w/n,q.x/n,q.y/n,q.z/n};}
Quaternion weighted(Quaternion a,double x,Quaternion b,double y){return {a.w*x+b.w*y,a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y};}
}
Quaternion displaySlerp(Quaternion a,Quaternion b,double t){if(!std::isfinite(t)||t<0||t>1)throw std::runtime_error("invalid SLERP fraction");a=normalized(a);b=normalized(b);double cosine=a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;if(cosine<0){b={-b.w,-b.x,-b.y,-b.z};cosine=-cosine;}cosine=std::clamp(cosine,0.,1.);if(cosine>.9995)return normalized(weighted(a,1-t,b,t));double angle=std::acos(cosine),denominator=std::sin(angle);return normalized(weighted(a,std::sin((1-t)*angle)/denominator,b,std::sin(t*angle)/denominator));}
Snapshot interpolateMotion(const Snapshot&a,const Snapshot&b,double time){
    if(!std::isfinite(time)||b.time<a.time)throw std::runtime_error("invalid playback interval");if(time<=a.time)return a;if(time>=b.time)return b;if(a.time==b.time)return a;
    double t=(time-a.time)/(b.time-a.time);auto scalar=[&](double x,double y){return x+(y-x)*t;};auto vector=[&](Vec3 x,Vec3 y){return x+(y-x)*t;};Snapshot result=a;result.time=time;result.center=vector(a.center,b.center);result.shapeOrigin=vector(a.shapeOrigin,b.shapeOrigin);result.velocity=vector(a.velocity,b.velocity);result.angularVelocity=vector(a.angularVelocity,b.angularVelocity);result.rotation=displaySlerp(a.rotation,b.rotation,t);
    auto&r=result.barrier;const auto&x=a.barrier;const auto&y=b.barrier;r.center=vector(x.center,y.center);r.rotation=displaySlerp(x.rotation,y.rotation,t);
    auto interpolate=[&](double BarrierSnapshot::*member){r.*member=scalar(x.*member,y.*member);};for(auto member:{&BarrierSnapshot::displacement,&BarrierSnapshot::velocity,&BarrierSnapshot::supportForce,&BarrierSnapshot::contactForce,&BarrierSnapshot::kineticEnergy,&BarrierSnapshot::springEnergy,&BarrierSnapshot::dampingEnergy,&BarrierSnapshot::supportWorkResidual,&BarrierSnapshot::contactImpulse,&BarrierSnapshot::maxDisplacement,&BarrierSnapshot::peakSupportForce,&BarrierSnapshot::peakContactForce})interpolate(member);return result;
}
void MotionBlockCache::clear(){blocks_.clear();lru_.clear();bytes_=0;location_.reset();}
std::shared_ptr<const std::vector<MotionFrame>> MotionBlockCache::get(const MotionDirectory&directory,const MotionBlock&block){
    if(location_!=directory.location){clear();location_=directory.location;}auto found=blocks_.find(block.offset);if(found!=blocks_.end()){lru_.splice(lru_.begin(),lru_,found->second.recency);return found->second.frames;}
    auto before=directory.location->path();std::vector<MotionFrame> decoded;try{decoded=readMotionBlock(directory,block);}catch(...){if(directory.location->path()==before)throw;decoded=readMotionBlock(directory,block);}
    auto frames=std::make_shared<const std::vector<MotionFrame>>(std::move(decoded));size_t size=frames->capacity()*sizeof(MotionFrame)+sizeof(Entry)+sizeof(std::uint64_t)*4;
    while(!lru_.empty()&&bytes_+size>budget_){auto key=lru_.back();auto old=blocks_.find(key);bytes_-=old->second.bytes;blocks_.erase(old);lru_.pop_back();}
    lru_.push_front(block.offset);blocks_.emplace(block.offset,Entry{frames,lru_.begin(),size});bytes_+=size;peak_=std::max(peak_,bytes_);return frames;
}
PlaybackSample MotionBlockCache::sample(const MotionDirectory&d,std::uint64_t id,double time){
    auto start=std::chrono::steady_clock::now();PlaybackSample result;result.trialId=id;result.requestedTime=time;result.barrierHistory=!d.legacy||d.legacyBarrier;
    try{if(id<1||id>d.trials.size()||!std::isfinite(time))throw std::runtime_error("playback trial/time outside directory");const auto&t=*d.trials[size_t(id-1)];if(!t.frames){result.error=t.error.empty()?t.status:t.error;return result;}time=std::clamp(time,t.firstTime,t.lastTime);
        auto block=std::lower_bound(t.blocks.begin(),t.blocks.end(),time,[](const MotionBlock&b,double at){return b.lastTime<at;});if(block==t.blocks.end())block=t.blocks.end()-1;auto frames=get(d,*block);auto upper=std::lower_bound(frames->begin(),frames->end(),time,[](const MotionFrame&f,double at){return f.snapshot.time<at;});
        if(upper==frames->end())result.snapshot=frames->back().snapshot;else if(upper->snapshot.time==time)result.snapshot=upper->snapshot;else if(upper==frames->begin()){if(block==t.blocks.begin())result.snapshot=upper->snapshot;else{auto previous=get(d,*(block-1));result.snapshot=interpolateMotion(previous->back().snapshot,upper->snapshot,time);result.interpolated=true;}}
        else{result.snapshot=interpolateMotion((upper-1)->snapshot,upper->snapshot,time);result.interpolated=true;}
        auto next=std::upper_bound(frames->begin(),frames->end(),time,[](double at,const MotionFrame&f){return at<f.snapshot.time;});if(next!=frames->end())result.nextRecordTime=next->snapshot.time;else if(block+1!=t.blocks.end())result.nextRecordTime=get(d,*(block+1))->front().snapshot.time;else result.nextRecordTime=t.lastTime;
        if(upper!=frames->begin())result.previousRecordTime=(upper-1)->snapshot.time;else if(block!=t.blocks.begin())result.previousRecordTime=get(d,*(block-1))->back().snapshot.time;else result.previousRecordTime=t.firstTime;result.available=true;
    }catch(const std::exception&e){result.error=e.what();}result.loadMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return result;
}
PlaybackReader::PlaybackReader(size_t bytes):cache_(bytes){worker_=std::thread([this]{run();});}
PlaybackReader::~PlaybackReader(){{std::lock_guard<std::mutex>lock(mutex_);stopping_=true;}wake_.notify_all();worker_.join();}
std::uint64_t PlaybackReader::request(std::shared_ptr<const MotionDirectory>d,std::uint64_t id,double time,std::shared_ptr<const RunConfig>config,std::optional<Snapshot>eventPose){std::lock_guard<std::mutex>lock(mutex_);pending_=Request{std::move(d),++sequence_,id,time,std::move(config),std::move(eventPose)};wake_.notify_one();return sequence_;}
std::shared_ptr<const PlaybackSample> PlaybackReader::latest()const{std::lock_guard<std::mutex>lock(mutex_);return result_;}
void PlaybackReader::invalidate(){std::lock_guard<std::mutex>lock(mutex_);++sequence_;pending_.reset();result_.reset();}
void PlaybackReader::run(){for(;;){Request request;{std::unique_lock<std::mutex>lock(mutex_);wake_.wait(lock,[&]{return stopping_||pending_.has_value();});if(stopping_)return;request=std::move(*pending_);pending_.reset();}auto sample=std::make_shared<PlaybackSample>();if(request.config)cache_.setBudget(size_t(request.config->source.history.cacheMiB)*1024*1024);
    if(request.eventPose){sample->snapshot=*request.eventPose;sample->available=true;sample->trialId=request.trial;sample->requestedTime=request.time;}
    else if(request.directory)*sample=cache_.sample(*request.directory,request.trial,request.time);else sample->error="no archive directory";
    if(sample->available&&!sample->barrierHistory&&request.config&&!request.config->barrier.elastic){auto&b=sample->snapshot.barrier;b.enabled=request.config->barrier.enabled;b.elastic=false;b.center=initialBarrierCenter(request.config->barrier);b.rotation=Quaternion::fromAxisAngle({0,0,1},request.config->barrier.yawDegrees*pi/180);}
    if(sample->available&&request.config){try{if(sceneTrial_!=request.trial||sceneLocation_!=request.directory->location){const auto&g=request.directory->trials[size_t(request.trial-1)]->geometry;if(!g)throw std::runtime_error("trial shape has not been published");auto old=request.directory->location->path();try{scene_=readArchivedGeometry(old,request.trial,*request.config,*g);}catch(...){if(old==request.directory->location->path())throw;scene_=readArchivedGeometry(request.directory->location->path(),request.trial,*request.config,*g);}sceneTrial_=request.trial;sceneLocation_=request.directory->location;}sample->scene=scene_;}catch(const std::exception&e){sample->available=false;sample->error=e.what();}}
    sample->request=request.sequence;std::lock_guard<std::mutex>lock(mutex_);if(request.sequence==sequence_)result_=std::move(sample);}}
}
