#pragma once
#include <ioj/layout/analysis/analysis_id.hpp>
namespace ioj::layout {
struct AnalysisContext;
struct AnalysisRequest;
struct ResolvedVariant;
}
namespace ioj::layout::analysis_detail {
void analyze_soa_layout(AnalysisContext& context,
                        AnalysisId id,
                        AnalysisRequest const& request,
                        ResolvedVariant const& variant);
}
