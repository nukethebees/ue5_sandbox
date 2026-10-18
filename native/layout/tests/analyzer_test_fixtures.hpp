#pragma once

#include <ioj/layout/analyzer.hpp>

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/enum_signedness.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

struct TypeFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId type;
};
struct QuantizedComparisonFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId first;
    lispb::schema::TypeId second;
};
struct VarintComparisonFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId first;
    lispb::schema::TypeId second;
};
struct RelationshipCapacityFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId packed;
    lispb::schema::TypeId target;
};
struct OptionalComparisonFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId source;
    lispb::schema::TypeId sentinel;
    lispb::schema::TypeId second_sentinel;
    lispb::schema::TypeId presence;
    lispb::schema::TypeId other_presence;
};

inline auto fixture_graph(lispb::CompilerStorage& storage,
                          std::vector<lispb::codegen::ModuleHandle> modules)
    -> lispb::schema::TypeGraph {
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::TypeRegistry{storage.registered_types},
                                      storage.declarations,
                                      std::move(modules)};
    return lispb::schema::resolve_type_graph(manifest);
}

inline auto fixture_manifest(lispb::CompilerStorage& storage, std::string_view source)
    -> lispb::codegen::Manifest {
    auto const file{storage.sources.add("layout-fixture.lispb", source)};
    return {lispb::codegen::manifest_schema_version,
            lispb::codegen::TypeRegistry{storage.registered_types},
            storage.declarations,
            lispb::codegen::parse_module_source(storage, file).modules};
}

inline auto fixture_graph(lispb::CompilerStorage& storage, std::string_view source)
    -> lispb::schema::TypeGraph {
    return lispb::schema::resolve_type_graph(fixture_manifest(storage, source));
}

inline auto fixture_declaration(lispb::CompilerStorage& storage, std::string_view source)
    -> lispb::codegen::DeclarationHandle {
    auto const manifest{fixture_manifest(
        storage, "(module fixture :header \"Fixture.h\" " + std::string{source} + ")")};
    auto const module{manifest.modules.front()};
    return storage.declarations
        .items(storage.declarations.normal_modules
                   .declarations[storage.declarations.normal_modules.index(module)])
        .front();
}

inline auto fixture_find(lispb::schema::TypeGraph const& types,
                         std::string_view module,
                         std::string_view name) -> std::optional<lispb::schema::TypeId> {
    auto const module_name{types.strings().find(module)};
    auto const type_name{types.strings().find(name)};
    for (auto const node : types.types()) {
        if (types.module_name(node) == module_name && types.name(node) == type_name) {
            return types.find_declared(types.module(node), type_name);
        }
    }
    return std::nullopt;
}

inline auto fixture_type(lispb::CompilerStorage& storage,
                         std::string_view source,
                         std::string const& module,
                         std::string const& name) -> TypeFixture {
    auto types{fixture_graph(storage, source)};
    auto const type{*fixture_find(types, module, name)};
    return {std::move(types), type};
}

inline auto fixture_reference(lispb::CompilerStorage& storage,
                              std::string_view name,
                              std::string_view suffix = {}) -> lispb::codegen::TypeRefId {
    return [&] {
        auto const new_type_refs{storage.declarations.type_refs.append()};
        storage.declarations.type_refs.name[storage.declarations.type_refs.index(new_type_refs)] =
            storage.strings.intern(name);
        storage.declarations.type_refs.suffix[storage.declarations.type_refs.index(new_type_refs)] =
            storage.strings.add(suffix);
        return new_type_refs;
    }();
}

