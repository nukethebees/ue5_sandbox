#include "packed_pass.hpp"

#include "../checked_arithmetic.hpp"
#include "../integer_code_domain.hpp"
#include "../variant_projection.hpp"
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/resolved_variant.hpp>

#include <codegen/schema/declarations.h>
#include <lispb/schema/bit_layout.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace ioj::layout::analysis_detail {
namespace {
auto field_kind(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> lispb::codegen::PackedFieldKind {
    using Kind = lispb::schema::TypeDefinitionKind;
    using Field = lispb::codegen::PackedFieldKind;
    switch (types.definition(type).kind()) {
        case Kind::enumeration:
            return Field::enumeration;
        case Kind::fixed_point:
            return Field::fixed_point;
        case Kind::linear_quantized:
            return Field::linear_quantized;
        case Kind::mini_float:
            return Field::mini_float;
        default:
            if (auto const domain{lispb::schema::integer_domain(types, type)}) {
                auto const& scalars{types.definitions().integer_scalars};
                return scalars.signedness[scalars.index(*domain)] ? Field::signed_integer
                                                                  : Field::unsigned_integer;
            }
            return Field::unsigned_integer;
    }
}
}

void
    analyze_packed_layout(AnalysisContext& context, AnalysisId id, ResolvedVariant const& variant) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type()[id.value]};
    auto const definition{types.definition(type)};
    auto const& baseline{types.bit_layout(type)};
    PackedLayout result{.schema_storage_type = context.storage.strings.intern(
                            baseline.storage_bits == 0
                                ? ""
                                : "std::uint" + std::to_string(baseline.storage_bits) + "_t"),
                        .storage_type = context.storage.strings.intern(
                            baseline.storage_bits == 0
                                ? ""
                                : "std::uint" + std::to_string(baseline.storage_bits) + "_t"),
                        .storage_overridden = variant.overrides.packed_storage_types.contains(type),
                        .storage_facts = context.storage.types.physical()[id.value]};
    if (definition.kind() == lispb::schema::TypeDefinitionKind::record) {
        auto const& records{types.definitions().records};
        auto const row{records.index(definition)};
        result.byte_order = records.byte_order[row];
        result.bit_order = records.bit_order[row];
        result.invalid_raw_value = records.invalid_raw_value[row];
    }
    if (result.storage_facts) {
        auto const facts{context.storage.physical_facts.get_const_view().resolve()};
        result.storage_bits = checked_multiply(facts.size_bytes()[result.storage_facts->value], 8);
    }

    lispb::schema::BitLayout layout;
    try {
        layout = effective_bit_layout(types, type, variant);
    } catch (std::exception const& error) {
        context.report({DiagnosticSeverity::error, error.what()});
        context.storage.types.packed()[id.value] = result;
        return;
    }
    result.storage_type = context.storage.strings.intern(
        layout.storage_bits == 0 ? "" : "std::uint" + std::to_string(layout.storage_bits) + "_t");

    auto& fields{context.storage.fields};
    result.fields.begin = fields.num();
    auto const& children{types.definitions().children};
    std::uint64_t payload_mask{};
    std::uint64_t reserved_mask{};
    std::vector<std::uint64_t> child_payload(layout.placements.size());
    std::vector<std::uint64_t> child_reserved(layout.placements.size());
    for (auto index{layout.placements.size()}; index != 0;) {
        --index;
        auto const& placement{layout.placements[index]};
        if (placement.offset >= 64 || placement.extent > 64 - placement.offset) {
            continue;
        }
        auto const mask{placement.extent == 64
                            ? ~std::uint64_t{}
                            : ((std::uint64_t{1} << placement.extent) - 1) << placement.offset};
        if (placement.reserved) {
            child_reserved[index] |= mask;
        } else {
            // Exclude nested reserved regions unless another alternative uses their bits.
            child_payload[index] |= mask & ~(child_reserved[index] & ~child_payload[index]);
        }
        if (placement.parent) {
            child_payload[*placement.parent] |= child_payload[index];
            child_reserved[*placement.parent] |= child_reserved[index];
        } else {
            payload_mask |= child_payload[index];
            reserved_mask |= child_reserved[index];
        }
    }
    for (auto const& placement : layout.placements) {
        auto const child{children.index(placement.child)};
        auto const row{fields.append()};
        auto const columns{fields.get_view().resolve()};
        columns.parent()[row] = id;
        columns.segment()[row] = placement.child;
        columns.reserved()[row] = placement.reserved;
        columns.semantic_type()[row] =
            placement.reserved ? std::nullopt : std::optional{placement.type};
        columns.logical_type()[row] = context.storage.strings.intern(
            placement.reserved ? "Reserved bits"
                               : std::string{types.strings().view(types.name(placement.type))});
        columns.kind()[row] = placement.reserved ? lispb::codegen::PackedFieldKind::unsigned_integer
                                                 : field_kind(types, placement.type);
        columns.schema_bit_width_auto()[row] = !children.bits[child].has_value();
        auto const original{std::ranges::find(
            baseline.placements, placement.path, &lispb::schema::BitPlacement::path)};
        assert(original != baseline.placements.end());
        columns.name()[row] = original->path;
        columns.schema_bit_width()[row] =
            original == baseline.placements.end() ? placement.extent : original->extent;
        columns.bit_width()[row] = placement.extent;
        columns.overridden()[row] = columns.schema_bit_width()[row] != placement.extent;
        columns.least_significant_bit()[row] = placement.offset;
        if (placement.extent != 0) {
            columns.most_significant_bit()[row] = placement.offset + placement.extent - 1;
        }
        if (placement.reserved) {
            continue;
        }
        columns.maximum_unsigned_value()[row] = maximum_unsigned_value(placement.bits);
        if (auto const domain{lispb::schema::integer_domain(types, placement.type)}) {
            auto const& scalars{types.definitions().integer_scalars};
            auto const scalar{scalars.index(*domain)};
            auto const minimum{scalars.minimum_value[scalar]};
            auto const maximum{scalars.maximum_value[scalar]};
            columns.minimum_semantic_value()[row] = minimum;
            columns.maximum_semantic_value()[row] = maximum;
            columns.semantic_value_count()[row] = packed_range_count(minimum, maximum);
            columns.minimum_required_bits()[row] = scalars.bit_width[scalar];
            if (scalars.signedness[scalar]) {
                columns.minimum_signed_value()[row] = minimum_signed_value(placement.bits);
                columns.maximum_signed_value()[row] = maximum_signed_value(placement.bits);
            }
            auto& codes{context.storage.codes};
            columns.codes()[row].begin = codes.num();
            auto const& named{types.declarations().packed_named_codes};
            for (auto const code : types.declarations().items(scalars.named_codes[scalar])) {
                auto const code_row{named.index(code)};
                codes.add(types.strings().view(named.name[code_row]),
                          named.sentinel[code_row],
                          0,
                          {},
                          named.value[code_row]);
                ++columns.codes()[row].count;
                columns.sentinel_code_count()[row] += named.sentinel[code_row] != 0;
            }
            if (columns.semantic_value_count()[row]) {
                columns.required_code_count()[row] = checked_add(
                    *columns.semantic_value_count()[row], columns.sentinel_code_count()[row]);
            }
            if (placement.bits < 64 && columns.required_code_count()[row]) {
                columns.unused_codes()[row] =
                    (std::uint64_t{1} << placement.bits) - *columns.required_code_count()[row];
            }
        }
    }
    result.fields.count = fields.num() - result.fields.begin;
    result.bits_used = layout.encoded_bits;
    if (layout.encoded_bits && *layout.encoded_bits <= 64) {
        result.payload_bits = std::popcount(payload_mask);
        result.reserved_bits = std::popcount(reserved_mask & ~payload_mask);
    }
    if (result.storage_bits && result.bits_used) {
        result.overflow_bits =
            *result.bits_used > *result.storage_bits ? *result.bits_used - *result.storage_bits : 0;
        auto const occupied{
            static_cast<std::uint64_t>(std::popcount(payload_mask | reserved_mask))};
        result.unused_bits = occupied <= *result.storage_bits ? *result.storage_bits - occupied : 0;
        if (*result.overflow_bits != 0) {
            context.report(
                {DiagnosticSeverity::error, "Packed layout exceeds the selected storage width."});
        }
    }
    context.storage.types.packed()[id.value] = result;
}
}
