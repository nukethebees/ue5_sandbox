#pragma once

#include <cstdint>
#include <string>

namespace ioj::sim {
struct LevelTelemetryEnvironment {
    std::string project_name{};
    std::string project_version{};
    std::string engine_version{};
    std::string build_version{};
    std::string build_configuration{};
    std::string execution_mode{};
    std::string world_type{};
    std::string platform{};
    std::string host_architecture{};
    std::string operating_system_version{};
    std::string operating_system_subversion{};
    std::string cpu_vendor{};
    std::string cpu_brand{};
    std::int32_t physical_core_count{};
    std::int32_t logical_core_count{};
    std::string primary_gpu_brand{};
    std::uint64_t total_physical_memory_bytes{};
};
}
