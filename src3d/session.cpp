#include "session.h"
#include "result_archive.h"
namespace rs3d {
using Clock=std::chrono::steady_clock;
const char*sessionStateName(SessionState s){switch(s){case SessionState::idle:return "Idle";case SessionState::running:return "Running";case SessionState::paused:return "Paused";case SessionState::cancelling:return "Cancelling";case SessionState::completed:return "Completed";case SessionState::failed:return "Failed";}return "Invalid";}
Session::Session(const SceneDraft&draft){apply(draft);}
Session::Session(std::shared_ptr<const ResultArchive>archive):draft_(archive->config->source),readOnly_(true){
    view_.archive=archive;view_.config=archive->config;view_.analysis=archive->analysis;view_.state=archive->analysis->normalTrials==draft_.count?SessionState::completed:SessionState::failed;view_.phase=SessionPhase::finished;view_.completed=archive->analysis->normalTrials;for(const auto&t:archive->trials)if(!t.metadata.empty())++view_.processed;
    auto scene=std::make_shared<PhysicsScene>();scene->terrain=view_.config->terrain;scene->obstacles=view_.config->obstacles;scene->barrier=view_.config->barrier;scene->radius=draft_.geometry.radius;view_.scene=scene;view_.snapshot.barrier.enabled=scene->barrier.enabled;view_.snapshot.barrier.elastic=scene->barrier.elastic;view_.snapshot.barrier.center=initialBarrierCenter(scene->barrier);view_.snapshot.barrier.rotation=Quaternion::fromAxisAngle({0,0,1},scene->barrier.yawDegrees*pi/180);
    for(const auto&t:archive->analysis->directory->trials)if(t->geometry){view_.trialId=t->id;view_.scene=archive->scene(t->id);view_.snapshot.shapeOrigin=view_.scene->initial.shapeOrigin;view_.snapshot.rotation=view_.scene->initial.rotation;view_.snapshot.center=view_.snapshot.shapeOrigin;if(t->frames)view_.snapshot=readMotionBlock(*archive->analysis->directory,t->blocks.front()).front().snapshot;break;}
    view_.error=archive->notice;
}
Session::~Session(){cancel();if(worker_.joinable())worker_.join();}
bool Session::busy()const{std::lock_guard<std::mutex>lock(mutex_);return !view_.workerFinished;}
SessionView Session::view()const{
    std::lock_guard<std::mutex>lock(mutex_);auto out=view_;if(!out.workerFinished){out.wallSeconds=std::chrono::duration<double>(Clock::now()-started_).count();if(control_)out.pauseAcknowledged=control_->pauseAcknowledged();}return out;
}
void Session::apply(const SceneDraft&draft){
    if(readOnly_)throw std::runtime_error("result archive is read-only");
    if(busy())throw std::runtime_error("running batch is immutable; cancel or finish before applying edits");
    auto config=freezeConfig(draft);auto previewDraft=draft;previewDraft.count=1;previewDraft.masterSeed=draft.previewSeed;
    // Separate seed configuration, no shared mutable generator or formal trial consumption.
    auto previewConfig=freezeConfig(previewDraft);auto input=sampleTrialInput(*previewConfig,1);auto generated=generatePolyhedron(draft.geometry,input.seed);auto scene=std::make_shared<PhysicsScene>(makePhysicsScene(*config,input,generated.geometry));validatePhysicsScene(*scene);
    // Validate everything before replacing the current scene or retaining any GUI model pointer.
    Trial initial(*scene,{draft.maxTime});auto snapshot=initial.snapshot();
    if(worker_.joinable())worker_.join();draft_=draft;previewInput_=input;previewDiagnostics_=generated.diagnostics;previewScene_=scene;
    std::lock_guard<std::mutex>lock(mutex_);view_={};view_.config=config;view_.scene=scene;view_.snapshot=snapshot;control_.reset();
}
void Session::randomize(){auto changed=draft_;changed.previewSeed=mix(changed.previewSeed);apply(changed);}
void Session::reset(){apply(draft_);}
void Session::launch(bool single,int previews){
    if(readOnly_)throw std::runtime_error("result archive cannot start physics");
    if(busy())throw std::runtime_error("a batch is already running");if(previews!=0&&previews!=2)throw std::runtime_error("preview must be 0 or 2");if(!single&&draft_.count<2)throw std::runtime_error("GUI batch requires N >= 2");
    if(worker_.joinable())worker_.join();auto frozen=draft_;if(single){frozen.count=1;frozen.masterSeed=draft_.previewSeed;}auto config=freezeConfig(frozen);auto control=std::make_shared<BatchControl>();
    BatchOptions options;options.preview=previews;if(single){options.singleScene=previewScene_;options.singleInput=previewInput_;options.singleDiagnostics=previewDiagnostics_;}
    {std::lock_guard<std::mutex>lock(mutex_);control_=control;started_=Clock::now();view_.config=config;view_.state=SessionState::running;view_.phase=SessionPhase::generating;view_.result.reset();view_.analysis.reset();view_.workerFinished=false;view_.error.clear();view_.processed=view_.completed=view_.failures=view_.trialId=0;}
    try{worker_=std::thread([this,config,control,options]()mutable{
        auto previewStart=Clock::now();auto pauseAtStart=control->pausedTime();
        options.observer=[&](const BatchEvent&e){
            if(e.kind==BatchEventKind::trialStarted&&e.preview){previewStart=Clock::now();pauseAtStart=control->pausedTime();}
            if(e.kind==BatchEventKind::snapshot&&e.preview&&!control->pace(e.snapshot->time,previewStart,pauseAtStart))return;
            std::lock_guard<std::mutex>lock(mutex_);if(e.trialId)view_.trialId=e.trialId;view_.processed=e.processed;view_.completed=e.completed;view_.failures=e.failures;
            if(e.kind==BatchEventKind::generating)view_.phase=SessionPhase::generating;
            if(e.kind==BatchEventKind::trialStarted){view_.phase=e.preview?SessionPhase::preview:SessionPhase::background;if(e.preview)view_.scene=std::make_shared<PhysicsScene>(*e.scene);}
            if(e.snapshot&&e.preview)view_.snapshot=*e.snapshot;
            if(e.analysis)view_.analysis=e.analysis;
        };
        try{auto result=std::make_shared<BatchResult>(runBatch(config,*control,options));std::lock_guard<std::mutex>lock(mutex_);view_.result=result;view_.state=result->status==BatchStatus::complete?SessionState::completed:SessionState::failed;view_.error=result->error;view_.phase=SessionPhase::finished;view_.workerFinished=true;view_.wallSeconds=result->timing.wall;}
        catch(const std::exception&e){std::lock_guard<std::mutex>lock(mutex_);view_.state=SessionState::failed;view_.error=e.what();view_.phase=SessionPhase::finished;view_.workerFinished=true;}
    });}catch(...){std::lock_guard<std::mutex>lock(mutex_);view_.workerFinished=true;view_.state=SessionState::failed;view_.phase=SessionPhase::finished;view_.error="failed to start numerical worker";throw;}
}
void Session::pause(bool value){std::shared_ptr<BatchControl>control;{std::lock_guard<std::mutex>lock(mutex_);if(view_.workerFinished||view_.state==SessionState::cancelling)return;control=control_;view_.state=value?SessionState::paused:SessionState::running;}if(control)control->pause(value);}
void Session::cancel(){std::shared_ptr<BatchControl>control;{std::lock_guard<std::mutex>lock(mutex_);if(view_.workerFinished)return;view_.state=SessionState::cancelling;control=control_;}if(control)control->cancel();}
}
