#include <ioj/layout/planner_session.hpp>

#include <ioj/layout/document.hpp>

#include <codegen/schema.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/union_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
using namespace lispb::schema;

namespace {

auto comparison_source(TypeGraph const& types, TypeId const type) -> std::optional<TypeId> {
    auto const definition{types.definition(type)};
    auto const& definitions{types.definitions()};
    std::optional<ResolvedOccurrenceId> source;
    switch (definition.kind()) {
        case TypeDefinitionKind::linear_quantized:
            source =
                definitions.linear_quantized.source[definitions.linear_quantized.index(definition)];
            break;
        case TypeDefinitionKind::integer_varint:
            source =
                definitions.integer_varints.source[definitions.integer_varints.index(definition)];
            break;
        case TypeDefinitionKind::optional_sentinel:
            source = definitions.optional_sentinels
                         .source[definitions.optional_sentinels.index(definition)];
            break;
        case TypeDefinitionKind::optional_presence_bit:
            source = definitions.optional_presence_bits
                         .source[definitions.optional_presence_bits.index(definition)];
            break;
        default:
            return std::nullopt;
    }
    return types.occurrences().type[types.occurrences().index(*source)];
}

auto comparison_kind(TypeDefinitionKind const kind) -> TypeDefinitionKind {
    return kind == TypeDefinitionKind::optional_presence_bit ? TypeDefinitionKind::optional_sentinel
                                                             : kind;
}

void choose_comparison(TypeGraph const& types,
                       TypeId const selected,
                       std::optional<TypeIdentity>& comparison) {
    auto const source{comparison_source(types, selected)};
    auto const kind{comparison_kind(types.definition(selected).kind())};
    auto const compatible{[&](TypeId const candidate) {
        return candidate != selected &&
               comparison_kind(types.definition(candidate).kind()) == kind &&
               comparison_source(types, candidate) == source;
    }};
    if (comparison) {
        auto const candidate{types.find(*comparison)};
        if (candidate && compatible(*candidate)) {
            return;
        }
    }
    comparison.reset();
    auto const count{types.types().size()};
    for (std::uint32_t row{}; row < count; ++row) {
        auto const candidate{TypeId{row}};
        if (compatible(candidate)) {
            comparison = types.identity(candidate);
            return;
        }
    }
}

auto same_profile(AbiProfile const& first, AbiProfile const& second) -> bool {
    return first.name() == second.name() && first.identity() == second.identity() &&
           first.types() == second.types() && first.representations() == second.representations() &&
           first.memory_facts() == second.memory_facts() &&
           first.object_pointer_representation() == second.object_pointer_representation();
}

auto set_distribution_weight(PlannerAnalysisSession::DistributionTable& distributions,
                             DeclarationId const declaration,
                             std::string name,
                             std::uint64_t const weight) -> bool {
    auto const entry{distributions.find(declaration)};
    if (weight == 0) {
        if (entry == distributions.end()) {
            return false;
        }
        if (entry->second.erase(name) == 0) {
            return false;
        }
        if (entry->second.empty()) {
            distributions.erase(entry);
        }
        return true;
    }

    if (entry != distributions.end()) {
        auto const found{entry->second.find(name)};
        if (found != entry->second.end() && found->second == weight) {
            return false;
        }
    }
    distributions[declaration].insert_or_assign(std::move(name), weight);
    return true;
}

} // namespace

PlannerAnalysisSession::PlannerAnalysisSession(TypeGraph const& types) {
    inputs_.workspace = LayoutWorkspace{types};
}

auto PlannerAnalysisSession::inputs() const -> PlannerAnalysisInputs const& {
    return inputs_;
}
auto PlannerAnalysisSession::edit_inputs() -> PlannerAnalysisInputs& {
    ++inputs_revision_;
    return inputs_;
}

auto PlannerAnalysisSession::primary_abi() const -> AbiProfile const& {
    return primary_abi_;
}

