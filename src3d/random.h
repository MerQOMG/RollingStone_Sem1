#pragma once
#include "math_types.h"
#include <cstdint>
#include <random>
namespace rs3d {
inline std::uint64_t mix(std::uint64_t x){auto z=x+0x9E3779B97F4A7C15ULL;z=(z^(z>>30))*0xBF58476D1CE4E5B9ULL;z=(z^(z>>27))*0x94D049BB133111EBULL;return z^(z>>31);}
constexpr std::uint64_t kDomain=0x4B00000000000000ULL,propertyDomain=0x5000000000000000ULL,shapeDomain=0x5300000000000000ULL;
inline std::uint64_t trialSeed(std::uint64_t master,std::uint64_t id){if(!id)throw std::runtime_error("trial_id starts at one");return mix(master+id-1);}
class Random {
    std::mt19937_64 engine_;
public:
    explicit Random(std::uint64_t seed):engine_(seed){}
    std::uint64_t next(){return engine_();}
    double uniform(){return double(next()>>11)*0x1.0p-53;}
    int integer(int lo,int hi){if(lo>hi)throw std::runtime_error("invalid integer interval");auto span=std::uint64_t(hi-lo)+1;auto threshold=(std::uint64_t(0)-span)%span;std::uint64_t x;do{x=next();}while(x<threshold);return lo+int(x%span);}
    Vec3 direction(){double z=2*uniform()-1,a=2*pi*uniform(),s=std::sqrt(std::max(0.0,1-z*z));return {s*std::cos(a),s*std::sin(a),z};}
};
}
