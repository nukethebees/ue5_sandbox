#pragma once
#include <ioj/cpu_simd_capabilities.h>

#include <cstdint>
#include <string>
namespace ioj {
enum class LargePageAccess { Unsupported, Enabled, PrivilegeUnavailable, QueryFailed };
struct WindowsMemoryCapabilities {
    std::uint64_t large_page_minimum_bytes{};
    LargePageAccess large_page_access_status{LargePageAccess::QueryFailed};
};
struct GameCapabilities {
    std::string platform_name;
    std::string host_architecture;
    std::string operating_system_version;
    std::string operating_system_subversion;
    std::string cpu_vendor;
    std::string cpu_brand;
    std::string primary_gpu_brand;
    int physical_core_count{};
    int logical_core_count{};
    std::uint64_t total_physical_memory_bytes{};
    CpuSimdCapabilities cpu_simd;
    WindowsMemoryCapabilities windows;
};

void query_cpu_capabilities(GameCapabilities& result);

namespace player {
auto query_game_capabilities() -> GameCapabilities;
}
}
