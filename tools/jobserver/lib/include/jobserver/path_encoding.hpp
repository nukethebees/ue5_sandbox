#pragma once

#include <filesystem>
#include <string>

namespace jobserver {
[[nodiscard]] auto path_to_utf8(std::filesystem::path const& value) -> std::string;
}
