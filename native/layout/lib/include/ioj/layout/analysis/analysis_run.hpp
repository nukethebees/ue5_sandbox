#pragma once
#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/distribution_id.hpp>
#include <ioj/layout/analysis/workload_id.hpp>

#include <optional>
namespace ioj::layout {
struct AnalysisRun {
    AnalysisId analysis{};
    std::optional<WorkloadId> access{};
    std::optional<DistributionId> distribution{};
};
}
