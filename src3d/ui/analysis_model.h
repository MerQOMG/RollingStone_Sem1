#pragma once
// Pure (graphics-free) interpretation of analysis data: what a heat map means, how many samples back it, and whether it may be read as a probability.
#include "session.h"
#include <optional>
#include <string>
#include <vector>
namespace rs3d::gui {
enum class MapKind { route,contact,toeXY,toeXZ };
enum class Metric { count,frequency,conditional };
enum class Level { ok,info,warn,error };
struct MapData {
    MapKind kind=MapKind::route;Metric metric=Metric::frequency;
    bool applicable=false,usable=false,conditional=false;
    Grid3D grid{{{0,0,0},{1,1,1},{1,1,1}}};
    const std::vector<std::uint64_t>*hits=nullptr;
    std::uint64_t n=0,known=0,hitsTotal=0,misses=0;
    double denominator=1;
    Level level=Level::info;std::string status,detail,legend;
};
MapData buildMap(const RunConfig&config,const AnalysisSnapshot&analysis,MapKind kind,Metric metric,bool readError);
std::string cellReadout(const MapData&map,const AnalysisSnapshot&analysis,std::size_t cell);
std::string stopReasonZh(const std::string&englishName);
std::string stopReasonZh(StopReason reason);
std::string contactStatusZh(FirstContactStatus status);
std::string batchStatusZh(BatchStatus status);
std::string sessionStateZh(const SessionView&view);
std::string formatPercent(double fraction);
std::string formatCount(std::uint64_t v);
// Translates the most common backend validation messages into actionable Chinese and names the fields involved.
struct ErrorHint {std::string message;std::vector<std::string> keys;};
ErrorHint explainError(const std::string&technical);
}
