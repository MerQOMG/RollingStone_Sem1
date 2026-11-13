#include "ui/replay.h"
#include "result_archive.h"
#include <chrono>
namespace {double nowSeconds(){using namespace std::chrono;return duration<double>(steady_clock::now().time_since_epoch()).count();}}
namespace rs3d::gui {
const MotionTrial*Replay::trial()const{if(!analysis_||!analysis_->directory||selected_<1||selected_>analysis_->directory->trials.size())return nullptr;return analysis_->directory->trials[size_t(selected_-1)].get();}
const FirstContactEvent*Replay::event()const{if(!analysis_||selected_<1||selected_>analysis_->events.size())return nullptr;return analysis_->events[size_t(selected_-1)].get();}
const std::vector<std::shared_ptr<const DisplayTrajectory>>&Replay::trajectories()const{static const std::vector<std::shared_ptr<const DisplayTrajectory>> none;if(loaded_)return loaded_->trajectories;return analysis_?analysis_->trajectories:none;}
void Replay::reset(){
    if(analysis_){trajectoryReader_.cancel();reader_.invalidate();}
    analysis_.reset();location_.reset();loaded_.reset();sample_.reset();readError_.clear();follow_=true;playing_=false;eventPose_=false;needSeek_=false;selected_=1;cursor_=0;listed_.clear();cell_.reset();historyContext_=false;
}
bool Replay::eventShown()const{auto e=event();return !follow_&&e&&e->status==FirstContactStatus::hit&&(eventPose_||(cursor_>=e->timeLower&&cursor_<=e->timeUpper));}
Snapshot Replay::eventSnapshot(Snapshot s)const{
    auto e=event();if(!e||e->status!=FirstContactStatus::hit||!config_)return s;
    s.time=cursor_;s.center=e->rock.center;s.shapeOrigin=e->rock.shapeOrigin;s.rotation=e->rock.rotation;s.velocity=e->rock.velocity;s.angularVelocity=e->rock.angularVelocity;
    s.barrier.enabled=config_->barrier.enabled;s.barrier.elastic=config_->barrier.elastic;s.barrier.center=e->barrier.center;s.barrier.rotation=e->barrier.rotation;
    s.barrier.displacement=dot(e->barrier.center-config_->barrier.center,barrierAxis(config_->barrier));s.barrier.velocity=dot(e->barrier.velocity,barrierAxis(config_->barrier));
    return s;
}
void Replay::select(std::uint64_t id){
    if(!analysis_||id<1||id>analysis_->directory->trials.size())return;
    selected_=id;follow_=false;playing_=false;eventPose_=false;sample_.reset();reader_.invalidate();readError_.clear();
    if(auto t=trial())cursor_=t->firstTime;needSeek_=true;
}
void Replay::seek(double time,bool pause){auto t=trial();if(!t||!t->frames)return;cursor_=std::clamp(time,t->firstTime,t->lastTime);follow_=false;if(pause)playing_=false;eventPose_=false;needSeek_=true;}
void Replay::setPlaying(bool on){
    auto t=trial();if(on&&(!t||!t->frames))return;
    if(on&&cursor_>=t->lastTime-1e-9)seek(t->firstTime,false);
    playing_=on;if(on)follow_=false;
}
void Replay::setFollow(bool on){if(on&&!followAvailable_)return;follow_=on;playing_=false;if(!on)needSeek_=true;}
void Replay::jumpToFirstContact(){auto e=event();if(!e||e->status!=FirstContactStatus::hit)return;cursor_=(e->timeLower+e->timeUpper)/2;follow_=playing_=false;eventPose_=needSeek_=true;}
void Replay::stepFrame(int dir){if(!sample_)return;seek(dir<0?sample_->previousRecordTime:sample_->nextRecordTime,true);}
void Replay::setCell(std::optional<std::size_t> c,MapKind k){cell_=c;listKind_=k;listVersion_=~0ull;}
void Replay::rebuildList(MapKind kind){
    listed_.clear();listKind_=kind;if(!analysis_)return;
    for(const auto&t:analysis_->directory->trials){
        if(kind==MapKind::contact&&cell_){const auto&e=analysis_->events[size_t(t->id-1)];if(!e||e->status!=FirstContactStatus::hit||!normalCompletion(e->trialStatus)||e->cell!=*cell_)continue;}
        listed_.push_back(t->id);
    }
}
void Replay::sync(const SessionView&v,double dt){
    config_=v.config;archive_=bool(v.archive);
    followAvailable_=v.phase==SessionPhase::preview&&!v.workerFinished&&v.trialId<=2;
    historyContext_=bool(v.analysis);
    if(!v.analysis){if(analysis_)reset();return;}
    bool newRun=location_!=v.analysis->directory->location,changed=analysis_!=v.analysis;analysis_=v.analysis;
    if(newRun){
        location_=analysis_->directory->location;trajectoryReader_.cancel();reader_.invalidate();loaded_.reset();sample_.reset();readError_.clear();
        selected_=1;cursor_=0;follow_=!v.archive;playing_=false;eventPose_=false;needSeek_=bool(v.archive);cell_.reset();listVersion_=~0ull;
        if(v.archive&&config_)trajectoryReader_.request(analysis_->directory,config_->source.history.displayVertices);
    }
    if(v.archive){auto l=trajectoryReader_.view();if(l&&l!=loaded_){loaded_=l;if(!loaded_->error.empty()){readError_=loaded_->error;playing_=false;}}}
    if(follow_&&followAvailable_){selected_=v.trialId;cursor_=v.snapshot.time;}
    else if(follow_&&(v.phase==SessionPhase::background||v.phase==SessionPhase::finished||(v.phase==SessionPhase::generating&&v.trialId>2))){
        if(selected_<=2)cursor_=v.snapshot.time;follow_=false;needSeek_=true;
    }
    auto sample=reader_.latest();
    if(sample&&sample->request==request_&&sample->trialId==selected_){sample_=sample;if(!sample->available&&!sample->error.empty()){readError_=sample->error;playing_=false;}}
    if(playing_&&!follow_){
        auto t=trial();
        if(t&&t->frames&&nowSeconds()>=holdUntil_){
            double before=cursor_;cursor_=std::min(t->lastTime,cursor_+std::max(0.,dt)*speed_);
            if(cursor_>=t->lastTime)playing_=false;
            auto e=event();
            if(e&&e->status==FirstContactStatus::hit&&before<e->timeLower&&cursor_>e->timeUpper){cursor_=(e->timeLower+e->timeUpper)/2;eventPose_=true;if(pauseAtContact_)holdUntil_=nowSeconds()+.9;}
            else if(eventPose_&&e&&cursor_>e->timeUpper)eventPose_=false;
            needSeek_=true;
        }
    }
    if(changed&&!follow_&&!sample_&&trial()&&trial()->frames)needSeek_=true;
    if(needSeek_&&!follow_){
        auto t=trial();
        if(t&&(t->frames||eventPose_)){request_=reader_.request(analysis_->directory,selected_,cursor_,config_,eventPose_?std::optional<Snapshot>(eventSnapshot({})):std::nullopt);needSeek_=false;}
    }
    if(changed||listVersion_!=analysis_->version){rebuildList(listKind_);listVersion_=analysis_->version;}
}
std::optional<std::pair<std::shared_ptr<const PhysicsScene>,Snapshot>> Replay::displayPose()const{
    if(follow_||!sample_||!sample_->available||sample_->trialId!=selected_||!sample_->scene)return {};
    Snapshot s=sample_->snapshot;if(eventShown())s=eventSnapshot(s);
    return std::make_pair(sample_->scene,s);
}
}
