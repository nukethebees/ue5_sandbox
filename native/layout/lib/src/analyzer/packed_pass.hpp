#pragma once
#include <ioj/layout/analysis/analysis_id.hpp>
namespace ioj::layout {
struct AnalysisContext;
struct ResolvedVariant;
}
namespace ioj::layout::analysis_detail {
void analyze_packed_layout(AnalysisContext& context, AnalysisId id, ResolvedVariant const& variant);
}
