#pragma once
#include <ioj/layout/analysis/analysis_id.hpp>
#include <ioj/layout/analysis/scenario_id.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/schema/module_handle.h>
#include <lispb/schema/indices.h>

#include <optional>
#include <string_view>
namespace ioj::layout {
struct AnalysisContext;
struct AnalysisRequest;
struct ResolvedVariant;
class AbiProfile;
}
namespace ioj::layout::analysis_detail {
void analyze_physical_layouts(AnalysisContext& context,
                              ScenarioId scenario,
                              AnalysisRequest const& request,
                              ResolvedVariant const& variant);
auto physical_occurrence(AnalysisContext& context,
                         ScenarioId scenario,
                         AbiProfile const& abi,
                         lispb::schema::ResolvedOccurrenceId use) -> std::optional<TypeFacts>;
auto physical_spelling(AnalysisContext& context,
                       ScenarioId scenario,
                       AbiProfile const& abi,
                       std::string_view spelling,
                       lispb::codegen::ModuleHandle module = {}) -> std::optional<TypeFacts>;
}
