#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include "physics.h"
#include "engine_geometry.h"
#include <map>

namespace rs3d {
namespace {
JPH::Vec3 jvec(Vec3 v){return {float(v.x),float(v.y),float(v.z)};}
JPH::RVec3 jpos(Vec3 v){return {v.x,v.y,v.z};}
Vec3 vec(JPH::Vec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
Vec3 vec(JPH::DVec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
Quaternion quat(JPH::QuatArg q){return {q.GetW(),q.GetX(),q.GetY(),q.GetZ()};}
JPH::Quat jquat(Quaternion q){return JPH::Quat(float(q.x),float(q.y),float(q.z),float(q.w)).Normalized();}
Mat3 mat(JPH::Mat44Arg a){Mat3 m;for(int i=0;i<3;++i)for(int j=0;j<3;++j)m(i,j)=a(i,j);return m;}
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
bool barrierOverlaps(const PhysicsScene&s){
    const auto&b=s.barrier;const auto rockRotation=s.initial.rotation.matrix(),boardRotation=axisRotation({0,0,1},b.yawDegrees*pi/180);
    std::vector<Vec3> rock,board,axes;
    for(auto p:s.shape.vertices)rock.push_back(s.initial.shapeOrigin+rockRotation*(p-s.shape.center));
    for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})board.push_back(b.center+boardRotation*Vec3{x*b.thickness/2,y*b.width/2,z*b.height/2});
    for(Vec3 axis:{Vec3{1,0,0},{0,1,0},{0,0,1}})axes.push_back(boardRotation*axis);
    for(auto f:s.shape.faces){axes.push_back(cross(rock[f.b]-rock[f.a],rock[f.c]-rock[f.a]));
        for(Vec3 edge:{rock[f.b]-rock[f.a],rock[f.c]-rock[f.b],rock[f.a]-rock[f.c]})for(int i=0;i<3;++i)axes.push_back(cross(edge,axes[i]));}
    for(auto axis:axes){if(norm(axis)<1e-12)continue;axis=unit(axis);double amin=1e300,amax=-1e300,bmin=1e300,bmax=-1e300;
        for(auto p:rock){double v=dot(p,axis);amin=std::min(amin,v);amax=std::max(amax,v);}for(auto p:board){double v=dot(p,axis);bmin=std::min(bmin,v);bmax=std::max(bmax,v);}
        if(std::min(amax,bmax)-std::max(amin,bmin)<=1e-8*s.radius)return false;
    }return true;
}
class Broad final:public JPH::BroadPhaseLayerInterface {
public:JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer)const override{return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer));}
};
class Pair final:public JPH::ObjectLayerPairFilter {
public:bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{return a||b;}
};
class Filter final:public JPH::ObjectVsBroadPhaseLayerFilter {
public:bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{return a||b.GetValue();}
};
float friction(const JPH::Body&a,const JPH::SubShapeID&,const JPH::Body&b,const JPH::SubShapeID&){return a.IsStatic()?a.GetFriction():b.GetFriction();}
float restitution(const JPH::Body&a,const JPH::SubShapeID&,const JPH::Body&b,const JPH::SubShapeID&){return a.IsStatic()?a.GetRestitution():b.GetRestitution();}
struct ContactAnchor {Vec3 staticPoint,rockLocal;};
struct GroundContact {Vec3 normal;std::vector<ContactAnchor>anchors;};
class Contacts final:public JPH::ContactListener {
    void record(const JPH::Body&a,const JPH::Body&b,const JPH::ContactManifold&m,const JPH::ContactSettings&s){
        const auto&fixed=a.IsStatic()?a:b;const auto&rock=a.IsDynamic()?a:b;
        metrics.maxPenetration=std::max(metrics.maxPenetration,double(m.mPenetrationDepth));
        if(fixed.GetUserData()==terrainObjectId){
            ++metrics.ground;if(m.mPenetrationDepth>=-slop)++metrics.touchingGround;
            metrics.groundFriction=s.mCombinedFriction;metrics.groundRestitution=s.mCombinedRestitution;
            if(m.mRelativeContactPointsOn1.empty())return;
            Vec3 normal=vec(m.mWorldSpaceNormal)*(a.IsStatic()?1:-1);
            GroundContact contact;contact.normal=normal;auto inverseRotation=transpose(quat(rock.GetRotation()).matrix());
            for(unsigned index=0;index<m.mRelativeContactPointsOn1.size();++index){
                Vec3 ps=vec(a.IsStatic()?m.GetWorldSpaceContactPointOn1(index):m.GetWorldSpaceContactPointOn2(index));
                Vec3 pr=vec(a.IsDynamic()?m.GetWorldSpaceContactPointOn1(index):m.GetWorldSpaceContactPointOn2(index));
                contact.anchors.push_back({ps,inverseRotation*(pr-vec(rock.GetCenterOfMassPosition()))});}
            ground[{a.GetID(),m.mSubShapeID1,b.GetID(),m.mSubShapeID2}]=std::move(contact);
        }else if(fixed.GetUserData()==barrierObjectId){
            ++metrics.barrier;if(m.mPenetrationDepth>=-slop)++metrics.touchingBarrier;
            metrics.barrierFriction=s.mCombinedFriction;metrics.barrierRestitution=s.mCombinedRestitution;
        }
    }
public:
    double slop=0;
    ContactMetrics metrics;
    std::map<JPH::SubShapeIDPair,GroundContact> ground;
    void OnContactAdded(const JPH::Body&a,const JPH::Body&b,const JPH::ContactManifold&m,JPH::ContactSettings&s)override{record(a,b,m,s);}
    void OnContactPersisted(const JPH::Body&a,const JPH::Body&b,const JPH::ContactManifold&m,JPH::ContactSettings&s)override{record(a,b,m,s);}
    void OnContactRemoved(const JPH::SubShapeIDPair&p)override{ground.erase(p);}
    Vec3 normal(Vec3 center,Quaternion rotation,Vec3 velocity,Vec3 omega)const{
        Vec3 sum;auto r=rotation.matrix();
        for(auto&item:ground){const auto&c=item.second;for(auto anchor:c.anchors){auto arm=r*anchor.rockLocal;double separation=dot(center+arm-anchor.staticPoint,c.normal);
            if(separation<=2*slop+1e-7&&dot(velocity+cross(omega,arm),c.normal)<=0.05){sum+=c.normal;break;}}}
        return norm(sum)>1e-12?unit(sum):Vec3{};
    }
};
}

