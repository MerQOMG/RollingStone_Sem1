#include "result_files.h"
#include <fstream>
#include <set>
#include <stdexcept>
namespace rs3d {
std::map<std::string,std::string> readResultManifest(const std::filesystem::path&directory){
    auto path=directory/"manifest.ini";if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("manifest capacity exceeded");std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("missing result manifest");std::map<std::string,std::string> result;std::string line;
    while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty()&&line.front()=='[')break;auto equals=line.find('=');if(equals==line.npos||!result.emplace(line.substr(0,equals),line.substr(equals+1)).second)throw std::runtime_error("invalid/duplicate result manifest field");}return result;
}
void validateResultFiles(const std::filesystem::path&directory,int schema){
    if(schema<1||schema>3)throw std::runtime_error("unsupported result schema");
    if(directory.extension()==L".pending"||std::filesystem::exists(directory/"INCOMPLETE.txt"))throw std::runtime_error("uncommitted result; only diagnostic raw-block browsing is permitted");
    std::ifstream complete(directory/"FILES_COMPLETE.txt",std::ios::binary);if(!complete)throw std::runtime_error("missing FILES_COMPLETE transaction marker");std::set<std::string> listed;std::string line;while(std::getline(complete,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.size()>128||!listed.insert(line).second)throw std::runtime_error("invalid completion registry");}
    for(auto f:resultFiles){if(f.introduced>schema)continue;std::string name=f.name;if(schema==1&&(name=="barrier_trajectory.csv"||name=="barrier_trials.csv"||name=="trajectory.csv")&&!listed.count(name))continue;
        if(!listed.count(name)||!std::filesystem::is_regular_file(directory/name))throw std::runtime_error("missing registered result file: "+name);}
}
}
