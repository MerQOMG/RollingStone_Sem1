#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#if !defined(JPH_DOUBLE_PRECISION) || !defined(JPH_CROSS_PLATFORM_DETERMINISTIC)
#error Missing public Jolt ABI definitions
#endif
static_assert(std::is_same_v<JPH::RVec3, JPH::DVec3>);
static_assert(sizeof(JPH::RVec3) == 32);
static_assert(_HAS_EXCEPTIONS == 1);
class Broad final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override { return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(l)); }
};
class Pair final : public JPH::ObjectLayerPairFilter {
public: bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override { return a || b; }
};
class Filter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public: bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
void require(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
void smoke() {
    using namespace JPH;
    Array<Vec3> tetra {Vec3(1,1,1), Vec3(1,-1,-1), Vec3(-1,1,-1), Vec3(-1,-1,1)};
    Array<Vec3> cube;
    for(int x : {-1,1}) for(int y : {-1,1}) for(int z : {-1,1}) cube.push_back(Vec3(float(x),float(y),float(z)));
    Broad broad; Pair pair; Filter filter;
    PhysicsSystem physics; physics.Init(16,0,64,64,broad,filter,pair);
    physics.SetGravity(Vec3(0,0,-9.81f));
    auto& bodies=physics.GetBodyInterface();
    for (const auto& points : {tetra,cube}) {
        ConvexHullShapeSettings settings(points,0);
        settings.mHullTolerance=1.1e-6f;
        settings.mDensity=2600;
        auto result=settings.Create();
        require(!result.HasError(),"zero-radius hull creation failed");
        const auto* hull=static_cast<const ConvexHullShape*>(result.Get().GetPtr());
        require(hull->GetNumPoints()==points.size(),"engine discarded vertices");
        require(hull->GetConvexRadius()==0,"nonzero convex radius");
        const double expected=points.size()==4 ? 8.0/3 : 8.0;
        require(std::abs(hull->GetVolume()/expected-1)<1e-6,"hull volume mismatch");
        require(hull->GetCenterOfMass().Length()<1e-6f,"center mismatch");
        require(std::abs(hull->GetMassProperties().mMass/(expected*2600)-1)<1e-6,"mass mismatch");
        for(auto p:points) {
            bool found=false;
            for(uint i=0;i<hull->GetNumPoints();++i) found |= (hull->GetPoint(i)+hull->GetCenterOfMass()-p).Length()<1e-6f;
            require(found,"read-back position mismatch");
        }
        BodyCreationSettings b(result.Get(),RVec3(0,0,5),Quat::sIdentity(),EMotionType::Dynamic,1);
        BodyID id=bodies.CreateAndAddBody(b,EActivation::Activate);
        require(!id.IsInvalid(),"body creation failed");
        bodies.RemoveBody(id); bodies.DestroyBody(id);
        std::cout<<"k="<<points.size()<<" radius="<<hull->GetConvexRadius()<<" V="<<hull->GetVolume()<<" mass="<<hull->GetMassProperties().mMass<<'\n';
    }
    BoxShape box(Vec3(1,2,3),0);
    require(box.GetConvexRadius()==0,"box nonzero radius");
    std::cout<<"PASS ABI: RVec3=DVec3 size="<<sizeof(RVec3)<<" exceptions=1 double=1 deterministic=1; body create/remove/destroy\n";
}
int main() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance=new JPH::Factory();
    JPH::RegisterTypes();
    int rc=0;
    try {smoke();} catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; rc=1; }
    JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance=nullptr;
    return rc;
}