InitialState supportedRelease(const Polyhedron&p,const TerrainMesh&t,double x,double y,double gap,Quaternion rotation){
    require(std::isfinite(gap)&&gap>=0,"release gap must be finite and nonnegative");
    return {{x,y,supportHeight(p,t,x,y,rotation.matrix())+gap},rotation,{},{}};
}
BarrierSettings groundedBarrier(const TerrainMesh&t,BarrierSettings b){
    std::vector<Vec3> vertices;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})vertices.push_back({x*b.thickness/2,y*b.width/2,z*b.height/2});
    auto box=convexPolyhedron(vertices);b.center.z=supportHeight(box,t,b.center.x,b.center.y,axisRotation({0,0,1},b.yawDegrees*pi/180));return b;
}
void validatePhysicsScene(const PhysicsScene&s){
    validatePolyhedron(s.shape);const auto&d=s.dynamics;const auto&b=s.barrier;
    for(double value:{s.radius,d.density,d.friction,d.restitution,d.airDensity,d.dragCoefficient,d.rollingCoefficient,b.width,b.height,b.thickness,b.yawDegrees,b.friction,b.restitution})require(std::isfinite(value),"non-finite physics parameter");
    require(s.radius>0&&d.density>0&&d.friction>=0&&d.restitution>=0&&d.restitution<=1&&d.airDensity>=0&&d.dragCoefficient>=0&&d.rollingCoefficient>=0,"invalid material parameter");
    require(b.width>0&&b.height>0&&b.thickness>0&&b.friction>=0&&b.restitution>=0&&b.restitution<=1&&finite(b.center),"invalid rigid barrier");
    require(finite(d.gravity)&&finite(s.initial.shapeOrigin)&&finite(s.initial.velocity)&&finite(s.initial.angularVelocity),"non-finite initial state/gravity");
    s.initial.rotation.matrix();
    if(s.terrain){double support=supportHeight(s.shape,*s.terrain,s.initial.shapeOrigin.x,s.initial.shapeOrigin.y,s.initial.rotation.matrix());require(s.initial.shapeOrigin.z>=support-1e-8*s.radius,"initial solid penetrates terrain");}
    if(b.enabled)require(!barrierOverlaps(s),"initial solid penetrates rigid barrier");
}

