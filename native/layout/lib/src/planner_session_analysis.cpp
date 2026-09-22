#include <ioj/layout/planner_session.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

namespace ioj::layout {
using namespace lispb::schema;

void PlannerAnalysisSession::analyze_selected(EditableSchemaDocument const* document,
                                              PlannerAnalysisResults previous,
                                              bool const reuse_primary,
                                              bool const reuse_comparison_target,
                                              bool const reuse_variant_comparison) {
    auto const& definition{inputs.workspace.types().type(*inputs.selection.type).definition};
    auto const& baseline{*inputs.workspace.variant(LayoutWorkspace::baseline_variant_id)};
    auto const& active{inputs.workspace.active_variant()};
    auto const& comparison_a{*inputs.workspace.variant(inputs.comparison_a_variant_id)};
    auto const& comparison_b{*inputs.workspace.variant(inputs.comparison_b_variant_id)};
    auto const element_count{inputs.workspace.element_count()};
    auto const relationship_targets_for{[this](Variant const& variant) {
        return Analyzer::derive_relationship_target_facts(inputs.workspace.types(),
                                                          variant,
                                                          primary_abi_,
                                                          inputs.workspace.default_capacity(),
                                                          inputs.soa_allocation_strategy);
    }};
    auto const relationship_targets_for_profile{
        [this](Variant const& variant, AbiProfile const& abi) {
            return Analyzer::derive_relationship_target_facts(inputs.workspace.types(),
                                                              variant,
                                                              abi,
                                                              inputs.workspace.default_capacity(),
                                                              inputs.soa_allocation_strategy);
        }};
    if (std::holds_alternative<EnumType>(definition)) {
        if (reuse_primary && previous.enum_domain.has_value()) {
            results_.enum_domain = std::move(previous.enum_domain);
        } else {
            results_.enum_domain = Analyzer::analyze_enum(
                inputs.workspace.types(), *inputs.selection.type, primary_abi_, element_count);
        }
        auto comparison_domain{reuse_comparison_target &&
                                       previous.enum_target_comparison.has_value()
                                   ? std::move(previous.enum_target_comparison->second)
                                   : Analyzer::analyze_enum(inputs.workspace.types(),
                                                            *inputs.selection.type,
                                                            comparison_abi_,
                                                            element_count)};
        results_.enum_target_comparison =
            Analyzer::compare_enum_targets(*results_.enum_domain, comparison_domain);
    } else if (std::holds_alternative<IntegerScalarType>(definition)) {
        auto const active_targets{relationship_targets_for(active)};
        results_.integer_scalar_analysis = Analyzer::analyze_integer_scalar(
            inputs.workspace.types(), *inputs.selection.type, active_targets);
        auto const comparison_a_targets{relationship_targets_for(comparison_a)};
        auto const comparison_b_targets{relationship_targets_for(comparison_b)};
        results_.integer_scalar_capacity_comparison =
            Analyzer::compare_integer_scalar_capacity(inputs.workspace.types(),
                                                      *inputs.selection.type,
                                                      comparison_a_targets,
                                                      comparison_b_targets);
    } else if (std::holds_alternative<LinearQuantizedType>(definition)) {
        results_.linear_quantized_analysis =
            Analyzer::analyze_linear_quantized(inputs.workspace.types(), *inputs.selection.type);
        if (inputs.quantized_comparison_type.has_value()) {
            auto const comparison_type{
                inputs.workspace.types().find(*inputs.quantized_comparison_type)};
            if (comparison_type.has_value()) {
                results_.linear_quantized_comparison =
                    Analyzer::compare_linear_quantized(inputs.workspace.types(),
                                                       *inputs.selection.type,
                                                       *comparison_type,
                                                       element_count);
            }
        }
    } else if (std::holds_alternative<IntegerVarintType>(definition)) {
        results_.integer_varint_analysis = Analyzer::analyze_integer_varint(
            inputs.workspace.types(), *inputs.selection.type, element_count);
        if (varint_distribution_source_ ==
            inputs.workspace.types()
                .type(std::get<IntegerVarintType>(definition).source.type)
                .identity) {
            results_.integer_varint_distribution =
                Analyzer::analyze_integer_varint_distribution(inputs.workspace.types(),
                                                              *inputs.selection.type,
                                                              varint_distribution_entries_,
                                                              element_count);
        }
        if (inputs.varint_comparison_type.has_value()) {
            auto const comparison_type{
                inputs.workspace.types().find(*inputs.varint_comparison_type)};
            if (comparison_type.has_value()) {
                results_.integer_varint_comparison =
                    Analyzer::compare_integer_varint(inputs.workspace.types(),
                                                     *inputs.selection.type,
                                                     *comparison_type,
                                                     element_count);
                if (varint_distribution_rows_present_ &&
                    results_.integer_varint_distribution.has_value()) {
                    results_.integer_varint_distribution_comparison =
                        Analyzer::compare_integer_varint_distribution(inputs.workspace.types(),
                                                                      *inputs.selection.type,
                                                                      *comparison_type,
                                                                      varint_distribution_entries_,
                                                                      element_count);
                }
            }
        }
    } else if (std::holds_alternative<FixedPointType>(definition)) {
        results_.fixed_point_analysis = Analyzer::analyze_fixed_point(
            inputs.workspace.types(), *inputs.selection.type, element_count);
    } else if (std::holds_alternative<MiniFloatType>(definition)) {
        results_.mini_float_analysis = Analyzer::analyze_mini_float(
            inputs.workspace.types(), *inputs.selection.type, element_count);
    } else if (std::holds_alternative<OptionalSentinelType>(definition)) {
        results_.optional_sentinel_analysis = Analyzer::analyze_optional_sentinel(
            inputs.workspace.types(), *inputs.selection.type, element_count);
        if (inputs.optional_comparison_type.has_value()) {
            auto const comparison_type{
                inputs.workspace.types().find(*inputs.optional_comparison_type)};
            if (comparison_type.has_value()) {
                results_.optional_encoding_comparison =
                    Analyzer::compare_optional_encodings(inputs.workspace.types(),
                                                         *inputs.selection.type,
                                                         *comparison_type,
                                                         element_count);
            }
        }
    } else if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        results_.optional_presence_bit_analysis = Analyzer::analyze_optional_presence_bit(
            inputs.workspace.types(), *inputs.selection.type, element_count);
        if (inputs.optional_comparison_type.has_value()) {
            auto const comparison_type{
                inputs.workspace.types().find(*inputs.optional_comparison_type)};
            if (comparison_type.has_value()) {
                results_.optional_encoding_comparison =
                    Analyzer::compare_optional_encodings(inputs.workspace.types(),
                                                         *inputs.selection.type,
                                                         *comparison_type,
                                                         element_count);
            }
        }
    } else if (std::holds_alternative<RecordType>(definition)) {
        if (reuse_primary && previous.record_analysis.has_value()) {
            results_.record_analysis = std::move(previous.record_analysis);
        } else {
            results_.record_analysis = Analyzer::analyze_record(
                inputs.workspace.types(), *inputs.selection.type, primary_abi_, element_count);
        }
        auto comparison_record{reuse_comparison_target &&
                                       previous.record_target_comparison.has_value()
                                   ? std::move(previous.record_target_comparison->second)
                                   : Analyzer::analyze_record(inputs.workspace.types(),
                                                              *inputs.selection.type,
                                                              comparison_abi_,
                                                              element_count)};
        results_.record_target_comparison =
            Analyzer::compare_record_targets(*results_.record_analysis, comparison_record);
        std::vector<AccessIntent> access_members;
        access_members.reserve(inputs.selection.record_access_members.size());
        for (auto const& [name, operation] : inputs.selection.record_access_members) {
            access_members.push_back({.name = name, .operation = operation});
        }
        if (!inputs.selection.record_access_set_explicit && !inputs.selection.field.empty()) {
            access_members.clear();
            access_members.push_back(
                {.name = inputs.selection.field, .operation = inputs.access_operation});
        }
        if (!access_members.empty()) {
            results_.record_access_analysis =
                Analyzer::analyze_record_access(*results_.record_analysis,
                                                access_members,
                                                primary_abi_,
                                                inputs.access_multiplicity);
            auto const comparison_access{Analyzer::analyze_record_access(
                comparison_record, access_members, comparison_abi_, inputs.access_multiplicity)};
            results_.record_target_access_comparison = Analyzer::compare_record_access(
                *results_.record_access_analysis, comparison_access);
        }
    } else if (std::holds_alternative<UnionType>(definition)) {
        if (reuse_primary && previous.union_analysis.has_value()) {
            results_.union_analysis = std::move(previous.union_analysis);
        } else {
            results_.union_analysis = Analyzer::analyze_union(
                inputs.workspace.types(), *inputs.selection.type, primary_abi_, element_count);
        }
        auto comparison_target_union{reuse_comparison_target &&
                                             previous.union_target_comparison.has_value()
                                         ? std::move(previous.union_target_comparison->second)
                                         : Analyzer::analyze_union(inputs.workspace.types(),
                                                                   *inputs.selection.type,
                                                                   comparison_abi_,
                                                                   element_count)};
        results_.union_target_comparison =
            Analyzer::compare_union_targets(*results_.union_analysis, comparison_target_union);
        auto const declaration{
            document != nullptr
                ? document->find_declaration(
                      inputs.workspace.types().type(*inputs.selection.type).identity)
                : std::optional<DeclarationId>{}};
        auto const found{declaration.has_value() ? union_distributions_.find(*declaration)
                                                 : union_distributions_.end()};
        if (found != union_distributions_.end()) {
            std::vector<UnionDistributionEntry> entries;
            for (auto const& [alternative_name, weight] : found->second) {
                if (weight != 0) {
                    entries.push_back({.alternative_name = alternative_name, .weight = weight});
                }
            }
            if (!entries.empty()) {
                results_.union_distribution_analysis = Analyzer::analyze_union_distribution(
                    *results_.union_analysis, entries, element_count);
                auto const comparison_target_distribution{Analyzer::analyze_union_distribution(
                    comparison_target_union, entries, element_count)};
                results_.union_target_distribution_comparison =
                    Analyzer::compare_union_distributions(*results_.union_distribution_analysis,
                                                          comparison_target_distribution);
            }
        }
    } else if (std::holds_alternative<TaggedUnionType>(definition)) {
        if (reuse_primary && previous.tagged_union_analysis.has_value()) {
            results_.tagged_union_analysis = std::move(previous.tagged_union_analysis);
        } else {
            results_.tagged_union_analysis = Analyzer::analyze_tagged_union(
                inputs.workspace.types(), *inputs.selection.type, primary_abi_, element_count);
        }
        auto comparison_target_tagged{
            reuse_comparison_target && previous.tagged_union_target_comparison.has_value()
                ? std::move(previous.tagged_union_target_comparison->second)
                : Analyzer::analyze_tagged_union(inputs.workspace.types(),
                                                 *inputs.selection.type,
                                                 comparison_abi_,
                                                 element_count)};
        results_.tagged_union_target_comparison = Analyzer::compare_tagged_union_targets(
            *results_.tagged_union_analysis, comparison_target_tagged);
        auto const declaration{
            document != nullptr
                ? document->find_declaration(
                      inputs.workspace.types().type(*inputs.selection.type).identity)
                : std::optional<DeclarationId>{}};
        auto const found{declaration.has_value() ? tagged_union_distributions_.find(*declaration)
                                                 : tagged_union_distributions_.end()};
        if (found != tagged_union_distributions_.end()) {
            std::vector<TaggedUnionDistributionEntry> entries;
            for (auto const& [tag, weight] : found->second) {
                if (weight != 0) {
                    entries.push_back({.tag = tag, .weight = weight});
                }
            }
            if (!entries.empty()) {
                results_.tagged_union_distribution_analysis =
                    Analyzer::analyze_tagged_union_distribution(
                        *results_.tagged_union_analysis, entries, element_count);
                auto const comparison_target_distribution{
                    Analyzer::analyze_tagged_union_distribution(
                        comparison_target_tagged, entries, element_count)};
                results_.tagged_union_target_distribution_comparison =
                    Analyzer::compare_tagged_union_distributions(
                        *results_.tagged_union_distribution_analysis,
                        comparison_target_distribution);
            }
        }
    } else if (std::holds_alternative<PackedType>(definition)) {
        if (reuse_primary && previous.baseline_packed.has_value() &&
            previous.active_packed.has_value()) {
            results_.baseline_packed = std::move(previous.baseline_packed);
            results_.active_packed = std::move(previous.active_packed);
        } else {
            auto const baseline_targets{relationship_targets_for(baseline)};
            auto const active_targets{relationship_targets_for(active)};
            results_.baseline_packed = Analyzer::analyze_packed(inputs.workspace.types(),
                                                                *inputs.selection.type,
                                                                baseline,
                                                                primary_abi_,
                                                                element_count,
                                                                baseline_targets);
            results_.active_packed = Analyzer::analyze_packed(inputs.workspace.types(),
                                                              *inputs.selection.type,
                                                              active,
                                                              primary_abi_,
                                                              element_count,
                                                              active_targets);
        }
        auto comparison_target_packed{
            reuse_comparison_target && previous.packed_target_comparison.has_value()
                ? std::move(previous.packed_target_comparison->second)
                : Analyzer::analyze_packed(
                      inputs.workspace.types(),
                      *inputs.selection.type,
                      active,
                      comparison_abi_,
                      element_count,
                      relationship_targets_for_profile(active, comparison_abi_))};
        results_.packed_target_comparison =
            Analyzer::compare_packed_targets(*results_.active_packed, comparison_target_packed);
        std::vector<AccessIntent> access_fields;
        access_fields.reserve(inputs.selection.packed_access_fields.size());
        for (auto const& [name, operation] : inputs.selection.packed_access_fields) {
            access_fields.push_back({.name = name, .operation = operation});
        }
        if (!inputs.selection.packed_access_set_explicit && !inputs.selection.field.empty()) {
            access_fields.clear();
            auto const selected{std::ranges::find(results_.active_packed->fields,
                                                  inputs.selection.field,
                                                  &PackedFieldAnalysis::name)};
            if (selected != results_.active_packed->fields.end() && !selected->reserved) {
                access_fields.push_back(
                    {.name = inputs.selection.field, .operation = inputs.access_operation});
            }
        }
        if (!access_fields.empty()) {
            results_.packed_access_analysis = Analyzer::analyze_packed_access(
                *results_.active_packed, access_fields, primary_abi_, inputs.access_multiplicity);
            auto const comparison_target_access{
                Analyzer::analyze_packed_access(comparison_target_packed,
                                                access_fields,
                                                comparison_abi_,
                                                inputs.access_multiplicity)};
            results_.packed_target_access_comparison = Analyzer::compare_packed_access(
                *results_.packed_access_analysis, comparison_target_access);
        }
        if (reuse_primary) {
            results_.packed_variants = std::move(previous.packed_variants);
        } else {
            for (auto const& variant : inputs.workspace.variants()) {
                if (variant.id != LayoutWorkspace::baseline_variant_id) {
                    auto const targets{relationship_targets_for(variant)};
                    results_.packed_variants.emplace_back(
                        variant.id,
                        Analyzer::analyze_packed(inputs.workspace.types(),
                                                 *inputs.selection.type,
                                                 variant,
                                                 primary_abi_,
                                                 element_count,
                                                 targets));
                }
            }
        }
        if (reuse_variant_comparison && previous.comparison_a_packed.has_value() &&
            previous.comparison_b_packed.has_value()) {
            results_.comparison_a_packed = std::move(previous.comparison_a_packed);
            results_.comparison_b_packed = std::move(previous.comparison_b_packed);
        } else {
            results_.comparison_a_packed =
                Analyzer::analyze_packed(inputs.workspace.types(),
                                         *inputs.selection.type,
                                         comparison_a,
                                         primary_abi_,
                                         element_count,
                                         relationship_targets_for(comparison_a));
            results_.comparison_b_packed =
                Analyzer::analyze_packed(inputs.workspace.types(),
                                         *inputs.selection.type,
                                         comparison_b,
                                         primary_abi_,
                                         element_count,
                                         relationship_targets_for(comparison_b));
        }
        if (!access_fields.empty()) {
            auto const comparison_a_access{
                Analyzer::analyze_packed_access(*results_.comparison_a_packed,
                                                access_fields,
                                                primary_abi_,
                                                inputs.access_multiplicity)};
            auto const comparison_b_access{
                Analyzer::analyze_packed_access(*results_.comparison_b_packed,
                                                access_fields,
                                                primary_abi_,
                                                inputs.access_multiplicity)};
            results_.packed_access_comparison =
                Analyzer::compare_packed_access(comparison_a_access, comparison_b_access);
        }
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        if (reuse_primary && previous.baseline_soa.has_value() && previous.active_soa.has_value()) {
            results_.baseline_soa = std::move(previous.baseline_soa);
            results_.active_soa = std::move(previous.active_soa);
        } else {
            results_.baseline_soa = Analyzer::analyze_soa(inputs.workspace.types(),
                                                          *inputs.selection.type,
                                                          baseline,
                                                          primary_abi_,
                                                          inputs.workspace.default_capacity(),
                                                          inputs.soa_allocation_strategy);
            results_.active_soa = Analyzer::analyze_soa(inputs.workspace.types(),
                                                        *inputs.selection.type,
                                                        active,
                                                        primary_abi_,
                                                        inputs.workspace.default_capacity(),
                                                        inputs.soa_allocation_strategy);
        }
        auto comparison_target_soa{reuse_comparison_target &&
                                           previous.soa_target_comparison.has_value()
                                       ? std::move(previous.soa_target_comparison->second)
                                       : Analyzer::analyze_soa(inputs.workspace.types(),
                                                               *inputs.selection.type,
                                                               active,
                                                               comparison_abi_,
                                                               inputs.workspace.default_capacity(),
                                                               inputs.soa_allocation_strategy)};
        results_.soa_target_comparison =
            Analyzer::compare_soa_targets(*results_.active_soa, comparison_target_soa);
        std::vector<AccessIntent> access_columns;
        access_columns.reserve(inputs.selection.soa_access_columns.size());
        for (auto const& [name, operation] : inputs.selection.soa_access_columns) {
            access_columns.push_back({.name = name, .operation = operation});
        }
        if (!inputs.selection.soa_access_set_explicit && !inputs.selection.field.empty()) {
            access_columns.clear();
            access_columns.push_back(
                {.name = inputs.selection.field, .operation = inputs.access_operation});
        }
        if (!access_columns.empty()) {
            results_.soa_access_analysis =
                Analyzer::analyze_soa_access(*results_.active_soa,
                                             access_columns,
                                             primary_abi_,
                                             inputs.workspace.element_count(),
                                             inputs.access_multiplicity);
            auto const comparison_target_access{
                Analyzer::analyze_soa_access(comparison_target_soa,
                                             access_columns,
                                             comparison_abi_,
                                             inputs.workspace.element_count(),
                                             inputs.access_multiplicity)};
            results_.soa_target_access_comparison = Analyzer::compare_soa_access(
                *results_.soa_access_analysis, comparison_target_access);
            if (soa->equivalent_type.has_value() &&
                std::holds_alternative<RecordType>(
                    inputs.workspace.types().type(soa->equivalent_type->type).definition)) {
                auto const equivalent_record{
                    Analyzer::analyze_record(inputs.workspace.types(),
                                             soa->equivalent_type->type,
                                             primary_abi_,
                                             inputs.workspace.element_count())};
                auto const record_access{Analyzer::analyze_record_access(
                    equivalent_record, access_columns, primary_abi_, inputs.access_multiplicity)};
                results_.record_soa_access_comparison = Analyzer::compare_record_soa_access(
                    record_access, *results_.soa_access_analysis);
            }
        }
        if (reuse_primary) {
            results_.soa_variants = std::move(previous.soa_variants);
        } else {
            for (auto const& variant : inputs.workspace.variants()) {
                if (variant.id != LayoutWorkspace::baseline_variant_id) {
                    results_.soa_variants.emplace_back(
                        variant.id,
                        Analyzer::analyze_soa(inputs.workspace.types(),
                                              *inputs.selection.type,
                                              variant,
                                              primary_abi_,
                                              inputs.workspace.default_capacity(),
                                              inputs.soa_allocation_strategy));
                }
            }
        }
        if (reuse_variant_comparison && previous.comparison_a_soa.has_value() &&
            previous.comparison_b_soa.has_value()) {
            results_.comparison_a_soa = std::move(previous.comparison_a_soa);
            results_.comparison_b_soa = std::move(previous.comparison_b_soa);
        } else {
            results_.comparison_a_soa = Analyzer::analyze_soa(inputs.workspace.types(),
                                                              *inputs.selection.type,
                                                              comparison_a,
                                                              primary_abi_,
                                                              inputs.workspace.default_capacity(),
                                                              inputs.soa_allocation_strategy);
            results_.comparison_b_soa = Analyzer::analyze_soa(inputs.workspace.types(),
                                                              *inputs.selection.type,
                                                              comparison_b,
                                                              primary_abi_,
                                                              inputs.workspace.default_capacity(),
                                                              inputs.soa_allocation_strategy);
        }
        if (!access_columns.empty()) {
            auto const first_access{Analyzer::analyze_soa_access(*results_.comparison_a_soa,
                                                                 access_columns,
                                                                 primary_abi_,
                                                                 inputs.workspace.element_count(),
                                                                 inputs.access_multiplicity)};
            auto const second_access{Analyzer::analyze_soa_access(*results_.comparison_b_soa,
                                                                  access_columns,
                                                                  primary_abi_,
                                                                  inputs.workspace.element_count(),
                                                                  inputs.access_multiplicity)};
            results_.soa_access_comparison =
                Analyzer::compare_soa_access(first_access, second_access);
        }
    }
}

} // namespace ioj::layout
