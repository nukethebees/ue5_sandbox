#include "aggregate_pass.hpp"

#include "../checked_arithmetic.hpp"
#include "../enum_backing.hpp"
#include "../relationship_extent.hpp"
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analyzer/physical_type_spelling.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>
namespace ioj::layout::analysis_detail {
void analyze_enum_backing(AnalysisContext& context, AnalysisId id) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type[id.value]};
    auto const enumeration{types.definition(type)};
    auto const& domain{*context.storage.scalars.enum_domain[context.storage.scalars.find(type)]};
    auto const& domains{types.enum_domains()};
    auto const domain_id{
        types.definitions().enums.domain[types.definitions().enums.index(enumeration)]};
    auto const domain_row{domains.index(domain_id)};
    auto const backing_type{
        lispb::schema::is_valid(
            types.definitions().enums.underlying_type[types.definitions().enums.index(enumeration)])
            ? physical_type_spelling(
                  types,
                  types.definitions()
                      .enums.underlying_type[types.definitions().enums.index(enumeration)])
                  .value_or(std::string{types.cpp_types().spelling(
                      types.occurrences().cpp_type[types.occurrences().index(
                          types.definitions().enums.underlying_type[types.definitions().enums.index(
                              enumeration)])])})
            : derived_enum_backing_type(types, enumeration).value_or("Unknown")};
    auto const facts{context.storage.physical_facts.get_const_view().resolve()};
    EnumBacking result;
    result.backing_type = context.storage.strings.intern(backing_type);
    result.backing_facts = context.storage.types.physical[id.value];
    if (backing_type == "Unknown") {
        context.report(
            {DiagnosticSeverity::error,
             "Enum C++ backing storage cannot be derived because semantic signedness or width is "
             "unknown."});
    } else if (!result.backing_facts.has_value()) {
        context.report({DiagnosticSeverity::warning,
                        "Unknown physical facts for enum backing type '" + backing_type + "'."});
    } else {
        result.backing_bits = checked_multiply(facts.size_bytes()[result.backing_facts->value], 8);
        if (!result.backing_bits.has_value()) {
            context.report({DiagnosticSeverity::error, "Enum backing bit count overflows uint64."});
        } else if (*result.backing_bits == 0) {
            result.backing_bits.reset();
            context.report(
                {DiagnosticSeverity::error, "Enum backing type has a zero-byte physical size."});
        }
    }

    if (domain.minimum_value.has_value() && domain.maximum_value.has_value() &&
        domain.minimum_required_bits.has_value()) {
        if (result.backing_facts.has_value() && result.backing_bits.has_value()) {
            result.backing_can_represent_domain =
                enum_domain_fits_backing(*domain.minimum_value,
                                         *domain.maximum_value,
                                         facts.signedness()[result.backing_facts->value],
                                         facts.unsigned_value_bits()[result.backing_facts->value],
                                         *result.backing_bits);
            if (!result.backing_can_represent_domain.has_value()) {
                context.report({DiagnosticSeverity::warning,
                                "Integer range facts are unavailable for enum backing type '" +
                                    backing_type + "'; backing fit is unknown."});
            } else if (!*result.backing_can_represent_domain) {
                context.report(
                    {DiagnosticSeverity::error,
                     "Enum value domain " + lispb::schema::format_enum_code(*domain.minimum_value) +
                         " .. " + lispb::schema::format_enum_code(*domain.maximum_value) +
                         " does not fit backing type '" + backing_type + "'."});
            }
        }
        auto backing_code_bits{std::optional<std::uint64_t>{}};
        if (result.backing_facts.has_value() &&
            facts.signedness()[result.backing_facts->value] ==
                lispb::schema::EnumSignedness::unsigned_ &&
            facts.unsigned_value_bits()[result.backing_facts->value].has_value()) {
            backing_code_bits = *facts.unsigned_value_bits()[result.backing_facts->value];
        } else if (result.backing_facts.has_value() &&
                   facts.signedness()[result.backing_facts->value] ==
                       lispb::schema::EnumSignedness::signed_) {
            backing_code_bits = result.backing_bits;
        }
        if (backing_code_bits.has_value() && *backing_code_bits < 64) {
            auto const backing_codes{std::uint64_t{1} << *backing_code_bits};
            if (domains.distinct_code_count[domain_row] <= backing_codes) {
                result.unused_backing_codes =
                    backing_codes - domains.distinct_code_count[domain_row];
            }
        }
    }

    context.storage.types.enum_backing[id.value] = std::move(result);
}

