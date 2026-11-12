#pragma once
#include "first_barrier_contact.h"
#include "route_heatmap.h"
#include "motion_archive.h"
namespace rs3d {
void writeAnalysisHeaders(const ArchiveWrite&write);
void writeFirstContact(const FirstContactEvent&event,const ArchiveWrite&write);
void writeAnalysisGrids(const RouteAccumulator&route,const FirstContactAccumulator&contact,bool filesComplete,const ArchiveWrite&write);
std::vector<FirstContactEvent> readFirstContacts(const std::filesystem::path&directory,const BarrierSettings&barrier,std::uint64_t n,double cell);
}
