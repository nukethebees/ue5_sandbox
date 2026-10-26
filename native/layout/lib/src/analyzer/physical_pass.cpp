#include "physical_pass.hpp"

#include "../checked_arithmetic.hpp"
#include "../enum_backing.hpp"
#include "../variant_projection.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/resolved_variant.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/physical_type_use.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <exception>
#include <string>
#include <vector>

namespace ioj::layout::analysis_detail {
using lispb::codegen::classify_physical_type_use;
using lispb::codegen::PhysicalTypeForm;

auto retain_facts(AnalysisContext& context, TypeFacts const& facts) -> PhysicalFactId {
    using Signedness = lispb::schema::EnumSignedness;
    auto& rows{context.storage.physical_facts};
    auto const id{PhysicalFactId{rows.num()}};
    rows.add(facts.size_bytes,
             facts.alignment_bytes,
             facts.integer_signed
                 ? (*facts.integer_signed ? Signedness::signed_ : Signedness::unsigned_)
                 : Signedness::unspecified,
             facts.unsigned_value_bits,
             context.storage.strings.intern(facts.provenance),
             facts.origin);
    return id;
}

auto lookup_physical(AnalysisContext& context, AbiProfile const& abi, std::string_view spelling)
    -> std::optional<PhysicalFactId> {
    auto facts{abi.find(lispb::codegen::native_spelling(std::string{spelling}))};
    if (!facts) {
        context.report({DiagnosticSeverity::warning,
                        "Unknown physical facts for '" + std::string{spelling} + "' in profile '" +
                            abi.name() + "'."});
    } else if (facts->size_bytes == 0 || facts->alignment_bytes == 0) {
        context.report(
            {DiagnosticSeverity::error,
             "Physical size and alignment must be nonzero for '" + std::string{spelling} + "'."});
        facts.reset();
    }
    return facts.transform([&](auto const& value) { return retain_facts(context, value); });
}

auto physical_use(AnalysisContext& context,
                  ScenarioId scenario,
                  AbiProfile const& abi,
                  lispb::schema::TypeId type,
                  lispb::codegen::CppTypeId cpp_type,
                  PhysicalTypeForm form,
                  lispb::StringId object_spelling,
                  bool names_semantic_type) -> std::optional<PhysicalFactId> {
    using Form = PhysicalTypeForm;
    auto const& types{context.types};
    if (form == Form::unsupported) {
        context.report({DiagnosticSeverity::warning,
                        lispb::codegen::physical_type_diagnostic(types.cpp_types(), cpp_type)});
        return {};
    }
    if (form == Form::lvalue_reference || form == Form::rvalue_reference) {
        context.report({DiagnosticSeverity::warning,
                        "Reference-member storage is not established by sizeof the referent."});
        return {};
    }
    if (form == Form::object_pointer) {
        if (abi.find(std::string{types.cpp_types().spelling(cpp_type)})) {
            return lookup_physical(context, abi, types.cpp_types().spelling(cpp_type));
        }
        if (abi.object_pointer_representation()) {
            auto const facts{lookup_physical(context, abi, *abi.object_pointer_representation())};
            auto const rows{context.storage.physical_facts.get_view().resolve()};
            if (facts &&
                rows.signedness()[facts->value] == lispb::schema::EnumSignedness::unspecified) {
                if (rows.origin()[facts->value] != FactOrigin::manual_assumption) {
                    rows.origin()[facts->value] = FactOrigin::target_abi;
                }
                auto const provenance{
                    "Object-pointer ABI policy for '" + abi.name() + "' using " +
                    *abi.object_pointer_representation() + ": " +
                    std::string{context.storage.strings.view(rows.provenance()[facts->value])}};
                rows.provenance()[facts->value] = context.storage.strings.intern(provenance);
                return facts;
            }
        }
        return lookup_physical(context, abi, types.cpp_types().spelling(cpp_type));
    }
    if (names_semantic_type && lispb::schema::is_valid(type)) {
        return context.storage.types.physical()[context.storage.types.find(type, scenario).value];
    }
    return lookup_physical(context, abi, types.strings().view(object_spelling));
}

auto physical_occurrence(AnalysisContext& context,
                         ScenarioId scenario,
                         AbiProfile const& abi,
                         lispb::schema::ResolvedOccurrenceId use) -> std::optional<PhysicalFactId> {
    auto const& occurrences{context.types.occurrences()};
    auto const row{occurrences.index(use)};
    return physical_use(context,
                        scenario,
                        abi,
                        occurrences.type[row],
                        occurrences.cpp_type[row],
                        occurrences.physical_form[row],
                        occurrences.object_spelling[row],
                        occurrences.names_semantic_type[row] != 0);
}

auto physical_spelling(AnalysisContext& context,
                       ScenarioId scenario,
                       AbiProfile const& abi,
                       std::string_view spelling,
                       lispb::codegen::ModuleHandle module) -> std::optional<PhysicalFactId> {
    auto const& types{context.types};
    auto const cpp{types.cpp_types().intern(spelling)};
    auto const physical{classify_physical_type_use(types.cpp_types(), cpp)};
    auto const target{types.find_named(types.strings().find(spelling), module)};
    return physical_use(context,
                        scenario,
                        abi,
                        target.value_or(lispb::schema::TypeId{}),
                        cpp,
                        physical.form,
                        physical.object_spelling,
                        target.has_value());
}

auto append_component(AnalysisContext& context,
                      AnalysisId parent,
                      ScenarioId scenario,
                      AbiProfile const& abi,
                      std::string_view name,
                      std::string_view tag,
                      lispb::schema::ResolvedOccurrenceId occurrence,
                      std::uint64_t count) -> std::size_t {
    auto& members{context.storage.components};
    auto const& occurrences{context.types.occurrences()};
    auto const semantic_type{occurrences.type[occurrences.index(occurrence)]};
    auto const facts{physical_occurrence(context, scenario, abi, occurrence)};
    auto const row{members.add(parent, name, tag, semantic_type, count, facts, {}, {}, {}, {})};
    if (facts) {
        members.extent_bytes()[row] =
            checked_multiply(context.storage.physical_facts.size_bytes()[facts->value], count);
        if (!members.extent_bytes()[row]) {
            context.report(
                {DiagnosticSeverity::error,
                 "Fixed-array extent overflows uint64 for '" + std::string{name} + "'."});
        }
    }
    return row;
}

void aggregate_layout(AnalysisContext& context,
                      AnalysisId id,
                      ScenarioId scenario,
                      AbiProfile const& abi) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type()[id.value]};
    auto const definition{types.definition(type)};
    auto const kind{definition.kind()};
    using Kind = lispb::schema::TypeDefinitionKind;
    auto& components{context.storage.components};
    AggregateLayout result;
    result.members.begin = components.num();
    auto const& schema{types.definitions().children};
    auto const children{kind == Kind::record
                            ? types.definitions().records.members_of(definition)
                            : types.definitions().unions.alternatives_of(definition)};
    for (auto const child : children) {
        auto const row{schema.index(child)};
        if (schema.reserved[row]) {
            continue;
        }
        append_component(context,
                         id,
                         scenario,
                         abi,
                         types.strings().view(schema.name[row]),
                         schema.case_label[row] ? types.strings().view(*schema.case_label[row])
                                                : std::string_view{},
                         schema.semantic_type[row],
                         schema.count[row].value_or(1));
    }
    result.members.count = components.num() - result.members.begin;

    auto const facts{context.storage.physical_facts.get_const_view().resolve()};
    auto const alignments{facts.alignment_bytes()};
    std::optional<std::uint64_t> offset{0};
    std::uint64_t payload{};
    std::uint64_t padding{};
    std::uint64_t alignment{1};
    std::uint64_t largest{};
    bool complete{true};
    auto const end{result.members.begin + result.members.count};
    for (auto row = result.members.begin; row < end; ++row) {
        if (!components.extent_bytes()[row] || !components.element_facts()[row]) {
            complete = false;
            offset.reset();
            continue;
        }
        auto const extent{*components.extent_bytes()[row]};
        auto const member_alignment{alignments[components.element_facts()[row]->value]};
        alignment = std::max(alignment, member_alignment);
        largest = std::max(largest, extent);
        if (kind != Kind::record) {
            components.offset_bytes()[row] = 0;
            continue;
        }
        if (!offset) {
            continue;
        }
        auto const aligned{align_up(*offset, member_alignment)};
        if (!aligned) {
            offset.reset();
            complete = false;
            context.report({DiagnosticSeverity::error, "Record aligned offset overflows uint64."});
            continue;
        }
        components.offset_bytes()[row] = aligned;
        components.padding_before_bytes()[row] = *aligned - *offset;
        auto const next{checked_add(*aligned, extent)};
        auto const next_payload{checked_add(payload, extent)};
        auto const next_padding{checked_add(padding, *components.padding_before_bytes()[row])};
        if (!next || !next_payload || !next_padding) {
            complete = false;
            offset.reset();
            context.report(
                {DiagnosticSeverity::error, "Record layout arithmetic overflows uint64."});
        } else {
            offset = next;
            payload = *next_payload;
            padding = *next_padding;
        }
    }
    if (complete) {
        auto const content{kind == Kind::record ? *offset : largest};
        auto const size{align_up(content, alignment)};
        if (!size) {
            context.report(
                {DiagnosticSeverity::error, "Aggregate tail alignment overflows uint64."});
        } else if (kind == Kind::record) {
            result.size_bytes = result.members.count == 0 ? 1 : *size;
            result.alignment_bytes = alignment;
            result.payload_bytes = payload;
            result.internal_padding_bytes = padding;
            result.tail_padding_bytes = *size - content;
        } else {
            result.largest_alternative_bytes = largest;
            for (auto row = result.members.begin; row < end; ++row) {
                components.slack_bytes()[row] = *size - *components.extent_bytes()[row];
            }
            result.size_bytes = size;
            result.alignment_bytes = alignment;
            result.tail_padding_bytes = *size - largest;
        }
    }
    auto const spelling{types.strings().view(types.cpp_spelling(type))};
    auto const supplied{abi.find(std::string{spelling})};
    if (result.size_bytes && result.alignment_bytes && supplied &&
        (supplied->size_bytes != *result.size_bytes ||
         supplied->alignment_bytes != *result.alignment_bytes)) {
        context.report(
            {DiagnosticSeverity::error,
             std::string{spelling} +
                 ": supplied complete-object facts conflict with the derived LispB layout (" +
                 supplied->provenance + ")."});
        result.size_bytes.reset();
        result.alignment_bytes.reset();
    }
    if (result.size_bytes && result.alignment_bytes) {
        auto& rows{context.storage.physical_facts};
        auto const fact{PhysicalFactId{rows.num()}};
        rows.add(*result.size_bytes,
                 *result.alignment_bytes,
                 lispb::schema::EnumSignedness::unspecified,
                 std::nullopt,
                 context.storage.strings.intern("Derived from LispB '" + std::string{spelling} +
                                                "' using profile '" + abi.name() + "'."),
                 FactOrigin::derived);
        context.storage.types.physical()[id.value] = fact;
    }
    context.storage.types.aggregate()[id.value] = result;
}

