#pragma once
#include "config.h"
#include "trial.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>
namespace rs3d {
enum class BatchStatus { complete,incompleteFailure,cancelled,ioError };
const char* batchStatusName(BatchStatus status);
class BatchControl {
    std::atomic<bool> cancelled_{false},paused_{false},acknowledged_{false};
    mutable std::mutex mutex_;std::condition_variable wake_;
    std::chrono::steady_clock::duration pausedTime_{};
    void waitPaused(std::unique_lock<std::mutex>&lock);
public:
    void cancel(){{std::lock_guard<std::mutex>lock(mutex_);cancelled_=true;}wake_.notify_all();}
    void pause(bool value){{std::lock_guard<std::mutex>lock(mutex_);paused_=value;}wake_.notify_all();}
    bool cancelled()const{return cancelled_;}
    bool paused()const{return paused_;}
    bool pauseAcknowledged()const{return acknowledged_;}
    bool checkpoint(); // True means continue. Pause/cancel only at safe boundaries.
    std::chrono::steady_clock::duration pausedTime()const{std::lock_guard<std::mutex>lock(mutex_);return pausedTime_;}
    bool pace(double simulatedSeconds,std::chrono::steady_clock::time_point start,std::chrono::steady_clock::duration pauseAtStart);
};
struct Timings {double generation=0,physics=0,statistics=0,output=0,wall=0;}; // seconds
struct TrialRecord {
    TrialInput input;
    ShapeDiagnostics shapeDiagnostics;
    double radius=0,requestedVolume=0,volume=0,innerRadius=0,tau=0,slenderness=0;
    Vec3 canonicalCenter;Mat3 canonicalInertia;
    bool doubleFallback=false,hasShape=false,hasInitialState=false;
    InitialState initial;
    Snapshot first,last;
    PhysicsDiagnostics physics;
    Timings timing;
};
enum class BatchEventKind { generating,trialStarted,snapshot,trialFinished,finished };
struct BatchEvent {
    BatchEventKind kind;
    std::uint64_t trialId=0,processed=0,completed=0,failures=0;
    bool preview=false;
    const PhysicsScene*scene=nullptr; // Only valid during callback; observers copy as needed.
    const Snapshot*snapshot=nullptr;
};
struct BatchTestFaults { // No CLI/GUI switch exposes these deterministic test hooks.
    std::uint64_t rejectTrial=0,numericFailureTrial=0;
    int rejectedAttempts=0;
    std::uint64_t writeFailureAfterBytes=0;
};
struct BatchOptions {
    int preview=0;
    std::function<void(const BatchEvent&)> observer;
    BatchTestFaults faults;
    // Idle GUI single-run freezes its exact preview specimen, not a regenerated shape.
    std::shared_ptr<const PhysicsScene> singleScene;
    std::optional<TrialInput> singleInput;
    std::optional<ShapeDiagnostics> singleDiagnostics;
};
struct BatchResult {
    std::shared_ptr<const RunConfig>config;
    Statistics statistics;
    BatchStatus status=BatchStatus::incompleteFailure;
    std::vector<TrialRecord>trials;
    std::filesystem::path directory;
    std::string error;
    Timings timing;
    std::uint64_t failures=0,timeouts=0,cancelledTrials=0,updates=0,internalSteps=0;
    explicit BatchResult(std::shared_ptr<const RunConfig>c):config(std::move(c)),statistics(config->grid,config->collectorX,config->source.count){}
    std::uint64_t unstarted()const{return config->source.count-trials.size();}
};
BatchResult runBatch(std::shared_ptr<const RunConfig>config,BatchControl&control,const BatchOptions&options={});
// Clip an absorbing exit before feeding statistics, including custom boxes outside terrain.
Vec3 clipToTerrainExit(Vec3 previous,Vec3 next,const TerrainMesh&terrain);
}
