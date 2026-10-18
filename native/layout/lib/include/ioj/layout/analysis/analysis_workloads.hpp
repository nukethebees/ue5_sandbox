#pragma once

#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/workload_id.hpp>

namespace ioj::layout {

struct AnalysisContext;
struct AnalysisRequest;

// Run after physical layout and allocation. The request selects a packed, record, or SoA row.
auto analyze_access(AnalysisContext& context, AnalysisRequest const& request, AnalysisId analysis)
    -> WorkloadId;

}