auto physical_inputs(AnalysisContext& context, lispb::schema::TypeId type, ResolvedVariant const&)
    -> std::vector<lispb::schema::ResolvedOccurrenceId> {
    auto const& types{context.types};
    auto const definition{types.definition(type)};
    auto const& tables{types.definitions()};
    using Kind = lispb::schema::TypeDefinitionKind;
    std::vector<lispb::schema::ResolvedOccurrenceId> uses;
    if (types.strings().view(types.cpp_spelling(type)).empty()) {
        return uses;
    }
    switch (definition.kind()) {
        case Kind::record:
            if (tables.records.layout[tables.records.index(definition)] ==
                lispb::codegen::LayoutKind::packed) {
                break;
            }
            for (auto member : tables.records.members_of(definition)) {
                uses.push_back(tables.children.semantic_type[tables.children.index(member)]);
            }
            break;
        case Kind::union_type:
            if (tables.unions.layout[tables.unions.index(definition)] ==
                lispb::codegen::LayoutKind::packed) {
                break;
            }
            for (auto member : tables.unions.alternatives_of(definition)) {
                uses.push_back(tables.children.semantic_type[tables.children.index(member)]);
            }
            break;
        case Kind::enumeration:
            uses.push_back(tables.enums.underlying_type[tables.enums.index(definition)]);
            break;
        case Kind::integer_scalar:
            uses.push_back(tables.integer_scalars
                               .cpp_representation[tables.integer_scalars.index(definition)]);
            break;
        default:
            break;
    }
    return uses;
}

