#pragma once
#include "motion_archive.h"
#include <list>
#include <map>
#include <thread>
#include <condition_variable>
namespace rs3d {
struct RunConfig;
Quaternion displaySlerp(Quaternion a,Quaternion b,double fraction);
Snapshot interpolateMotion(const Snapshot&before,const Snapshot&after,double time);
struct PlaybackSample {
    std::uint64_t request=0,trialId=0;
    Snapshot snapshot;
    bool available=false,interpolated=false,barrierHistory=true;
    double requestedTime=0,loadMilliseconds=0;
    double previousRecordTime=0,nextRecordTime=0;
    std::string error;
    std::shared_ptr<const PhysicsScene> scene;
};
// Only used by the file reader thread (or bounded synchronous tests). The cache
// stores decoded blocks, not entire trials; all file handles close after a read.
class MotionBlockCache {
    struct Entry {std::shared_ptr<const std::vector<MotionFrame>>frames;std::list<std::uint64_t>::iterator recency;std::size_t bytes=0;};
    std::shared_ptr<ArchiveLocation> location_;
    std::map<std::uint64_t,Entry> blocks_;
    std::list<std::uint64_t> lru_;
    std::size_t budget_,bytes_=0,peak_=0;
    std::shared_ptr<const std::vector<MotionFrame>>get(const MotionDirectory&,const MotionBlock&);
public:
    explicit MotionBlockCache(std::size_t bytes=256*1024*1024):budget_(bytes){}
    PlaybackSample sample(const MotionDirectory&,std::uint64_t trialId,double time);
    void clear();
    void setBudget(std::size_t bytes){if(bytes!=budget_){clear();budget_=bytes;}}
    std::size_t bytes()const{return bytes_;}
    std::size_t peakBytes()const{return peak_;}
};
class PlaybackReader {
    struct Request {std::shared_ptr<const MotionDirectory>directory;std::uint64_t sequence=0,trial=0;double time=0;std::shared_ptr<const RunConfig>config;std::optional<Snapshot>eventPose;};
    mutable std::mutex mutex_;std::condition_variable wake_;std::thread worker_;
    bool stopping_=false;std::uint64_t sequence_=0;std::optional<Request> pending_;
    std::shared_ptr<const PlaybackSample> result_;MotionBlockCache cache_;
    std::shared_ptr<const PhysicsScene> scene_;
    std::shared_ptr<ArchiveLocation> sceneLocation_;
    std::uint64_t sceneTrial_=0;
    void run();
public:
    explicit PlaybackReader(std::size_t cacheBytes=256*1024*1024);
    ~PlaybackReader();
    std::uint64_t request(std::shared_ptr<const MotionDirectory>,std::uint64_t trialId,double time,std::shared_ptr<const RunConfig>config={},std::optional<Snapshot>eventPose={});
    std::shared_ptr<const PlaybackSample> latest()const;
    void invalidate();
};
}
