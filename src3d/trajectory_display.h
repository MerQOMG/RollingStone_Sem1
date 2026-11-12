#pragma once
#include "motion_archive.h"
#include <atomic>
#include <thread>
#include <condition_variable>
namespace rs3d {
struct TimedPosition {double time=0;Vec3 center;};
struct DisplayTrajectory {
    std::uint64_t trialId=0,sourcePoints=0;
    StopReason status=StopReason::running;
    double maximumDeviation=0;
    std::vector<TimedPosition> points;
};
// Deterministic largest-error segment splitting. Keeps exact first/last points,
// then important turns, within each trial's share of the total display budget.
std::shared_ptr<const DisplayTrajectory> simplifyTrajectory(std::uint64_t trialId,const std::vector<TimedPosition>&source,StopReason status,std::size_t maximumPoints);
struct TrajectoryLoadView {
    std::uint64_t generation=0,version=0,processed=0;
    std::vector<std::shared_ptr<const DisplayTrajectory>> trajectories;
    bool finished=false;
    std::string error;
};
class TrajectoryArchiveReader {
    struct Request {std::shared_ptr<const MotionDirectory>directory;std::size_t budget=0;std::uint64_t generation=0;};
    mutable std::mutex mutex_;std::condition_variable wake_;std::thread worker_;
    std::atomic<std::uint64_t>generation_{0};std::atomic<bool>stopping_{false};
    std::optional<Request>pending_;std::shared_ptr<const TrajectoryLoadView>view_;
    void run();
public:
    TrajectoryArchiveReader();~TrajectoryArchiveReader();
    void request(std::shared_ptr<const MotionDirectory>,std::size_t budget);
    std::shared_ptr<const TrajectoryLoadView>view()const;
    void cancel();
};
}