auto PlannerAnalysisSession::set_external_type_facts(TypeId const type, TypeFacts facts)
    -> std::expected<bool, std::string> {
    auto const& node{type};
    if (!((inputs_.workspace.types().definition(node)).kind() ==
          lispb::schema::TypeDefinitionKind::external)) {
        return std::unexpected{"Manual facts can only be supplied for external types."};
    }
    if (auto const valid{validate_type_facts(facts)}; !valid) {
        return std::unexpected{valid.error()};
    }
    facts.origin = FactOrigin::manual_assumption;
    if (facts.provenance.empty()) {
        facts.provenance = "Entered in Memory Layout Planner";
    }
    auto profile{primary_abi_};
    profile.set(
        lispb::codegen::native_spelling(std::string{inputs_.workspace.types().strings().view(
            inputs_.workspace.types().cpp_spelling(node))}),
        facts);
    return set_primary_abi(std::move(profile));
}

auto PlannerAnalysisSession::comparison_abi() const -> AbiProfile const& {
    return comparison_abi_;
}

auto PlannerAnalysisSession::set_primary_abi(AbiProfile abi) -> bool {
    if (same_profile(primary_abi_, abi)) {
        return false;
    }
    primary_abi_ = std::move(abi);
    ++inputs_revision_;
    return true;
}

auto PlannerAnalysisSession::set_comparison_abi(AbiProfile abi) -> bool {
    if (same_profile(comparison_abi_, abi)) {
        return false;
    }
    comparison_abi_ = std::move(abi);
    ++inputs_revision_;
    return true;
}

auto PlannerAnalysisSession::union_distributions() const -> DistributionTable const& {
    return union_distributions_;
}

auto PlannerAnalysisSession::tagged_union_distributions() const -> DistributionTable const& {
    return tagged_union_distributions_;
}

auto PlannerAnalysisSession::union_distribution(DeclarationId const declaration) const
    -> DistributionWeights const* {
    auto const found{union_distributions_.find(declaration)};
    return found == union_distributions_.end() ? nullptr : &found->second;
}

auto PlannerAnalysisSession::tagged_union_distribution(DeclarationId const declaration) const
    -> DistributionWeights const* {
    auto const found{tagged_union_distributions_.find(declaration)};
    return found == tagged_union_distributions_.end() ? nullptr : &found->second;
}

auto PlannerAnalysisSession::set_union_distribution_weight(DeclarationId const declaration,
                                                           std::string name,
                                                           std::uint64_t const weight) -> bool {
    if (!set_distribution_weight(union_distributions_, declaration, std::move(name), weight)) {
        return false;
    }
    ++inputs_revision_;
    return true;
}

auto PlannerAnalysisSession::set_tagged_union_distribution_weight(DeclarationId const declaration,
                                                                  std::string name,
                                                                  std::uint64_t const weight)
    -> bool {
    if (!set_distribution_weight(
            tagged_union_distributions_, declaration, std::move(name), weight)) {
        return false;
    }
    ++inputs_revision_;
    return true;
}

auto PlannerAnalysisSession::clear_union_distribution(DeclarationId const declaration) -> bool {
    if (union_distributions_.erase(declaration) == 0) {
        return false;
    }
    ++inputs_revision_;
    return true;
}

auto PlannerAnalysisSession::clear_tagged_union_distribution(DeclarationId const declaration)
    -> bool {
    if (tagged_union_distributions_.erase(declaration) == 0) {
        return false;
    }
    ++inputs_revision_;
    return true;
}

void PlannerAnalysisSession::clear_distributions() {
    if (!union_distributions_.empty()) {
        union_distributions_.clear();
        ++inputs_revision_;
    }
    if (!tagged_union_distributions_.empty()) {
        tagged_union_distributions_.clear();
        ++inputs_revision_;
    }
    set_varint_distribution(std::nullopt, {}, false);
}

auto PlannerAnalysisSession::set_varint_distribution(
    std::optional<TypeIdentity> source,
    std::vector<IntegerVarintDistributionEntry> entries,
    bool const rows_present) -> bool {
    auto const same_entries{std::ranges::equal(
        varint_distribution_entries_, entries, [](auto const& first, auto const& second) {
            return first.value == second.value && first.weight == second.weight;
        })};
    if (source == varint_distribution_source_ && same_entries &&
        rows_present == varint_distribution_rows_present_) {
        return false;
    }
    varint_distribution_source_ = std::move(source);
    varint_distribution_entries_ = std::move(entries);
    varint_distribution_rows_present_ = rows_present;
    ++inputs_revision_;
    return true;
}

