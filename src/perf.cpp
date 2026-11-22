#include "perf.h"
#include <chrono>
#include <cstdio>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#endif

float Perf::cpuPercent() {
    static double lastCpu = -1, lastWall = 0;
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    const double ticks = kernel.dwHighDateTime * 4294967296.0 + kernel.dwLowDateTime +
                         user.dwHighDateTime * 4294967296.0 + user.dwLowDateTime;
    double cpu = ticks * 1e-7;
#else
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    double cpu = u.ru_utime.tv_sec + u.ru_utime.tv_usec * 1e-6 + u.ru_stime.tv_sec + u.ru_stime.tv_usec * 1e-6;
#endif
    double wall = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    float pct = 0;
    if (lastCpu >= 0 && wall > lastWall) pct = (float)(100.0 * (cpu - lastCpu) / (wall - lastWall));
    lastCpu = cpu;
    lastWall = wall;
    return pct;
}

float Perf::memoryMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) return 0;
    return (float)(info.WorkingSetSize / (1024.0 * 1024.0));
#elif defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        return (float)(info.phys_footprint / (1024.0 * 1024.0));
    return 0;
#elif defined(__linux__)
    long pages = 0, resident = 0;
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    if (fscanf(f, "%ld %ld", &pages, &resident) != 2) resident = 0;
    fclose(f);
    return (float)(resident * (double)sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0));
#else
    return 0;
#endif
}
