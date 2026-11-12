#include "route_heatmap.h"
namespace rs3d {
RouteAccumulator::RouteAccumulator(double x0,double x1,double y0,double y1,std::uint64_t n,double cell):grid_({{x0,y0,0},{x1,y1,1},{cell,cell,1}}),requested_(n),hits_(grid_.count()){
    if(!n)throw std::runtime_error("route grid requires a positive target count");
}
void RouteAccumulator::visit(Visits&v,Vec3 a,Vec3 b)const{
    if(!finite(a)||!finite(b))throw std::runtime_error("non-finite full-route segment");
    a.z=b.z=.5;grid_.visitSegment(a,b,[&](size_t i){v.insert(i);});
}
void RouteAccumulator::finish(const Visits&v,bool normal){
    if(!normal)return;if(completed_>=requested_)throw std::runtime_error("too many normal route trials");
    for(auto i:v)if(i>=hits_.size())throw std::runtime_error("route visit outside grid");
    ++completed_;for(auto i:v)++hits_[i];
}
Probability RouteAccumulator::probability(std::uint64_t h,bool filesComplete)const{
    if(h>completed_)throw std::runtime_error("route hits exceed completed trials");
    double lo=double(h)/requested_,hi=double(h+requested_-completed_)/requested_;
    return {filesComplete&&completed_==requested_?std::optional<double>(lo):std::nullopt,lo,hi};
}
void RouteAccumulator::restore(std::vector<std::uint64_t> hits,std::uint64_t completed){if(hits.size()!=hits_.size()||completed>requested_)throw std::runtime_error("invalid archived route dimensions/denominator");for(auto h:hits)if(h>completed)throw std::runtime_error("archived route hits exceed normal trials");hits_=std::move(hits);completed_=completed;}
}
