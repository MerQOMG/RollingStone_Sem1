#include "engine_geometry.h"
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <set>

namespace rs3d {
JPH::RefConst<JPH::Shape> createCollisionShape(const Polyhedron&p,double radius,double density,ShapeDiagnostics&d){
    if(!(radius>0&&density>0&&std::isfinite(radius)&&std::isfinite(density)))throw std::runtime_error("invalid collision units/density");
    std::vector<Vec3> canonical;JPH::Array<JPH::Vec3> points;
    for(auto v:p.vertices){auto q=(v-p.center)/radius;canonical.push_back(q);points.emplace_back(float(q.x),float(q.y),float(q.z));}
    JPH::ConvexHullShapeSettings settings(points,0);settings.mHullTolerance=float(hullTolerance(canonical));settings.mDensity=float(density);
    auto made=settings.Create();if(made.HasError())throw std::runtime_error("Jolt shape: "+std::string(made.GetError().c_str()));
    const auto* hull=static_cast<const JPH::ConvexHullShape*>(made.Get().GetPtr());
    if(hull->GetConvexRadius()!=0)throw std::runtime_error("engine convex radius is not zero");
    if(hull->GetNumPoints()!=p.vertices.size())throw std::runtime_error("engine discarded vertices");
    JPH::RefConst<JPH::Shape> scaled=new JPH::ScaledShape(hull,JPH::Vec3::sReplicate(float(radius)));
    auto mass=scaled->GetMassProperties();Mat3 inertia;for(int i=0;i<3;++i)for(int j=0;j<3;++j)inertia(i,j)=mass.mInertia(i,j);
    d.engineVolumeError=std::abs(scaled->GetVolume()/p.volume-1);
    d.engineMassError=std::abs(mass.mMass/(p.volume*density)-1);
    d.engineCenterError=scaled->GetCenterOfMass().Length();
    d.engineInertiaError=frobenius(inertia-p.inertia*density)/frobenius(p.inertia*density);
    d.engineInnerRadius=scaled->GetInnerRadius();d.engineVertexError=0;std::set<unsigned> matched;
    for(auto target:canonical){double best=1e300;unsigned index=0;
        for(unsigned i=0;i<hull->GetNumPoints();++i){auto q=hull->GetPoint(i)+hull->GetCenterOfMass();double error=norm(Vec3{q.GetX(),q.GetY(),q.GetZ()}-target)*radius;if(error<best){best=error;index=i;}}
        d.engineVertexError=std::max(d.engineVertexError,best);matched.insert(index);
    }
    if(matched.size()!=p.vertices.size()||d.engineVertexError>1e-5*radius)throw std::runtime_error("engine vertex position matching failed");
    if(d.engineVolumeError>1e-4||d.engineMassError>1e-4||d.engineCenterError>1e-5*radius||d.engineInertiaError>1e-3||!(d.engineInnerRadius>0))throw std::runtime_error("engine mass/COM/inertia/inner radius tolerance failed");
    return scaled;
}
}