inline auto fixture_module(lispb::CompilerStorage& storage,
                           std::string_view name,
                           std::vector<lispb::codegen::DeclarationHandle> const& declarations)
    -> lispb::codegen::ModuleHandle {
    auto const settings{[&] {
        auto const new_module_settings_store{storage.declarations.module_settings_store.append()};
        storage.declarations.module_settings_store
            .name[storage.declarations.module_settings_store.index(new_module_settings_store)] =
            storage.strings.intern(name);
        storage.declarations.module_settings_store
            .header[storage.declarations.module_settings_store.index(new_module_settings_store)] =
            storage.strings.add(std::string{name} + ".h");
        return new_module_settings_store;
    }()};
    return [&] {
        auto const new_normal_modules{storage.declarations.normal_modules.append()};
        storage.declarations.normal_modules
            .settings[storage.declarations.normal_modules.index(new_normal_modules)] = settings;
        storage.declarations.normal_modules
            .declarations[storage.declarations.normal_modules.index(new_normal_modules)] =
            storage.declarations.range(declarations);
        return new_normal_modules;
    }();
}

inline auto scalar_source(std::string_view name,
                          bool const signedness,
                          lispb::codegen::PackedIntegerValue const minimum,
                          lispb::codegen::PackedIntegerValue const maximum) -> std::string {
    return "(integer-scalar " + std::string{name} + " :signed " + (signedness ? "true" : "false") +
           " :minimum " + lispb::codegen::format_packed_integer(minimum) + " :maximum " +
           lispb::codegen::format_packed_integer(maximum);
}

inline auto
    entity_id_type(lispb::CompilerStorage& storage,
                   bool const include_reserved = false,
                   bool const constrain_index = false,
                   std::optional<lispb::codegen::PackedByteOrder> const byte_order = std::nullopt,
                   std::optional<lispb::codegen::PackedBitOrder> const bit_order = std::nullopt)
        -> TypeFixture {
    std::string source{R"((module entity_types :header "EntityType.h" :namespace project
      (enum EntityType std::uint8_t :count COUNT
        (value PlayerShip :value "0") (value COUNT :value "1")))
      (module entity_ids :header "EntityId.h"
        (packed-value EntityUniqueId :storage std::uint32_t :invalid-value 4294967295 )"};
    if (byte_order) {
        source += *byte_order == lispb::codegen::PackedByteOrder::little_endian
                    ? " :byte-order little"
                    : " :byte-order big";
    }
    if (bit_order) {
        source += *bit_order == lispb::codegen::PackedBitOrder::least_significant_first
                    ? " :bit-order lsb-first"
                    : " :bit-order msb-first";
    }
    source += " (field index std::uint32_t :bits ";
    source += constrain_index  ? "auto :minimum 0 :maximum 1000000 (code Player :value 42) (code "
                                 "Invalid :value 1048575 :sentinel true) (code Pending :value "
                                 "1048574 :sentinel true) (relation index_into project::EntityType)"
            : include_reserved ? "20"
                               : "24";
    source += ')';
    if (include_reserved) {
        source += " (reserved future :bits 4)";
    }
    source += " (field entity_type project::EntityType :bits 8 :kind enum)))";
    return fixture_type(storage, source, "entity_ids", "EntityUniqueId");
}

inline auto signed_delta_type(lispb::CompilerStorage& storage,
                              bool const constrained = false,
                              bool const full_range = false) -> TypeFixture {
    std::string source{
        R"((module signed_values :header "SignedValues.h" (packed-value SignedDelta :storage )"};
    source +=
        full_range ? "std::uint64_t (field delta std::int64_t :bits auto :kind signed :minimum "
                     "-9223372036854775808 :maximum 9223372036854775807)"
        : constrained
            ? "std::uint32_t (field delta std::int32_t :bits auto :kind signed :minimum -100 "
              ":maximum 100 (code Unknown :value -128 :sentinel true)) (reserved future :bits 24)"
            : "std::uint32_t (field delta std::int32_t :bits 17 :kind signed) (reserved future "
              ":bits 15)";
    source += "))";
    return fixture_type(storage, source, "signed_values", "SignedDelta");
}

inline auto packed_integer_scalar_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"(
      (module domains :header "Domains.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 1000
          (code Invalid :value 4095 :sentinel true)))
      (module packed :header "Packed.h"
        (packed-value Status :storage std::uint16_t (field health Health :bits auto))))",
                        "packed",
                        "Status");
}

