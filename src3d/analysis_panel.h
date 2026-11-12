#pragma once
#include "analysis_render.h"
#include "playback.h"
#include <array>
namespace rs3d::ui {
class AnalysisPanel {
    struct Plot {Grid3D grid;const std::vector<std::uint64_t>*hits=nullptr;std::uint64_t n=0,known=0,hitsTotal=0,misses=0;bool applicable=false;};
    std::shared_ptr<const RunConfig>config_;
    std::shared_ptr<const AnalysisSnapshot>analysis_;
    std::shared_ptr<ArchiveLocation>location_;
    std::shared_ptr<const TrajectoryLoadView>loaded_;
    PlaybackReader reader_;TrajectoryArchiveReader trajectoryReader_;AnalysisRenderer renderer_;
    std::shared_ptr<const PlaybackSample>sample_;
    std::uint64_t selected_=1,request_=0,lastLoggedRequest_=0,traceVersion_=0,surfaceVersion_=0;
    double cursor_=0,speed_=1,lastLoggedTime_=-1;bool follow_=true,playing_=false,capturing_=false,editing_=false,needSeek_=false,eventPose_=false;
    bool followAvailable_=false,historyContext_=false,scatter_=true;
    bool blocked_=false;
    std::string readError_;
    int tab_=0,mode_=2,metric_=1,lastTab_=-1,lastMetric_=-1,listScroll_=0,listActive_=-1;
    float alpha_=.65f;
    std::optional<size_t> selectedCell_;
    std::vector<std::uint64_t>listedIds_;std::vector<std::string>listLabels_;
    std::array<char,24>trialText_{};
    std::ostream*log_=nullptr;
    Plot plot()const;
    const std::vector<std::shared_ptr<const DisplayTrajectory>>&trajectories()const;
    const MotionTrial*trial()const;
    const FirstContactEvent*event()const;
    void seek(double time,const char*action,bool pause=true);
    void select(std::uint64_t id,const char*action);
    void rebuildList();
    void log(const char*action);
    void jumpEvent();
    Snapshot eventSnapshot(Snapshot)const;
    void drawPlot(Font,int height);
public:
    bool followCamera=false;
    static const char*glyphs();
    void sync(const SessionView&,double elapsed,std::ostream&);
    void beforeInput(int width,int height,bool focused,bool blocked);
    bool capturesInput()const{return capturing_||editing_;}
    bool hasHistoryContext()const{return historyContext_;}
    bool following()const{return follow_;}
    bool showRock()const{return !historyContext_||follow_||(sample_&&sample_->available&&sample_->trialId==selected_);}
    bool showBarrier()const{return !historyContext_||follow_||!config_->barrier.elastic||(sample_&&sample_->available&&sample_->barrierHistory);}
    std::optional<std::pair<std::shared_ptr<const PhysicsScene>,Snapshot>>displayPose()const;
    void drawWorld(bool showTrails);
    void drawEndpoints(Font,Camera3D,int renderWidth,int renderHeight,float dpi)const;
    void drawControls(Font,int height);
    void drawTimeline(Font,int width,int height);
    std::size_t trajectoryCount()const{return renderer_.trajectoryCount();}
    std::uint64_t selectedTrial()const{return selected_;}
    bool showingEvent()const{auto e=event();return !follow_&&e&&e->status==FirstContactStatus::hit&&(eventPose_||(cursor_>=e->timeLower&&cursor_<=e->timeUpper));}
};
}
