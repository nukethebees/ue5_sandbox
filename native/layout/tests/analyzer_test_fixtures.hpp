#pragma once

#include "document_test_support.hpp"
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
#include <vector>

namespace ioj::layout {

struct AnalysisCase {
    std::string_view name;
    std::string_view source;
    void (*verify)(AnalysisContext&);
};

class AnalysisTest : public testing::TestWithParam<AnalysisCase> {
  protected:
    void verify_case() {
        EditorSession editor;
        auto const& test{GetParam()};
        auto const document{editor.from_source(test.source)};
        ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
        AnalysisStorage storage;
        AnalysisContext context{document, storage};
        test.verify(context);
    }
};

inline void expect_analysis_diagnostics(AnalysisContext const& context, bool expected = true) {
    auto const& session{context.document.session()};
    EXPECT_EQ(session.diagnostics().size() > session.last_diagnostic_begin(), expected)
        << diagnostic_messages(session);
}

inline void analyze_requests(AnalysisContext& context,
                             std::span<AnalysisRequest const> requests,
                             bool expect_diagnostics = false) {
    Analyzer::analyze(context, requests);
    expect_analysis_diagnostics(context, expect_diagnostics);
}

inline auto run_analysis(AnalysisContext& context,
                         std::string_view name = "Value",
                         AnalysisRequest request = {},
                         bool expect_diagnostics = false) -> AnalysisRun {
    static auto const abi{AbiProfile::host_common()};
    request.type = context.document.find_type("game", name).value();
    if (!request.abi) {
        request.abi = &abi;
    }
    analyze_requests(context, std::span{&request, 1}, expect_diagnostics);
    return context.storage.runs.back();
}

template <typename T>
auto scalar_result(AnalysisContext const& context,
                   std::vector<std::optional<T>> const& column,
                   std::string_view name = "Value") -> T const& {
    auto const type{context.document.find_type("game", name).value()};
    return column[context.storage.scalars.find(type)].value();
}

}