inline auto packed_linear_quantized_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"(
      (module domains :header "Domains.h" :namespace project
        (integer-scalar Health :signed false :minimum 0 :maximum 1000))
      (module representations :header "Representations.h" :namespace project
        (linear-quantized HealthQ8 :source project::Health :bits 8 :reserved-codes 2 :clipping clamp))
      (module packed :header "Packed.h" :namespace project
        (packed-value Status :storage std::uint16_t
          (field health project::HealthQ8 :bits auto :kind linear-quantized)
          (field state std::uint8_t :bits 8))))",
                        "packed",
                        "Status");
}

inline auto packed_fixed_point_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"(
      (module representations :header "Representations.h" :namespace project
        (fixed-point VelocityQ8_4 :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero))
      (module packed :header "Packed.h" :namespace project
        (packed-value Motion :storage std::uint16_t
          (field velocity project::VelocityQ8_4 :bits auto :kind fixed-point)
          (reserved future :bits 4))))",
                        "packed",
                        "Motion");
}

inline auto integer_scalar_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"((module semantic_values :header "SemanticValues.h"
      (integer-scalar DamageReason :signed false :minimum 0 :maximum 10
        (code Unknown :value 0) (code Invalid :value 15 :sentinel true))))",
                        "semantic_values",
                        "DamageReason");
}

inline auto optional_sentinel_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"(
      (module semantic_values :header "SemanticValues.h" :namespace project
        (integer-scalar DamageReason :signed false :minimum 0 :maximum 10
          (code Invalid :value 15 :sentinel true) (code Pending :value 14 :sentinel true)))
      (module representations :header "Representations.h" :namespace project
        (optional-sentinel OptionalDamageReason :source project::DamageReason :sentinel Invalid)))",
                        "representations",
                        "OptionalDamageReason");
}

inline auto optional_presence_bit_type(lispb::CompilerStorage& storage,
                                       std::uint32_t const source_bits = 4) -> TypeFixture {
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project )"};
    source += scalar_source("DamageReason",
                            false,
                            0,
                            source_bits == 64 ? (std::numeric_limits<std::uint64_t>::max)() : 10);
    source += " :bit-width " + std::to_string(source_bits);
    if (source_bits != 64) {
        source += " (code Invalid :value 15 :sentinel true)"
                  " (code Pending :value 14 :sentinel true)";
    }
    source += R"()) (module representations :header "Representations.h" :namespace project
      (optional-presence-bit PresentDamageReason :source project::DamageReason)))";
    return fixture_type(storage, source, "representations", "PresentDamageReason");
}

inline auto optional_comparison_types(lispb::CompilerStorage& storage, bool const wide = false)
    -> OptionalComparisonFixture {
    auto const maximum{wide ? (std::numeric_limits<std::uint64_t>::max)() - 1 : 10};
    auto const invalid{wide ? (std::numeric_limits<std::uint64_t>::max)() : 15};
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project )"};
    source += scalar_source("DamageReason", false, 0, maximum) + " :bit-width " +
              (wide ? "64" : "4") + " (code Invalid :value " + std::to_string(invalid) +
              " :sentinel true)";
    if (!wide) {
        source += " (code Pending :value 14 :sentinel true)";
    }
    source += R"() (integer-scalar Other :signed false :minimum 0 :maximum 3 :bit-width 2))
      (module representations :header "Representations.h" :namespace project
        (optional-sentinel OptionalDamageReason :source project::DamageReason :sentinel Invalid))";
    if (!wide) {
        source += " (optional-sentinel OptionalDamageReasonPending :source project::DamageReason "
                  ":sentinel Pending)";
    }
    source += " (optional-presence-bit PresentDamageReason :source project::DamageReason) "
              "(optional-presence-bit PresentOther :source project::Other))";
    auto types{fixture_graph(storage, source)};
    auto const original{*fixture_find(types, "semantic_values", "DamageReason")};
    auto const sentinel{*fixture_find(types, "representations", "OptionalDamageReason")};
    auto const second{
        wide ? sentinel : *fixture_find(types, "representations", "OptionalDamageReasonPending")};
    auto const presence{*fixture_find(types, "representations", "PresentDamageReason")};
    auto const other{*fixture_find(types, "representations", "PresentOther")};
    return {std::move(types), original, sentinel, second, presence, other};
}

