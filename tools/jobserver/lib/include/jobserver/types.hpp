#pragma once
#include "jobserver/handles.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace jobserver {
enum class LeaseMode { shared, exclusive };
struct GateClaim {
    std::string name;
    LeaseMode mode{LeaseMode::shared};
};
struct Command {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
};
struct Error {
    std::string code;
    std::string message;
};
[[nodiscard]] auto path_from_utf8(std::string const& value) -> std::filesystem::path;
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& value) -> std::string;
}
