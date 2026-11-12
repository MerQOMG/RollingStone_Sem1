#pragma once
#include <raylib.h>
#include <vector>
// Immutable diagnostic/display lines. Does not own or advance any simulation.
class StaticLines {
    unsigned vao_=0,vbo_=0;
    Shader shader_{};
    int count_=0,mvp_=-1;
public:
    StaticLines()=default;
    StaticLines(const StaticLines&)=delete;
    StaticLines&operator=(const StaticLines&)=delete;
    ~StaticLines(){clear();}
    void load(const std::vector<Vector3>&polylinePoints,int pointsPerLine);
    void draw()const;
    void clear(); // Must run before closing the OpenGL context.
};
