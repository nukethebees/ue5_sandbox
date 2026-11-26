#include <ioj/system_capabilities.h>

#include "player/windows/system.h"
#include <ioj/files.h>

#include <cpu_features_macros.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_platform.h>
#ifdef CPU_FEATURES_ARCH_X86
#include <cpuinfo_x86.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <vector>

namespace ioj {
namespace {
void query_cpu_capabilities(SystemCapabilities& result) {
#ifdef CPU_FEATURES_ARCH_X86
    auto const cpu{cpu_features::GetX86Info()};
    result.cpu_vendor = cpu.vendor;
    result.cpu_brand = cpu.brand_string;
    auto const& features{cpu.features};
    result.cpu_simd = CpuSimdCapabilities{.available = true,
                                          .sse = features.sse != 0,
                                          .sse2 = features.sse2 != 0,
                                          .sse3 = features.sse3 != 0,
                                          .ssse3 = features.ssse3 != 0,
                                          .sse4_1 = features.sse4_1 != 0,
                                          .sse4_2 = features.sse4_2 != 0,
                                          .sse4a = features.sse4a != 0,
                                          .avx = features.avx != 0,
                                          .avx2 = features.avx2 != 0,
                                          .avx_vnni = features.avx_vnni != 0,
                                          .avx512_f = features.avx512f != 0,
                                          .avx512_cd = features.avx512cd != 0,
                                          .avx512_bw = features.avx512bw != 0,
                                          .avx512_dq = features.avx512dq != 0,
                                          .avx512_vl = features.avx512vl != 0,
                                          .amx_tile = features.amx_tile != 0,
                                          .amx_bf16 = features.amx_bf16 != 0,
                                          .amx_int8 = features.amx_int8 != 0,
                                          .amx_fp16 = features.amx_fp16 != 0};
#else
    result.cpu_simd = {};
#endif
}

auto query_windows_platform_capabilities() -> WindowsMemoryCapabilities {
    WindowsMemoryCapabilities capabilities;

    auto const large_page_minimum{GetLargePageMinimum()};
    if (large_page_minimum == 0) {
        capabilities.large_page_access_status = LargePageAccess::Unsupported;
        return capabilities;
    }

    capabilities.large_page_minimum_bytes = static_cast<std::uint64_t>(large_page_minimum);

    HANDLE process_token{nullptr};
    if (!OpenProcessToken(
            GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process_token)) {
        std::println(stderr,
                     "Could not open the process token while querying large-page access: {}.",
                     GetLastError());
        return capabilities;
    }

    LUID privilege_id{};
    if (!LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &privilege_id)) {
        auto const error{GetLastError()};
        CloseHandle(process_token);
        std::println(stderr,
                     "Could not look up the large-page privilege while querying capabilities: {}.",
                     error);
        return capabilities;
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = privilege_id;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(ERROR_SUCCESS);
    BOOL const adjusted{AdjustTokenPrivileges(process_token, 0, &privileges, 0, nullptr, nullptr)};
    DWORD const error{GetLastError()};
    CloseHandle(process_token);

    if (adjusted == 0) {
        std::println(stderr,
                     "Could not adjust the process token while querying large-page access: {}.",
                     error);
        return capabilities;
    }

    if (error == ERROR_NOT_ALL_ASSIGNED) {
        capabilities.large_page_access_status = LargePageAccess::PrivilegeUnavailable;
        return capabilities;
    }

    if (error != ERROR_SUCCESS) {
        std::println(stderr, "Unexpected result while enabling large-page access: {}.", error);
        return capabilities;
    }

    capabilities.large_page_access_status = LargePageAccess::Enabled;
    return capabilities;
}
auto windows_version_value(wchar_t const* name) -> std::string {
    std::array<wchar_t, 256> value{};
    DWORD bytes{sizeof(value)};
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     name,
                     RRF_RT_REG_SZ,
                     nullptr,
                     value.data(),
                     &bytes) != ERROR_SUCCESS) {
        return {};
    }

    return path_to_utf8(std::filesystem::path{value.data()});
}
auto physical_core_count() -> std::uint32_t {
    DWORD bytes{};
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    std::vector<std::byte> buffer(bytes);
    auto* info{reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data())};
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &bytes)) {
        return 0;
    }

    std::uint32_t cores{};
    for (DWORD offset{}; offset < bytes; offset += info->Size) {
        info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        ++cores;
    }

    return cores;
}
}
auto query_system_capabilities() -> SystemCapabilities {
    SystemCapabilities result;
    result.platform_name = SDL_GetPlatform();
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    switch (system.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            result.host_architecture = "x86_64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            result.host_architecture = "ARM64";
            break;
        default:
            result.host_architecture = "Unknown";
            break;
    }

    result.operating_system_version = windows_version_value(L"ProductName");
    result.operating_system_subversion = std::format("{} (build {})",
                                                     windows_version_value(L"DisplayVersion"),
                                                     windows_version_value(L"CurrentBuildNumber"));
    query_cpu_capabilities(result);
    result.logical_core_count = static_cast<std::uint32_t>(SDL_GetNumLogicalCPUCores());
    result.physical_core_count = physical_core_count();
    MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
    if (GlobalMemoryStatusEx(&memory)) {
        result.total_physical_memory_bytes = memory.ullTotalPhys;
    }

    DISPLAY_DEVICEW adapter{};
    adapter.cb = sizeof(adapter);
    for (DWORD index{}; EnumDisplayDevicesW(nullptr, index, &adapter, 0); ++index) {
        if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0) {
            result.primary_gpu_brand = path_to_utf8(std::filesystem::path{adapter.DeviceString});
            break;
        }
    }

    result.windows = query_windows_platform_capabilities();
    return result;
}
}
