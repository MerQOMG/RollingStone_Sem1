#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rs3d {
// Compare (boundary-a)/(b-a) without rounding distinct crossings to the same t.
// Fast, separated estimates use ordinary double; near ties use error-free sums
// and FMA product residuals, accumulating a short floating-point expansion.
// Approach: Shewchuk, https://www.cs.cmu.edu/~quake/robust.html.
// This is a local scalar predicate, not an additional geometry dependency.
class ExactParameter {
    std::array<double,2> numerator_{0,0},denominator_{1,0}; // high, low
    bool infinite_=false;
    static std::array<double,2> difference(double a,double b){
        double high=a-b,bVirtual=a-high,aVirtual=high+bVirtual;
        return {high,(a-aVirtual)+(bVirtual-b)};
    }
    static void add(std::vector<double>& expansion,double value){
        std::vector<double> next;next.reserve(expansion.size()+1);double q=value;
        for(double term:expansion){double sum=q+term,bVirtual=sum-q,aVirtual=sum-bVirtual;
            double residual=(q-aVirtual)+(term-bVirtual);if(residual!=0)next.push_back(residual);q=sum;}
        if(q!=0||next.empty())next.push_back(q);expansion=std::move(next);
    }
public:
    explicit ExactParameter(double value=0):numerator_{value,0}{}
    static ExactParameter infinity(){ExactParameter p;p.infinite_=true;return p;}
    static ExactParameter crossing(double boundary,double a,double b){
        if(a==b)return infinity();ExactParameter p;p.numerator_=difference(boundary,a);p.denominator_=difference(b,a);
        for(double value:{p.numerator_[0],p.numerator_[1],p.denominator_[0],p.denominator_[1]})if(!std::isfinite(value))throw std::runtime_error("exact crossing difference magnitude overflow");
        if(p.denominator_[0]<0){for(auto&v:p.numerator_)v=-v;for(auto&v:p.denominator_)v=-v;}
        return p;
    }
    double approximate()const{return infinite_?std::numeric_limits<double>::infinity():numerator_[0]/denominator_[0];}
    int compare(const ExactParameter&b)const{
        if(infinite_||b.infinite_)return infinite_==b.infinite_?0:(infinite_?1:-1);
        double aEstimate=approximate(),bEstimate=b.approximate();
        double margin=64*std::numeric_limits<double>::epsilon()*std::max({1.0,std::abs(aEstimate),std::abs(bEstimate)});
        if(std::isfinite(aEstimate)&&std::isfinite(bEstimate)&&std::abs(aEstimate-bEstimate)>margin)return aEstimate<bEstimate?-1:1;
        std::vector<double> expansion;
        auto product=[&](double x,double y,double sign){double high=x*y;if(!std::isfinite(high))throw std::runtime_error("exact crossing predicate magnitude overflow");if(x!=0&&y!=0&&std::abs(high)<std::numeric_limits<double>::min())throw std::runtime_error("exact crossing predicate magnitude underflow");double low=std::fma(x,y,-high);add(expansion,sign*low);add(expansion,sign*high);};
        for(double x:numerator_)for(double y:b.denominator_)product(x,y,1);
        for(double x:b.numerator_)for(double y:denominator_)product(x,y,-1);
        for(auto it=expansion.rbegin();it!=expansion.rend();++it)if(*it!=0)return *it<0?-1:1;return 0;
    }
    bool operator<(const ExactParameter&b)const{return compare(b)<0;}
    bool operator>(const ExactParameter&b)const{return compare(b)>0;}
    bool operator==(const ExactParameter&b)const{return compare(b)==0;}
    bool operator!=(const ExactParameter&b)const{return compare(b)!=0;}
    bool operator<=(const ExactParameter&b)const{return compare(b)<=0;}
    bool operator>=(const ExactParameter&b)const{return compare(b)>=0;}
};
}
