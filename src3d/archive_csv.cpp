#include "archive_csv.h"
#include <fstream>
#include <sstream>
#include <locale>
#include <charconv>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <set>
namespace rs3d {
namespace {
std::vector<std::string> split(const std::string&s){std::vector<std::string> out;std::string cell;bool quoted=false,closed=false;
    for(size_t i=0;i<s.size();++i){char c=s[i];if(quoted){if(c=='"'){if(i+1<s.size()&&s[i+1]=='"'){cell+='"';++i;}else{quoted=false;closed=true;}}else cell+=c;}
        else if(c==','){out.push_back(cell);cell.clear();closed=false;}else if(c=='"'){if(!cell.empty()||closed)throw std::runtime_error("CSV quote");quoted=true;}else{if(closed)throw std::runtime_error("CSV content after quote");cell+=c;}}
    if(quoted)throw std::runtime_error("CSV unterminated quote");out.push_back(cell);return out;
}
bool line(std::istream&f,std::string&s){s.clear();char c;while(f.get(c)){if(c=='\n')break;if(s.size()>=1024*1024)throw std::runtime_error("CSV row capacity exceeded");s+=c;}if(!s.empty()&&s.back()=='\r')s.pop_back();return bool(f)||!s.empty();}
}
void scanArchiveCsv(const std::filesystem::path&p,size_t maximum,const std::function<void(const CsvRow&,std::uint64_t,std::uint64_t)>& consume){
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("missing archive CSV: "+p.u8string());std::string s;if(!line(f,s))throw std::runtime_error("empty archive CSV");auto header=split(s);size_t n=0;
    std::set<std::string> unique;for(const auto&name:header)if(name.empty()||!unique.insert(name).second)throw std::runtime_error("empty/duplicate CSV header");
    auto fileSize=std::filesystem::file_size(p);for(;;){auto begin=f.tellg();if(!line(f,s))break;if(++n>maximum)throw std::runtime_error("CSV row count exceeds declared safety capacity");auto cells=split(s);if(cells.size()!=header.size())throw std::runtime_error("CSV column count mismatch");CsvRow row;for(size_t i=0;i<header.size();++i)row.emplace(header[i],cells[i]);auto end=f.tellg();consume(row,std::uint64_t(begin),end<0?fileSize-std::uint64_t(begin):std::uint64_t(end-begin));}if(f.bad())throw std::runtime_error("CSV read failed");
}
void readArchiveCsv(const std::filesystem::path&p,size_t maximum,const std::function<void(const CsvRow&)>&consume){scanArchiveCsv(p,maximum,[&](const CsvRow&r,std::uint64_t,std::uint64_t){consume(r);});}
void readArchiveCsvRange(const std::filesystem::path&p,std::uint64_t offset,std::uint64_t length,size_t maximum,const std::function<void(const CsvRow&)>&consume){
    auto size=std::filesystem::file_size(p);if(offset>size||length>size-offset||length>64*1024*1024)throw std::runtime_error("CSV block range exceeds file/capacity");std::ifstream f(p,std::ios::binary);std::string s;if(!line(f,s))throw std::runtime_error("missing CSV header");auto header=split(s);if(offset<std::uint64_t(f.tellg()))throw std::runtime_error("CSV block overlaps header");std::set<std::string>unique;for(const auto&name:header)if(name.empty()||!unique.insert(name).second)throw std::runtime_error("invalid CSV header");f.seekg(std::streamoff(offset));std::string bytes(size_t(length),'\0');f.read(bytes.data(),std::streamsize(length));if(!f)throw std::runtime_error("truncated CSV block");f.close();std::istringstream rows(bytes);size_t count=0;while(line(rows,s)){if(++count>maximum)throw std::runtime_error("CSV block row capacity exceeded");auto values=split(s);if(values.size()!=header.size())throw std::runtime_error("CSV block column mismatch");CsvRow row;for(size_t i=0;i<header.size();++i)row[header[i]]=values[i];consume(row);}
}
double archiveNumber(const std::string&s){std::istringstream f(s);f.imbue(std::locale::classic());double v;f>>v;if(!f||!std::isfinite(v))throw std::runtime_error("invalid finite archive number");f>>std::ws;if(!f.eof())throw std::runtime_error("trailing archive number data");return v;}
std::uint64_t archiveInteger(const std::string&s){std::uint64_t v=0;auto r=std::from_chars(s.data(),s.data()+s.size(),v);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())throw std::runtime_error("invalid archive integer");return v;}
}
