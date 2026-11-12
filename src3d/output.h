#pragma once
#include "batch.h"
#include <fstream>
#include <map>
namespace rs3d {
class OutputFailure:public std::runtime_error{public:using std::runtime_error::runtime_error;};
class RunOutput {
    std::filesystem::path pending_,final_;
    std::map<std::string,std::ofstream>files_;
    std::uint64_t written_=0,failAfter_=0;
    std::string failAt_;
    double radius_=0;
    std::unique_ptr<MotionArchiveWriter> motion_;
    std::shared_ptr<ArchiveLocation> location_;
    void put(const std::string&name,const std::string&text);
    void flush(const std::string&name);
    void closeFiles();
public:
    explicit RunOutput(const RunConfig&config,std::uint64_t failAfter=0,std::string failAt={});
    const std::filesystem::path&pending()const{return pending_;}
    void initialize(const RunConfig&config);
    void shape(std::uint64_t id,const Polyhedron&shape);
    void trajectory(std::uint64_t id,const Snapshot&snapshot);
    void trial(const TrialRecord&record);
    void contact(const FirstContactEvent&event);
    MotionArchiveWriter&motion(){return *motion_;}
    bool hasMotion()const{return bool(motion_);}
    void finish(BatchResult&result);
    void diagnose(BatchResult&result)noexcept;
};
struct ReplayInput {PhysicsScene scene;TrialSettings settings;GridSettings grid;double collectorX;TrialInput input;Snapshot expected;bool hasBarrierState=false;};
bool matchesReplay(const Snapshot&actual,const ReplayInput&replay);
ReplayInput loadReplay(const std::filesystem::path&directory,std::uint64_t trialId);
}
