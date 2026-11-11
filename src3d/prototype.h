#pragma once
#include <filesystem>
struct PrototypeOptions {
    bool benchmark = false;
    bool stress = false;
    bool selfTest = false;
    bool capture = false;
    bool renderCheck = false;
    double duration = 0;
    std::filesystem::path output = L"results3d/prototype";
    std::filesystem::path font = L"C:/Windows/Fonts/simhei.ttf";
};
int runPrototype(const PrototypeOptions& options);
