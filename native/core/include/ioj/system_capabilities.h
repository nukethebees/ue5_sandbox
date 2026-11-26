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
struct SystemCapabilities {
    std::string platform_name;
    std::string host_architecture;
    std::string operating_system_version;
    std::string operating_system_subversion;
    std::string cpu_vendor;
    std::string cpu_brand;
    std::string primary_gpu_brand;
    std::uint32_t physical_core_count{};
    std::uint32_t logical_core_count{};
    std::uint64_t total_physical_memory_bytes{};
    CpuSimdCapabilities cpu_simd;
    WindowsMemoryCapabilities windows;
};

auto query_system_capabilities() -> SystemCapabilities;
}
