#include <ioj/layout/physical_facts.hpp>

#include "checked_arithmetic.hpp"
#include "enum_backing.hpp"
#include <ioj/layout/string_set.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <lispb/project_loader.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/union_alternatives.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
using namespace analysis_detail;

PhysicalFactsResolver::PhysicalFactsResolver(lispb::schema::TypeGraph const& types,
                                             AbiProfile const& abi,
                                             ResolvedVariant const* variant)
    : types_{types}
    , abi_{abi}
    , variant_{variant} {}

auto PhysicalFactsResolver::analyze_record(lispb::schema::TypeId const type) -> RecordAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_record(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

auto PhysicalFactsResolver::analyze_union(lispb::schema::TypeId const type) -> UnionAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_union(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

auto PhysicalFactsResolver::analyze_tagged_union(lispb::schema::TypeId const type)
    -> TaggedUnionAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_tagged_union(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

void PhysicalFactsResolver::check_supplied_layout(lispb::schema::TypeId const type,
                                                  std::optional<std::uint64_t>& size,
                                                  std::optional<std::uint64_t>& alignment,
                                                  std::vector<Diagnostic>& diagnostics) const {
    auto const& spelling{std::string{types_.strings().view(types_.cpp_spelling(type))}};
    auto const supplied{abi_.find(spelling)};
    if (size.has_value() && alignment.has_value() && supplied.has_value() &&
        (supplied->size_bytes != *size || supplied->alignment_bytes != *alignment)) {
        diagnostics.push_back(
            {DiagnosticSeverity::error,
             spelling +
                 ": supplied complete-object facts conflict with the derived LispB layout (" +
                 supplied->provenance + ")."});
        size.reset();
        alignment.reset();
    }
}

auto PhysicalFactsResolver::resolve(lispb::schema::ResolvedOccurrenceId const use)
    -> PhysicalFactsResult {
    PhysicalFactsResult result{};
    std::vector<lispb::schema::TypeId> active;
    result.facts = facts_for(use,
                             active,
                             result.diagnostics,
                             std::string{types_.cpp_types().spelling(
                                 types_.occurrences().cpp_type[types_.occurrences().index(use)])});
    return result;
}

auto PhysicalFactsResolver::resolve(lispb::schema::TypeId const type) -> PhysicalFactsResult {
    PhysicalFactsResult result{};
    auto const& node{type};
    if (std::string{types_.strings().view(types_.cpp_spelling(node))}.empty()) {
        result.diagnostics.push_back({DiagnosticSeverity::warning,
                                      "Logical layout schema '" +
                                          std::string{types_.strings().view(types_.name(node))} +
                                          "' has no C++ owner."});
        return result;
    }
    std::vector<lispb::schema::TypeId> active;
    result.facts = facts_for(type,
                             active,
                             result.diagnostics,
                             std::string{types_.strings().view(types_.cpp_spelling(type))});
    return result;
}

auto PhysicalFactsResolver::resolve_spelling(std::string const& spelling,
                                             lispb::codegen::ModuleHandle const module)
    -> PhysicalFactsResult {
    auto const cpp_type{types_.cpp_types().intern(spelling)};
    auto const target{types_.find_named(types_.strings().find(spelling), module)};
    auto physical{lispb::codegen::classify_physical_type_use(types_.cpp_types(), cpp_type)};
    physical.names_semantic_type = target.has_value();
    PhysicalFactsResult result;
    std::vector<lispb::schema::TypeId> active;
    result.facts = facts_for(target.value_or(lispb::schema::TypeId{}),
                             cpp_type,
                             physical.form,
                             physical.object_spelling,
                             physical.names_semantic_type,
                             active,
                             result.diagnostics,
                             spelling);
    return result;
}

auto PhysicalFactsResolver::lookup(std::string const& spelling,
                                   std::vector<Diagnostic>& diagnostics,
                                   std::string const& context) const -> std::optional<TypeFacts> {
    auto const normalized{lispb::codegen::native_spelling(spelling)};
    auto facts{abi_.find(normalized)};
    if (!facts.has_value()) {
        diagnostics.push_back({DiagnosticSeverity::warning,
                               context + " has unknown physical facts for '" + normalized +
                                   "' in profile '" + abi_.name() + "'.",
                               normalized});
    }
    return facts;
}

auto PhysicalFactsResolver::facts_for(lispb::schema::ResolvedOccurrenceId const use,
                                      std::vector<lispb::schema::TypeId>& active,
                                      std::vector<Diagnostic>& diagnostics,
                                      std::string const& context) -> std::optional<TypeFacts> {
    auto const row{types_.occurrences().index(use)};
    return facts_for(types_.occurrences().type[row],
                     types_.occurrences().cpp_type[row],
                     types_.occurrences().physical_form[row],
                     types_.occurrences().object_spelling[row],
                     types_.occurrences().names_semantic_type[row] != 0,
                     active,
                     diagnostics,
                     context);
}

auto PhysicalFactsResolver::facts_for(lispb::schema::TypeId const semantic_type,
                                      lispb::codegen::CppTypeId const cpp_type,
                                      lispb::codegen::PhysicalTypeForm const form,
                                      lispb::StringId const object_spelling,
                                      bool const names_semantic_type,
                                      std::vector<lispb::schema::TypeId>& active,
                                      std::vector<Diagnostic>& diagnostics,
                                      std::string const& context) -> std::optional<TypeFacts> {
    if (form == lispb::codegen::PhysicalTypeForm::unsupported) {
        diagnostics.push_back(
            {DiagnosticSeverity::warning,
             context + ": " +
                 lispb::codegen::physical_type_diagnostic(types_.cpp_types(), cpp_type)});
        return std::nullopt;
    }
    if (form == lispb::codegen::PhysicalTypeForm::lvalue_reference ||
        form == lispb::codegen::PhysicalTypeForm::rvalue_reference) {
        diagnostics.push_back(
            {DiagnosticSeverity::warning,
             context + ": reference-member storage is not established by sizeof the referent."});
        return std::nullopt;
    }
    if (form == lispb::codegen::PhysicalTypeForm::object_pointer) {
        if (auto exact{abi_.find(std::string{types_.cpp_types().spelling(cpp_type)})}) {
            return exact;
        }
        if (abi_.object_pointer_representation().has_value()) {
            auto facts{abi_.find(*abi_.object_pointer_representation())};
            if (facts.has_value() && !facts->integer_signed.has_value()) {
                if (facts->origin != FactOrigin::manual_assumption) {
                    facts->origin = FactOrigin::target_abi;
                }
                facts->provenance = "Object-pointer ABI policy for '" + abi_.name() + "' using " +
                                  *abi_.object_pointer_representation() + ": " + facts->provenance;
                return facts;
            }
        }
        return lookup(std::string{types_.cpp_types().spelling(cpp_type)}, diagnostics, context);
    }
    if (names_semantic_type && lispb::schema::is_valid(semantic_type)) {
        return facts_for(semantic_type, active, diagnostics, context);
    }
    return lookup(
        std::string{types_.cpp_types().strings().view(object_spelling)}, diagnostics, context);
}

auto PhysicalFactsResolver::facts_for(lispb::schema::TypeId const type,
                                      std::vector<lispb::schema::TypeId>& active,
                                      std::vector<Diagnostic>& diagnostics,
                                      std::string const& context) -> std::optional<TypeFacts> {
    if (std::ranges::find(active, type) != active.end()) {
        diagnostics.push_back(
            {DiagnosticSeverity::error, context + ": recursive physical representation."});
        return std::nullopt;
    }
    if (auto const cached{known_.find(type)}; cached != known_.end()) {
        return cached->second;
    }
    auto const& node{type};
    auto aggregate_facts = [&](auto nested) -> std::optional<TypeFacts> {
        for (auto& diagnostic : nested.diagnostics) {
            diagnostic.message = context + ": " + diagnostic.message;
            diagnostics.push_back(std::move(diagnostic));
        }
        if (!nested.size_bytes.has_value() || !nested.alignment_bytes.has_value()) {
            return std::nullopt;
        }
        TypeFacts facts{.size_bytes = *nested.size_bytes,
                        .alignment_bytes = *nested.alignment_bytes,
                        .integer_signed = std::nullopt,
                        .unsigned_value_bits = std::nullopt,
                        .provenance =
                            "Derived from LispB '" +
                            std::string{types_.strings().view(types_.cpp_spelling(node))} +
                            "' using profile '" + abi_.name() + "'.",
                        .origin = FactOrigin::derived};
        check_supplied_layout(type, nested.size_bytes, nested.alignment_bytes, diagnostics);
        if (!nested.size_bytes.has_value()) {
            return std::nullopt;
        }
        known_.emplace(type, facts);
        return facts;
    };
    if (((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::record)) {
        return aggregate_facts(analyze_record(type, active));
    }
    if (((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::union_type)) {
        return aggregate_facts(analyze_union(type, active));
    }
    if (((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::tagged_union)) {
        return aggregate_facts(analyze_tagged_union(type, active));
    }
    if (auto const external{
            ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external
                 ? std::optional{types_.definition(node)}
                 : std::nullopt)}) {
        auto const physical{lispb::codegen::classify_physical_type_use(
            types_.cpp_types(),
            types_.definitions()
                .externals.cpp_type[types_.definitions().externals.index(*external)])};
        return facts_for(type,
                         types_.definitions()
                             .externals.cpp_type[types_.definitions().externals.index(*external)],
                         physical.form,
                         physical.object_spelling,
                         physical.names_semantic_type,
                         active,
                         diagnostics,
                         context);
    }
    active.push_back(type);
    std::optional<TypeFacts> facts;
    if (auto const enumeration{
            ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::enumeration
                 ? std::optional{types_.definition(node)}
                 : std::nullopt)}) {
        if (lispb::schema::is_valid(
                types_.definitions()
                    .enums.underlying_type[types_.definitions().enums.index(*enumeration)])) {
            facts = facts_for(
                types_.definitions()
                    .enums.underlying_type[types_.definitions().enums.index(*enumeration)],
                active,
                diagnostics,
                context);
        } else if (auto const backing{derived_enum_backing_type(types_, *enumeration)}) {
            facts = lookup(*backing, diagnostics, context);
        }
    } else if (auto const packed{
                   ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::packed
                        ? std::optional{types_.definition(node)}
                        : std::nullopt)}) {
        auto const overridden{variant_ != nullptr &&
                              variant_->overrides.packed_storage_types.contains(type)};
        if (overridden) {
            auto const& spelling{variant_->overrides.packed_storage_types.at(type)};
            auto const cpp_type{types_.cpp_types().intern(spelling)};
            auto physical{lispb::codegen::classify_physical_type_use(types_.cpp_types(), cpp_type)};
            auto const target{
                types_.find_named(types_.strings().find(spelling), types_.module(node))};
            physical.names_semantic_type = target.has_value();
            facts = facts_for(target.value_or(lispb::schema::TypeId{}),
                              cpp_type,
                              physical.form,
                              physical.object_spelling,
                              physical.names_semantic_type,
                              active,
                              diagnostics,
                              context);
        } else {
            facts = facts_for(types_.definitions()
                                  .packed.storage_type[types_.definitions().packed.index(*packed)],
                              active,
                              diagnostics,
                              context);
        }
    } else if (auto const scalar{((types_.definition(node)).kind() ==
                                          lispb::schema::TypeDefinitionKind::integer_scalar
                                      ? std::optional{types_.definition(node)}
                                      : std::nullopt)};
               static_cast<bool>(scalar) &&
               lispb::schema::is_valid(
                   types_.definitions()
                       .integer_scalars
                       .cpp_representation[types_.definitions().integer_scalars.index(*scalar)])) {
        facts =
            facts_for(types_.definitions()
                          .integer_scalars
                          .cpp_representation[types_.definitions().integer_scalars.index(*scalar)],
                      active,
                      diagnostics,
                      context);
    } else if (auto const soa{
                   ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::soa
                        ? std::optional{types_.definition(node)}
                        : std::nullopt)};
               static_cast<bool>(soa) &&
               !std::string{types_.strings().view(types_.cpp_spelling(node))}.empty()) {
        facts = lookup(
            std::string{types_.strings().view(types_.cpp_spelling(node))}, diagnostics, context);
    } else {
        diagnostics.push_back({DiagnosticSeverity::warning,
                               context + " has no supported complete-object representation."});
    }
    active.pop_back();
    if (facts.has_value()) {
        known_.emplace(type, *facts);
    }
    return facts;
}

auto PhysicalFactsResolver::analyze_record(lispb::schema::TypeId const type,
                                           std::vector<lispb::schema::TypeId>& active)
    -> RecordAnalysis {
    RecordAnalysis result{.type = type,
                          .members = {},
                          .payload_bytes = std::nullopt,
                          .internal_padding_bytes = std::nullopt,
                          .tail_padding_bytes = std::nullopt,
                          .size_bytes = std::nullopt,
                          .alignment_bytes = std::nullopt,
                          .diagnostics = {},
                          .aggregate = {}};
    auto const& node{type};
    auto const record{((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::record
                           ? std::optional{types_.definition(node)}
                           : std::nullopt)};
    if (!record) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a record."});
        return result;
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Illegal by-value record cycle involving '" +
                                          std::string{types_.strings().view(types_.name(node))} +
                                          "'."});
        return result;
    }
    active.push_back(type);

    std::optional<std::uint64_t> offset{0};
    std::uint64_t payload{};
    std::uint64_t internal_padding{};
    std::uint64_t record_alignment{1};
    result.members.reserve(types_.definitions().records.members_of(*record).size());
    for (auto const member_id : types_.definitions().records.members_of(*record)) {
        auto const member{member_id};
        auto member_result{RecordMemberAnalysis{
            .name = std::string{types_.strings().view(
                types_.definitions()
                    .record_members.name[types_.definitions().record_members.index(member)])},
            .semantic_type = types_.occurrences().type[types_.occurrences().index(
                types_.definitions().record_members.semantic_type
                    [types_.definitions().record_members.index(member)])],
            .element_count =
                types_.definitions()
                    .record_members.count[types_.definitions().record_members.index(member)]
                    .value_or(1),
            .element_facts = std::nullopt,
            .offset_bytes = std::nullopt,
            .extent_bytes = std::nullopt,
            .padding_before_bytes = std::nullopt}};
        auto const context{
            "Record '" + std::string{types_.strings().view(types_.name(node))} + "' member '" +
            std::string{types_.strings().view(
                types_.definitions()
                    .record_members.name[types_.definitions().record_members.index(member)])} +
            "'"};
        member_result.element_facts = facts_for(
            types_.definitions()
                .record_members.semantic_type[types_.definitions().record_members.index(member)],
            active,
            result.diagnostics,
            context);
        if (!member_result.element_facts.has_value()) {
            offset.reset();
            result.members.push_back(std::move(member_result));
            continue;
        }
        auto const alignment{member_result.element_facts->alignment_bytes};
        if (member_result.element_facts->size_bytes == 0 || alignment == 0) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        member_result.extent_bytes =
            checked_multiply(member_result.element_facts->size_bytes, member_result.element_count);
        if (!member_result.extent_bytes.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        if (!offset.has_value()) {
            result.members.push_back(std::move(member_result));
            continue;
        }
        auto const aligned{align_up(*offset, alignment)};
        if (!aligned.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " aligned offset overflows uint64."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        member_result.offset_bytes = *aligned;
        member_result.padding_before_bytes = *aligned - *offset;
        auto const next_offset{checked_add(*aligned, *member_result.extent_bytes)};
        auto const next_payload{checked_add(payload, *member_result.extent_bytes)};
        auto const next_padding{checked_add(internal_padding, *member_result.padding_before_bytes)};
        if (!next_offset.has_value() || !next_payload.has_value() || !next_padding.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " layout arithmetic overflows uint64."});
        } else {
            offset = *next_offset;
            payload = *next_payload;
            internal_padding = *next_padding;
            record_alignment = std::max(record_alignment, alignment);
        }
        result.members.push_back(std::move(member_result));
    }

    if (offset.has_value()) {
        if (types_.definitions().records.members_of(*record).empty()) {
            result.payload_bytes = 0;
            result.internal_padding_bytes = 0;
            result.tail_padding_bytes = 0;
            result.size_bytes = 1;
            result.alignment_bytes = 1;
        } else {
            auto const size{align_up(*offset, record_alignment)};
            if (!size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Record tail alignment overflows uint64."});
            } else {
                result.payload_bytes = payload;
                result.internal_padding_bytes = internal_padding;
                result.tail_padding_bytes = *size - *offset;
                result.size_bytes = *size;
                result.alignment_bytes = record_alignment;
            }
        }
    }
    active.pop_back();
    return result;
}

