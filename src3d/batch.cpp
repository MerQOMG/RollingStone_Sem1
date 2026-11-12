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
    try{
        auto ioStart=Clock::now();output=std::make_unique<RunOutput>(*config,o.faults.writeFailureAfterBytes);result.directory=output->pending();output->initialize(*config);result.timing.output+=seconds(ioStart);
        for(std::uint64_t id=1;id<=config->source.count;++id){
            if(!control.checkpoint())break;
            TrialRecord record;record.input=o.singleInput?*o.singleInput:sampleTrialInput(*config,id);record.input.id=id;record.radius=config->source.geometry.radius;record.requestedVolume=config->source.geometry.volume;
            notify(BatchEventKind::generating,id);auto visits=result.statistics.newTrial();PhysicsScene scene;auto generationStart=Clock::now();bool geometryReady=false;
            try{
                GeneratedShape generated;
                if(o.singleScene){generated.geometry=o.singleScene->shape;if(o.singleDiagnostics)generated.diagnostics=*o.singleDiagnostics;else{generated.diagnostics.k=int(generated.geometry.vertices.size());generated.diagnostics.trialSeed=record.input.seed;}}
                else generated=generatePolyhedron(config->source.geometry,record.input.seed,[&]{return !control.checkpoint();},id==o.faults.rejectTrial?o.faults.rejectedAttempts:0);
                record.timing.generation=seconds(generationStart);geometryReady=true;record.shapeDiagnostics=generated.diagnostics;const auto&p=generated.geometry;
                record.hasShape=true;record.volume=p.volume;record.canonicalCenter=p.center;record.canonicalInertia=p.inertia;record.innerRadius=p.innerRadius;record.tau=p.tau;record.slenderness=p.slenderness;record.doubleFallback=p.doubleFallback;
                ioStart=Clock::now();output->shape(id,p);record.timing.output+=seconds(ioStart);
                auto physicsStart=Clock::now();scene=o.singleScene?*o.singleScene:makePhysicsScene(*config,record.input,p);record.initial=scene.initial;record.hasInitialState=true;Trial trial(scene,{config->source.maxTime});record.first=trial.snapshot();record.hasPhysicsState=true;record.last=record.first;record.timing.physics+=seconds(physicsStart);
                notify(BatchEventKind::trialStarted,id,&scene,&record.first);
                auto statsStart=Clock::now();visits.visitSegment(record.first.center,record.first.center);record.timing.statistics+=seconds(statsStart);
                if(config->source.trajectoryStride){ioStart=Clock::now();output->trajectory(id,record.first);record.timing.output+=seconds(ioStart);}
                while(record.last.reason==StopReason::running){
                    if(!control.checkpoint()){trial.cancel();record.last=trial.snapshot();break;}
                    Vec3 previous=record.last.center;physicsStart=Clock::now();record.last=trial.step();record.physics=trial.diagnostics();record.timing.physics+=seconds(physicsStart);
                    if(id==o.faults.numericFailureTrial&&record.physics.updates==3){record.last.reason=StopReason::numericFailure;record.last.error="injected numeric failure";}
                    if(record.last.reason!=StopReason::numericFailure){statsStart=Clock::now();visits.visitSegment(previous,clipToTerrainExit(previous,record.last.center,*config->terrain));record.timing.statistics+=seconds(statsStart);}
                    if(config->source.trajectoryStride&&(record.physics.updates%config->source.trajectoryStride==0||record.last.reason!=StopReason::running)){ioStart=Clock::now();output->trajectory(id,record.last);record.timing.output+=seconds(ioStart);}
                    if(id<=std::uint64_t(o.preview))notify(BatchEventKind::snapshot,id,nullptr,&record.last);
                }
                record.physics=trial.diagnostics();
            }catch(const OutputFailure&e){record.last.reason=StopReason::numericFailure;record.last.error=std::string("output transaction aborted: ")+e.what();result.trials.push_back(record);++result.failures;throw;}
            catch(const GenerationFailure&e){record.shapeDiagnostics=e.diagnostics;record.last.reason=e.cancelled?StopReason::cancelled:StopReason::geometryFailure;record.last.error=e.what();record.timing.generation=seconds(generationStart);}
            catch(const std::exception&e){record.last.reason=geometryReady?StopReason::numericFailure:StopReason::geometryFailure;record.last.error=e.what();if(!geometryReady)record.timing.generation=seconds(generationStart);}
            auto statsStart=Clock::now();result.statistics.finishTrial(visits,normalCompletion(record.last.reason));record.timing.statistics+=seconds(statsStart);
            if(record.last.reason==StopReason::timeout)++result.timeouts;else if(record.last.reason==StopReason::cancelled)++result.cancelledTrials;else if(!normalCompletion(record.last.reason))++result.failures;
            result.updates+=record.physics.updates;result.internalSteps+=record.physics.internalSteps;
            result.timing.generation+=record.timing.generation;result.timing.physics+=record.timing.physics;result.timing.statistics+=record.timing.statistics;result.timing.output+=record.timing.output;
            result.trials.push_back(record);ioStart=Clock::now();output->trial(result.trials.back());result.timing.output+=seconds(ioStart);notify(BatchEventKind::trialFinished,id,nullptr,&result.trials.back().last);
            if(control.cancelled())break;
        }
        result.status=control.cancelled()?BatchStatus::cancelled:(result.statistics.completed()==config->source.count?BatchStatus::complete:BatchStatus::incompleteFailure);
        result.timing.wall=seconds(start);output->finish(result);
    }catch(const std::exception&e){result.status=BatchStatus::ioError;result.error=e.what();if(output)output->diagnose(result);}
    result.timing.wall=seconds(start);notify(BatchEventKind::finished,0);return result;
}
}
