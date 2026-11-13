#pragma once
// Entry point of the redesigned RollingStone3D front end.
#include "config.h"
#include <filesystem>
#include <string>
namespace rs3d::gui {
struct AppOptions {
    std::filesystem::path viewResults,shotsDir,inputLog;
    bool diagnostic=false,demo=false,tour=false;
    std::string tab;            // initial tab: scene | run | analysis
    std::string terrainTex;     // terrain texture set (--terrain-tex NAME); empty = bundled default, "procedural" = generated look
    int width=0,height=0;       // physical window size; 0 = automatic
    float scale=0;              // UI scale; 0 = system DPI
    double autoExitSeconds=0;   // smoke runs
    bool autoStartSingle=false,autoStartBatch=false;
};
int runApplication(const SceneDraft&draft,const AppOptions&options);
}
