#include <ioj/layout/planner_session.hpp>

#include <algorithm>
#include <iterator>
#include <limits>
#include <ranges>
#include <set>
#include <utility>

namespace ioj::layout {
using namespace lispb::schema;

namespace {

auto same_profile(AbiProfile const& first, AbiProfile const& second) -> bool {
    return first.name() == second.name() && first.identity() == second.identity() &&
           first.types() == second.types() && first.representations() == second.representations() &&
           first.memory_facts() == second.memory_facts() &&
           first.object_pointer_representation() == second.object_pointer_representation();
}

auto reconcile_weight_keys(std::map<std::string, std::uint64_t>& weights,
                           std::vector<std::string> const& previous_keys,
                           std::vector<std::string> const& current_keys) -> bool {
    std::set<std::string, std::less<>> previous{previous_keys.begin(), previous_keys.end()};
    std::set<std::string, std::less<>> current{current_keys.begin(), current_keys.end()};
    std::vector<std::string> removed;
    std::vector<std::string> added;
    std::ranges::set_difference(previous, current, std::back_inserter(removed));
    std::ranges::set_difference(current, previous, std::back_inserter(added));

    auto changed{false};
    if (removed.size() == 1 && added.size() == 1) {
        auto const previous_weight{weights.find(removed.front())};
        if (previous_weight != weights.end()) {
            auto const weight{previous_weight->second};
            weights.erase(previous_weight);
            weights.insert_or_assign(added.front(), weight);
            changed = true;
        }
    }

    auto const previous_size{weights.size()};
    std::erase_if(weights, [&](auto const& entry) { return !current.contains(entry.first); });
    return changed || weights.size() != previous_size;
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

PlannerAnalysisSession::PlannerAnalysisSession(TypeGraph types) {
    inputs.workspace = LayoutWorkspace{std::move(types)};
}

auto PlannerAnalysisSession::primary_abi() const -> AbiProfile const& {
    return primary_abi_;
}

auto PlannerAnalysisSession::comparison_abi() const -> AbiProfile const& {
    return comparison_abi_;
}

auto PlannerAnalysisSession::set_primary_abi(AbiProfile abi) -> bool {
    if (same_profile(primary_abi_, abi)) {
        return false;
    }
    primary_abi_ = std::move(abi);
    ++target_profile_revision_;
    return true;
}

auto PlannerAnalysisSession::set_comparison_abi(AbiProfile abi) -> bool {
    if (same_profile(comparison_abi_, abi)) {
        return false;
    }
    comparison_abi_ = std::move(abi);
    ++comparison_target_profile_revision_;
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
    ++union_distribution_revision_;
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
    ++tagged_distribution_revision_;
    return true;
}

auto PlannerAnalysisSession::clear_union_distribution(DeclarationId const declaration) -> bool {
    if (union_distributions_.erase(declaration) == 0) {
        return false;
    }
    ++union_distribution_revision_;
    return true;
}

auto PlannerAnalysisSession::clear_tagged_union_distribution(DeclarationId const declaration)
    -> bool {
    if (tagged_union_distributions_.erase(declaration) == 0) {
        return false;
    }
    ++tagged_distribution_revision_;
    return true;
}

void PlannerAnalysisSession::clear_distributions() {
    if (!union_distributions_.empty()) {
        union_distributions_.clear();
        ++union_distribution_revision_;
    }
    if (!tagged_union_distributions_.empty()) {
        tagged_union_distributions_.clear();
        ++tagged_distribution_revision_;
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
    ++varint_distribution_revision_;
    return true;
}

void PlannerAnalysisSession::replace_types(EditableSchemaDocument const& document,
                                           std::optional<TypeIdentity> selection) {
    auto raw_weights_changed{false};
    for (auto entry = union_distributions_.begin(); entry != union_distributions_.end();) {
        auto const* declaration{document.declaration(entry->first)};
        auto const* schema{document.union_schema(entry->first)};
        if (declaration == nullptr || schema == nullptr) {
            entry = union_distributions_.erase(entry);
            raw_weights_changed = true;
            continue;
        }

        std::vector<std::string> previous_keys;
        if (auto const previous_type{inputs.workspace.types().find(declaration->identity)};
            previous_type.has_value()) {
            auto const* previous_union{
                std::get_if<UnionType>(&inputs.workspace.types().type(*previous_type).definition)};
            if (previous_union != nullptr) {
                previous_keys.reserve(previous_union->alternatives.size());
                for (auto const& alternative : previous_union->alternatives) {
                    previous_keys.push_back(alternative.name);
                }
            }
        }
        std::vector<std::string> current_keys;
        current_keys.reserve(schema->alternatives.size());
        for (auto const& alternative : schema->alternatives) {
            current_keys.push_back(alternative.name);
        }
        raw_weights_changed |= reconcile_weight_keys(entry->second, previous_keys, current_keys);
        ++entry;
    }
    if (raw_weights_changed) {
        ++union_distribution_revision_;
    }

    auto tagged_weights_changed{false};
    for (auto entry = tagged_union_distributions_.begin();
         entry != tagged_union_distributions_.end();) {
        auto const* declaration{document.declaration(entry->first)};
        auto const* schema{document.tagged_union_schema(entry->first)};
        if (declaration == nullptr || schema == nullptr) {
            entry = tagged_union_distributions_.erase(entry);
            tagged_weights_changed = true;
            continue;
        }

        std::vector<std::string> previous_keys;
        if (auto const previous_type{inputs.workspace.types().find(declaration->identity)};
            previous_type.has_value()) {
            auto const* previous_union{std::get_if<TaggedUnionType>(
                &inputs.workspace.types().type(*previous_type).definition)};
            if (previous_union != nullptr) {
                previous_keys.reserve(previous_union->alternatives.size());
                for (auto const& alternative : previous_union->alternatives) {
                    previous_keys.push_back(alternative.tag);
                }
            }
        }
        std::vector<std::string> current_keys;
        current_keys.reserve(schema->alternatives.size());
        for (auto const& alternative : schema->alternatives) {
            current_keys.push_back(alternative.tag);
        }
        tagged_weights_changed |= reconcile_weight_keys(entry->second, previous_keys, current_keys);
        ++entry;
    }
    if (tagged_weights_changed) {
        ++tagged_distribution_revision_;
    }

    inputs.workspace.replace_types(document.types());
    inputs.selection.reconcile(inputs.workspace.types(), std::move(selection), &document);
}

auto PlannerAnalysisSession::results() const -> PlannerAnalysisResults const& {
    return results_;
}

auto PlannerAnalysisSession::status(TypeId const type) -> LayoutStatus {
    if (status_revision_ != inputs.workspace.revision() ||
        status_profile_revision_ != target_profile_revision_ ||
        status_allocation_strategy_ != inputs.soa_allocation_strategy) {
        status_cache_.clear();
        status_relationship_targets_ =
            Analyzer::derive_relationship_target_facts(inputs.workspace.types(),
                                                       inputs.workspace.active_variant(),
                                                       primary_abi_,
                                                       inputs.workspace.default_capacity(),
                                                       inputs.soa_allocation_strategy);
        status_revision_ = inputs.workspace.revision();
        status_profile_revision_ = target_profile_revision_;
        status_allocation_strategy_ = inputs.soa_allocation_strategy;
    }
    auto const found{status_cache_.find(type.value)};
    if (found != status_cache_.end()) {
        return found->second;
    }
    auto const value{declaration_status(inputs.workspace.types(),
                                        type,
                                        inputs.workspace.active_variant(),
                                        primary_abi_,
                                        inputs.workspace.default_capacity(),
                                        inputs.workspace.element_count(),
                                        inputs.soa_allocation_strategy,
                                        status_relationship_targets_)};
    status_cache_.emplace(type.value, value);
    return value;
}

void PlannerAnalysisSession::validate_comparison_variants() {
    if (inputs.workspace.variant(inputs.comparison_a_variant_id) == nullptr) {
        inputs.comparison_a_variant_id = LayoutWorkspace::baseline_variant_id;
    }
    if (inputs.comparison_b_follows_active) {
        inputs.comparison_b_variant_id = inputs.workspace.active_variant_id();
    } else if (inputs.workspace.variant(inputs.comparison_b_variant_id) == nullptr) {
        inputs.comparison_b_variant_id = LayoutWorkspace::baseline_variant_id;
    }
}

auto PlannerAnalysisSession::refresh(EditableSchemaDocument const* document) -> bool {
    validate_comparison_variants();

    if (inputs.selection.type.has_value()) {
        auto const* selected_quantized{std::get_if<LinearQuantizedType>(
            &inputs.workspace.types().type(*inputs.selection.type).definition)};
        if (selected_quantized != nullptr) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                if (candidate == *inputs.selection.type) {
                    return false;
                }
                auto const* candidate_quantized{std::get_if<LinearQuantizedType>(
                    &inputs.workspace.types().type(candidate).definition)};
                return candidate_quantized != nullptr &&
                       candidate_quantized->source.type == selected_quantized->source.type;
            };

            std::optional<TypeId> comparison_id;
            if (inputs.quantized_comparison_type.has_value()) {
                comparison_id = inputs.workspace.types().find(*inputs.quantized_comparison_type);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                inputs.quantized_comparison_type.reset();
                auto const types{inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        inputs.quantized_comparison_type = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            inputs.quantized_comparison_type.reset();
        }
    } else {
        inputs.quantized_comparison_type.reset();
    }

    if (inputs.selection.type.has_value()) {
        auto const* selected_varint{std::get_if<IntegerVarintType>(
            &inputs.workspace.types().type(*inputs.selection.type).definition)};
        if (selected_varint != nullptr) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                if (candidate == *inputs.selection.type) {
                    return false;
                }
                auto const* candidate_varint{std::get_if<IntegerVarintType>(
                    &inputs.workspace.types().type(candidate).definition)};
                return candidate_varint != nullptr &&
                       candidate_varint->source.type == selected_varint->source.type;
            };

            std::optional<TypeId> comparison_id;
            if (inputs.varint_comparison_type.has_value()) {
                comparison_id = inputs.workspace.types().find(*inputs.varint_comparison_type);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                inputs.varint_comparison_type.reset();
                auto const types{inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        inputs.varint_comparison_type = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            inputs.varint_comparison_type.reset();
        }
    } else {
        inputs.varint_comparison_type.reset();
    }

    auto optional_source = [&](TypeId const type) -> std::optional<TypeId> {
        auto const& definition{inputs.workspace.types().type(type).definition};
        if (auto const* sentinel{std::get_if<OptionalSentinelType>(&definition)}) {
            return sentinel->source.type;
        }
        if (auto const* presence{std::get_if<OptionalPresenceBitType>(&definition)}) {
            return presence->source.type;
        }
        return std::nullopt;
    };
    if (inputs.selection.type.has_value()) {
        auto const selected_source{optional_source(*inputs.selection.type)};
        if (selected_source.has_value()) {
            auto valid_comparison_type = [&](TypeId const candidate) {
                return candidate != *inputs.selection.type &&
                       optional_source(candidate) == selected_source;
            };

            std::optional<TypeId> comparison_id;
            if (inputs.optional_comparison_type.has_value()) {
                comparison_id = inputs.workspace.types().find(*inputs.optional_comparison_type);
            }
            if (!comparison_id.has_value() || !valid_comparison_type(*comparison_id)) {
                inputs.optional_comparison_type.reset();
                auto const types{inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    if (valid_comparison_type(candidate)) {
                        inputs.optional_comparison_type = types[index].identity;
                        break;
                    }
                }
            }
        } else {
            inputs.optional_comparison_type.reset();
        }
    } else {
        inputs.optional_comparison_type.reset();
    }

    auto const same_declaration{cached_revision_ == inputs.workspace.revision() &&
                                cached_type_ == inputs.selection.type};
    auto const same_targets{cached_target_profile_revision_ == target_profile_revision_ &&
                            cached_comparison_target_profile_revision_ ==
                                comparison_target_profile_revision_};
    auto const same_access{
        cached_access_operation_ == inputs.access_operation &&
        cached_access_multiplicity_ == inputs.access_multiplicity &&
        cached_selected_field_ == inputs.selection.field &&
        cached_packed_access_fields_ == inputs.selection.packed_access_fields &&
        cached_packed_access_set_explicit_ == inputs.selection.packed_access_set_explicit &&
        cached_record_access_members_ == inputs.selection.record_access_members &&
        cached_record_access_set_explicit_ == inputs.selection.record_access_set_explicit &&
        cached_soa_access_columns_ == inputs.selection.soa_access_columns &&
        cached_soa_access_set_explicit_ == inputs.selection.soa_access_set_explicit};
    auto const same_variants{cached_comparison_a_variant_id_ == inputs.comparison_a_variant_id &&
                             cached_comparison_b_variant_id_ == inputs.comparison_b_variant_id};
    if (same_declaration && inputs.selection.type.has_value()) {
        auto const& definition{inputs.workspace.types().type(*inputs.selection.type).definition};
        if (std::holds_alternative<EnumType>(definition) && same_targets) {
            return false;
        }
        if (std::holds_alternative<RecordType>(definition) && same_targets && same_access) {
            return false;
        }
        if (std::holds_alternative<UnionType>(definition) && same_targets &&
            cached_union_distribution_revision_ == union_distribution_revision_) {
            return false;
        }
        if (std::holds_alternative<TaggedUnionType>(definition) && same_targets &&
            cached_tagged_distribution_revision_ == tagged_distribution_revision_) {
            return false;
        }
        if (std::holds_alternative<PackedType>(definition) && same_targets && same_access &&
            same_variants && cached_soa_allocation_strategy_ == inputs.soa_allocation_strategy) {
            return false;
        }
        if (std::holds_alternative<SoaType>(definition) && same_targets && same_access &&
            same_variants && cached_soa_allocation_strategy_ == inputs.soa_allocation_strategy) {
            return false;
        }
    }
    if (cached_revision_ == inputs.workspace.revision() && cached_type_ == inputs.selection.type &&
        cached_target_profile_revision_ == target_profile_revision_ &&
        cached_comparison_target_profile_revision_ == comparison_target_profile_revision_ &&
        cached_access_operation_ == inputs.access_operation &&
        cached_access_multiplicity_ == inputs.access_multiplicity &&
        cached_soa_allocation_strategy_ == inputs.soa_allocation_strategy &&
        cached_selected_field_ == inputs.selection.field &&
        cached_packed_access_fields_ == inputs.selection.packed_access_fields &&
        cached_packed_access_set_explicit_ == inputs.selection.packed_access_set_explicit &&
        cached_record_access_members_ == inputs.selection.record_access_members &&
        cached_record_access_set_explicit_ == inputs.selection.record_access_set_explicit &&
        cached_soa_access_columns_ == inputs.selection.soa_access_columns &&
        cached_soa_access_set_explicit_ == inputs.selection.soa_access_set_explicit &&
        cached_comparison_a_variant_id_ == inputs.comparison_a_variant_id &&
        cached_comparison_b_variant_id_ == inputs.comparison_b_variant_id &&
        cached_quantized_comparison_type_ == inputs.quantized_comparison_type &&
        cached_varint_comparison_type_ == inputs.varint_comparison_type &&
        cached_varint_distribution_revision_ == varint_distribution_revision_ &&
        cached_optional_comparison_type_ == inputs.optional_comparison_type &&
        cached_union_distribution_revision_ == union_distribution_revision_ &&
        cached_tagged_distribution_revision_ == tagged_distribution_revision_) {
        return false;
    }
    auto const reuse_primary{same_declaration &&
                             cached_target_profile_revision_ == target_profile_revision_ &&
                             cached_soa_allocation_strategy_ == inputs.soa_allocation_strategy};
    auto const reuse_comparison_target{reuse_primary &&
                                       cached_comparison_target_profile_revision_ ==
                                           comparison_target_profile_revision_};
    auto const reuse_variant_comparison{reuse_primary && same_variants};
    auto previous{std::move(results_)};
    results_ = {};
    cached_revision_ = inputs.workspace.revision();
    cached_target_profile_revision_ = target_profile_revision_;
    cached_comparison_target_profile_revision_ = comparison_target_profile_revision_;
    cached_access_operation_ = inputs.access_operation;
    cached_access_multiplicity_ = inputs.access_multiplicity;
    cached_soa_allocation_strategy_ = inputs.soa_allocation_strategy;
    cached_type_ = inputs.selection.type;
    cached_selected_field_ = inputs.selection.field;
    cached_packed_access_fields_ = inputs.selection.packed_access_fields;
    cached_packed_access_set_explicit_ = inputs.selection.packed_access_set_explicit;
    cached_record_access_members_ = inputs.selection.record_access_members;
    cached_record_access_set_explicit_ = inputs.selection.record_access_set_explicit;
    cached_soa_access_columns_ = inputs.selection.soa_access_columns;
    cached_soa_access_set_explicit_ = inputs.selection.soa_access_set_explicit;
    cached_comparison_a_variant_id_ = inputs.comparison_a_variant_id;
    cached_comparison_b_variant_id_ = inputs.comparison_b_variant_id;
    cached_quantized_comparison_type_ = inputs.quantized_comparison_type;
    cached_varint_comparison_type_ = inputs.varint_comparison_type;
    cached_varint_distribution_revision_ = varint_distribution_revision_;
    cached_optional_comparison_type_ = inputs.optional_comparison_type;
    cached_union_distribution_revision_ = union_distribution_revision_;
    cached_tagged_distribution_revision_ = tagged_distribution_revision_;
    if (!inputs.selection.type.has_value()) {
        return true;
    }
    analyze_selected(document,
                     std::move(previous),
                     reuse_primary,
                     reuse_comparison_target,
                     reuse_variant_comparison);
    return true;
}

} // namespace ioj::layout
