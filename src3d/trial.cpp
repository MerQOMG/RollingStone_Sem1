#include "trial.h"
namespace rs3d {
const char*stopReasonName(StopReason r){switch(r){case StopReason::running:return "running";case StopReason::resting:return "resting";case StopReason::downstream:return "downstream";case StopReason::upstream:return "upstream";case StopReason::lateral:return "lateral";case StopReason::timeout:return "timeout";case StopReason::geometryFailure:return "geometry_failure";case StopReason::numericFailure:return "numeric_failure";case StopReason::cancelled:return "cancelled";}return "invalid";}
bool normalCompletion(StopReason r){return r==StopReason::resting||r==StopReason::downstream||r==StopReason::upstream||r==StopReason::lateral;}
Trial::Trial(const PhysicsScene&scene,TrialSettings settings):world_(scene),settings_(settings),terrain_(scene.terrain){
    if(!std::isfinite(settings.timeLimit)||settings.timeLimit<=0)throw std::runtime_error("invalid trial time limit");
    static_cast<PhysicsSnapshot&>(current_)=world_.snapshot();
}
const Snapshot&Trial::step(double dt){
    if(current_.reason!=StopReason::running)return current_;
    try{
        world_.advance(dt);static_cast<PhysicsSnapshot&>(current_)=world_.snapshot();
        if(!settings_.diagnosticContinue&&terrain_){
            if(current_.center.x<terrain_->xMin)current_.reason=StopReason::upstream;
            else if(current_.center.x>terrain_->xEnd)current_.reason=StopReason::downstream;
            else if(current_.center.y<terrain_->yMin||current_.center.y>terrain_->yMax)current_.reason=StopReason::lateral;
        }
        if(norm(current_.velocity)<0.05&&norm(current_.angularVelocity)<0.05&&current_.barrier.settled)quietTime_+=dt;else quietTime_=0;
        if(!settings_.diagnosticContinue&&current_.reason==StopReason::running&&quietTime_>=2-1e-12)current_.reason=StopReason::resting;
        if(current_.reason==StopReason::running&&current_.time>=settings_.timeLimit-1e-12)current_.reason=StopReason::timeout;
    }catch(const std::exception&e){current_.reason=StopReason::numericFailure;current_.error=e.what();}
    return current_;
}
void Trial::cancel(){if(current_.reason==StopReason::running)current_.reason=StopReason::cancelled;}
}
