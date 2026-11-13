#pragma once
// Read-only replay controller: which trial is selected, where the cursor is, whether it plays,
// and which stored pose to show. Never advances physics; all poses come from PlaybackReader.
#include "session.h"
#include "playback.h"
#include "trajectory_display.h"
#include "ui/analysis_model.h"
namespace rs3d::gui {
class Replay {
    std::shared_ptr<const RunConfig> config_;
    std::shared_ptr<const AnalysisSnapshot> analysis_;
    std::shared_ptr<ArchiveLocation> location_;
    std::shared_ptr<const TrajectoryLoadView> loaded_;
    PlaybackReader reader_;
    TrajectoryArchiveReader trajectoryReader_;
    std::shared_ptr<const PlaybackSample> sample_;
    std::uint64_t selected_=1,request_=0;
    double cursor_=0,speed_=1,holdUntil_=0;
    bool follow_=true,followAvailable_=false,playing_=false,needSeek_=false,eventPose_=false,pauseAtContact_=true,historyContext_=false,archive_=false;
    std::string readError_;
    std::vector<std::uint64_t> listed_;
    std::optional<std::size_t> cell_;MapKind listKind_=MapKind::route;
    std::uint64_t listVersion_=0;
    Snapshot eventSnapshot(Snapshot base)const;
public:
    Replay()=default;
    void sync(const SessionView&view,double dt);
    void reset();
    // selection and transport
    void select(std::uint64_t id);
    void seek(double time,bool pause=true);
    void setPlaying(bool on);
    void setSpeed(double s){speed_=s;}
    void setFollow(bool on);
    void jumpToFirstContact();
    void stepFrame(int direction);
    void setPauseAtContact(bool on){pauseAtContact_=on;}
    void setCell(std::optional<std::size_t> cell,MapKind kind);
    // queries
    const MotionTrial*trial()const;
    const FirstContactEvent*event()const;
    bool hasHistory()const{auto t=trial();return t&&t->frames;}
    bool hasAnalysis()const{return bool(analysis_);}
    bool archive()const{return archive_;}
    bool following()const{return follow_;}
    bool followAvailable()const{return followAvailable_;}
    bool playing()const{return playing_;}
    bool eventShown()const;
    bool pauseAtContact()const{return pauseAtContact_;}
    double cursor()const{return cursor_;}
    double speed()const{return speed_;}
    std::uint64_t selected()const{return selected_;}
    const std::string&error()const{return readError_;}
    const PlaybackSample*sample()const{return sample_.get();}
    bool sampleReady()const{return sample_&&sample_->available&&sample_->trialId==selected_;}
    const std::vector<std::uint64_t>&listed()const{return listed_;}
    std::optional<std::size_t> cell()const{return cell_;}
    const std::vector<std::shared_ptr<const DisplayTrajectory>>&trajectories()const;
    std::uint64_t trajectoryVersion()const{return loaded_?loaded_->version:(analysis_?analysis_->version:0);}
    std::shared_ptr<const AnalysisSnapshot> analysis()const{return analysis_;}
    std::optional<std::pair<std::shared_ptr<const PhysicsScene>,Snapshot>> displayPose()const;
    bool barrierHistoryMissing()const{return sample_&&!sample_->barrierHistory&&config_&&config_->barrier.elastic;}
    void rebuildList(MapKind kind);
};
}
