#include "trajectory_display.h"
#include <queue>
#include <set>
#include <chrono>
namespace rs3d {
namespace {
struct Segment {size_t begin=0,end=0,split=0;double error=0;};
struct Less {bool operator()(const Segment&a,const Segment&b)const{return a.error!=b.error?a.error<b.error:a.begin>b.begin;}};
Segment segment(const std::vector<TimedPosition>&points,size_t begin,size_t end){Segment s{begin,end,begin,0};auto a=points[begin].center,d=points[end].center-a;double length=norm2(d);for(size_t i=begin+1;i<end;++i){auto p=points[i].center;double t=length?std::clamp(dot(p-a,d)/length,0.,1.):0;double error=norm2(p-a-d*t);if(error>s.error){s.error=error;s.split=i;}}return s;}
}
std::shared_ptr<const DisplayTrajectory> simplifyTrajectory(std::uint64_t id,const std::vector<TimedPosition>&source,StopReason status,size_t maximum){
    if(!id||maximum<2)throw std::runtime_error("invalid trajectory display budget");auto result=std::make_shared<DisplayTrajectory>();result->trialId=id;result->sourcePoints=source.size();result->status=status;
    for(size_t i=0;i<source.size();++i)if(!finite(source[i].center)||!std::isfinite(source[i].time)||(i&&source[i].time<=source[i-1].time))throw std::runtime_error("invalid display trajectory source");
    if(source.size()<=maximum){result->points=source;return result;}std::set<size_t> keep{0,source.size()-1};std::priority_queue<Segment,std::vector<Segment>,Less> pending;pending.push(segment(source,0,source.size()-1));
    while(keep.size()<maximum&&!pending.empty()&&pending.top().error>1e-24){auto s=pending.top();pending.pop();keep.insert(s.split);if(s.split>s.begin+1)pending.push(segment(source,s.begin,s.split));if(s.end>s.split+1)pending.push(segment(source,s.split,s.end));}
    result->maximumDeviation=pending.empty()?0:std::sqrt(pending.top().error);result->points.reserve(keep.size());for(auto i:keep)result->points.push_back(source[i]);return result;
}
TrajectoryArchiveReader::TrajectoryArchiveReader(){worker_=std::thread([this]{run();});}
TrajectoryArchiveReader::~TrajectoryArchiveReader(){stopping_=true;++generation_;wake_.notify_all();worker_.join();}
void TrajectoryArchiveReader::request(std::shared_ptr<const MotionDirectory>d,size_t budget){if(!d||d->trials.empty()||budget<2*d->trials.size())throw std::runtime_error("invalid archive trajectory display budget");std::lock_guard<std::mutex>lock(mutex_);pending_=Request{std::move(d),budget,++generation_};view_.reset();wake_.notify_one();}
void TrajectoryArchiveReader::cancel(){std::lock_guard<std::mutex>lock(mutex_);++generation_;pending_.reset();view_.reset();}
std::shared_ptr<const TrajectoryLoadView>TrajectoryArchiveReader::view()const{std::lock_guard<std::mutex>lock(mutex_);return view_;}
void TrajectoryArchiveReader::run(){using Clock=std::chrono::steady_clock;for(;;){Request request;{std::unique_lock<std::mutex>lock(mutex_);wake_.wait(lock,[&]{return stopping_||pending_.has_value();});if(stopping_)return;request=std::move(*pending_);pending_.reset();}TrajectoryLoadView result;result.generation=request.generation;result.trajectories.resize(request.directory->trials.size());auto lastPublish=Clock::now();auto publish=[&]{auto value=std::make_shared<const TrajectoryLoadView>(result);std::lock_guard<std::mutex>lock(mutex_);if(generation_==request.generation)view_=std::move(value);lastPublish=Clock::now();};
    try{for(const auto&t:request.directory->trials){if(generation_!=request.generation||stopping_)break;std::vector<TimedPosition>points;if(t->frames>1000000)throw std::runtime_error("trajectory source exceeds one-million-frame display safety limit");points.reserve(size_t(t->frames));StopReason status=StopReason::running;for(const auto&block:t->blocks){if(generation_!=request.generation||stopping_)break;auto frames=readMotionBlock(*request.directory,block);for(const auto&frame:frames){points.push_back({frame.snapshot.time,frame.snapshot.center});status=frame.snapshot.reason;}}if(generation_!=request.generation||stopping_)break;for(auto reason:{StopReason::running,StopReason::resting,StopReason::upstream,StopReason::downstream,StopReason::lateral,StopReason::timeout,StopReason::cancelled,StopReason::numericFailure,StopReason::geometryFailure})if(t->status==stopReasonName(reason))status=reason;if(!points.empty())result.trajectories[size_t(t->id-1)]=simplifyTrajectory(t->id,points,status,request.budget/request.directory->trials.size());++result.processed;++result.version;if(Clock::now()-lastPublish>=std::chrono::milliseconds(250))publish();}result.finished=result.processed==request.directory->trials.size();}
    catch(const std::exception&e){result.error=e.what();result.finished=true;}++result.version;publish();}
}
}
