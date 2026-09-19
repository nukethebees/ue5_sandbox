#pragma once

#include <string>

namespace ioj::layout {

enum class DiagnosticSeverity { info, warning, error };

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::info};
    std::string message;
};

} // namespace ioj::layout
