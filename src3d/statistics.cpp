#include "statistics.h"
#include "exact_parameter.h"
#include <limits>
namespace rs3d {
namespace {
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
bool clipped(Vec3 a,Vec3 b,Vec3 lo,Vec3 hi,ExactParameter&first,ExactParameter&last){
    first=ExactParameter(0);last=ExactParameter(1);Vec3 d=b-a;
    for(int i=0;i<3;++i){if(d[i]==0){if(a[i]<lo[i]||a[i]>hi[i])return false;}
        else{auto t0=ExactParameter::crossing(lo[i],a[i],b[i]),t1=ExactParameter::crossing(hi[i],a[i],b[i]);if(t0>t1)std::swap(t0,t1);first=std::max(first,t0);last=std::min(last,t1);if(first>last)return false;}}
    return true;
}
}
Grid3D::Grid3D(GridSettings s):settings_(s){
    require(finite(s.lower)&&finite(s.upper)&&finite(s.cell),"non-finite statistics bounds");
    size_t total=1;
    for(int axis=0;axis<3;++axis){require(s.upper[axis]>s.lower[axis]&&s.cell[axis]>0,"invalid statistics extent/cell size");
        double n=std::ceil((s.upper[axis]-s.lower[axis])/s.cell[axis]);require(n>=1&&n<=2000000,"statistics axis too large");
        auto&line=lines_[axis];line.push_back(s.lower[axis]);for(int i=1;i<int(n);++i){double value=s.lower[axis]+i*s.cell[axis];if(value<s.upper[axis]){require(value>line.back(),"statistics cell width below coordinate precision");line.push_back(value);}}line.push_back(s.upper[axis]);
        total*=line.size()-1;require(total<=2000000,"statistics exceeds 2000000-cell resource limit");
    }
}
std::array<int,3> Grid3D::indices(size_t value)const{require(value<count(),"voxel index out of range");int z=int(value%size(2));value/=size(2);int y=int(value%size(1));return {int(value/size(1)),y,z};}
std::optional<int> Grid3D::index(double p,int axis)const{
    if(!std::isfinite(p)||p<lines_[axis].front()||p>lines_[axis].back())return {};
    auto i=std::upper_bound(lines_[axis].begin(),lines_[axis].end(),p)-lines_[axis].begin()-1;
    return std::min(int(i),size(axis)-1);
}
std::optional<size_t> Grid3D::cellAt(Vec3 p)const{auto x=index(p.x,0),y=index(p.y,1),z=index(p.z,2);if(!x||!y||!z)return {};return id(*x,*y,*z);}
std::optional<size_t> Grid3D::collectorCellAt(Vec3 a,Vec3 b,double x)const{
    require(finite(a)&&finite(b)&&b.x>a.x&&a.x<=x&&b.x>=x,"invalid positive collector crossing");
    auto t=ExactParameter::crossing(x,a.x,b.x);std::array<int,2> cells{};
    for(int axis=1;axis<3;++axis){
        if(a[axis]==b[axis]){auto cell=index(a[axis],axis);if(!cell)return {};cells[axis-1]=*cell;continue;}
        const auto&line=lines_[axis];bool positive=b[axis]>a[axis];
        auto first=ExactParameter::crossing(line.front(),a[axis],b[axis]),last=ExactParameter::crossing(line.back(),a[axis],b[axis]);
        if(t<std::min(first,last)||t>std::max(first,last))return {};
        auto pointLess=[&](double boundary){auto crossing=ExactParameter::crossing(boundary,a[axis],b[axis]);return positive?t<crossing:t>crossing;};
        // Upper-bound search in coordinate order, using exact parameter comparisons.
        int lo=0,hi=int(line.size());while(lo<hi){int mid=lo+(hi-lo)/2;if(pointLess(line[mid]))hi=mid;else lo=mid+1;}
        cells[axis-1]=std::min(lo-1,size(axis)-1);
    }
    return size_t(cells[0])*size(2)+cells[1];
}
void Grid3D::visitSegment(Vec3 a,Vec3 b,const std::function<void(size_t)>&visit)const{
    require(finite(a)&&finite(b),"non-finite statistics segment");ExactParameter first(0),last(1);
    if(!clipped(a,b,settings_.lower,settings_.upper,first,last))return;
    Vec3 delta=b-a;require(finite(delta),"statistics segment magnitude overflow");
    if(delta.x==0&&delta.y==0&&delta.z==0){if(auto c=cellAt(a))visit(*c);return;}
    std::array<int,3> cell{},direction{};std::array<ExactParameter,3> next{};
    auto ownerAt=[&](ExactParameter t,bool interior){std::array<int,3> result{};
        for(int axis=0;axis<3;++axis){auto&line=lines_[axis];auto before=[&](ExactParameter parameter,double boundary){return parameter<ExactParameter::crossing(boundary,a[axis],b[axis]);};
            if(delta[axis]>0){auto it=std::upper_bound(line.begin()+1,line.end()-1,t,before);result[axis]=int(it-(line.begin()+1));}
            else if(delta[axis]<0){auto it=std::upper_bound(line.rbegin()+1,line.rend()-1,t,before);result[axis]=size(axis)-1-int(it-(line.rbegin()+1));
                if(!interior&&result[axis]+1<size(axis)&&ExactParameter::crossing(line[result[axis]+1],a[axis],b[axis])==t)++result[axis];}
            else result[axis]=*index(a[axis],axis);
        }return result;
    };
    auto point=[&](ExactParameter t){auto p=ownerAt(t,false);visit(id(p[0],p[1],p[2]));};point(first);if(first==last)return;
    cell=ownerAt(first,true);for(int axis=0;axis<3;++axis)direction[axis]=delta[axis]>0?1:(delta[axis]<0?-1:0);
    // Retain original segment parameters, not re-interpolated clipped endpoints.
    // Distinct crossings must remain distinct even when only one ulp apart.
    auto nextCross=[&](int axis){if(!direction[axis])return ExactParameter::infinity();int boundary=cell[axis]+(direction[axis]>0?1:0);return ExactParameter::crossing(lines_[axis][boundary],a[axis],b[axis]);};
    for(int axis=0;axis<3;++axis)next[axis]=nextCross(axis);
    const int limit=size(0)+size(1)+size(2)+4;
    for(int iteration=0;iteration<limit;++iteration){
        visit(id(cell[0],cell[1],cell[2]));auto t=std::min({next[0],next[1],next[2]});if(t>last){point(last);return;}
        std::array<bool,3> crossing{};auto boundary=cell;
        for(int axis=0;axis<3;++axis)if(next[axis]==t){crossing[axis]=true;if(direction[axis]>0)boundary[axis]=std::min(cell[axis]+1,size(axis)-1);}
        visit(id(boundary[0],boundary[1],boundary[2]));if(t>=last){point(last);return;}
        bool outside=false;for(int axis=0;axis<3;++axis)if(crossing[axis]){cell[axis]+=direction[axis];outside|=cell[axis]<0||cell[axis]>=size(axis);}
        if(outside){point(last);return;}for(int axis=0;axis<3;++axis)if(crossing[axis])next[axis]=nextCross(axis);
    }
    throw std::runtime_error("DDA exceeded bounded crossing count");
}
void TrialVisits::visitSegment(Vec3 a,Vec3 b){
    grid->visitSegment(a,b,[&](size_t cell){voxels.insert(cell);auto p=grid->indices(cell);xy.insert(size_t(p[0])*grid->size(1)+p[1]);xz.insert(size_t(p[0])*grid->size(2)+p[2]);});
    if(!crossedCollector&&b.x>a.x&&a.x<=collectorX&&b.x>=collectorX){
        crossedCollector=true;collectorCell=grid->collectorCellAt(a,b,collectorX);
    }
}
Statistics::Statistics(GridSettings s,double x,std::uint64_t requested):grid_(s),collectorX_(x),requested_(requested),voxels_(grid_.count()),xy_(size_t(grid_.size(0))*grid_.size(1)),xz_(size_t(grid_.size(0))*grid_.size(2)),collector_(size_t(grid_.size(1))*grid_.size(2)){
    require(requested>0&&std::isfinite(x)&&x>=s.lower.x&&x<=s.upper.x,"invalid requested count/collector plane");
}
void Statistics::finishTrial(const TrialVisits&v,bool normal){
    require(v.grid==&grid_,"trial visits belong to another statistics grid");if(!normal)return;require(completed_<requested_,"more completed trials than requested");++completed_;
    for(auto i:v.voxels)++voxels_.at(i);for(auto i:v.xy)++xy_.at(i);for(auto i:v.xz)++xz_.at(i);
    if(v.collectorCell)++collector_.at(*v.collectorCell);else if(v.crossedCollector)++collectorOutside_;else ++collectorNotCrossed_;
}
Probability Statistics::probability(std::uint64_t hits,bool complete)const{
    require(hits<=completed_,"hits exceed completed trials");require(!complete||completed_==requested_,"cannot label incomplete trials complete");
    double lower=double(hits)/double(requested_),upper=double(hits+requested_-completed_)/double(requested_);return {complete?std::optional<double>(lower):std::nullopt,lower,upper};
}
}
