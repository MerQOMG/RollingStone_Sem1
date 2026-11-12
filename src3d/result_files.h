#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <string>
namespace rs3d {
struct ResultFile {const char*name;bool probability;int introduced=1;};
// Single registry for transaction completion and I/O-error invalidation.
inline constexpr std::array<ResultFile,25> resultFiles{{
    {"scene.ini",false},{"manifest.ini",false},{"trials.csv",false},{"shape_vertices.csv",false},{"shape_faces.csv",false},
    {"terrain_vertices.csv",false},{"terrain_faces.csv",false},{"voxel_probability.csv",true},{"collector_yz.csv",true},
    {"collector_summary.csv",true},{"projection_xy.csv",true},{"projection_xz.csv",true},{"trajectory.csv",false},
    {"barrier_trajectory.csv",false},{"barrier_trials.csv",false},{"attempt_rejections.csv",false},
    {"obstacles.csv",false,2},{"obstacle_vertices.csv",false,2},{"obstacle_faces.csv",false,2},
    {"route_heatmap_xy.csv",true,3},{"barrier_first_contact.csv",false,3},{"barrier_first_contact_points.csv",false,3},
    {"barrier_first_contact_heatmap.csv",true,3},{"motion.bin",false,3},{"motion_index.csv",false,3}
}};
std::map<std::string,std::string> readResultManifest(const std::filesystem::path&directory);
void validateResultFiles(const std::filesystem::path&directory,int schema);
}
