#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analysis/analyzer.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout {

struct AnalysisCase {
    std::string_view name;
    std::string_view source;
    void (*verify)(AnalysisContext&);
};

inline auto run_analysis(AnalysisContext& context,
                         std::string_view name = "Value",
                         AnalysisRequest request = {}) -> AnalysisRun {
    static auto const abi{AbiProfile::host_common()};
    request.type = context.document.find_type("game", name).value();
    if (!request.abi) {
        request.abi = &abi;
    }
    Analyzer::analyze(context, std::span{&request, 1});
    return context.storage.runs.back();
}

inline auto has_analysis_diagnostics(AnalysisContext const& context) -> bool {
    auto const& session{context.document.session()};
    return session.diagnostics().size() > session.last_diagnostic_begin();
}

}
