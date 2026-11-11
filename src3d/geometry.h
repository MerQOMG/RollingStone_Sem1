#pragma once
#include "math_types.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace rs3d {
struct Face{int a,b,c;};
struct Polyhedron {
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    double volume=0;
    Vec3 center;
    Mat3 inertia; // About center, density=1; multiply by rho in SI.
    double innerRadius=0,tau=0,slenderness=0;
    bool doubleFallback=false;
};
struct GeometrySettings{int kMin=20,kMax=50;double radius=2,volume=7;};
struct ShapeDiagnostics {
    int k=0,attempts=0,rootIterations=0;
    std::uint64_t trialSeed=0,attemptSeed=0;
    double milliseconds=0,engineVolumeError=0,engineMassError=0,engineCenterError=0,engineInertiaError=0,engineVertexError=0,engineInnerRadius=0;
    std::vector<std::string> rejections;
};
struct GeneratedShape{Polyhedron geometry;ShapeDiagnostics diagnostics;};
class GenerationFailure:public std::runtime_error {
public:
    ShapeDiagnostics diagnostics;
    bool cancelled;
    GenerationFailure(std::string message,ShapeDiagnostics details,bool wasCancelled=false):std::runtime_error(message),diagnostics(std::move(details)),cancelled(wasCancelled){}
};
constexpr const char* samplerVersion="stereographic-bipyramid-v1";
double referenceVolume(int k,double radius);
void validateGeometrySettings(const GeometrySettings& settings);
double hullTolerance(const std::vector<Vec3>& normalizedPoints);
// Explicit-vertex kernel intentionally has no production k/r/V support-domain check.
Polyhedron convexPolyhedron(const std::vector<Vec3>& vertices);
Polyhedron polyhedronFromMesh(const std::vector<Vec3>&vertices,const std::vector<Face>&faces);
void validatePolyhedron(const Polyhedron& p);
GeneratedShape generatePolyhedron(const GeometrySettings& settings,std::uint64_t trialSeed,
    const std::function<bool()>& cancelled={},int rejectFirstAttemptsForTest=0);
}
