#include <ioj/layout/analysis/compare_runs.hpp>

#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>

#include <lispb/schema/type_graph.h>

#include <cassert>

namespace ioj::layout {
auto compare_runs(AnalysisContext& context, std::size_t first, std::size_t second) -> bool {
    auto const& storage{context.storage};
    assert(first < storage.runs.size() && second < storage.runs.size());
    auto const first_type{storage.types.schema_type[storage.runs[first].analysis.value]};
    auto const second_type{storage.types.schema_type[storage.runs[second].analysis.value]};
    auto source = [&](lispb::schema::TypeId type) {
        auto const row{storage.scalars.find(type)};
        auto const& scalars{storage.scalars};
        if (scalars.optional_sentinel_encoding[row]) {
            return scalars.optional_sentinel_encoding[row]->source_type;
        }
        if (scalars.optional_presence_encoding[row]) {
            return scalars.optional_presence_encoding[row]->source_type;
        }
        if (scalars.quantized_encoding[row]) {
            return scalars.quantized_encoding[row]->source_type;
        }
        if (scalars.varint_encoding[row]) {
            return scalars.varint_encoding[row]->source_type;
        }
        return type;
    };
    auto const first_kind{context.types.definition(first_type).kind()};
    auto const second_kind{context.types.definition(second_type).kind()};
    using Kind = lispb::schema::TypeDefinitionKind;
    auto optional_kind = [](Kind kind) {
        return kind == Kind::optional_sentinel || kind == Kind::optional_presence_bit;
    };
    if (source(first_type) != source(second_type) ||
        (first_kind != second_kind && !(optional_kind(first_kind) && optional_kind(second_kind)))) {
        context.report(
            {DiagnosticSeverity::error,
             "Compared analyses require the same semantic source and compatible encoding kinds."});
        return false;
    }
    if (storage.totals[first].element_count != storage.totals[second].element_count) {
        context.report(
            {DiagnosticSeverity::error, "Compared analyses use different element counts."});
        return false;
    }
    return true;
}
}
