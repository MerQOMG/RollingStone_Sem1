#include "platform.h"
#include <Windows.h>
#include <Psapi.h>
#include <sstream>
#include <intrin.h>
std::size_t peakWorkingSet() {
    PROCESS_MEMORY_COUNTERS m{};
    return GetProcessMemoryInfo(GetCurrentProcess(),&m,sizeof(m)) ? m.PeakWorkingSetSize : 0;
}
std::string machineDescription() {
    char cpu[49]{};int info[4];
    for(int i=0;i<3;++i) {__cpuid(info,static_cast<int>(0x80000002U+i));memcpy(cpu+16*i,info,16);}
    MEMORYSTATUSEX mem{};mem.dwLength=sizeof(mem);GlobalMemoryStatusEx(&mem);
    std::ostringstream out;out<<"CPU="<<cpu<<" RAM_bytes="<<mem.ullTotalPhys<<" logical_processors=";
    SYSTEM_INFO sys{};GetNativeSystemInfo(&sys);out<<sys.dwNumberOfProcessors;
    // File version is independent of app manifest compatibility shims.
    wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);
    out<<" OS=Windows (build details in configure log)";
    return out.str();
}
