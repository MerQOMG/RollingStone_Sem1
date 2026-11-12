#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include "geometry.h"
namespace rs3d {
// Local input is centered on the canonical double COM; engine's small COM offset is reported.
JPH::RefConst<JPH::Shape> createCollisionShape(const Polyhedron& p,double radius,double density,ShapeDiagnostics& diagnostics);
}
