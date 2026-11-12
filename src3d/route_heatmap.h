#pragma once
#include "statistics.h"
#include "terrain.h"
#include <unordered_set>
namespace rs3d {
// A separate full-terrain XY grid: height never gates a visit. The exact DDA
// kernel is shared with the old toe statistics, but no counts are shared.
class RouteAccumulator {
    Grid3D grid_;
    std::uint64_t requested_,completed_=0;
    std::vector<std::uint64_t> hits_;
public:
    using Visits=std::unordered_set<std::size_t>;
    RouteAccumulator(double xMin,double xMax,double yMin,double yMax,std::uint64_t requested,double cell=1);
    RouteAccumulator(const TerrainMesh&t,std::uint64_t requested,double cell=1):RouteAccumulator(t.xMin,t.xEnd,t.yMin,t.yMax,requested,cell){}
    void visit(Visits&visits,Vec3 previous,Vec3 current)const;
    void finish(const Visits&visits,bool normal);
    void restore(std::vector<std::uint64_t>hits,std::uint64_t completed);
    const Grid3D& grid()const{return grid_;}
    const std::vector<std::uint64_t>& hits()const{return hits_;}
    std::uint64_t requested()const{return requested_;}
    std::uint64_t completed()const{return completed_;}
    Probability probability(std::uint64_t hits,bool filesComplete)const;
};
}
