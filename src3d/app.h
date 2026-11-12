#pragma once
#include "config.h"
struct AppOptions {
    std::filesystem::path font=L"C:/Windows/Fonts/simhei.ttf",evidence=L"results3d/gui";
    bool startSingle=false,startBatch=false,capture=false,selfTest=false,exitAfterRun=false,benchmark=false,exerciseView=false;
    int preview=2,fps=60;
    double duration=0;
    std::filesystem::path viewResults;
    bool diagnosticResults=false;
};
int runApplication(const rs3d::SceneDraft&draft,const AppOptions&options);
