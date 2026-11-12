#pragma once
#include "terrain.h"
#include <memory>
namespace rs3d {
constexpr int terrainObjectId=1,rockObjectId=2,barrierObjectId=3;
struct BarrierSettings {
    bool enabled=false;
    Vec3 center{0,0,0};
    double width=10,height=5,thickness=0.2,yawDegrees=0,friction=0.45,restitution=0.45;
};
struct DynamicsSettings {
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
    BarrierSettings barrier;
    DynamicsSettings dynamics;
    InitialState initial;
};
struct ContactMetrics {
    // Listener manifolds include speculative contacts that can resolve a collision
    // before positive penetration. They are not measured impulses/contact forces.
    int ground=0,barrier=0;
    int touchingGround=0,touchingBarrier=0;
    double maxPenetration=0;
    double groundFriction=0,groundRestitution=0,barrierFriction=0,barrierRestitution=0;
};
struct PhysicsSnapshot {
    double time=0;
    Vec3 center,shapeOrigin,velocity,angularVelocity,angularMomentum;
    Quaternion rotation;
    double kineticEnergy=0,potentialEnergy=0,rollingEnergyRemoved=0,airWork=0;
    ContactMetrics contacts;
    bool active=false;
};
struct PhysicsDiagnostics {
    double mass=0,slop=0,engineInnerRadius=0,linearCastThreshold=0.25,linearCastMaxPenetration=0;
    int collisionSteps=4,velocityIterations=10,positionIterations=2;
    std::uint64_t updates=0,internalSteps=0;
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
};
}
