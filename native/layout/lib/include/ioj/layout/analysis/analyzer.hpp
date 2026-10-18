#pragma once
#include <span>
namespace ioj::layout {
struct AnalysisContext;
struct AnalysisRequest;
class Analyzer {
  public:
    // Rebuild the output for this batch. Views and row IDs expire at the next call.
    static void analyze(AnalysisContext& context, std::span<AnalysisRequest const> requests);
};
}
