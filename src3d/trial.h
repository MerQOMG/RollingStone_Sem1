#pragma once
#include "physics.h"
#include <string>
namespace rs3d {
enum class StopReason { running,resting,downstream,upstream,lateral,timeout,geometryFailure,numericFailure,cancelled };
const char* stopReasonName(StopReason reason);
bool normalCompletion(StopReason reason);
struct TrialSettings {
    double timeLimit=60;
    // Diagnostic tests can continue past rest/bounds; production leaves this false.
    bool diagnosticContinue=false;
};
struct Snapshot:PhysicsSnapshot {
    StopReason reason=StopReason::running;
    std::string error;
};
class Trial {
    PhysicsWorld world_;
    TrialSettings settings_;
    std::shared_ptr<const TerrainMesh> terrain_;
    Snapshot current_;
    double quietTime_=0;
public:
    explicit Trial(const PhysicsScene& scene,TrialSettings settings={});
    const Snapshot&step(double dt=0.002);
    const Snapshot&snapshot()const{return current_;}
    void cancel();
    PhysicsDiagnostics diagnostics()const{return world_.diagnostics();}
};
}
