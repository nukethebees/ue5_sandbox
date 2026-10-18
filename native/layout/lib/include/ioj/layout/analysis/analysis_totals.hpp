#pragma once

#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/element_totals.hpp>

namespace ioj::layout {

struct AnalysisContext;
struct AnalysisRequest;

// Scale completed layout/encoding facts for this request; SoA totals describe its allocation.
auto compute_element_totals(AnalysisContext& context,
                            AnalysisRequest const& request,
                            AnalysisId analysis) -> ElementTotals;

}