void derive_leaf(AnalysisContext& context,
                 AnalysisId id,
                 ScenarioId scenario,
                 AbiProfile const& abi,
                 ResolvedVariant const& variant,
                 bool required) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type()[id.value]};
    auto const definition{types.definition(type)};
    auto const& definitions{types.definitions()};
    using Kind = lispb::schema::TypeDefinitionKind;
    auto& facts{context.storage.types.physical()[id.value]};
    if (lispb::schema::is_packed_projection(types, type)) {
        try {
            facts = physical_spelling(context,
                                      scenario,
                                      abi,
                                      effective_storage_type(types, type, definition, variant),
                                      types.module(type));
        } catch (std::exception const& error) {
            context.report({DiagnosticSeverity::error, error.what()});
        }
        return;
    }
    if (types.origin(type) == lispb::schema::TypeOrigin::declaration &&
        types.strings().view(types.cpp_spelling(type)).empty()) {
        return;
    }
    switch (definition.kind()) {
        case Kind::record:
        case Kind::union_type:
            if (!types.strings().view(types.cpp_spelling(type)).empty()) {
                aggregate_layout(context, id, scenario, abi);
            }
            return;
        case Kind::external: {
            auto const cpp{definitions.externals.cpp_type[definitions.externals.index(definition)]};
            auto const physical{classify_physical_type_use(types.cpp_types(), cpp)};
            facts = physical_use(
                context, scenario, abi, type, cpp, physical.form, physical.object_spelling, false);
            return;
        }
        case Kind::enumeration: {
            auto const occurrence{
                definitions.enums.underlying_type[definitions.enums.index(definition)]};
            if (lispb::schema::is_valid(occurrence)) {
                facts = physical_occurrence(context, scenario, abi, occurrence);
            } else if (auto const backing{derived_enum_backing_type(types, definition)}) {
                facts = lookup_physical(context, abi, *backing);
            }
            return;
        }
        case Kind::soa: {
            auto const spelling{types.strings().view(types.cpp_spelling(type))};
            if (!spelling.empty()) {
                facts = abi.find(std::string{spelling}).transform([&](auto const& value) {
                    return retain_facts(context, value);
                });
            }
            break;
        }
        default:
            break;
    }
    if (required && !facts) {
        context.report({DiagnosticSeverity::warning,
                        "Type '" + std::string{types.strings().view(types.name(type))} +
                            "' has no supported complete-object representation."});
    }
}