void relationship_check(AnalysisContext& context,
                        AnalysisId owner,
                        std::optional<std::size_t> field,
                        lispb::schema::RelationshipId relationship,
                        lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> codes,
                        std::uint32_t width,
                        std::optional<lispb::codegen::PackedIntegerValue> minimum,
                        std::optional<lispb::codegen::PackedIntegerValue> maximum) {
    if (!lispb::schema::is_valid(relationship)) {
        return;
    }
    auto const& types{context.types};
    auto const& schema{types.relationships()};
    auto const schema_row{schema.index(relationship)};
    auto const kind{schema.kind[schema_row]};
    using Relation = lispb::codegen::SemanticRelationKind;
    if (kind != Relation::index_into && kind != Relation::count_of &&
        kind != Relation::offset_into) {
        return;
    }
    auto& relationships{context.storage.relationships};
    auto const row{relationships.append()};
    relationships.owner[row] = owner;
    relationships.field[row] = field;
    relationships.relationship[row] = relationship;
    auto const target_type{
        types.occurrences().type[types.occurrences().index(schema.target[schema_row])]};
    auto const target{
        context.storage.types.find(target_type, context.storage.types.scenario[owner.value])};
    auto const& allocation{context.storage.types.soa[target.value]};
    if (!allocation) {
        return;
    }
    std::optional<std::uint64_t> extent;
    if (kind == Relation::index_into || kind == Relation::count_of) {
        extent = allocation->capacity;
    } else if (schema.unit[schema_row]) {
        extent = *schema.unit[schema_row] == lispb::codegen::SemanticRelationUnit::elements
                   ? std::optional{allocation->capacity}
                   : allocation->total_allocation_bytes;
    }
    if (!extent) {
        return;
    }
    auto label{std::string{
        types.strings().view(types.name(context.storage.types.schema_type[owner.value]))}};
    if (field) {
        label += '.';
        label += context.storage.fields.name[*field];
    }
    relationships.metrics[row] = analyze_unsigned_relationship_extent(
        context, types.declarations(), label, kind, *extent, codes, width, minimum, maximum);
}

void analyze_relationships(AnalysisContext& context, ScenarioId scenario) {
    auto const& types{context.types};
    for (auto const type : types.types()) {
        auto const definition{types.definition(type)};
        auto const owner{context.storage.types.find(type, scenario)};
        if (definition.kind() == lispb::schema::TypeDefinitionKind::integer_scalar) {
            auto const& table{types.definitions().integer_scalars};
            auto const row{table.index(definition)};
            relationship_check(context,
                               owner,
                               {},
                               table.relationship[row],
                               table.named_codes[row],
                               table.bit_width[row],
                               table.minimum_value[row],
                               table.maximum_value[row]);
        } else if (auto const& packed = context.storage.types.packed[owner.value]; packed) {
            auto const& table{types.definitions().packed_fields};
            auto const end{packed->fields.begin + packed->fields.count};
            for (auto row = packed->fields.begin; row < end; ++row) {
                if (context.storage.fields.reserved[row]) {
                    continue;
                }
                auto const schema_row{table.index(context.storage.fields.segment[row])};
                relationship_check(context,
                                   owner,
                                   row,
                                   table.relationship[schema_row],
                                   table.named_codes[schema_row],
                                   context.storage.fields.bit_width[row],
                                   table.minimum_value[schema_row],
                                   table.maximum_value[schema_row]);
            }
        }
    }
}
}
