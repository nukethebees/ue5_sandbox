#include <ioj/layout/analysis/analysis_distributions.hpp>

#include "../checked_arithmetic.hpp"
#include "../varint_encoding.hpp"
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/string_set.hpp>

#include <codegen/schema/declarations.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cassert>
#include <string>

namespace ioj::layout {
namespace distribution_detail {

using namespace analysis_detail;
using Quantity = std::optional<std::uint64_t>;

void add_exact(AnalysisContext& context,
               Quantity& total,
               Quantity weighted,
               std::string_view label) {
    if (!total) {
        return;
    }
    if (!weighted) {
        total.reset();
        return;
    }
    total = checked_add(*total, *weighted);
    if (!total) {
        context.report({DiagnosticSeverity::error,
                        "Distribution " + std::string{label} + " total overflows uint64."});
    }
}

auto weighted_value(AnalysisContext& context,
                    Quantity value,
                    std::uint64_t weight,
                    std::string_view label) -> Quantity {
    if (!value) {
        return std::nullopt;
    }
    auto const result{checked_multiply(*value, weight)};
    if (!result) {
        context.report({DiagnosticSeverity::error,
                        "Distribution weighted " + std::string{label} + " overflows uint64."});
    }
    return result;
}

} // namespace distribution_detail

auto analyze_distribution(AnalysisContext& context,
                          AnalysisRequest const& request,
                          AnalysisId analysis) -> DistributionId {
    using namespace distribution_detail;
    auto& storage{context.storage};
    auto& table{storage.distributions};
    auto& entries{storage.distribution_entries};
    DistributionId const id{
        table.append(analysis, {entries.weight.size(), 0}, request.element_count)};
    auto const row{id.value};
    auto numerical_weight{0.0L};
    auto numerical_extent{0.0L};
    auto numerical_slack{0.0L};
    bool extent_complete{true};
    bool slack_complete{true};

    auto append_entry = [&](std::optional<std::size_t> component,
                            std::optional<lispb::codegen::PackedIntegerValue> value,
                            std::uint64_t weight,
                            Quantity extent,
                            Quantity slack) {
        auto const weighted_extent{weighted_value(context, extent, weight, "extent bytes")};
        auto const weighted_slack{weighted_value(context, slack, weight, "slack bytes")};
        entries.component.push_back(component);
        entries.value.push_back(value);
        entries.weight.push_back(weight);
        entries.extent_bytes.push_back(extent);
        entries.slack_bytes.push_back(slack);
        entries.weighted_extent_bytes.push_back(weighted_extent);
        entries.weighted_slack_bytes.push_back(weighted_slack);
        ++table.entries[row].count;

        add_exact(context, table.total_weight[row], weight, "weight");
        add_exact(context, table.total_extent_bytes[row], weighted_extent, "extent bytes");
        add_exact(context, table.total_slack_bytes[row], weighted_slack, "slack bytes");
        numerical_weight += static_cast<long double>(weight);
        if (extent) {
            numerical_extent +=
                static_cast<long double>(weight) * static_cast<long double>(*extent);
        } else if (weight != 0) {
            extent_complete = false;
        }
        if (slack) {
            numerical_slack += static_cast<long double>(weight) * static_cast<long double>(*slack);
        } else if (weight != 0) {
            slack_complete = false;
        }
    };

    auto const& types{context.types};
    auto const definition{types.definition(request.type)};
    auto const kind{definition.kind()};
    if (kind == lispb::schema::TypeDefinitionKind::integer_varint) {
        auto const& varints{types.definitions().integer_varints};
        auto const varint{varints.index(definition)};
        auto const source_type{
            types.occurrences().type[types.occurrences().index(varints.source[varint])]};
        auto const source{lispb::schema::integer_domain(types, source_type)};
        assert(source.has_value());
        auto const& scalars{types.definitions().integer_scalars};
        auto const scalar{scalars.index(*source)};
        auto const& named_codes{types.declarations().packed_named_codes};
        for (auto const& entry : request.varint_distribution) {
            auto const in_range{lispb::codegen::packed_integer_less_equal(
                                    scalars.minimum_value[scalar], entry.value) &&
                                lispb::codegen::packed_integer_less_equal(
                                    entry.value, scalars.maximum_value[scalar])};
            auto const named{std::ranges::any_of(
                types.declarations().items(scalars.named_codes[scalar]), [&](auto code) {
                    return named_codes.value[named_codes.index(code)] == entry.value;
                })};
            if (!in_range && !named) {
                context.report({DiagnosticSeverity::error,
                                "Distribution value " +
                                    lispb::codegen::format_packed_integer(entry.value) +
                                    " is outside the semantic source domain."});
                continue;
            }
            append_entry(std::nullopt,
                         entry.value,
                         entry.weight,
                         integer_varint_bytes(entry.value, varints.encoding[varint]),
                         0);
        }
    } else {
        auto const tagged{kind == lispb::schema::TypeDefinitionKind::tagged_union};
        assert(tagged || kind == lispb::schema::TypeDefinitionKind::union_type);
        auto const& aggregate{storage.types.aggregate[analysis.value]};
        assert(aggregate.has_value());
        auto const range{aggregate->members};
        auto const& components{storage.components};
        auto const& names{tagged ? components.tag : components.name};
        StringSet seen;
        auto add_alternative = [&](std::string const& name, std::uint64_t weight) {
            if (!seen.insert(name).second) {
                context.report({DiagnosticSeverity::error,
                                "Distribution contains duplicate alternative '" + name + "'."});
                return;
            }
            auto const end{range.begin + range.count};
            auto component{range.begin};
            for (; component != end; ++component) {
                if (names[component] == name) {
                    break;
                }
            }
            if (component == end) {
                auto role{std::string{"unknown"}};
                if (tagged) {
                    auto const& tags{storage.tags};
                    auto const tags_end{aggregate->tags.begin + aggregate->tags.count};
                    for (auto tag{aggregate->tags.begin}; tag != tags_end; ++tag) {
                        if (tags.name[tag] != name) {
                            continue;
                        }
                        role = tags.count_sentinel[tag] ? "count sentinel"
                             : tags.sentinel[tag]       ? "sentinel"
                                                        : "unmapped live";
                        break;
                    }
                }
                context.report({DiagnosticSeverity::error,
                                "Distribution alternative '" + name + "' is " + role +
                                    " and has no payload alternative."});
                return;
            }
            append_entry(component,
                         std::nullopt,
                         weight,
                         components.extent_bytes[component],
                         components.slack_bytes[component]);
        };
        if (tagged) {
            for (auto const& entry : request.tagged_union_distribution) {
                add_alternative(entry.tag, entry.weight);
            }
        } else {
            for (auto const& entry : request.union_distribution) {
                add_alternative(entry.alternative_name, entry.weight);
            }
        }
    }

    if (numerical_weight > 0.0L) {
        if (extent_complete) {
            table.expected_extent_bytes[row] = numerical_extent / numerical_weight;
            table.expected_selected_extent_bytes[row] =
                *table.expected_extent_bytes[row] * static_cast<long double>(request.element_count);
        }
        if (slack_complete) {
            table.expected_slack_bytes[row] = numerical_slack / numerical_weight;
            table.expected_selected_slack_bytes[row] =
                *table.expected_slack_bytes[row] * static_cast<long double>(request.element_count);
        }
    }
    if (numerical_weight == 0.0L || !extent_complete || !slack_complete) {
        context.report({DiagnosticSeverity::warning,
                        "Expected distribution size is Unknown because there is no positive valid "
                        "weight with complete target facts."});
    }
    return id;
}

} // namespace ioj::layout
