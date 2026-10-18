#pragma once
#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/scenario_id.hpp>
namespace ioj::layout {
struct AnalysisContext;
}
namespace ioj::layout::analysis_detail {
void analyze_enum_backing(AnalysisContext& context, AnalysisId id);
void analyze_relationships(AnalysisContext& context, ScenarioId scenario);
}
