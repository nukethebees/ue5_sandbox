#pragma once

#include <ioj/levels/diagnostic_code.h>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ioj::levels {
struct Diagnostic {
    DiagnosticCode code{};
    std::string node_path{};
    std::string message{};
    std::filesystem::path source_path{};
};
using Diagnostics = std::vector<Diagnostic>;
[[nodiscard]] auto format_diagnostics(Diagnostics const& diagnostics) -> std::string;
}