auto PhysicalFactsResolver::analyze_union(lispb::schema::TypeId const type,
                                          std::vector<lispb::schema::TypeId>& active)
    -> UnionAnalysis {
    UnionAnalysis result{.type = type,
                         .alternatives = {},
                         .largest_alternative_bytes = std::nullopt,
                         .tail_padding_bytes = std::nullopt,
                         .size_bytes = std::nullopt,
                         .alignment_bytes = std::nullopt,
                         .diagnostics = {},
                         .aggregate = {}};
    auto const& node{type};
    auto const union_type{
        ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::union_type
             ? std::optional{types_.definition(node)}
             : std::nullopt)};
    if (!union_type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a union."});
        return result;
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Illegal by-value aggregate cycle involving '" +
                                          std::string{types_.strings().view(types_.name(node))} +
                                          "'."});
        return result;
    }
    active.push_back(type);

    std::uint64_t largest_extent{};
    std::uint64_t union_alignment{1};
    bool complete{true};
    result.alternatives.reserve(types_.definitions().unions.alternatives_of(*union_type).size());
    for (auto const alternative_id : types_.definitions().unions.alternatives_of(*union_type)) {
        auto const alternative{alternative_id};
        auto alternative_result{UnionAlternativeAnalysis{
            .name = std::string{types_.strings().view(
                types_.definitions()
                    .union_alternatives
                    .name[types_.definitions().union_alternatives.index(alternative)])},
            .semantic_type = types_.occurrences().type[types_.occurrences().index(
                types_.definitions().union_alternatives.semantic_type
                    [types_.definitions().union_alternatives.index(alternative)])],
            .element_count = types_.definitions()
                                 .union_alternatives
                                 .count[types_.definitions().union_alternatives.index(alternative)]
                                 .value_or(1),
            .element_facts = std::nullopt,
            .extent_bytes = std::nullopt,
            .slack_bytes = std::nullopt,
            .total_slack_bytes = std::nullopt}};
        auto const context{
            "Union '" + std::string{types_.strings().view(types_.name(node))} + "' alternative '" +
            std::string{types_.strings().view(
                types_.definitions()
                    .union_alternatives
                    .name[types_.definitions().union_alternatives.index(alternative)])} +
            "'"};
        alternative_result.element_facts = facts_for(
            types_.definitions()
                .union_alternatives
                .semantic_type[types_.definitions().union_alternatives.index(alternative)],
            active,
            result.diagnostics,
            context);
        if (!alternative_result.element_facts.has_value()) {
            complete = false;
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        auto const alignment{alternative_result.element_facts->alignment_bytes};
        if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        alternative_result.extent_bytes = checked_multiply(
            alternative_result.element_facts->size_bytes, alternative_result.element_count);
        if (!alternative_result.extent_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
        union_alignment = std::max(union_alignment, alignment);
        result.alternatives.push_back(std::move(alternative_result));
    }

    if (complete) {
        auto const size{align_up(largest_extent, union_alignment)};
        if (!size.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Union tail alignment overflows uint64."});
        } else {
            result.largest_alternative_bytes = largest_extent;
            result.tail_padding_bytes = *size - largest_extent;
            result.size_bytes = *size;
            result.alignment_bytes = union_alignment;
            for (auto& alternative : result.alternatives) {
                alternative.slack_bytes = *size - *alternative.extent_bytes;
            }
        }
    }
    active.pop_back();
    return result;
}

auto PhysicalFactsResolver::analyze_tagged_union(lispb::schema::TypeId const type,
                                                 std::vector<lispb::schema::TypeId>& active)
    -> TaggedUnionAnalysis {
    TaggedUnionAnalysis result{.type = type,
                               .discriminant_type = {},
                               .discriminant_facts = std::nullopt,
                               .alternatives = {},
                               .largest_alternative_bytes = std::nullopt,
                               .payload_size_bytes = std::nullopt,
                               .payload_alignment_bytes = std::nullopt,
                               .payload_offset_bytes = std::nullopt,
                               .internal_padding_bytes = std::nullopt,
                               .tail_padding_bytes = std::nullopt,
                               .size_bytes = std::nullopt,
                               .alignment_bytes = std::nullopt,
                               .mapped_live_tags = {},
                               .unmapped_live_tags = {},
                               .sentinel_tags = {},
                               .count_sentinel_tag = std::nullopt,
                               .diagnostics = {},
                               .aggregate = {}};
    auto const& node{type};
    auto const tagged{
        ((types_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::tagged_union
             ? std::optional{types_.definition(node)}
             : std::nullopt)};
    if (!tagged) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a tagged union."});
        return result;
    }
    result.discriminant_type = types_.occurrences().type[types_.occurrences().index(
        types_.definitions()
            .tagged_unions.discriminant[types_.definitions().tagged_unions.index(*tagged)])];
    auto const& discriminant_node{types_.occurrences().type[types_.occurrences().index(
        types_.definitions()
            .tagged_unions.discriminant[types_.definitions().tagged_unions.index(*tagged)])]};
    auto const enumeration{((types_.definition(discriminant_node)).kind() ==
                                    lispb::schema::TypeDefinitionKind::enumeration
                                ? std::optional{types_.definition(discriminant_node)}
                                : std::nullopt)};
    if (static_cast<bool>(enumeration)) {
        StringSet mapped;
        for (auto const alternative_id :
             types_.definitions().tagged_unions.alternatives_of(*tagged)) {
            auto const alternative{alternative_id};
            mapped.insert(std::string{types_.strings().view(
                types_.definitions()
                    .tagged_union_alternatives
                    .tag[types_.definitions().tagged_union_alternatives.index(alternative)])});
        }
        auto const& domains{types_.enum_domains()};
        auto const domain{
            types_.definitions().enums.domain[types_.definitions().enums.index(*enumeration)]};
        auto const& enumerators{types_.declarations().enumerators};
        for (auto const enumerator : domains.values(domain)) {
            auto const row{enumerators.index(enumerator)};
            auto name{std::string{types_.strings().view(enumerators.name[row])}};
            if (enumerator == domains.count_value[domains.index(domain)]) {
                result.count_sentinel_tag = std::move(name);
            } else if (enumerators.sentinel[row]) {
                result.sentinel_tags.push_back(std::move(name));
            } else if (mapped.contains(name)) {
                result.mapped_live_tags.push_back(std::move(name));
            } else {
                result.unmapped_live_tags.push_back(std::move(name));
            }
        }
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Illegal by-value aggregate cycle involving '" +
                                          std::string{types_.strings().view(types_.name(node))} +
                                          "'."});
        return result;
    }
    active.push_back(type);

    result.discriminant_facts = facts_for(
        types_.definitions()
            .tagged_unions.discriminant[types_.definitions().tagged_unions.index(*tagged)],
        active,
        result.diagnostics,
        "Tagged union '" + std::string{types_.strings().view(types_.name(node))} +
            "' discriminant");
    std::uint64_t largest_extent{};
    std::uint64_t payload_alignment{1};
    bool payload_complete{true};
    result.alternatives.reserve(types_.definitions().tagged_unions.alternatives_of(*tagged).size());
    for (auto const alternative_id : types_.definitions().tagged_unions.alternatives_of(*tagged)) {
        auto const alternative{alternative_id};
        auto alternative_result{TaggedUnionAlternativeAnalysis{
            .name = std::string{types_.strings().view(
                types_.definitions()
                    .tagged_union_alternatives
                    .name[types_.definitions().tagged_union_alternatives.index(alternative)])},
            .tag = std::string{types_.strings().view(
                types_.definitions()
                    .tagged_union_alternatives
                    .tag[types_.definitions().tagged_union_alternatives.index(alternative)])},
            .semantic_type = types_.occurrences().type[types_.occurrences().index(
                types_.definitions().tagged_union_alternatives.semantic_type
                    [types_.definitions().tagged_union_alternatives.index(alternative)])],
            .element_count =
                types_.definitions()
                    .tagged_union_alternatives
                    .count[types_.definitions().tagged_union_alternatives.index(alternative)]
                    .value_or(1),
            .element_facts = std::nullopt,
            .extent_bytes = std::nullopt,
            .payload_slack_bytes = std::nullopt,
            .total_payload_slack_bytes = std::nullopt}};
        auto const context{
            "Tagged union '" + std::string{types_.strings().view(types_.name(node))} +
            "' alternative '" +
            std::string{types_.strings().view(
                types_.definitions()
                    .tagged_union_alternatives
                    .name[types_.definitions().tagged_union_alternatives.index(alternative)])} +
            "'"};
        alternative_result.element_facts = facts_for(
            types_.definitions()
                .tagged_union_alternatives
                .semantic_type[types_.definitions().tagged_union_alternatives.index(alternative)],
            active,
            result.diagnostics,
            context);
        if (!alternative_result.element_facts.has_value()) {
            payload_complete = false;
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        auto const alignment{alternative_result.element_facts->alignment_bytes};
        if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
            payload_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        alternative_result.extent_bytes = checked_multiply(
            alternative_result.element_facts->size_bytes, alternative_result.element_count);
        if (!alternative_result.extent_bytes.has_value()) {
            payload_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
        payload_alignment = std::max(payload_alignment, alignment);
        result.alternatives.push_back(std::move(alternative_result));
    }

    if (payload_complete) {
        auto const payload_size{align_up(largest_extent, payload_alignment)};
        if (!payload_size.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Tagged union payload alignment overflows uint64."});
        } else {
            result.largest_alternative_bytes = largest_extent;
            result.payload_size_bytes = *payload_size;
            result.payload_alignment_bytes = payload_alignment;
            for (auto& alternative : result.alternatives) {
                alternative.payload_slack_bytes = *payload_size - *alternative.extent_bytes;
            }
        }
    }

    if (result.discriminant_facts.has_value() && result.payload_size_bytes.has_value()) {
        auto const& discriminant{*result.discriminant_facts};
        if (discriminant.size_bytes == 0 || discriminant.alignment_bytes == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged union discriminant has zero-sized target facts."});
        } else {
            auto const payload_offset{
                align_up(discriminant.size_bytes, *result.payload_alignment_bytes)};
            auto const content_end{payload_offset.has_value()
                                       ? checked_add(*payload_offset, *result.payload_size_bytes)
                                       : std::nullopt};
            auto const alignment{
                std::max(discriminant.alignment_bytes, *result.payload_alignment_bytes)};
            auto const size{content_end.has_value() ? align_up(*content_end, alignment)
                                                    : std::nullopt};
            if (!payload_offset.has_value() || !content_end.has_value() || !size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged union object layout arithmetic overflows uint64."});
            } else {
                result.payload_offset_bytes = *payload_offset;
                result.internal_padding_bytes = *payload_offset - discriminant.size_bytes;
                result.tail_padding_bytes = *size - *content_end;
                result.size_bytes = *size;
                result.alignment_bytes = alignment;
            }
        }
    }
    active.pop_back();
    return result;
}

} // namespace ioj::layout