void PlannerAnalysisSession::replace_types(Document const& document,
                                           std::optional<TypeIdentity> selection) {
    inputs_.workspace.bind(document);
    inputs_.selection.reconcile(document.types(), std::move(selection));
    document_ = &document;
    document_revision_ = document.revision();
    ++inputs_revision_;
}

auto PlannerAnalysisSession::results() const -> PlannerAnalysisResults const& {
    return results_;
}

auto PlannerAnalysisSession::status(TypeId const type) -> LayoutStatus {
    if (status_inputs_revision_ != inputs_revision_) {
        status_cache_.clear();
        status_relationship_targets_ =
            Analyzer::derive_relationship_target_facts(inputs_.workspace.types(),
                                                       inputs_.workspace.active_variant(),
                                                       primary_abi_,
                                                       inputs_.workspace.default_capacity(),
                                                       inputs_.soa_allocation_strategy);
        status_inputs_revision_ = inputs_revision_;
    }
    auto const found{status_cache_.find(type.value)};
    if (found != status_cache_.end()) {
        return found->second;
    }
    auto const value{declaration_status(inputs_.workspace.types(),
                                        type,
                                        inputs_.workspace.active_variant(),
                                        primary_abi_,
                                        inputs_.workspace.default_capacity(),
                                        inputs_.workspace.element_count(),
                                        inputs_.soa_allocation_strategy,
                                        status_relationship_targets_)};
    status_cache_.emplace(type.value, value);
    return value;
}

void PlannerAnalysisSession::validate_comparison_variants() {
    if (inputs_.workspace.variant(inputs_.comparison_a_variant_id) == nullptr) {
        inputs_.comparison_a_variant_id = VariantId{};
    }
    if (inputs_.comparison_b_follows_active) {
        inputs_.comparison_b_variant_id = inputs_.workspace.active_variant_id();
    } else if (inputs_.workspace.variant(inputs_.comparison_b_variant_id) == nullptr) {
        inputs_.comparison_b_variant_id = VariantId{};
    }
}

