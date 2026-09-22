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

} // namespace

PlannerAnalysisSession::PlannerAnalysisSession(TypeGraph types) {
    inputs.workspace = LayoutWorkspace{std::move(types)};
}

void PlannerAnalysisSession::replace_types(EditableSchemaDocument const& document,
                                           std::optional<TypeIdentity> selection) {
    auto raw_weights_changed{false};
    for (auto entry = inputs.union_distributions.begin();
         entry != inputs.union_distributions.end();) {
        auto const* declaration{document.declaration(entry->first)};
        auto const* schema{document.union_schema(entry->first)};
        if (declaration == nullptr || schema == nullptr) {
            entry = inputs.union_distributions.erase(entry);
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
        ++inputs.union_distribution_revision;
    }

    auto tagged_weights_changed{false};
    for (auto entry = inputs.tagged_union_distributions.begin();
         entry != inputs.tagged_union_distributions.end();) {
        auto const* declaration{document.declaration(entry->first)};
        auto const* schema{document.tagged_union_schema(entry->first)};
        if (declaration == nullptr || schema == nullptr) {
            entry = inputs.tagged_union_distributions.erase(entry);
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
        ++inputs.tagged_distribution_revision;
    }

    inputs.workspace.replace_types(document.types());
    inputs.selection.reconcile(inputs.workspace.types(), selection);
}

auto PlannerAnalysisSession::results() const -> PlannerAnalysisResults const& {
    return results_;
}

auto PlannerAnalysisSession::status(TypeId const type) -> LayoutStatus {
    if (status_revision_ != inputs.workspace.graph_revision() ||
        status_profile_revision_ != inputs.target_profile_revision) {
        status_cache_.clear();
        status_revision_ = inputs.workspace.graph_revision();
        status_profile_revision_ = inputs.target_profile_revision;
    }
    auto const found{status_cache_.find(type.value)};
    if (found != status_cache_.end()) {
        return found->second;
    }
    auto const value{
        declaration_status(inputs.workspace.types(),
                           type,
                           *inputs.workspace.variant(LayoutWorkspace::baseline_variant_id),
                           inputs.abi,
                           inputs.workspace.default_capacity())};
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
    auto const same_targets{cached_target_profile_revision_ == inputs.target_profile_revision &&
                            cached_comparison_target_profile_revision_ ==
                                inputs.comparison_target_profile_revision};
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
            cached_union_distribution_revision_ == inputs.union_distribution_revision) {
            return false;
        }
        if (std::holds_alternative<TaggedUnionType>(definition) && same_targets &&
            cached_tagged_distribution_revision_ == inputs.tagged_distribution_revision) {
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
        cached_target_profile_revision_ == inputs.target_profile_revision &&
        cached_comparison_target_profile_revision_ == inputs.comparison_target_profile_revision &&
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
        cached_varint_distribution_revision_ == inputs.varint_distribution_revision &&
        cached_optional_comparison_type_ == inputs.optional_comparison_type &&
        cached_union_distribution_revision_ == inputs.union_distribution_revision &&
        cached_tagged_distribution_revision_ == inputs.tagged_distribution_revision) {
        return false;
    }
    auto const reuse_primary{same_declaration &&
                             cached_target_profile_revision_ == inputs.target_profile_revision &&
                             cached_soa_allocation_strategy_ == inputs.soa_allocation_strategy};
    auto const reuse_comparison_target{reuse_primary &&
                                       cached_comparison_target_profile_revision_ ==
                                           inputs.comparison_target_profile_revision};
    auto const reuse_variant_comparison{reuse_primary && same_variants};
    auto previous{std::move(results_)};
    results_ = {};
    cached_revision_ = inputs.workspace.revision();
    cached_target_profile_revision_ = inputs.target_profile_revision;
    cached_comparison_target_profile_revision_ = inputs.comparison_target_profile_revision;
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
    cached_varint_distribution_revision_ = inputs.varint_distribution_revision;
    cached_optional_comparison_type_ = inputs.optional_comparison_type;
    cached_union_distribution_revision_ = inputs.union_distribution_revision;
    cached_tagged_distribution_revision_ = inputs.tagged_distribution_revision;
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
