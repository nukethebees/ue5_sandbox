#pragma once

#include <optional>
#include <string>

namespace ioj::layout {

enum class DiagnosticSeverity { info, warning, error };

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::info};
    std::string message;
    std::optional<std::string> missing_physical_type{};
};

} // namespace ioj::layout
