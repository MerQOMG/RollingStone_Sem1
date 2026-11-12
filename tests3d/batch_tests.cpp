#include "batch.h"
#include "output.h"
#include "jolt_runtime.h"
#include <Windows.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
using namespace rs3d;
namespace {
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
std::string read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"missing output");return {std::istreambuf_iterator<char>(in),{}};}
SceneDraft stable(const std::filesystem::path&root){SceneDraft s;s.count=3;s.terrain.type=TerrainType::flat;s.terrain.width=20;s.releaseGap=0;s.stableFaceRelease=true;s.properties[restitutionValue]={0,0};s.maxTime=10;s.trajectoryStride=25;s.outputRoot=root;return s;}
void equal(const BatchResult&a,const BatchResult&b){check(a.trials.size()==b.trials.size(),"trial count differs");check(a.statistics.voxelHits()==b.statistics.voxelHits()&&a.statistics.xyHits()==b.statistics.xyHits()&&a.statistics.xzHits()==b.statistics.xzHits()&&a.statistics.collectorHits()==b.statistics.collectorHits(),"integer counts differ");
    for(size_t i=0;i<a.trials.size();++i){auto&x=a.trials[i];auto&y=b.trials[i];check(x.input.seed==y.input.seed&&x.input.values==y.input.values&&x.shapeDiagnostics.k==y.shapeDiagnostics.k,"inputs differ");check(x.last.reason==y.last.reason&&x.last.time==y.last.time&&norm(x.last.center-y.last.center)<=1e-8&&norm(x.last.velocity-y.last.velocity)<=1e-8,"terminal states differ");}
    for(auto name:{L"shape_vertices.csv",L"shape_faces.csv",L"trajectory.csv",L"barrier_trajectory.csv",L"barrier_trials.csv",L"voxel_probability.csv",L"collector_yz.csv",L"projection_xy.csv",L"projection_xz.csv"})check(read(a.directory/name)==read(b.directory/name),"deterministic output differs");
}
void incomplete(const BatchResult&r){check(r.status!=BatchStatus::complete,"unexpected complete");auto text=read(r.directory/L"voxel_probability.csv");check(text.find(",,")!=text.npos,"formal probabilities must be blank");check(read(r.directory/L"manifest.ini").find("complete=0")!=std::string::npos,"manifest complete flag");}
}
int main(){try{
    JoltRuntime runtime;auto root=std::filesystem::temp_directory_path()/L"RollingStone3D batch 中文目录 with spaces";std::filesystem::create_directories(root);auto draft=stable(root);auto config=freezeConfig(draft);BatchControl firstControl;auto first=runBatch(config,firstControl);check(first.status==BatchStatus::complete&&first.statistics.completed()==3,"stable N=3 failed");
    BatchControl previewControl;BatchOptions options;options.preview=2;int previews=0,background=0;std::uint64_t snapshots=0;options.observer=[&](const BatchEvent&e){if(e.kind==BatchEventKind::trialStarted){if(e.preview)++previews;else ++background;}if(e.kind==BatchEventKind::snapshot){check(e.trialId<=2&&e.preview,"background animation event");++snapshots;}};
    auto preview=runBatch(config,previewControl,options);equal(first,preview);check(previews==2&&background==1&&snapshots>1000,"N3 preview routing");check(first.directory!=preview.directory,"output overwritten");
    draft.count=2;BatchControl twoControl;previews=background=0;auto two=runBatch(freezeConfig(draft),twoControl,options);check(two.status==BatchStatus::complete&&previews==2&&background==0,"N2 preview count");std::cout<<"PASS S2 identical observer preview=0/2 inputs/states/all deterministic CSV; N2/N3 routing (not GUI pacing)\n";
    auto replay=loadReplay(first.directory,2);Trial loaded(replay.scene,replay.settings);Statistics replayStats(replay.grid,replay.collectorX,1);auto visits=replayStats.newTrial();auto previous=loaded.snapshot().center;visits.visitSegment(previous,previous);
    while(loaded.snapshot().reason==StopReason::running){auto now=loaded.step();visits.visitSegment(previous,clipToTerrainExit(previous,now.center,*replay.scene.terrain));previous=now.center;}check(loaded.snapshot().reason==replay.expected.reason&&norm(loaded.snapshot().center-replay.expected.center)<=1e-8,"S5 replay state");check(loaded.snapshot().time==replay.expected.time,"S5 replay time");std::cout<<"PASS S5 actual vertices/faces/pose/terrain/material replay, position error="<<norm(loaded.snapshot().center-replay.expected.center)<<'\n';
    draft.count=3;auto failedConfig=freezeConfig(draft);BatchOptions retry;retry.faults.rejectTrial=1;retry.faults.rejectedAttempts=1;BatchControl retryControl;auto retried=runBatch(failedConfig,retryControl,retry);check(retried.status==BatchStatus::complete&&retried.trials[0].shapeDiagnostics.attempts==2&&retried.statistics.completed()==3,"same-k retry status");check(retried.trials[0].shapeDiagnostics.k==first.trials[0].shapeDiagnostics.k&&retried.trials[0].input.values==first.trials[0].input.values&&norm(retried.trials[1].last.center-first.trials[1].last.center)==0,"retry perturbed other streams");
    retry.faults.rejectedAttempts=64;BatchControl exhaustControl;auto exhausted=runBatch(failedConfig,exhaustControl,retry);check(exhausted.trials.size()==3&&exhausted.failures==1&&exhausted.statistics.completed()==2&&exhausted.trials[0].shapeDiagnostics.attempts==64,"exhaustion handling");incomplete(exhausted);
    BatchOptions numeric;numeric.faults.numericFailureTrial=2;BatchControl numericControl;auto failed=runBatch(failedConfig,numericControl,numeric);check(failed.failures==1&&failed.statistics.completed()==2&&failed.unstarted()==0,"numeric failure slots");incomplete(failed);
    draft.maxTime=.01;BatchControl timeoutControl;auto timed=runBatch(freezeConfig(draft),timeoutControl);check(timed.timeouts==3&&timed.statistics.completed()==0&&timed.failures==0,"timeout handling");incomplete(timed);std::cout<<"PASS S4 retries/exhaustion/numeric failure/timeout, no replacement slots, blank formal probabilities\n";
    BatchControl cancelControl;BatchOptions cancel;cancel.preview=2;cancel.observer=[&](const BatchEvent&e){if(e.kind==BatchEventKind::snapshot&&e.snapshot->time>=.01)cancelControl.cancel();};auto cancelled=runBatch(failedConfig,cancelControl,cancel);check(cancelled.status==BatchStatus::cancelled&&cancelled.cancelledTrials==1&&cancelled.unstarted()==2,"mid-step cancellation");incomplete(cancelled);
    BatchControl geometryCancel;BatchOptions cancelGeometry;cancelGeometry.observer=[&](const BatchEvent&e){if(e.kind==BatchEventKind::generating)geometryCancel.cancel();};auto cancelledGeometry=runBatch(failedConfig,geometryCancel,cancelGeometry);check(cancelledGeometry.status==BatchStatus::cancelled&&cancelledGeometry.trials[0].shapeDiagnostics.attempts==0&&cancelledGeometry.unstarted()==2,"geometry cancellation");incomplete(cancelledGeometry);
    BatchControl pauseControl;BatchOptions pausing;pausing.preview=2;std::atomic<bool>requested{false};std::atomic<unsigned>countSnapshots{0};pausing.observer=[&](const BatchEvent&e){if(e.kind==BatchEventKind::snapshot){++countSnapshots;if(!requested.exchange(true))pauseControl.pause(true);}};
    std::optional<BatchResult>pausedResult;std::thread worker([&]{pausedResult.emplace(runBatch(config,pauseControl,pausing));});auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!pauseControl.pauseAcknowledged()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));bool acknowledged=pauseControl.pauseAcknowledged();auto before=countSnapshots.load();std::this_thread::sleep_for(std::chrono::milliseconds(10));bool unchanged=before==countSnapshots;pauseControl.pause(false);if(!acknowledged)pauseControl.cancel();worker.join();check(acknowledged&&unchanged,"pause not acknowledged between full steps");equal(first,*pausedResult);std::cout<<"PASS controlled generation/step cancellation and pause/resume determinism\n";
    auto badRoot=root/L"regular_file_not_directory";{std::ofstream file(badRoot);file<<"sentinel";}draft=stable(badRoot);BatchControl badControl;auto bad=runBatch(freezeConfig(draft),badControl);check(bad.status==BatchStatus::ioError&&bad.trials.empty()&&!bad.error.empty(),"unwritable output root");check(read(badRoot)=="sentinel","existing output overwritten");
    draft=stable(root);BatchOptions disk;disk.faults.writeFailureAfterBytes=50000;BatchControl diskControl;auto diskFail=runBatch(freezeConfig(draft),diskControl,disk);check(diskFail.status==BatchStatus::ioError&&diskFail.directory.extension()==L".pending","write failure published as success");check(read(diskFail.directory/L"manifest.ini").find("status=io_error")!=std::string::npos,"I/O diagnostic manifest");std::cout<<"PASS S4 unwritable root, injected stream write failure, transaction marker\n";
    disk.faults.writeFailureAfterBytes=0;disk.faults.outputFailureAt="manifest.ini";BatchControl lateDiskControl;auto lateDisk=runBatch(config,lateDiskControl,disk);check(lateDisk.status==BatchStatus::ioError&&lateDisk.statistics.completed()==3,"late write failure fixture");check(read(lateDisk.directory/L"voxel_probability.csv")=="io_error_no_formal_probability\n","late failure left formal probabilities");
    BatchControl pausedCancel;pausedCancel.pause(true);std::optional<BatchResult>pausedCancelledResult;std::thread pausedWorker([&]{pausedCancelledResult.emplace(runBatch(config,pausedCancel));});deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!pausedCancel.pauseAcknowledged()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));acknowledged=pausedCancel.pauseAcknowledged();pausedCancel.cancel();pausedWorker.join();check(acknowledged&&pausedCancelledResult->status==BatchStatus::cancelled&&pausedCancelledResult->unstarted()==3,"cancel while paused");
    std::cout<<"PASS late I/O failure invalidates partially written probabilities; paused cancellation joins worker\n";
    // A real Windows directory handle without FILE_SHARE_DELETE blocks rename.
    // Exercise the same transaction finish path as GUI deployment, including
    // recovery from a short lock and invalidation when the lock persists.
    auto lockDraft=stable(root);lockDraft.automaticGrid=false;lockDraft.automaticCollector=false;lockDraft.collectorX=.5;auto lockConfig=freezeConfig(lockDraft);
    for(bool childFile:{false,true})for(bool transient:{true,false}){
        RunOutput transaction(*lockConfig);transaction.initialize(*lockConfig);BatchResult locked(lockConfig);locked.status=BatchStatus::cancelled;locked.directory=transaction.pending();
        auto lockPath=childFile?transaction.pending()/L"trials.csv":transaction.pending();
        HANDLE directory=CreateFileW(lockPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        check(directory!=INVALID_HANDLE_VALUE,"cannot create real directory-lock fixture");
        auto destination=transaction.pending();destination.replace_extension();
        BOOL moved=MoveFileExW(transaction.pending().c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH);DWORD denied=GetLastError();
        if(moved||!(denied==ERROR_ACCESS_DENIED||denied==ERROR_SHARING_VIOLATION)){CloseHandle(directory);check(false,"directory lock did not reproduce commit refusal");}
        std::thread unlock;if(transient)unlock=std::thread([directory]{std::this_thread::sleep_for(std::chrono::milliseconds(25));CloseHandle(directory);});
        auto commitStart=std::chrono::steady_clock::now();bool failed=false;
        try{transaction.finish(locked);}catch(const OutputFailure&e){failed=true;locked.status=BatchStatus::ioError;locked.error=e.what();transaction.diagnose(locked);}
        double commitMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-commitStart).count();
        if(transient)unlock.join();else CloseHandle(directory);
        check(commitMs<250,"output commit lock exceeded cancellation budget");
        if(transient){check(!failed&&locked.directory==destination,"short output-directory lock not recovered");check(!std::filesystem::exists(destination/L"INCOMPLETE.txt"),"recovered commit still incomplete");}
        else{check(failed&&locked.directory.extension()==L".pending","persistent output lock published as success");check(read(locked.directory/L"voxel_probability.csv")=="io_error_no_formal_probability\n","locked commit left formal probabilities");check(read(locked.directory/L"INCOMPLETE.txt").find("io_error")!=std::string::npos,"locked commit lost failure marker");}
        std::cout<<"PASS real commit lock child_file="<<childFile<<" transient="<<transient<<" initial_win32="<<denied<<" commit_ms="<<commitMs<<'\n';
    }
    TerrainMesh bounds;bounds.xMin=0;bounds.xEnd=20;bounds.yMin=-10;bounds.yMax=10;auto clipped=clipToTerrainExit({19,0,2},{21,2,4},bounds);check(norm(clipped-Vec3{20,1,3})<1e-12,"absorbing exit clipping");
    auto elastic=stable(root);elastic.barrier.enabled=true;elastic.barrier.elastic=true;elastic.barrierAutoX=false;elastic.barrierAutoZ=false;elastic.barrier.center={14,0,3};elastic.barrier.width=12;elastic.barrier.height=6;
    elastic.properties[releaseX]={7,7};elastic.properties[velocityX]={8,8};elastic.properties[frictionValue]={.2,.2};elastic.trajectoryStride=5;elastic.maxTime=60;
    BatchControl elasticControl;auto er=runBatch(freezeConfig(elastic),elasticControl);check(er.status==BatchStatus::complete,"elastic batch incomplete");
    BatchControl elasticPreview;BatchOptions ep;ep.preview=2;auto er2=runBatch(freezeConfig(elastic),elasticPreview,ep);equal(er,er2);
    check(er.trials[0].last.barrier.touched&&er.trials[0].last.barrier.maxDisplacement>.01&&er.trials[0].last.barrier.dampingEnergy>0,"elastic outputs missed coupled impact");
    auto saved=loadReplay(er.directory,1);Trial rerun(saved.scene,saved.settings);while(rerun.snapshot().reason==StopReason::running)rerun.step();
    check(saved.hasBarrierState&&matchesReplay(rerun.snapshot(),saved),"elastic physical/diagnostic replay mismatch");
    auto tampered=saved;tampered.expected.barrier.contactImpulse+=10;check(!matchesReplay(rerun.snapshot(),tampered),"replay ignored changed contact impulse");
    std::cout<<"PASS elastic batch/preview CSV identity and full barrier state/energy/impulse replay\n";
    std::cout<<"PASS batch suite; evidence root="<<root.u8string()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
