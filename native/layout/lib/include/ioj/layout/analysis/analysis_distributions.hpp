#pragma once

#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/distribution_id.hpp>

namespace ioj::layout {

struct AnalysisContext;
struct AnalysisRequest;

// Run after semantic and physical layout. The row must be an integer varint or union kind.
auto analyze_distribution(AnalysisContext& context,
                          AnalysisRequest const& request,
                          AnalysisId analysis) -> DistributionId;

}
