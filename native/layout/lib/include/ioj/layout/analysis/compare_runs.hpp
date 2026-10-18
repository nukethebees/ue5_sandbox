#pragma once
#include <cstddef>
namespace ioj::layout {
struct AnalysisContext;
// Check the semantic comparison boundary; numeric differences are read from the two runs.
auto compare_runs(AnalysisContext& context, std::size_t first, std::size_t second) -> bool;
}
