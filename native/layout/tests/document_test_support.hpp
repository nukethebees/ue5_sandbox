#pragma once

#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>

#include <lispb/diagnostics.h>

#include <cstdint>
#include <string>

namespace ioj::layout {
inline auto diagnostic_messages(EditorSession const& editor) -> std::string {
    std::string result;
    auto const count{editor.diagnostics().size()};
    for (std::uint32_t index{}; index < count; ++index) {
        result += editor.format_diagnostic(lispb::DiagnosticId{index});
        result += '\n';
    }
    return result;
}
}
