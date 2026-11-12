#pragma once
#include "batch.h"
#include "archive_csv.h"
namespace rs3d {
struct ArchivedTrial {
    std::uint64_t id=0;
    CsvSpan vertices,faces;
    CsvRow metadata;
    std::string status="not_started",error;
};
struct ResultArchive {
    std::filesystem::path directory;
    int schema=0;
    std::shared_ptr<const RunConfig>config;
    std::shared_ptr<const AnalysisSnapshot>analysis;
    std::vector<ArchivedTrial> trials;
    bool routeRecorded=false,contactRecorded=false,diagnostic=false;
    std::string notice;
    // Actual geometry, bounded independently of the numerical sampler's k<=100.
    std::shared_ptr<const PhysicsScene> scene(std::uint64_t trialId)const;
};
std::shared_ptr<const ResultArchive> openResultArchive(const std::filesystem::path&directory,bool diagnostic=false);
std::shared_ptr<const PhysicsScene> readArchivedGeometry(const std::filesystem::path&directory,std::uint64_t trialId,const RunConfig&config,const ArchivedGeometry&geometry);
}
