#pragma once
#include <filesystem>
#include <vector>
namespace rs3d::gui {
void initDialogs();
std::filesystem::path pickSceneFile(bool save,const std::filesystem::path&current,void*owner);
std::filesystem::path pickFolder(const std::filesystem::path&start,void*owner,const wchar_t*title);
void openInExplorer(const std::filesystem::path&path);
// Result folders (containing manifest.ini) directly below `root`, newest first.
std::vector<std::filesystem::path> recentResults(const std::filesystem::path&root,std::size_t limit);
void*nativeWindowHandle();
}