void analyze_physical_layouts(AnalysisContext& context,
                              ScenarioId scenario,
                              AnalysisRequest const& request,
                              ResolvedVariant const& variant) {
    auto const& types{context.types};
    auto const count{static_cast<std::size_t>(std::ranges::distance(types.types()))};
    std::vector<std::vector<lispb::schema::TypeId>> users(count);
    std::vector<std::size_t> remaining(count);
    auto dependency = [&](lispb::schema::TypeId owner, lispb::schema::TypeId child) {
        users[child.value].push_back(owner);
        ++remaining[owner.value];
    };
    auto const& occurrences{types.occurrences()};
    for (auto const type : types.types()) {
        for (auto const use : physical_inputs(context, type, variant)) {
            if (!lispb::schema::is_valid(use)) {
                continue;
            }
            auto const row{occurrences.index(use)};
            if (occurrences.names_semantic_type[row] &&
                occurrences.physical_form[row] == PhysicalTypeForm::value &&
                lispb::schema::is_valid(occurrences.type[row])) {
                dependency(type, occurrences.type[row]);
            }
        }
        if (auto const override = variant.overrides.packed_storage_types.find(type);
            override != variant.overrides.packed_storage_types.end()) {
            auto const target{
                types.find_named(types.strings().find(override->second), types.module(type))};
            auto const form{classify_physical_type_use(types.cpp_types(),
                                                       types.cpp_types().intern(override->second))
                                .form};
            if (target && form == PhysicalTypeForm::value) {
                dependency(type, *target);
            }
        }
    }
    std::vector<lispb::schema::TypeId> ready;
    for (auto const type : types.types()) {
        if (remaining[type.value] == 0) {
            ready.push_back(type);
        }
    }
    for (std::size_t index{}; index < ready.size(); ++index) {
        auto const type{ready[index]};
        derive_leaf(context,
                    context.storage.types.find(type, scenario),
                    scenario,
                    *request.abi,
                    variant,
                    !users[type.value].empty());
        for (auto const user : users[type.value]) {
            if (--remaining[user.value] == 0) {
                ready.push_back(user);
            }
        }
    }
    for (auto const type : types.types()) {
        if (remaining[type.value] != 0) {
            context.report({DiagnosticSeverity::error,
                            "Illegal by-value physical dependency cycle involving '" +
                                std::string{types.strings().view(types.name(type))} + "'."});
        }
    }
}
}