auto PlannerAnalysisSession::refresh(Document* document) -> bool {
    if (document && (document_ != document || document_revision_ != document->revision())) {
        replace_types(*document, std::nullopt);
    }
    if (cached_inputs_revision_ == inputs_revision_) {
        return false;
    }
    validate_comparison_variants();

    auto const& types{inputs_.workspace.types()};
    auto const selected{inputs_.selection.type};
    auto const kind{selected ? types.definition(*selected).kind() : TypeDefinitionKind{}};
    if (selected && kind == TypeDefinitionKind::linear_quantized) {
        choose_comparison(types, *selected, inputs_.quantized_comparison_type);
    } else {
        inputs_.quantized_comparison_type.reset();
    }
    if (selected && kind == TypeDefinitionKind::integer_varint) {
        choose_comparison(types, *selected, inputs_.varint_comparison_type);
    } else {
        inputs_.varint_comparison_type.reset();
    }
    if (selected && (kind == TypeDefinitionKind::optional_sentinel ||
                     kind == TypeDefinitionKind::optional_presence_bit)) {
        choose_comparison(types, *selected, inputs_.optional_comparison_type);
    } else {
        inputs_.optional_comparison_type.reset();
    }

    results_ = {};
    cached_inputs_revision_ = inputs_revision_;
    if (inputs_.selection.type) {
        analyze_selected(document);
    }
    if (document) {
        std::set<std::string> errors;
        auto const collect_diagnostics{[&errors](std::span<Diagnostic const> diagnostics) {
            for (auto const& diagnostic : diagnostics) {
                if (diagnostic.severity == DiagnosticSeverity::error) {
                    errors.insert(diagnostic.message);
                }
            }
        }};
        auto const collect{[&collect_diagnostics](auto const& result) {
            if (result) {
                collect_diagnostics(result->diagnostics);
            }
        }};
        if (results_.external_type) {
            collect_diagnostics(results_.external_type->physical.diagnostics);
        }
        collect(results_.enum_domain);
        collect(results_.enum_target_comparison);
        collect(results_.integer_scalar_analysis);
        collect(results_.linear_quantized_comparison);
        collect(results_.integer_varint_analysis);
        collect(results_.integer_varint_comparison);
        collect(results_.integer_varint_distribution);
        collect(results_.integer_varint_distribution_comparison);
        collect(results_.fixed_point_analysis);
        collect(results_.mini_float_analysis);
        collect(results_.optional_sentinel_analysis);
        collect(results_.optional_presence_bit_analysis);
        collect(results_.optional_encoding_comparison);
        collect(results_.baseline_packed);
        collect(results_.active_packed);
        collect(results_.packed_target_comparison);
        collect(results_.packed_access_analysis);
        collect(results_.packed_target_access_comparison);
        collect(results_.packed_access_comparison);
        collect(results_.baseline_soa);
        collect(results_.active_soa);
        collect(results_.soa_target_comparison);
        collect(results_.soa_access_analysis);
        collect(results_.soa_target_access_comparison);
        collect(results_.record_soa_access_comparison);
        collect(results_.soa_access_comparison);
        collect(results_.comparison_a_packed);
        collect(results_.comparison_b_packed);
        collect(results_.record_analysis);
        collect(results_.record_target_comparison);
        collect(results_.record_access_analysis);
        collect(results_.record_target_access_comparison);
        collect(results_.union_analysis);
        collect(results_.union_target_comparison);
        collect(results_.union_distribution_analysis);
        collect(results_.union_target_distribution_comparison);
        collect(results_.tagged_union_analysis);
        collect(results_.tagged_union_target_comparison);
        collect(results_.tagged_union_distribution_analysis);
        collect(results_.tagged_union_target_distribution_comparison);
        collect(results_.comparison_a_soa);
        collect(results_.comparison_b_soa);
        for (auto const& [id, result] : results_.packed_variants) {
            collect_diagnostics(result.diagnostics);
        }
        for (auto const& [id, result] : results_.soa_variants) {
            collect_diagnostics(result.diagnostics);
        }
        for (auto const& message : errors) {
            document->record_error(message);
        }
    }
    return true;
}

