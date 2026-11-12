#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <cstdint>
namespace rs3d {
using CsvRow=std::map<std::string,std::string>;
struct CsvSpan {std::uint64_t offset=0,length=0,count=0;};
void readArchiveCsv(const std::filesystem::path&,std::size_t maximumRows,const std::function<void(const CsvRow&)>&);
void scanArchiveCsv(const std::filesystem::path&,std::size_t maximumRows,const std::function<void(const CsvRow&,std::uint64_t offset,std::uint64_t length)>&);
void readArchiveCsvRange(const std::filesystem::path&,std::uint64_t offset,std::uint64_t length,std::size_t maximumRows,const std::function<void(const CsvRow&)>&);
double archiveNumber(const std::string&);
std::uint64_t archiveInteger(const std::string&);
}