struct PhysicsWorld::Impl {
    Broad broad;Pair pair;Filter filter;Contacts contacts;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl allocator{10*1024*1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::RefConst<JPH::Shape> stoneShape;
    JPH::BodyID rock;std::vector<JPH::BodyID> bodies;
    DynamicsSettings settings;double radius,time=0,rollingRemoved=0,airWork=0;
    Mat3 bodyInertia;
    PhysicsDiagnostics diagnostic;
    explicit Impl(const PhysicsScene&s):settings(s.dynamics),radius(s.radius){
        validatePhysicsScene(s);ShapeDiagnostics shapeDiagnostic;
        stoneShape=createCollisionShape(s.shape,s.radius,s.dynamics.density,shapeDiagnostic);
        auto mass=stoneShape->GetMassProperties();diagnostic.mass=mass.mMass;bodyInertia=mat(mass.mInertia);
        diagnostic.engineInnerRadius=stoneShape->GetInnerRadius();
        diagnostic.slop=std::min({1e-3,1e-4*s.radius,0.01*s.shape.innerRadius});
        diagnostic.linearCastMaxPenetration=std::min(0.02,std::max(0.002,0.002*s.radius)/(4*diagnostic.engineInnerRadius));
        system.Init(16,0,1024,1024,broad,filter,pair);system.SetGravity(jvec(settings.gravity));
        JPH::PhysicsSettings physics;physics.mPenetrationSlop=float(diagnostic.slop);physics.mMinVelocityForRestitution=0.15f;
        physics.mLinearCastThreshold=0.25f;physics.mLinearCastMaxPenetration=float(diagnostic.linearCastMaxPenetration);
        physics.mNumVelocitySteps=10;physics.mNumPositionSteps=2;system.SetPhysicsSettings(physics);
        system.SetCombineFriction(friction);system.SetCombineRestitution(restitution);contacts.slop=diagnostic.slop;system.SetContactListener(&contacts);
        auto&bi=system.GetBodyInterface();
        auto add=[&](const JPH::BodyCreationSettings&b){auto id=bi.CreateAndAddBody(b,b.mMotionType==JPH::EMotionType::Dynamic?JPH::EActivation::Activate:JPH::EActivation::DontActivate);require(!id.IsInvalid(),"Jolt body allocation failed");bodies.push_back(id);return id;};
        if(s.terrain){
            JPH::VertexList vertices;JPH::IndexedTriangleList triangles;
            for(auto p:s.terrain->vertices)vertices.emplace_back(float(p.x),float(p.y),float(p.z));
            for(auto f:s.terrain->faces)triangles.emplace_back(f.a,f.b,f.c,0);
            JPH::MeshShapeSettings mesh(std::move(vertices),std::move(triangles));auto built=mesh.Create();
            if(built.HasError())throw std::runtime_error("terrain MeshShape: "+std::string(built.GetError().c_str()));
            JPH::BodyCreationSettings ground(built.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,0);
            ground.mUserData=terrainObjectId;ground.mFriction=float(settings.friction);ground.mRestitution=float(settings.restitution);add(ground);
        }
        if(s.barrier.enabled){
            const auto&b=s.barrier;JPH::RefConst<JPH::Shape> box=new JPH::BoxShape(JPH::Vec3(float(b.thickness/2),float(b.width/2),float(b.height/2)),0);
            JPH::BodyCreationSettings board(box,jpos(b.center),JPH::Quat::sRotation(JPH::Vec3::sAxisZ(),float(b.yawDegrees*pi/180)),JPH::EMotionType::Static,0);
            board.mUserData=barrierObjectId;board.mFriction=float(b.friction);board.mRestitution=float(b.restitution);auto id=add(board);
            auto actual=bi.GetShape(id);auto half=static_cast<const JPH::BoxShape*>(actual.GetPtr())->GetHalfExtent();auto world=bi.GetWorldTransform(id);int corner=0;diagnostic.hasBarrier=true;
            for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})diagnostic.barrierWorldCorners[corner++]=vec(world*JPH::Vec3(x*half.GetX(),y*half.GetY(),z*half.GetZ()));
        }
        JPH::BodyCreationSettings body(stoneShape,jpos(s.initial.shapeOrigin),jquat(s.initial.rotation),JPH::EMotionType::Dynamic,1);
        body.mUserData=rockObjectId;body.mLinearVelocity=jvec(s.initial.velocity);body.mAngularVelocity=jvec(s.initial.angularVelocity);
        body.mLinearDamping=0;body.mAngularDamping=0;body.mApplyGyroscopicForce=true;body.mAllowSleeping=settings.allowSleeping;
        body.mMaxLinearVelocity=10000;body.mMaxAngularVelocity=10000;
        body.mMotionQuality=JPH::EMotionQuality::LinearCast;body.mEnhancedInternalEdgeRemoval=true;
        // Deliberately different dynamic-body values: pair callbacks must select static surface values.
        body.mFriction=0.731f;body.mRestitution=0.817f;rock=add(body);system.OptimizeBroadPhase();
    }
    ~Impl(){auto&bi=system.GetBodyInterface();for(auto id:bodies){bi.RemoveBody(id);bi.DestroyBody(id);}system.SetContactListener(nullptr);}
    PhysicsSnapshot snapshot()const{
        const auto&bi=system.GetBodyInterface();PhysicsSnapshot s;s.time=time;
        s.center=vec(bi.GetCenterOfMassPosition(rock));s.shapeOrigin=vec(bi.GetPosition(rock));s.rotation=quat(bi.GetRotation(rock));
        s.velocity=vec(bi.GetLinearVelocity(rock));s.angularVelocity=vec(bi.GetAngularVelocity(rock));s.active=bi.IsActive(rock);
        auto r=s.rotation.matrix();auto inertia=r*bodyInertia*transpose(r);s.angularMomentum=inertia*s.angularVelocity;
        s.kineticEnergy=0.5*diagnostic.mass*norm2(s.velocity)+0.5*dot(s.angularVelocity,s.angularMomentum);
        s.potentialEnergy=-diagnostic.mass*dot(settings.gravity,s.center);s.rollingEnergyRemoved=rollingRemoved;s.airWork=airWork;s.contacts=contacts.metrics;
        require(finite(s.center)&&finite(s.velocity)&&finite(s.angularVelocity)&&std::isfinite(s.kineticEnergy)&&std::isfinite(s.potentialEnergy),"non-finite physics snapshot");return s;
    }
    void advance(double dt){
        require(std::isfinite(dt)&&dt>0&&dt<=0.01,"physics dt must lie in (0,0.01] seconds");
        auto before=snapshot();auto&bi=system.GetBodyInterface();
        if(before.active){
            Vec3 drag=before.velocity*(-0.5*settings.airDensity*settings.dragCoefficient*pi*radius*radius*norm(before.velocity));
            if(norm2(drag)>0){bi.AddForce(rock,jvec(drag),JPH::EActivation::DontActivate);airWork+=dot(drag,before.velocity)*dt;}
            Vec3 n=contacts.normal(before.center,before.rotation,before.velocity,before.angularVelocity);
            if(norm2(n)>0)++diagnostic.rollingContactSteps;
            Vec3 tangent=before.angularVelocity-n*dot(before.angularVelocity,n);
            if(settings.rollingCoefficient>0&&norm2(n)>0&&norm(tangent)>1e-10){
                Vec3 u=unit(tangent);Mat3 inv=mat(bi.GetInverseInertia(rock));double inverseEffective=dot(u,inv*u);
                require(inverseEffective>0,"invalid effective rolling inertia");
                double impulse=std::min(settings.rollingCoefficient*diagnostic.mass*norm(settings.gravity)*radius*dt,norm(tangent)/inverseEffective);
                Mat3 inertia=inverse(inv);double initialEnergy=0.5*dot(before.angularVelocity,inertia*before.angularVelocity);
                bi.AddAngularImpulse(rock,jvec(u*(-impulse)));auto omega=vec(bi.GetAngularVelocity(rock));
                double finalEnergy=0.5*dot(omega,inertia*omega);
                if(finalEnergy>initialEnergy||dot(omega,u)<-1e-7){++diagnostic.rollingRejectedSteps;bi.SetAngularVelocity(rock,jvec(before.angularVelocity));}
                else {++diagnostic.rollingAppliedSteps;rollingRemoved+=initialEnergy-finalEnergy;}
            }
        }
        contacts.metrics={};auto result=system.Update(float(dt),4,&allocator,&jobs);
        if(result!=JPH::EPhysicsUpdateError::None)throw std::runtime_error("Jolt Update error flags="+std::to_string(unsigned(result)));
        time+=dt;++diagnostic.updates;diagnostic.internalSteps+=4;snapshot();
    }
};
PhysicsWorld::PhysicsWorld(const PhysicsScene&s):impl_(std::make_unique<Impl>(s)){}
PhysicsWorld::~PhysicsWorld()=default;
void PhysicsWorld::advance(double dt){impl_->advance(dt);}
PhysicsSnapshot PhysicsWorld::snapshot()const{return impl_->snapshot();}
PhysicsDiagnostics PhysicsWorld::diagnostics()const{return impl_->diagnostic;}
}
