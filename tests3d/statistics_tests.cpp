#include "statistics.h"
#include "random.h"
#include "exact_parameter.h"
#include <iostream>
#include <set>
#include <fstream>
#include <iomanip>
using namespace rs3d;
namespace {
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::set<size_t> visits(const Grid3D&g,Vec3 a,Vec3 b){std::set<size_t> result;g.visitSegment(a,b,[&](size_t i){result.insert(i);});return result;}
// Independent oracle: intersect the segment's parameter interval with each cell's
// six half-open inequalities. Equal interval endpoints retain their inclusivity.
bool intersects(const Grid3D&g,std::array<int,3>cell,Vec3 a,Vec3 b){
    ExactParameter low(0),high(1);bool lowClosed=true,highClosed=true;
    auto lower=[&](ExactParameter v,bool closed){if(v>low){low=v;lowClosed=closed;}else if(v==low)lowClosed=lowClosed&&closed;};
    auto upper=[&](ExactParameter v,bool closed){if(v<high){high=v;highClosed=closed;}else if(v==high)highClosed=highClosed&&closed;};
    for(int axis=0;axis<3;++axis){double l=g.lines(axis)[cell[axis]],h=g.lines(axis)[cell[axis]+1],d=b[axis]-a[axis];bool outer=cell[axis]+1==g.size(axis);
        if(d==0){if(a[axis]<l||a[axis]>h||(!outer&&a[axis]==h))return false;}
        else if(d>0){lower(ExactParameter::crossing(l,a[axis],b[axis]),true);upper(ExactParameter::crossing(h,a[axis],b[axis]),outer);}
        else{lower(ExactParameter::crossing(h,a[axis],b[axis]),outer);upper(ExactParameter::crossing(l,a[axis],b[axis]),true);}
    }
    return low<high||(low==high&&lowClosed&&highClosed);
}
std::set<size_t> oracle(const Grid3D&g,Vec3 a,Vec3 b){std::set<size_t> out;for(size_t i=0;i<g.count();++i)if(intersects(g,g.indices(i),a,b))out.insert(i);return out;}
void fixtures(std::ostream*dump){
    if(dump)*dump<<std::setprecision(17)<<"grid_x,grid_y,grid_z,ax,ay,az,bx,by,bz,ids\n";
    Grid3D g({{0,0,0},{2,2,2},{1,1,1}});
    require(visits(g,{-1,.5,.5},{3,.5,.5})==std::set<size_t>{g.id(0,0,0),g.id(1,0,0)},"cross multi-cell and clip");
    require(visits(g,{1,1,0},{1,1,2})==std::set<size_t>{g.id(1,1,0),g.id(1,1,1)},"edge ownership");
    require(visits(g,{0,0,0},{2,2,2})==std::set<size_t>{g.id(0,0,0),g.id(1,1,1)},"diagonal corner ownership");
    require(visits(g,{0,2,0},{2,0,2})==std::set<size_t>{g.id(0,1,0),g.id(1,1,1),g.id(1,0,1)},"mixed-sign singleton corner owner");
    require(visits(g,{1,.5,.5},{0,.5,.5})==std::set<size_t>{g.id(1,0,0),g.id(0,0,0)},"negative start on face includes starting owner");
    require(visits(g,{2,2,2},{2,2,2})==std::set<size_t>{g.id(1,1,1)},"closed outer maximum");
    require(visits(g,{-1,3,0},{3,3,2}).empty(),"outside segment");
    require(visits(g,{1-1e-13,.5,.5},{1-1e-13,.5,.5})==std::set<size_t>{g.id(0,0,0)},"near-boundary point must not be rounded to another cell");
    Random rng(20261111);int checked=0;
    for(auto settings:{GridSettings{{0,0,0},{3,3,3},{1,1,1}},GridSettings{{-.3,-1,.2},{2.5,3,2},{1,.75,.6}}}){
        Grid3D grid(settings);
        for(int i=0;i<2000;++i){auto coordinate=[&](){return i<1000?double(rng.integer(-2,8))/2:-2+7*rng.uniform();};Vec3 a{coordinate(),coordinate(),coordinate()},b{coordinate(),coordinate(),coordinate()};
            auto expected=oracle(grid,a,b),actual=visits(grid,a,b);
            if(dump){for(int axis=0;axis<3;++axis){*dump<<'"';for(auto v:grid.lines(axis))*dump<<v<<';';*dump<<"\",";}*dump<<a.x<<','<<a.y<<','<<a.z<<','<<b.x<<','<<b.y<<','<<b.z<<",\"";for(auto id:actual)*dump<<id<<';';*dump<<"\"\n";}
            if(actual!=expected){std::cerr<<"DDA mismatch a="<<a.x<<','<<a.y<<','<<a.z<<" b="<<b.x<<','<<b.y<<','<<b.z<<" actual=";for(auto id:actual)std::cerr<<id<<',';std::cerr<<" expected=";for(auto id:expected)std::cerr<<id<<',';std::cerr<<'\n';throw std::runtime_error("DDA disagrees with independent half-open interval oracle");}
            auto reversed=visits(grid,b,a);if(reversed!=expected){std::cerr<<"reverse mismatch a="<<a.x<<','<<a.y<<','<<a.z<<" b="<<b.x<<','<<b.y<<','<<b.z<<" forward=";for(auto id:actual)std::cerr<<id<<',';std::cerr<<" reverse=";for(auto id:reversed)std::cerr<<id<<',';std::cerr<<" reverse_oracle=";for(auto id:oracle(grid,b,a))std::cerr<<id<<',';std::cerr<<'\n';throw std::runtime_error("segment reversal changes closed-point ownership");}++checked;
        }
    }
    std::cout<<"PASS S1 manual face/edge/corner/outer/clip fixtures and "<<checked<<" independent oracle segments + reversals\n";
}
void counts(){
    Statistics stats({{0,0,0},{2,2,2},{1,1,1}},1,10);
    for(int i=0;i<8;++i){auto trial=stats.newTrial();if(i<3){trial.visitSegment({-.5,.5,.5},{2.5,.5,.5});trial.visitSegment({2.5,.5,.5},{-.5,.5,.5});for(int j=0;j<50;++j)trial.visitSegment({.5,.5,.5},{.5,.5,.5});}else trial.visitSegment({-2,-2,-2},{-1,-1,-1});stats.finishTrial(trial,true);}
    auto failed=stats.newTrial();failed.visitSegment({0,0,0},{2,2,2});stats.finishTrial(failed,false);
    require(stats.completed()==8,"failed trial included in completed denominator");require(stats.voxelHits().at(0)==3,"within-trial deduplication");auto p=stats.probability(3,false);require(!p.value&&p.lower==.3&&p.upper==.5,"N10 C8 H3 bounds");
    require(stats.collectorHits()[0]==3&&stats.collectorNotCrossed()==5,"first positive collector/dedup/not-crossed");
    for(int i=0;i<2;++i){auto trial=stats.newTrial();trial.visitSegment({-2,-2,-2},{-1,-1,-1});stats.finishTrial(trial,true);}p=stats.probability(3,true);require(p.value&&*p.value==.3&&p.upper==.3,"complete probability collapse");
    for(auto v:stats.xyHits())require(v<=10,"projection xy exceeds N");for(auto v:stats.xzHits())require(v<=10,"projection xz exceeds N");
    Statistics collector({{0,0,0},{2,2,2},{1,1,1}},1,2);auto first=collector.newTrial();first.visitSegment({0,.2,.2},{2,1.8,1.8});first.visitSegment({2,1.8,1.8},{0,1.8,1.8});first.visitSegment({0,.2,.2},{2,.2,.2});collector.finishTrial(first,true);
    require(first.collectorCell&&*first.collectorCell==3,"collector interpolation/first-only ownership");
    auto outside=collector.newTrial();outside.visitSegment({0,3,3},{2,3,3});outside.visitSegment({0,.2,.2},{2,.2,.2});collector.finishTrial(outside,true);require(collector.collectorOutside()==1&&collector.collectorHits()[0]==0,"outside first collector must not be replaced by later crossing");
    Grid3D nearCorner({{-.3,.2,0},{2.5,2,2},{1,.6,1}});auto precise=nearCorner.collectorCellAt({-1,-1,.5},{2,1,.5},1.7);require(precise&&*precise==0,"collector interpolation must not round into adjacent cell");
    std::cout<<"PASS S1/S4/S5 dedup, first collector, N/C bounds, incomplete formal blank, projection counts\n";
}
}
int main(int argc,char**argv){try{std::ofstream dump;if(argc>1){dump.open(argv[1]);require(bool(dump),"cannot open DDA oracle dump");}fixtures(argc>1?&dump:nullptr);counts();if(argc>1){dump.close();require(bool(dump),"DDA dump write failed");}return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
