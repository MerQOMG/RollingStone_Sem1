#include "batch.h"
#include "output.h"
#include "jolt_runtime.h"
#include "prototype.h"
#include "app.h"
#include "result_archive.h"
#include <Windows.h>
#include <iostream>
#include <iomanip>
namespace {
rs3d::BatchControl* activeControl=nullptr;
std::mutex consoleMutex;
BOOL WINAPI interrupt(DWORD signal){std::lock_guard<std::mutex>lock(consoleMutex);if((signal==CTRL_C_EVENT||signal==CTRL_BREAK_EVENT||signal==CTRL_CLOSE_EVENT)&&activeControl){activeControl->cancel();return TRUE;}return FALSE;}
std::uint64_t integer(const std::wstring&s){if(s.empty()||s.find_first_not_of(L"0123456789")!=s.npos)throw std::runtime_error("expected unsigned integer");return std::stoull(s);}
int selfTest(const std::filesystem::path&folder){
    for(auto name:{L"rs3d_abi.exe",L"rs3d_input_tests.exe",L"rs3d_geometry_tests.exe",L"rs3d_morphology_tests.exe",L"rs3d_terrain_tests.exe",L"rs3d_physics_tests.exe",L"rs3d_elastic_tests.exe",L"rs3d_statistics_tests.exe",L"rs3d_config_tests.exe",L"rs3d_settling_tests.exe",L"rs3d_batch_tests.exe",L"rs3d_session_tests.exe",L"rs3d_contact_observer_tests.exe",L"rs3d_obstacle_tests.exe",L"rs3d_analysis_tests.exe",L"rs3d_motion_tests.exe",L"rs3d_archive_tests.exe",L"rs3d_playback_tests.exe"}){
        auto path=folder/name;std::wstring command=L"\""+path.wstring()+L"\"";STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(path.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))throw std::runtime_error("cannot start self-test executable; build all test targets");WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);if(code)return int(code);
    }return 0;
}
}
int wmain(int argc,wchar_t**argv){try{
    PrototypeOptions proto;AppOptions app;bool prototype=false,headless=false,single=false,self=false,previewSet=false;std::optional<std::uint64_t>count,seed;int preview=0;std::filesystem::path configPath,outputPath,replayPath,writeDefault;std::uint64_t replayId=1;bool throughput=false;
    for(int i=1;i<argc;++i){std::wstring arg=argv[i];auto next=[&](){if(++i>=argc)throw std::runtime_error("missing option value");return std::wstring(argv[i]);};
        if(arg==L"--ui-prototype")prototype=true;else if(arg==L"--ui-self-test"){prototype=true;proto.selfTest=true;}else if(arg==L"--render-check"){prototype=true;proto.renderCheck=true;proto.capture=true;}
        else if(arg==L"--capture")proto.capture=true;else if(arg==L"--benchmark")proto.benchmark=true;else if(arg==L"--stress")proto.stress=true;else if(arg==L"--duration"){auto value=next();size_t used=0;proto.duration=std::stod(value,&used);if(used!=value.size()||!std::isfinite(proto.duration)||proto.duration<0)throw std::runtime_error("duration must be finite and nonnegative");}else if(arg==L"--font")proto.font=next();
        else if(arg==L"--config")configPath=next();else if(arg==L"--output"){outputPath=next();proto.output=outputPath;}else if(arg==L"--headless")headless=true;else if(arg==L"--single")single=true;else if(arg==L"--batch")count=integer(next());else if(arg==L"--seed")seed=integer(next());else if(arg==L"--preview"){auto n=integer(next());if(n!=0&&n!=2)throw std::runtime_error("preview must be 0 or 2");preview=int(n);previewSet=true;}
        else if(arg==L"--exercise-view")app.exerciseView=true;else if(arg==L"--app-self-test")app.selfTest=true;else if(arg==L"--exit-after-run")app.exitAfterRun=true;else if(arg==L"--evidence")app.evidence=next();else if(arg==L"--fps"){auto n=integer(next());if(n!=0&&n!=30&&n!=60)throw std::runtime_error("fps must be 0, 30 or 60");app.fps=int(n);}
        else if(arg==L"--view-results")app.viewResults=next();else if(arg==L"--diagnostic-results")app.diagnosticResults=true;
        else if(arg==L"--self-test")self=true;else if(arg==L"--replay")replayPath=next();else if(arg==L"--trial")replayId=integer(next());else if(arg==L"--write-config")writeDefault=next();else if(arg==L"--throughput-scene")throughput=true;
        else if(arg==L"--help"){std::cout<<"RollingStone3D: SI units, x downslope / y across / z up.\n"
            "--config scene.ini --batch N --seed integer --preview 0 --headless [--output path]\n"
            "--config scene.ini --single [--headless] [--output path]\n"
            "--config scene.ini --batch N --preview 2   (real GUI previews, then background)\n"
            "--view-results run_directory [--headless] [--diagnostic-results]  (read-only history)\n"
            "--replay run_directory --trial id --headless\n--self-test\n--write-config scene.ini [--throughput-scene]\n"
            "--ui-prototype [--stress] [--benchmark] [--duration seconds] [--font path] [--output path]\n"
            "No arguments opens the 3D editor. Ctrl+C cancels headless safely.\n"
            "--app-self-test [--evidence path]; GUI diagnostics: --capture --duration seconds --exit-after-run --fps 0|30|60\n";return 0;
        }else throw std::runtime_error("unknown option; see --help");
    }
    if(self){wchar_t executable[32768];DWORD length=GetModuleFileNameW(nullptr,executable,32768);if(!length||length>=32768)throw std::runtime_error("cannot locate test binaries");return selfTest(std::filesystem::path(executable).parent_path());}
    if(prototype){
#ifdef RS3D_GUI
        return runPrototype(proto);
#else
        throw std::runtime_error("this build excludes GUI; run build_3d.cmd Release without --core");
#endif
    }
    rs3d::JoltRuntime runtime;auto draft=configPath.empty()?rs3d::SceneDraft{}:rs3d::loadScene(configPath);
    if(!app.viewResults.empty()&&(single||count||!replayPath.empty()))throw std::runtime_error("--view-results is read-only and cannot combine with run/replay options");
    if(headless&&!app.viewResults.empty()){auto before=rs3d::physicsUpdateCount();auto archive=rs3d::openResultArchive(app.viewResults,app.diagnosticResults);auto directory=archive->analysis->directory;std::uint64_t frames=0;for(const auto&t:directory->trials)frames+=t->frames;std::cout<<"readonly_result N="<<directory->trials.size()<<" frames="<<frames<<" legacy="<<directory->legacy<<" diagnostic="<<directory->diagnostic<<" physics_updates="<<rs3d::physicsUpdateCount()-before<<'\n';return 0;}
    if(throughput){draft.terrain.type=rs3d::TerrainType::flat;draft.terrain.width=20;draft.releaseGap=0;draft.properties[rs3d::restitutionValue]={0,0};draft.maxTime=10;draft.trajectoryStride=0;draft.stableFaceRelease=true;}
    if(count)draft.count=*count;if(seed)draft.masterSeed=*seed;if(!outputPath.empty())draft.outputRoot=outputPath;
    if(!writeDefault.empty()){rs3d::saveScene(writeDefault,draft);std::cout<<"Saved UTF-8 scene: "<<writeDefault.u8string()<<'\n';return 0;}
    if(single&&count)throw std::runtime_error("--single and --batch are mutually exclusive");
    if(!headless&&replayPath.empty()){
#ifdef RS3D_GUI
        app.startSingle=single;app.startBatch=count.has_value();app.preview=previewSet?preview:2;app.font=proto.font;app.capture=proto.capture;app.duration=proto.duration;app.benchmark=proto.benchmark;return runApplication(draft,app);
#else
        throw std::runtime_error("this build excludes GUI; use --headless or build_3d.cmd Release");
#endif
    }
    if(preview)throw std::runtime_error("headless cannot display previews; use --preview 0");
    if(!replayPath.empty()){
        auto replay=rs3d::loadReplay(replayPath,replayId);rs3d::Trial trial(replay.scene,replay.settings);while(trial.snapshot().reason==rs3d::StopReason::running)trial.step();auto actual=trial.snapshot();double error=rs3d::norm(actual.center-replay.expected.center);bool same=rs3d::matchesReplay(actual,replay);
        double barrierError=replay.hasBarrierState?rs3d::norm(actual.barrier.center-replay.expected.barrier.center):0;
        std::cout<<std::setprecision(17)<<"replay trial="<<replayId<<" reason="<<rs3d::stopReasonName(actual.reason)<<" position_error_m="<<error<<" barrier_position_error_m="<<barrierError<<" pass="<<same<<'\n';return same?0:1;
    }
    if(single&&count)throw std::runtime_error("--single and --batch are mutually exclusive");if(single)draft.count=1;auto config=rs3d::freezeConfig(draft);rs3d::BatchControl control;{std::lock_guard<std::mutex>lock(consoleMutex);activeControl=&control;}SetConsoleCtrlHandler(interrupt,TRUE);
    rs3d::BatchOptions options;options.observer=[](const rs3d::BatchEvent&e){if(e.kind==rs3d::BatchEventKind::trialFinished&&(e.trialId<=3||e.trialId%50==0))std::cout<<"processed="<<e.processed<<" complete="<<e.completed<<" failures="<<e.failures<<'\n';};
    auto result=rs3d::runBatch(config,control,options);SetConsoleCtrlHandler(interrupt,FALSE);{std::lock_guard<std::mutex>lock(consoleMutex);activeControl=nullptr;}
    std::cout<<std::setprecision(10)<<"status="<<rs3d::batchStatusName(result.status)<<" N="<<draft.count<<" C="<<result.statistics.completed()<<" failures="<<result.failures<<" timeouts="<<result.timeouts<<" unstarted="<<result.unstarted()<<" updates="<<result.updates<<" internal_steps="<<result.internalSteps<<"\ngeneration_s="<<result.timing.generation<<" physics_s="<<result.timing.physics<<" recording_s="<<result.timing.recording<<" statistics_s="<<result.timing.statistics<<" output_s="<<result.timing.output<<" wall_s="<<result.timing.wall<<"\noutput="<<result.directory.u8string()<<"\nerror="<<result.error<<'\n';
    return result.status==rs3d::BatchStatus::complete?0:(result.status==rs3d::BatchStatus::cancelled?2:1);
}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
