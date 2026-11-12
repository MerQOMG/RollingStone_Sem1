#pragma once
#include "app_view.h"
#include <map>
namespace rs3d::ui {
Color heatColor(double value,unsigned char alpha=150);
class AnalysisRenderer {
    struct Line {unsigned vao=0,vbo=0;int count=0;std::shared_ptr<const DisplayTrajectory>source;};
    std::map<std::uint64_t,Line>lines_;
    Shader shader_{};int mvp_=-1,color_=-1;
    Model surface_{};std::vector<size_t>surfaceCells_;std::string surfaceKey_;
    std::vector<std::uint64_t>surfaceCounts_;double surfaceDenominator_=-1;
    void clearLine(Line&);
    void loadLine(Line&,std::shared_ptr<const DisplayTrajectory>);
public:
    ~AnalysisRenderer();
    void clear();
    void updateTrajectories(const std::vector<std::shared_ptr<const DisplayTrajectory>>&);
    void drawTrajectories(std::uint64_t selected,float alpha)const;
    void updateSurface(const RunConfig&,const Grid3D&,const std::vector<std::uint64_t>&counts,double denominator,const std::string&layer);
    void drawSurface()const;
    std::size_t trajectoryCount()const{return lines_.size();}
    void drawSelectedProgress(const DisplayTrajectory&,double time)const;
};
}
