#pragma once
#include "batch.h"
#include <thread>
namespace rs3d {
enum class SessionState { idle,running,paused,cancelling,completed,failed };
enum class SessionPhase { idle,generating,preview,background,finished };
const char*sessionStateName(SessionState state);
struct SessionView {
    SessionState state=SessionState::idle;
    SessionPhase phase=SessionPhase::idle;
    std::shared_ptr<const RunConfig>config;
    std::shared_ptr<const PhysicsScene>scene;
    Snapshot snapshot;
    std::shared_ptr<const BatchResult>result;
    std::uint64_t trialId=0,processed=0,completed=0,failures=0;
    double wallSeconds=0;
    bool pauseAcknowledged=false,workerFinished=true;
    std::string error;
};
// UI-neutral owner of one numerical worker. No graphics API, no camera RNG.
// The latest snapshot is bounded to one value; simulation is paced by its own clock.
class Session {
    mutable std::mutex mutex_;
    SessionView view_;
    SceneDraft draft_;
    TrialInput previewInput_;
    ShapeDiagnostics previewDiagnostics_;
    std::shared_ptr<const PhysicsScene>previewScene_;
    std::shared_ptr<BatchControl>control_;
    std::thread worker_;
    std::chrono::steady_clock::time_point started_;
    void launch(bool single,int preview);
public:
    explicit Session(const SceneDraft&draft);
    ~Session();
    Session(const Session&)=delete;Session&operator=(const Session&)=delete;
    bool busy()const;
    const SceneDraft&draft()const{return draft_;} // UI thread only.
    SessionView view()const;
    void apply(const SceneDraft&draft);
    void randomize();
    void startSingle(){launch(true,2);}
    void startBatch(int preview=2){launch(false,preview);}
    void pause(bool value);
    void cancel();
    void reset(); // Only once worker has acknowledged completion/cancellation.
};
}
