#include "batch.h"
#include "output.h"
#include <chrono>
namespace rs3d {
namespace {using Clock=std::chrono::steady_clock;double seconds(Clock::time_point begin){return std::chrono::duration<double>(Clock::now()-begin).count();}}
const char*batchStatusName(BatchStatus s){switch(s){case BatchStatus::complete:return "complete";case BatchStatus::incompleteFailure:return "incomplete_failure";case BatchStatus::cancelled:return "cancelled";case BatchStatus::ioError:return "io_error";}return "invalid";}
void BatchControl::waitPaused(std::unique_lock<std::mutex>&lock){
    if(paused_&&!cancelled_){auto begin=Clock::now();acknowledged_=true;wake_.wait(lock,[&]{return !paused_||cancelled_;});pausedTime_+=Clock::now()-begin;acknowledged_=false;}
}
bool BatchControl::checkpoint(){
    if(paused_&&!cancelled_){std::unique_lock<std::mutex>lock(mutex_);waitPaused(lock);}
    return !cancelled_;
}
bool BatchControl::pace(double simulatedSeconds,Clock::time_point start,Clock::duration pauseAtStart){
    std::unique_lock<std::mutex>lock(mutex_);
    for(;;){waitPaused(lock);if(cancelled_)return false;auto deadline=start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(simulatedSeconds))+pausedTime_-pauseAtStart;
        if(Clock::now()>=deadline)return true;wake_.wait_until(lock,deadline,[&]{return bool(paused_)||bool(cancelled_);});}
}
Vec3 clipToTerrainExit(Vec3 a,Vec3 b,const TerrainMesh&t){
    double fraction=1;auto d=b-a;
    for(int axis=0;axis<2;++axis){double lo=axis==0?t.xMin:t.yMin,hi=axis==0?t.xEnd:t.yMax;
        if(b[axis]<lo&&d[axis]<0)fraction=std::min(fraction,(lo-a[axis])/d[axis]);
        if(b[axis]>hi&&d[axis]>0)fraction=std::min(fraction,(hi-a[axis])/d[axis]);}
    auto p=a+d*std::clamp(fraction,0.0,1.0);p.x=std::clamp(p.x,t.xMin,t.xEnd);p.y=std::clamp(p.y,t.yMin,t.yMax);return p;
}
BatchResult runBatch(std::shared_ptr<const RunConfig>config,BatchControl&control,const BatchOptions&o){
    if(o.preview!=0&&o.preview!=2)throw std::runtime_error("preview must be 0 or 2");
    if(o.singleScene&&config->source.count!=1)throw std::runtime_error("frozen preview requires a single trial");
    BatchResult result(config);auto start=Clock::now();std::unique_ptr<RunOutput>output;
    auto notify=[&](BatchEventKind kind,std::uint64_t id,const PhysicsScene*scene=nullptr,const Snapshot*snapshot=nullptr){if(o.observer)o.observer({kind,id,std::uint64_t(result.trials.size()),result.statistics.completed(),result.failures+result.timeouts,id>0&&id<=std::uint64_t(o.preview),scene,snapshot});};
    auto lastPublish=start;std::uint64_t analysisVersion=0;
    auto publish=[&](bool force=false,bool filesComplete=false){if(!force&&(!o.observer||seconds(lastPublish)<.25))return;auto a=std::make_shared<AnalysisSnapshot>();a->version=++analysisVersion;a->directory=output->motion().directory();a->route=std::make_shared<const RouteAccumulator>(result.route);a->contact=std::make_shared<const FirstContactAccumulator>(result.contact);a->events=result.contacts;a->trajectories=result.trajectories;a->filesComplete=filesComplete;a->ioError=result.status==BatchStatus::ioError;a->normalTrials=result.statistics.completed();a->toeXY=result.statistics.xyHits();a->toeXZ=result.statistics.xzHits();result.analysis=a;lastPublish=Clock::now();if(o.observer)o.observer({BatchEventKind::analysis,0,std::uint64_t(result.trials.size()),result.statistics.completed(),result.failures+result.timeouts,false,nullptr,nullptr,a});};
    try{
        auto ioStart=Clock::now();output=std::make_unique<RunOutput>(*config,o.faults.writeFailureAfterBytes,o.faults.outputFailureAt);result.directory=output->pending();output->initialize(*config);result.timing.output+=seconds(ioStart);
        publish(true);
        for(std::uint64_t id=1;id<=config->source.count;++id){
            if(!control.checkpoint())break;
            TrialRecord record;record.input=o.singleInput?*o.singleInput:sampleTrialInput(*config,id);record.input.id=id;record.radius=config->source.geometry.radius;record.requestedVolume=config->source.geometry.volume;
            notify(BatchEventKind::generating,id);auto visits=result.statistics.newTrial();RouteAccumulator::Visits routeVisits;PhysicsScene scene;auto generationStart=Clock::now();bool geometryReady=false;
            std::unique_ptr<MotionRecorder> recorder;std::unique_ptr<FirstBarrierContactCollector> contact;std::uint64_t lastRecordedStep=0;
            std::vector<TimedPosition> displayPoints;const auto displayBudget=std::max(size_t(2),config->source.history.displayVertices/size_t(config->source.count));
            try{
                GeneratedShape generated;
                if(o.singleScene){generated.geometry=o.singleScene->shape;if(o.singleDiagnostics)generated.diagnostics=*o.singleDiagnostics;else{generated.diagnostics.k=int(generated.geometry.vertices.size());generated.diagnostics.trialSeed=record.input.seed;}}
                else generated=generatePolyhedron(config->source.geometry,record.input.seed,[&]{return !control.checkpoint();},id==o.faults.rejectTrial?o.faults.rejectedAttempts:0);
                record.timing.generation=seconds(generationStart);geometryReady=true;record.shapeDiagnostics=generated.diagnostics;const auto&p=generated.geometry;
                record.hasShape=true;record.volume=p.volume;record.canonicalCenter=p.center;record.canonicalInertia=p.inertia;record.innerRadius=p.innerRadius;record.tau=p.tau;record.slenderness=p.slenderness;record.doubleFallback=p.doubleFallback;
                ioStart=Clock::now();output->shape(id,p);record.timing.output+=seconds(ioStart);
                auto physicsStart=Clock::now();scene=o.singleScene?*o.singleScene:makePhysicsScene(*config,record.input,p);record.initial=scene.initial;record.hasInitialState=true;Trial trial(scene,{config->source.maxTime});record.first=trial.snapshot();record.hasPhysicsState=true;record.last=record.first;record.timing.physics+=seconds(physicsStart);
                record.physics=trial.diagnostics();contact=std::make_unique<FirstBarrierContactCollector>(id,config->barrier,record.physics.slop,record.physics.mass,config->source.contactCell);
                displayPoints.push_back({record.first.time,record.first.center});
                auto recordingStart=Clock::now();recorder=std::make_unique<MotionRecorder>(output->motion(),id,config->history.stride,config->source.history.enabled);recorder->sample(0,record.first);record.timing.recording+=seconds(recordingStart);
                notify(BatchEventKind::trialStarted,id,&scene,&record.first);
                auto statsStart=Clock::now();visits.visitSegment(record.first.center,record.first.center);result.route.visit(routeVisits,record.first.center,record.first.center);record.timing.statistics+=seconds(statsStart);
                if(config->source.trajectoryStride){ioStart=Clock::now();output->trajectory(id,record.first);record.timing.output+=seconds(ioStart);}
                while(record.last.reason==StopReason::running){
                    if(!control.checkpoint()){trial.cancel();record.last=trial.snapshot();break;}
                    Vec3 previous=record.last.center;double previousTime=record.last.time;physicsStart=Clock::now();record.last=trial.step();record.physics=trial.diagnostics();record.timing.physics+=seconds(physicsStart);
                    if(record.last.contacts.obstacle>0)++record.obstacleContactSteps;
                    bool response=false;for(const auto&event:trial.observations())if(event.retained&&(event.object1==barrierObjectId||event.object2==barrierObjectId)&&event.normalImpulse>std::max(1e-6,1e-8*record.physics.mass))response=true;
                    if(response)++record.barrierResponseSteps;
                    if(id==o.faults.numericFailureTrial&&record.physics.updates==3){record.last.reason=StopReason::numericFailure;record.last.error="injected numeric failure";}
                    statsStart=Clock::now();contact->observe(trial.observations(),record.physics.observerOverflow||!record.physics.contactObservationAvailable);
                    if(record.last.reason!=StopReason::numericFailure){auto end=clipToTerrainExit(previous,record.last.center,*config->terrain);visits.visitSegment(previous,end);result.route.visit(routeVisits,previous,end);}record.timing.statistics+=seconds(statsStart);
                    recordingStart=Clock::now();if(record.last.time>previousTime){lastRecordedStep=record.physics.updates;recorder->sample(lastRecordedStep,record.last);displayPoints.push_back({record.last.time,record.last.center});}if(o.observer&&seconds(lastPublish)>=.25){recorder->publish();if(id<=std::uint64_t(o.preview))result.trajectories[size_t(id-1)]=simplifyTrajectory(id,displayPoints,record.last.reason,displayBudget);publish();}record.timing.recording+=seconds(recordingStart);
                    if(config->source.trajectoryStride&&(record.physics.updates%config->source.trajectoryStride==0||record.last.reason!=StopReason::running)){ioStart=Clock::now();output->trajectory(id,record.last);record.timing.output+=seconds(ioStart);}
                    if(id<=std::uint64_t(o.preview))notify(BatchEventKind::snapshot,id,nullptr,&record.last);
                }
                record.physics=trial.diagnostics();
            }catch(const OutputFailure&e){record.last.reason=StopReason::numericFailure;record.last.error=std::string("output transaction aborted: ")+e.what();result.trials.push_back(record);++result.failures;throw;}
            catch(const GenerationFailure&e){record.shapeDiagnostics=e.diagnostics;record.last.reason=e.cancelled?StopReason::cancelled:StopReason::geometryFailure;record.last.error=e.what();record.timing.generation=seconds(generationStart);}
            catch(const std::exception&e){record.last.reason=geometryReady?StopReason::numericFailure:StopReason::geometryFailure;record.last.error=e.what();if(!geometryReady)record.timing.generation=seconds(generationStart);}
            auto statsStart=Clock::now();result.statistics.finishTrial(visits,normalCompletion(record.last.reason));result.route.finish(routeVisits,normalCompletion(record.last.reason));
            auto event=std::make_shared<FirstContactEvent>();if(contact){contact->finish(record.last.reason);*event=contact->event();}else{event->trialId=id;event->status=config->barrier.enabled?FirstContactStatus::unresolved:FirstContactStatus::disabled;event->trialStatus=record.last.reason;event->reason=record.last.error;}result.contacts[size_t(id-1)]=event;result.contact.finish(*event);record.timing.statistics+=seconds(statsStart);
            auto recordingStart=Clock::now();if(recorder)recorder->finish(lastRecordedStep,record.last);else output->motion().status(id,stopReasonName(record.last.reason),record.last.error);
            if(!displayPoints.empty())result.trajectories[size_t(id-1)]=simplifyTrajectory(id,displayPoints,record.last.reason,displayBudget);record.timing.recording+=seconds(recordingStart);
            if(record.last.reason==StopReason::timeout)++result.timeouts;else if(record.last.reason==StopReason::cancelled)++result.cancelledTrials;else if(!normalCompletion(record.last.reason))++result.failures;
            result.updates+=record.physics.updates;result.internalSteps+=record.physics.internalSteps;
            result.timing.generation+=record.timing.generation;result.timing.physics+=record.timing.physics;result.timing.statistics+=record.timing.statistics;result.timing.output+=record.timing.output;result.timing.recording+=record.timing.recording;
            result.trials.push_back(record);ioStart=Clock::now();output->trial(result.trials.back());output->contact(*event);result.timing.output+=seconds(ioStart);notify(BatchEventKind::trialFinished,id,nullptr,&result.trials.back().last);publish(id<=std::uint64_t(o.preview));
            if(control.cancelled())break;
        }
        result.status=control.cancelled()?BatchStatus::cancelled:(result.statistics.completed()==config->source.count?BatchStatus::complete:BatchStatus::incompleteFailure);
        result.timing.wall=seconds(start);output->finish(result);publish(true,true);
    }catch(const std::exception&e){result.status=BatchStatus::ioError;result.error=e.what();if(output){output->diagnose(result);if(output->hasMotion())publish(true);}}
    result.timing.wall=seconds(start);notify(BatchEventKind::finished,0);return result;
}
}