inline auto linear_quantized_type(lispb::CompilerStorage& storage,
                                  std::uint32_t const bit_width = 8,
                                  std::uint64_t const reserved_codes = 1,
                                  bool const signedness = false,
                                  lispb::codegen::PackedIntegerValue const minimum_value = 0,
                                  lispb::codegen::PackedIntegerValue const maximum_value = 1000)
    -> TypeFixture {
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project )"};
    source += scalar_source("Health", signedness, minimum_value, maximum_value);
    source += R"()) (module representations :header "Representations.h" :namespace project
      (linear-quantized HealthQuantized :source project::Health :bits )";
    source += std::to_string(bit_width) + " :reserved-codes " + std::to_string(reserved_codes) +
              " :clipping clamp))";
    return fixture_type(storage, source, "representations", "HealthQuantized");
}

inline auto linear_quantized_pair(lispb::CompilerStorage& storage,
                                  bool const same_source = true,
                                  std::uint32_t const first_bits = 8,
                                  std::uint32_t const second_bits = 10,
                                  bool const signedness = false,
                                  lispb::codegen::PackedIntegerValue const minimum_value = 0,
                                  lispb::codegen::PackedIntegerValue const maximum_value = 1000)
    -> QuantizedComparisonFixture {
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project )"};
    source += scalar_source("Health", signedness, minimum_value, maximum_value);
    source += R"() (integer-scalar Shield :signed false :minimum 0 :maximum 2000))
      (module representations :header "Representations.h" :namespace project
        (linear-quantized HealthQ8 :source project::Health :bits )";
    source += std::to_string(first_bits) +
              " :reserved-codes 1 :clipping clamp) (linear-quantized HealthQ10 :source " +
              (same_source ? "project::Health" : "project::Shield") + " :bits " +
              std::to_string(second_bits) + " :reserved-codes 2 :clipping reject))";
    auto types{fixture_graph(storage, source)};
    auto const first{*fixture_find(types, "representations", "HealthQ8")};
    auto const second{*fixture_find(types, "representations", "HealthQ10")};
    return {std::move(types), first, second};
}

