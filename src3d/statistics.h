#pragma once
#include "math_types.h"
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>
#include <cstdint>
namespace rs3d {
struct GridSettings {Vec3 lower,upper,cell{1,1,1};};
class Grid3D {
    GridSettings settings_;
    std::array<std::vector<double>,3> lines_;
public:
    explicit Grid3D(GridSettings settings);
    const GridSettings&settings()const{return settings_;}
    int size(int axis)const{return int(lines_[axis].size())-1;}
    size_t count()const{return size_t(size(0))*size(1)*size(2);}
    const std::vector<double>&lines(int axis)const{return lines_[axis];}
    size_t id(int x,int y,int z)const{return (size_t(x)*size(1)+y)*size(2)+z;}
    std::array<int,3> indices(size_t id)const;
    std::optional<int> index(double coordinate,int axis)const;
    std::optional<size_t> cellAt(Vec3 point)const;
    std::optional<size_t> collectorCellAt(Vec3 a,Vec3 b,double collectorX)const;
    // Closed segment, half-open cells; outermost upper faces are closed.
    // A singleton crossing point belongs to its half-open owner, including mixed-sign corners.
    void visitSegment(Vec3 a,Vec3 b,const std::function<void(size_t)>& visitor)const;
};
struct TrialVisits {
    const Grid3D* grid;
    double collectorX;
    std::unordered_set<size_t> voxels,xy,xz;
    bool crossedCollector=false;
    std::optional<size_t> collectorCell;
    void visitSegment(Vec3 previous,Vec3 current);
};
struct Probability {std::optional<double> value;double lower=0,upper=1;};
class Statistics {
    Grid3D grid_;double collectorX_;
    std::uint64_t requested_,completed_=0,collectorNotCrossed_=0,collectorOutside_=0;
    std::vector<std::uint64_t> voxels_,xy_,xz_,collector_;
public:
    Statistics(GridSettings grid,double collectorX,std::uint64_t requested);
    TrialVisits newTrial()const{return {&grid_,collectorX_,{},{},{},false,{}};}
    void finishTrial(const TrialVisits& visits,bool normalCompletion);
    Probability probability(std::uint64_t hits,bool complete)const;
    const Grid3D& grid()const{return grid_;}
    const auto& voxelHits()const{return voxels_;}
    const auto& xyHits()const{return xy_;}
    const auto& xzHits()const{return xz_;}
    const auto& collectorHits()const{return collector_;}
    std::uint64_t requested()const{return requested_;}
    std::uint64_t completed()const{return completed_;}
    std::uint64_t collectorNotCrossed()const{return collectorNotCrossed_;}
    std::uint64_t collectorOutside()const{return collectorOutside_;}
    double collectorX()const{return collectorX_;}
};
}
