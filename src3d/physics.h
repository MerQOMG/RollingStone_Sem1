#pragma once
#include "terrain.h"
#include "obstacles.h"
#include <memory>
namespace rs3d {
constexpr int terrainObjectId=1,rockObjectId=2,barrierObjectId=3;
struct BarrierSettings {
    bool enabled=false,elastic=false;
    Vec3 center{0,0,0};
    double width=10,height=5,thickness=0.2,yawDegrees=0,friction=0.45,restitution=0.45;
    double mass=2000,stiffness=500000,damping=30000;
    double initialDisplacement=0,initialVelocity=0;
};
struct DynamicsSettings {
    bool observeContacts=true; // Read-only diagnostics; excluded from physical sample identity.
    double density=2600,friction=0.45,restitution=0.45,airDensity=1.225,dragCoefficient=1,rollingCoefficient=0.02;
    Vec3 gravity{0,0,-9.81};
    bool allowSleeping=true;
    // Keep separate coplanar mesh contacts for stable irregular support faces.
    // Replay explicitly restores true for output produced before this setting.
    bool manifoldReduction=false;
};
struct InitialState {
    Vec3 shapeOrigin{10,0,0}; // World position of canonical double COM; Jolt COM offset is added internally.
    Quaternion rotation;
    Vec3 velocity,angularVelocity;
};
struct PhysicsScene {
    Polyhedron shape;
    double radius=2;
    std::shared_ptr<const TerrainMesh> terrain;
    std::shared_ptr<const ObstacleSet> obstacles;
    BarrierSettings barrier;
    DynamicsSettings dynamics;
    InitialState initial;
    unsigned diagnosticBodyCapacity=0; // Failure-injection tests only; 0 uses the validated budget.
};
struct PhysicsCapacity {unsigned bodies=16,pairs=1024,contacts=1024;};
PhysicsCapacity physicsCapacity(std::size_t obstacleCount);
std::uint64_t physicsUpdateCount(); // Process-wide read-only audit counter.
struct ContactMetrics {
    int obstacle=0,touchingObstacle=0,unknown=0;
    // Listener manifolds include speculative contacts that can resolve a collision
    // before positive penetration. They are not measured impulses/contact forces.
    int ground=0,barrier=0;
    int touchingGround=0,touchingBarrier=0;
    double maxPenetration=0;
    double groundFriction=0,groundRestitution=0,barrierFriction=0,barrierRestitution=0;
};
struct PairMaterial {float friction=0,restitution=0;bool recognized=false;};
PairMaterial contactMaterial(std::uint64_t object1,float mu1,float e1,std::uint64_t object2,float mu2,float e2);
struct BarrierSnapshot {
    bool enabled=false,elastic=false,touched=false,settled=true;
    Vec3 center;
    Quaternion rotation;
    double displacement=0,velocity=0,supportForce=0,contactForce=0;
    double kineticEnergy=0,springEnergy=0,dampingEnergy=0,supportWorkResidual=0;
    double contactImpulse=0,maxDisplacement=0,peakSupportForce=0,peakContactForce=0;
};
void validateBarrierSettings(const BarrierSettings&barrier);
Vec3 barrierAxis(const BarrierSettings&barrier);
Vec3 initialBarrierCenter(const BarrierSettings&barrier);
struct PhysicsSnapshot {
    double time=0;
    Vec3 center,shapeOrigin,velocity,angularVelocity,angularMomentum;
    Quaternion rotation;
    double kineticEnergy=0,potentialEnergy=0,rollingEnergyRemoved=0,airWork=0;
    ContactMetrics contacts;
    BarrierSnapshot barrier;
    bool active=false;
};
enum class ContactSource { constraint,ccdCandidate,ccdResolved,ccdDiscarded };
struct ContactBodyPose {
    Vec3 center,shapeOrigin,velocity,angularVelocity;
    Quaternion rotation;
};
struct SolvedContact {
    std::uint64_t step=0,object1=0,object2=0;
    unsigned substep=0,body1=0,body2=0,subshape1=0,subshape2=0;
    double timeLower=0,timeUpper=0,gap=0,normalImpulse=0,ccdFraction=-1;
    ContactSource source=ContactSource::constraint;
    bool retained=false;
    Vec3 point1,point2,normal;
    ContactBodyPose pose1,pose2;
};
struct PhysicsDiagnostics {
    PhysicsCapacity capacity;
    std::uint64_t temporaryAllocatorBytes=10*1024*1024;
    std::uint64_t temporaryAllocatorPeakBytes=0;
    double mass=0,slop=0,engineInnerRadius=0,linearCastThreshold=0.25,linearCastMaxPenetration=0;
    int collisionSteps=4,velocityIterations=10,positionIterations=2;
    std::uint64_t updates=0,internalSteps=0;
    bool observerOverflow=false;
    bool contactObservationAvailable=false;
    bool hasBarrier=false;
    std::array<Vec3,8> barrierWorldCorners{}; // Read back from actual Jolt body/shape.
    std::uint64_t rollingContactSteps=0,rollingAppliedSteps=0,rollingRejectedSteps=0;
};
void validatePhysicsScene(const PhysicsScene& scene);
InitialState supportedRelease(const Polyhedron& shape,const TerrainMesh& terrain,double x,double y,double gap,Quaternion rotation={});
BarrierSettings groundedBarrier(const TerrainMesh& terrain,BarrierSettings barrier);
class Trial;
class PhysicsWorld {
    struct Impl;std::unique_ptr<Impl> impl_;
    void advance(double dt); // Only Trial owns time advancement.
    friend class Trial;
public:
    explicit PhysicsWorld(const PhysicsScene& scene);
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&)=delete;
    PhysicsWorld&operator=(const PhysicsWorld&)=delete;
    PhysicsSnapshot snapshot()const;
    PhysicsDiagnostics diagnostics()const;
    const std::vector<SolvedContact>& observations()const;
    std::vector<Obstacle> obstacleReadback()const;
};
}