inline auto fixed_point_type(lispb::CompilerStorage& storage,
                             bool const signedness,
                             std::uint32_t const total_bits,
                             std::uint32_t const fractional_bits,
                             lispb::codegen::FixedPointRounding const rounding =
                                 lispb::codegen::FixedPointRounding::nearest_even) -> TypeFixture {
    std::string source{R"((module representations :header "Representations.h" :namespace project
      (fixed-point ValueFixed :signed )"};
    source += std::string{signedness ? "true" : "false"} + " :total-bits " +
              std::to_string(total_bits) + " :fractional-bits " + std::to_string(fractional_bits) +
              " :rounding " +
              (rounding == lispb::codegen::FixedPointRounding::nearest_even ? "nearest-even"
                                                                            : "toward-zero") +
              "))";
    return fixture_type(storage, source, "representations", "ValueFixed");
}

inline auto mini_float_type(lispb::CompilerStorage& storage,
                            std::uint32_t const sign_bits,
                            std::uint32_t const exponent_bits,
                            std::uint32_t const significand_bits,
                            std::int32_t const exponent_bias) -> TypeFixture {
    std::string source{R"((module representations :header "Representations.h" :namespace project
      (mini-float CompactFloat :sign-bits )"};
    source += std::to_string(sign_bits) + " :exponent-bits " + std::to_string(exponent_bits) +
              " :significand-bits " + std::to_string(significand_bits) + " :bias " +
              std::to_string(exponent_bias) + "))";
    return fixture_type(storage, source, "representations", "CompactFloat");
}

inline auto integer_varint_type(lispb::CompilerStorage& storage,
                                bool const signedness,
                                lispb::codegen::PackedIntegerValue const minimum,
                                lispb::codegen::PackedIntegerValue const maximum,
                                lispb::codegen::IntegerVarintEncoding const encoding,
                                std::optional<lispb::codegen::PackedIntegerValue> const sentinel =
                                    std::nullopt) -> TypeFixture {
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project )"};
    source += scalar_source("Value", signedness, minimum, maximum);
    if (sentinel) {
        source += " (code Invalid :value " + lispb::codegen::format_packed_integer(*sentinel) +
                  " :sentinel true)";
    }
    source += R"()) (module representations :header "Representations.h" :namespace project
      (integer-varint ValueVarint :source project::Value :encoding )";
    source += encoding == lispb::codegen::IntegerVarintEncoding::signed_varint ? "signed"
            : encoding == lispb::codegen::IntegerVarintEncoding::zigzag_varint ? "zigzag"
                                                                               : "unsigned";
    source += "))";
    return fixture_type(storage, source, "representations", "ValueVarint");
}

inline auto integer_varint_pair(lispb::CompilerStorage& storage, bool const same_source = true)
    -> VarintComparisonFixture {
    std::string source{R"((module semantic_values :header "SemanticValues.h" :namespace project
      (integer-scalar Value :signed true :minimum -65 :maximum 64)
      (integer-scalar Other :signed true :minimum -1000 :maximum 1000))
      (module representations :header "Representations.h" :namespace project
        (integer-varint ValueSigned :source project::Value :encoding signed)
        (integer-varint ValueZigZag :source )"};
    source +=
        std::string{same_source ? "project::Value" : "project::Other"} + " :encoding zigzag))";
    auto types{fixture_graph(storage, source)};
    auto const first{*fixture_find(types, "representations", "ValueSigned")};
    auto const second{*fixture_find(types, "representations", "ValueZigZag")};
    return {std::move(types), first, second};
}

inline auto soa_type(lispb::CompilerStorage& storage,
                     std::vector<std::pair<std::string, std::string>> columns = {
                         {"min_xs", "float"},
                         {"min_ys", "float"},
                         {"min_zs", "float"},
                         {"max_xs", "float"},
                         {"max_ys", "float"},
                         {"max_zs", "float"}}) -> TypeFixture {
    std::string source{R"((module soa :header "Soa.h" :backend standard-library (struct Columns )"};
    for (auto const& [name, type] : columns) {
        source += "(member " + name + " array " + type + ")";
    }
    source += "))";
    return fixture_type(storage, source, "soa", "Columns");
}

inline auto capacity_relation(lispb::codegen::SemanticRelationKind const kind,
                              lispb::codegen::SemanticRelationUnit const unit) -> std::string {
    std::string result{" (relation "};
    result += lispb::codegen::semantic_relation_kind_name(kind);
    result += " Entities";
    if (kind == lispb::codegen::SemanticRelationKind::offset_into) {
        result += " :unit ";
        result += lispb::codegen::semantic_relation_unit_name(unit);
    }
    result += ')';
    return result;
}

inline auto
    relationship_capacity_type(lispb::CompilerStorage& storage,
                               lispb::codegen::SemanticRelationKind const kind,
                               std::uint32_t const bits,
                               std::optional<std::uint64_t> const sentinel = std::nullopt,
                               std::optional<std::uint64_t> const second_sentinel = std::nullopt,
                               lispb::codegen::SemanticRelationUnit const offset_unit =
                                   lispb::codegen::SemanticRelationUnit::elements)
        -> RelationshipCapacityFixture {
    std::string source{R"((module entities :header "Entities.h" :backend standard-library
      (struct Entities (member values array std::uint8_t)))
      (module handles :header "Handles.h" (packed-value EntityValue :storage std::uint64_t
        (field value std::uint64_t :bits )"};
    source += std::to_string(bits);
    if (sentinel) {
        source += " :minimum 0 :maximum " + std::to_string(*sentinel - 1) +
                  " (code Invalid :value " + std::to_string(*sentinel) + " :sentinel true)";
    }
    if (second_sentinel) {
        source += " (code Pending :value " + std::to_string(*second_sentinel) + " :sentinel true)";
    }
    source += capacity_relation(kind, offset_unit) + ")))";
    auto types{fixture_graph(storage, source)};
    auto const packed{*fixture_find(types, "handles", "EntityValue")};
    auto const target{*fixture_find(types, "entities", "Entities")};
    return {std::move(types), packed, target};
}

inline auto
    relationship_capacity_scalar_type(lispb::CompilerStorage& storage,
                                      lispb::codegen::SemanticRelationKind const kind,
                                      std::uint32_t const bits,
                                      std::uint64_t const maximum,
                                      std::optional<std::uint64_t> const sentinel = std::nullopt,
                                      lispb::codegen::SemanticRelationUnit const offset_unit =
                                          lispb::codegen::SemanticRelationUnit::elements)
        -> RelationshipCapacityFixture {
    std::string source{R"((module entities :header "Entities.h" :backend standard-library
      (struct Entities (member values array std::uint8_t)))
      (module handles :header "Handles.h" )"};
    source +=
        scalar_source("EntityValue", false, 0, maximum) + " :bit-width " + std::to_string(bits);
    if (sentinel) {
        source += " (code Invalid :value " + std::to_string(*sentinel) + " :sentinel true)";
    }
    source += capacity_relation(kind, offset_unit) + "))";
    auto types{fixture_graph(storage, source)};
    auto const scalar{*fixture_find(types, "handles", "EntityValue")};
    auto const target{*fixture_find(types, "entities", "Entities")};
    return {std::move(types), scalar, target};
}

inline auto enum_domain_type(lispb::CompilerStorage& storage,
                             std::vector<lispb::codegen::EnumeratorId> const& values,
                             std::optional<std::string> const& count = std::nullopt,
                             std::optional<std::string> const& underlying = "std::uint8_t",
                             std::optional<std::uint8_t> const bit_width = std::nullopt,
                             lispb::schema::EnumSignedness const signedness =
                                 lispb::schema::EnumSignedness::unspecified) -> TypeFixture {
    auto const declaration{[&] {
        auto const new_enums{storage.declarations.enums.append()};
        storage.declarations.enums.name[storage.declarations.enums.index(new_enums)] =
            storage.strings.intern("Domain");
        storage.declarations.enums.underlying_type[storage.declarations.enums.index(new_enums)] =
            underlying ? std::optional{fixture_reference(storage, *underlying)} : std::nullopt;
        storage.declarations.enums.values[storage.declarations.enums.index(new_enums)] =
            storage.declarations.range(values);
        storage.declarations.enums.count[storage.declarations.enums.index(new_enums)] =
            count ? std::optional{storage.strings.intern(*count)} : std::nullopt;
        storage.declarations.enums.bit_width[storage.declarations.enums.index(new_enums)] =
            bit_width;
        storage.declarations.enums.signedness[storage.declarations.enums.index(new_enums)] =
            signedness;
        return new_enums;
    }()};
    auto types{fixture_graph(storage, {fixture_module(storage, "domains", {declaration})})};
    auto const type{*fixture_find(types, "domains", "Domain")};
    return {std::move(types), type};
}

inline auto enum_value(lispb::CompilerStorage& storage,
                       std::string_view name,
                       std::optional<std::string> const& initializer,
                       bool const sentinel = false) -> lispb::codegen::EnumeratorId {
    return [&] {
        auto const new_enumerators{storage.declarations.enumerators.append()};
        storage.declarations.enumerators
            .name[storage.declarations.enumerators.index(new_enumerators)] =
            storage.strings.intern(name);
        storage.declarations.enumerators
            .initializer[storage.declarations.enumerators.index(new_enumerators)] =
            initializer ? std::optional{storage.strings.add(*initializer)} : std::nullopt;
        storage.declarations.enumerators
            .sentinel[storage.declarations.enumerators.index(new_enumerators)] = sentinel;
        return new_enumerators;
    }();
}

inline auto record_member(lispb::CompilerStorage& storage,
                          std::string_view name,
                          std::string_view type,
                          std::optional<std::uint64_t> const count = std::nullopt)
    -> lispb::codegen::RecordMemberId {
    auto& members{storage.declarations.record_members};
    auto const id{members.append()};
    auto const index{members.index(id)};
    members.name[index] = storage.strings.intern(name);
    members.type[index] = fixture_reference(storage, type);
    members.count[index] = count;
    return id;
}

inline auto record_members(lispb::CompilerStorage& storage,
                           std::initializer_list<lispb::codegen::RecordMemberId> values)
    -> lispb::codegen::SchemaRange<lispb::codegen::RecordMemberId> {
    std::vector<lispb::codegen::RecordMemberId> ids;
    for (auto const value : values) {
        ids.push_back(value);
    }
    return storage.declarations.range(ids);
}

inline auto record_type(lispb::CompilerStorage& storage,
                        std::vector<lispb::codegen::DeclarationHandle> const& records,
                        std::string const& selected = "Record") -> TypeFixture {
    std::vector<lispb::codegen::DeclarationHandle> declarations;
    for (auto const& record : records) {
        declarations.push_back(record);
    }
    auto types{fixture_graph(storage, {fixture_module(storage, "records", declarations)})};
    auto const type{*fixture_find(types, "records", selected)};
    return {std::move(types), type};
}

inline auto union_alternative(lispb::CompilerStorage& storage,
                              std::string_view name,
                              std::string_view type,
                              std::optional<std::uint64_t> const count = std::nullopt)
    -> lispb::codegen::UnionAlternativeId {
    auto& alternatives{storage.declarations.union_alternatives};
    auto const id{alternatives.append()};
    auto const index{alternatives.index(id)};
    alternatives.name[index] = storage.strings.intern(name);
    alternatives.type[index] = fixture_reference(storage, type);
    alternatives.count[index] = count;
    return id;
}

inline auto union_alternatives(lispb::CompilerStorage& storage,
                               std::initializer_list<lispb::codegen::UnionAlternativeId> values)
    -> lispb::codegen::SchemaRange<lispb::codegen::UnionAlternativeId> {
    std::vector<lispb::codegen::UnionAlternativeId> ids;
    for (auto const value : values) {
        ids.push_back(value);
    }
    return storage.declarations.range(ids);
}

inline auto union_type(lispb::CompilerStorage& storage,
                       std::vector<lispb::codegen::DeclarationHandle> const& unions,
                       std::string const& selected = "Union") -> TypeFixture {
    std::vector<lispb::codegen::DeclarationHandle> declarations;
    for (auto const& value : unions) {
        declarations.push_back(value);
    }
    auto types{fixture_graph(storage, {fixture_module(storage, "unions", declarations)})};
    auto const type{*fixture_find(types, "unions", selected)};
    return {std::move(types), type};
}

inline auto tagged_union_type(lispb::CompilerStorage& storage) -> TypeFixture {
    return fixture_type(storage,
                        R"(
      (module events :header "Events.h" :namespace events
        (enum Kind std::uint8_t :count COUNT
          (value Small) (value Bytes) (value Spare) (value Invalid :sentinel true) (value COUNT)))
      (module payloads :header "Payloads.h" :namespace payloads
        (tagged-union Event :discriminant events::Kind
          (alternative small std::uint32_t :tag Small)
          (alternative bytes std::uint8_t :tag Bytes :count 10))))",
                        "payloads",
                        "Event");
}

} // namespace
} // namespace ioj::layout
