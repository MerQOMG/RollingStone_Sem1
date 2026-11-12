#pragma once
#include "obstacles.h"
#include <filesystem>
#include <functional>
namespace rs3d {
void writeObstacleFiles(const ObstacleSet&,const std::function<void(const std::string&,const std::string&)>&write);
std::shared_ptr<const ObstacleSet> readObstacleFiles(const std::filesystem::path&,const std::string& inputHash,const std::string& expectedHash);
}