void PlannerAnalysisSession::analyze_selected(Document const* document) {
    auto const& definition{inputs_.workspace.types().definition(*inputs_.selection.type)};
    auto const& baseline{*inputs_.workspace.variant(VariantId{})};
    auto const& active{inputs_.workspace.active_variant()};
    auto const& comparison_a{*inputs_.workspace.variant(inputs_.comparison_a_variant_id)};
    auto const& comparison_b{*inputs_.workspace.variant(inputs_.comparison_b_variant_id)};
    auto const element_count{inputs_.workspace.element_count()};
    auto const relationship_targets_for{[this](ResolvedVariant const& variant) {
        return Analyzer::derive_relationship_target_facts(inputs_.workspace.types(),
                                                          variant,
                                                          primary_abi_,
                                                          inputs_.workspace.default_capacity(),
                                                          inputs_.soa_allocation_strategy);
    }};
    auto const relationship_targets_for_profile{
        [this](ResolvedVariant const& variant, AbiProfile const& abi) {
            return Analyzer::derive_relationship_target_facts(inputs_.workspace.types(),
                                                              variant,
                                                              abi,
                                                              inputs_.workspace.default_capacity(),
                                                              inputs_.soa_allocation_strategy);
        }};
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::external)) {
        results_.external_type = analyze_external_type(inputs_.workspace.types(),
                                                       *inputs_.selection.type,
                                                       active,
                                                       primary_abi_,
                                                       inputs_.workspace.default_capacity(),
                                                       inputs_.soa_allocation_strategy);
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::enumeration)) {
        results_.enum_domain = Analyzer::analyze_enum(
            inputs_.workspace.types(), *inputs_.selection.type, primary_abi_, element_count);

        auto comparison_domain{Analyzer::analyze_enum(
            inputs_.workspace.types(), *inputs_.selection.type, comparison_abi_, element_count)};
        results_.enum_target_comparison =
            Analyzer::compare_enum_targets(*results_.enum_domain, comparison_domain);
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::integer_scalar)) {
        auto const active_targets{relationship_targets_for(active)};
        results_.integer_scalar_analysis = Analyzer::analyze_integer_scalar(
            inputs_.workspace.types(), *inputs_.selection.type, active_targets);
        auto const comparison_a_targets{relationship_targets_for(comparison_a)};
        auto const comparison_b_targets{relationship_targets_for(comparison_b)};
        results_.integer_scalar_capacity_comparison =
            Analyzer::compare_integer_scalar_capacity(inputs_.workspace.types(),
                                                      *inputs_.selection.type,
                                                      comparison_a_targets,
                                                      comparison_b_targets);
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::linear_quantized)) {
        results_.linear_quantized_analysis =
            Analyzer::analyze_linear_quantized(inputs_.workspace.types(), *inputs_.selection.type);
        if (inputs_.quantized_comparison_type.has_value()) {
            auto const comparison_type{
                inputs_.workspace.types().find(*inputs_.quantized_comparison_type)};
            if (comparison_type.has_value()) {
                results_.linear_quantized_comparison =
                    Analyzer::compare_linear_quantized(inputs_.workspace.types(),
                                                       *inputs_.selection.type,
                                                       *comparison_type,
                                                       element_count);
            }
        }
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::integer_varint)) {
        results_.integer_varint_analysis = Analyzer::analyze_integer_varint(
            inputs_.workspace.types(), *inputs_.selection.type, element_count);
        if (varint_distribution_source_ ==
            inputs_.workspace.types().identity(
                inputs_.workspace.types()
                    .occurrences()
                    .type[inputs_.workspace.types().occurrences().index(
                        inputs_.workspace.types().definitions().integer_varints.source
                            [inputs_.workspace.types().definitions().integer_varints.index(
                                definition)])])) {
            results_.integer_varint_distribution =
                Analyzer::analyze_integer_varint_distribution(inputs_.workspace.types(),
                                                              *inputs_.selection.type,
                                                              varint_distribution_entries_,
                                                              element_count);
        }
        if (inputs_.varint_comparison_type.has_value()) {
            auto const comparison_type{
                inputs_.workspace.types().find(*inputs_.varint_comparison_type)};
            if (comparison_type.has_value()) {
                results_.integer_varint_comparison =
                    Analyzer::compare_integer_varint(inputs_.workspace.types(),
                                                     *inputs_.selection.type,
                                                     *comparison_type,
                                                     element_count);
                if (varint_distribution_rows_present_ &&
                    results_.integer_varint_distribution.has_value()) {
                    results_.integer_varint_distribution_comparison =
                        Analyzer::compare_integer_varint_distribution(inputs_.workspace.types(),
                                                                      *inputs_.selection.type,
                                                                      *comparison_type,
                                                                      varint_distribution_entries_,
                                                                      element_count);
                }
            }
        }
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::fixed_point)) {
        results_.fixed_point_analysis = Analyzer::analyze_fixed_point(
            inputs_.workspace.types(), *inputs_.selection.type, element_count);
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::mini_float)) {
        results_.mini_float_analysis = Analyzer::analyze_mini_float(
            inputs_.workspace.types(), *inputs_.selection.type, element_count);
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::optional_sentinel)) {
        results_.optional_sentinel_analysis = Analyzer::analyze_optional_sentinel(
            inputs_.workspace.types(), *inputs_.selection.type, element_count);
        if (inputs_.optional_comparison_type.has_value()) {
            auto const comparison_type{
                inputs_.workspace.types().find(*inputs_.optional_comparison_type)};
            if (comparison_type.has_value()) {
                results_.optional_encoding_comparison =
                    Analyzer::compare_optional_encodings(inputs_.workspace.types(),
                                                         *inputs_.selection.type,
                                                         *comparison_type,
                                                         element_count);
            }
        }
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::optional_presence_bit)) {
        results_.optional_presence_bit_analysis = Analyzer::analyze_optional_presence_bit(
            inputs_.workspace.types(), *inputs_.selection.type, element_count);
        if (inputs_.optional_comparison_type.has_value()) {
            auto const comparison_type{
                inputs_.workspace.types().find(*inputs_.optional_comparison_type)};
            if (comparison_type.has_value()) {
                results_.optional_encoding_comparison =
                    Analyzer::compare_optional_encodings(inputs_.workspace.types(),
                                                         *inputs_.selection.type,
                                                         *comparison_type,
                                                         element_count);
            }
        }
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::record)) {
        results_.record_analysis = Analyzer::analyze_record(
            inputs_.workspace.types(), *inputs_.selection.type, primary_abi_, element_count);

        auto comparison_record{Analyzer::analyze_record(
            inputs_.workspace.types(), *inputs_.selection.type, comparison_abi_, element_count)};
        results_.record_target_comparison =
            Analyzer::compare_record_targets(*results_.record_analysis, comparison_record);
        std::vector<AccessIntent> access_members;
        access_members.reserve(inputs_.selection.record_access_members.size());
        for (auto const& [name, operation] : inputs_.selection.record_access_members) {
            access_members.push_back({.name = name, .operation = operation});
        }
        if (!inputs_.selection.record_access_set_explicit && !inputs_.selection.field.empty()) {
            access_members.clear();
            access_members.push_back(
                {.name = inputs_.selection.field, .operation = inputs_.access_operation});
        }
        if (!access_members.empty()) {
            results_.record_access_analysis =
                Analyzer::analyze_record_access(*results_.record_analysis,
                                                access_members,
                                                primary_abi_,
                                                inputs_.access_multiplicity);
            auto const comparison_access{Analyzer::analyze_record_access(
                comparison_record, access_members, comparison_abi_, inputs_.access_multiplicity)};
            results_.record_target_access_comparison = Analyzer::compare_record_access(
                *results_.record_access_analysis, comparison_access);
        }
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::union_type)) {
        results_.union_analysis = Analyzer::analyze_union(
            inputs_.workspace.types(), *inputs_.selection.type, primary_abi_, element_count);

        auto comparison_target_union{Analyzer::analyze_union(
            inputs_.workspace.types(), *inputs_.selection.type, comparison_abi_, element_count)};
        results_.union_target_comparison =
            Analyzer::compare_union_targets(*results_.union_analysis, comparison_target_union);
        auto const declaration{document != nullptr
                                   ? document->find_declaration(inputs_.workspace.types().identity(
                                         *inputs_.selection.type))
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
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::tagged_union)) {
        results_.tagged_union_analysis = Analyzer::analyze_tagged_union(
            inputs_.workspace.types(), *inputs_.selection.type, primary_abi_, element_count);

        auto comparison_target_tagged{Analyzer::analyze_tagged_union(
            inputs_.workspace.types(), *inputs_.selection.type, comparison_abi_, element_count)};
        results_.tagged_union_target_comparison = Analyzer::compare_tagged_union_targets(
            *results_.tagged_union_analysis, comparison_target_tagged);
        auto const declaration{document != nullptr
                                   ? document->find_declaration(inputs_.workspace.types().identity(
                                         *inputs_.selection.type))
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
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::packed)) {
        auto const baseline_targets{relationship_targets_for(baseline)};
        auto const active_targets{relationship_targets_for(active)};
        results_.baseline_packed = Analyzer::analyze_packed(inputs_.workspace.types(),
                                                            *inputs_.selection.type,
                                                            baseline,
                                                            primary_abi_,
                                                            element_count,
                                                            baseline_targets);
        results_.active_packed = Analyzer::analyze_packed(inputs_.workspace.types(),
                                                          *inputs_.selection.type,
                                                          active,
                                                          primary_abi_,
                                                          element_count,
                                                          active_targets);

        auto comparison_target_packed{
            Analyzer::analyze_packed(inputs_.workspace.types(),
                                     *inputs_.selection.type,
                                     active,
                                     comparison_abi_,
                                     element_count,
                                     relationship_targets_for_profile(active, comparison_abi_))};
        results_.packed_target_comparison =
            Analyzer::compare_packed_targets(*results_.active_packed, comparison_target_packed);
        std::vector<AccessIntent> access_fields;
        access_fields.reserve(inputs_.selection.packed_access_fields.size());
        for (auto const& [name, operation] : inputs_.selection.packed_access_fields) {
            access_fields.push_back({.name = name, .operation = operation});
        }
        if (!inputs_.selection.packed_access_set_explicit && !inputs_.selection.field.empty()) {
            access_fields.clear();
            auto const selected{std::ranges::find(results_.active_packed->fields,
                                                  inputs_.selection.field,
                                                  &PackedFieldAnalysis::name)};
            if (selected != results_.active_packed->fields.end() && !selected->reserved) {
                access_fields.push_back(
                    {.name = inputs_.selection.field, .operation = inputs_.access_operation});
            }
        }
        if (!access_fields.empty()) {
            results_.packed_access_analysis = Analyzer::analyze_packed_access(
                *results_.active_packed, access_fields, primary_abi_, inputs_.access_multiplicity);
            auto const comparison_target_access{
                Analyzer::analyze_packed_access(comparison_target_packed,
                                                access_fields,
                                                comparison_abi_,
                                                inputs_.access_multiplicity)};
            results_.packed_target_access_comparison = Analyzer::compare_packed_access(
                *results_.packed_access_analysis, comparison_target_access);
        }
        for (auto const& variant : inputs_.workspace.variants()) {
            if (variant.id != VariantId{}) {
                auto const targets{relationship_targets_for(variant)};
                results_.packed_variants.emplace_back(
                    variant.id,
                    Analyzer::analyze_packed(inputs_.workspace.types(),
                                             *inputs_.selection.type,
                                             variant,
                                             primary_abi_,
                                             element_count,
                                             targets));
            }
        }

        results_.comparison_a_packed =
            Analyzer::analyze_packed(inputs_.workspace.types(),
                                     *inputs_.selection.type,
                                     comparison_a,
                                     primary_abi_,
                                     element_count,
                                     relationship_targets_for(comparison_a));
        results_.comparison_b_packed =
            Analyzer::analyze_packed(inputs_.workspace.types(),
                                     *inputs_.selection.type,
                                     comparison_b,
                                     primary_abi_,
                                     element_count,
                                     relationship_targets_for(comparison_b));

        if (!access_fields.empty()) {
            auto const comparison_a_access{
                Analyzer::analyze_packed_access(*results_.comparison_a_packed,
                                                access_fields,
                                                primary_abi_,
                                                inputs_.access_multiplicity)};
            auto const comparison_b_access{
                Analyzer::analyze_packed_access(*results_.comparison_b_packed,
                                                access_fields,
                                                primary_abi_,
                                                inputs_.access_multiplicity)};
            results_.packed_access_comparison =
                Analyzer::compare_packed_access(comparison_a_access, comparison_b_access);
        }
    } else if (auto const soa{((definition).kind() == lispb::schema::TypeDefinitionKind::soa
                                   ? std::optional{definition}
                                   : std::nullopt)};
               static_cast<bool>(soa) &&
               inputs_.workspace.types()
                       .definitions()
                       .soas.backend[inputs_.workspace.types().definitions().soas.index(*soa)] ==
                   lispb::codegen::SoaBackend::standard_library) {
        results_.baseline_soa = Analyzer::analyze_soa(inputs_.workspace.types(),
                                                      *inputs_.selection.type,
                                                      baseline,
                                                      primary_abi_,
                                                      inputs_.workspace.default_capacity(),
                                                      inputs_.soa_allocation_strategy);
        results_.active_soa = Analyzer::analyze_soa(inputs_.workspace.types(),
                                                    *inputs_.selection.type,
                                                    active,
                                                    primary_abi_,
                                                    inputs_.workspace.default_capacity(),
                                                    inputs_.soa_allocation_strategy);

        auto comparison_target_soa{Analyzer::analyze_soa(inputs_.workspace.types(),
                                                         *inputs_.selection.type,
                                                         active,
                                                         comparison_abi_,
                                                         inputs_.workspace.default_capacity(),
                                                         inputs_.soa_allocation_strategy)};
        results_.soa_target_comparison =
            Analyzer::compare_soa_targets(*results_.active_soa, comparison_target_soa);
        std::vector<AccessIntent> access_columns;
        access_columns.reserve(inputs_.selection.soa_access_columns.size());
        for (auto const& [name, operation] : inputs_.selection.soa_access_columns) {
            access_columns.push_back({.name = name, .operation = operation});
        }
        if (!inputs_.selection.soa_access_set_explicit && !inputs_.selection.field.empty()) {
            access_columns.clear();
            access_columns.push_back(
                {.name = inputs_.selection.field, .operation = inputs_.access_operation});
        }
        if (!access_columns.empty()) {
            results_.soa_access_analysis =
                Analyzer::analyze_soa_access(*results_.active_soa,
                                             access_columns,
                                             primary_abi_,
                                             inputs_.workspace.element_count(),
                                             inputs_.access_multiplicity);
            auto const comparison_target_access{
                Analyzer::analyze_soa_access(comparison_target_soa,
                                             access_columns,
                                             comparison_abi_,
                                             inputs_.workspace.element_count(),
                                             inputs_.access_multiplicity)};
            results_.soa_target_access_comparison = Analyzer::compare_soa_access(
                *results_.soa_access_analysis, comparison_target_access);
            if (lispb::schema::is_valid(
                    inputs_.workspace.types().definitions().soas.equivalent_type
                        [inputs_.workspace.types().definitions().soas.index(*soa)]) &&
                ((inputs_.workspace.types().definition(
                      inputs_.workspace.types()
                          .occurrences()
                          .type[inputs_.workspace.types().occurrences().index(
                              inputs_.workspace.types().definitions().soas.equivalent_type
                                  [inputs_.workspace.types().definitions().soas.index(*soa)])]))
                     .kind() == lispb::schema::TypeDefinitionKind::record)) {
                auto const equivalent_record{Analyzer::analyze_record(
                    inputs_.workspace.types(),
                    inputs_.workspace.types()
                        .occurrences()
                        .type[inputs_.workspace.types().occurrences().index(
                            inputs_.workspace.types().definitions().soas.equivalent_type
                                [inputs_.workspace.types().definitions().soas.index(*soa)])],
                    primary_abi_,
                    inputs_.workspace.element_count())};
                auto const record_access{Analyzer::analyze_record_access(
                    equivalent_record, access_columns, primary_abi_, inputs_.access_multiplicity)};
                results_.record_soa_access_comparison = Analyzer::compare_record_soa_access(
                    record_access, *results_.soa_access_analysis);
            }
        }
        for (auto const& variant : inputs_.workspace.variants()) {
            if (variant.id != VariantId{}) {
                results_.soa_variants.emplace_back(
                    variant.id,
                    Analyzer::analyze_soa(inputs_.workspace.types(),
                                          *inputs_.selection.type,
                                          variant,
                                          primary_abi_,
                                          inputs_.workspace.default_capacity(),
                                          inputs_.soa_allocation_strategy));
            }
        }

        results_.comparison_a_soa = Analyzer::analyze_soa(inputs_.workspace.types(),
                                                          *inputs_.selection.type,
                                                          comparison_a,
                                                          primary_abi_,
                                                          inputs_.workspace.default_capacity(),
                                                          inputs_.soa_allocation_strategy);
        results_.comparison_b_soa = Analyzer::analyze_soa(inputs_.workspace.types(),
                                                          *inputs_.selection.type,
                                                          comparison_b,
                                                          primary_abi_,
                                                          inputs_.workspace.default_capacity(),
                                                          inputs_.soa_allocation_strategy);

        if (!access_columns.empty()) {
            auto const first_access{Analyzer::analyze_soa_access(*results_.comparison_a_soa,
                                                                 access_columns,
                                                                 primary_abi_,
                                                                 inputs_.workspace.element_count(),
                                                                 inputs_.access_multiplicity)};
            auto const second_access{Analyzer::analyze_soa_access(*results_.comparison_b_soa,
                                                                  access_columns,
                                                                  primary_abi_,
                                                                  inputs_.workspace.element_count(),
                                                                  inputs_.access_multiplicity)};
            results_.soa_access_comparison =
                Analyzer::compare_soa_access(first_access, second_access);
        }
    }
}

} // namespace ioj::layout
