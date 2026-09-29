#pragma once
#include "jobserver/handles.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace jobserver {
enum class Mode { shared, exclusive };
struct Error {
    std::string code;
    std::string message;
};
[[nodiscard]] auto path_from_utf8(std::string const& value) -> std::filesystem::path;
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& value) -> std::string;
}
