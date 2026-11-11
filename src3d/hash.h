#pragma once
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string_view>
namespace rs3d {
// Stable content identifier, not a security signature: IEEE754 words in little-endian order.
class ContentHash {
    std::uint64_t value_=14695981039346656037ULL;
public:
    void word(std::uint64_t n){for(int i=0;i<8;++i){value_^=(n>>(8*i))&255;value_*=1099511628211ULL;}}
    void number(double n){std::uint64_t bits;static_assert(sizeof(bits)==sizeof(n));std::memcpy(&bits,&n,sizeof(bits));word(bits);}
    void text(std::string_view text){for(unsigned char c:text){value_^=c;value_*=1099511628211ULL;}}
    std::string hex()const{std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<value_;return out.str();}
};
}
