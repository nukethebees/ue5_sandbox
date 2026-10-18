#pragma once

#include <ioj/layout/document.hpp>

#include <lispb/diagnostics.h>

#include <cstdint>
#include <string>

namespace ioj::layout {
inline auto diagnostic_messages(Document const& document) -> std::string {
    std::string result;
    auto const count{document.diagnostics().size()};
    for (std::uint32_t index{}; index < count; ++index) {
        result += document.format_diagnostic(lispb::DiagnosticId{index});
        result += '\n';
    }
    return result;
}
}
