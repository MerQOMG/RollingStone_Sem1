#pragma once
#include "trial.h"
#include "archive_csv.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <mutex>
namespace rs3d {
constexpr std::uint32_t motionVersion=1,motionFrameBytes=308,motionBlockFrames=256,motionFileHeaderBytes=24,motionBlockHeaderBytes=40;
constexpr const char* motionRule="fixed-steps-plus-contact-brackets-v1";
struct HistorySettings {bool enabled=true;double interval=0;std::uint64_t cacheMiB=256;std::size_t displayVertices=2000000;}; // interval=0: auto
struct HistoryBudget {std::uint64_t stride=5;double interval=.01;std::uint64_t nominalBytes=0,maximumBytes=0;};
HistoryBudget historyBudget(const HistorySettings&,std::uint64_t n,double maxTime,double dt=.002);
struct MotionFrame {std::uint64_t step=0;Snapshot snapshot;};
struct ArchivedGeometry {CsvSpan vertices,faces;Vec3 center;double volume=0,radius=0;bool knownMass=false;};
struct MotionBlock {
    std::uint64_t trialId=0,offset=0,length=0,frames=0,firstStep=0,lastStep=0,checksum=0;
    double firstTime=0,lastTime=0;
    std::uint64_t legacyBarrierOffset=0,legacyBarrierLength=0;
};
struct MotionTrial {
    std::uint64_t id=0,frames=0;
    double firstTime=0,lastTime=0;
    std::string status="not_started",error;
    std::vector<MotionBlock> blocks;
    std::optional<ArchivedGeometry> geometry;
};
// A path can change once at transaction commit; readers never retain a file handle.
class ArchiveLocation {
    mutable std::mutex mutex_;std::filesystem::path path_;
public:
    explicit ArchiveLocation(std::filesystem::path p):path_(std::move(p)){}
    std::filesystem::path path()const{std::lock_guard<std::mutex>lock(mutex_);return path_;}
    void relocate(std::filesystem::path p){std::lock_guard<std::mutex>lock(mutex_);path_=std::move(p);}
};
struct MotionDirectory {
    std::shared_ptr<ArchiveLocation> location;
    std::uint64_t version=0;
    std::vector<std::shared_ptr<const MotionTrial>> trials;
    bool legacy=false,legacyBarrier=false,diagnostic=false;
    std::string diagnosticReason;
};
using ArchiveWrite=std::function<void(const std::string&,const std::string&)>;
using ArchiveFlush=std::function<void(const std::string&)>;
class MotionArchiveWriter {
    ArchiveWrite write_;ArchiveFlush flush_;
    std::uint64_t offset_=motionFileHeaderBytes;
    MotionDirectory directory_;
    void index(const MotionTrial&);
public:
    MotionArchiveWriter(std::shared_ptr<ArchiveLocation>,std::uint64_t n,ArchiveWrite,ArchiveFlush);
    void append(std::uint64_t id,const std::vector<MotionFrame>&frames);
    void status(std::uint64_t id,const std::string&status,const std::string&error={});
    void shape(std::uint64_t id,const ArchivedGeometry&geometry);
    std::shared_ptr<const MotionDirectory> directory()const{return std::make_shared<const MotionDirectory>(directory_);}
};
class MotionRecorder {
    MotionArchiveWriter&writer_;std::uint64_t id_,stride_;
    bool enabled_;std::optional<MotionFrame> previous_;
    std::vector<MotionFrame> block_;
    void emit(const MotionFrame&frame);
public:
    MotionRecorder(MotionArchiveWriter&writer,std::uint64_t id,std::uint64_t stride,bool enabled);
    void sample(std::uint64_t step,const Snapshot&snapshot);
    void publish();
    void finish(std::uint64_t step,const Snapshot&snapshot);
};
std::shared_ptr<const MotionDirectory> readMotionDirectory(const std::filesystem::path&directory,std::uint64_t expectedTrials);
std::vector<MotionFrame> readMotionBlock(const MotionDirectory&,const MotionBlock&);
std::shared_ptr<const MotionDirectory> recoverMotionDirectory(const std::filesystem::path&directory,std::uint64_t expectedTrials);
std::shared_ptr<const MotionDirectory> readLegacyMotionDirectory(const std::filesystem::path&directory,std::uint64_t expectedTrials);
std::vector<MotionFrame> readLegacyMotionBlock(const MotionDirectory&,const MotionBlock&);
std::uint32_t motionContactFlags(const ContactMetrics&);
}
