#pragma once

#include <ioj/layout/analyzer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
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

inline auto entity_id_type(bool const include_reserved = false,
                           bool const constrain_index = false,
                           std::optional<codegen::PackedByteOrder> const byte_order = std::nullopt,
                           std::optional<codegen::PackedBitOrder> const bit_order = std::nullopt)
    -> TypeFixture {
    codegen::NormalModuleSchema enums{};
    enums.settings.name = "entity_types";
    enums.settings.header = "EntityType.h";
    enums.settings.namespace_name = "project";
    codegen::EnumSchema enumeration{};
    enumeration.name = "EntityType";
    enumeration.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    enumeration.values = {codegen::EnumeratorSchema{.name = "PlayerShip",
                                                    .initializer = "0",
                                                    .display_name = std::nullopt,
                                                    .hidden = false,
                                                    .serialized_name = std::nullopt},
                          codegen::EnumeratorSchema{.name = "COUNT",
                                                    .initializer = "1",
                                                    .display_name = std::nullopt,
                                                    .hidden = false,
                                                    .serialized_name = std::nullopt}};
    enumeration.count = "COUNT";
    enums.declarations.push_back(std::move(enumeration));

    codegen::NormalModuleSchema packed{};
    packed.settings.name = "entity_ids";
    packed.settings.header = "EntityId.h";
    codegen::PackedValueSchema packed_value{};
    packed_value.name = "EntityUniqueId";
    packed_value.storage_type.name = "std::uint32_t";
    codegen::PackedFieldSchema index{};
    index.name = "index";
    index.type.name = "std::uint32_t";
    index.bits = include_reserved ? 20 : 24;
    if (constrain_index) {
        index.bits.reset();
        index.minimum_value = 0;
        index.maximum_value = 1'000'000;
        index.named_codes = {{.name = "Player", .value = 42, .sentinel = false},
                             {.name = "Invalid", .value = 1'048'575, .sentinel = true},
                             {.name = "Pending", .value = 1'048'574, .sentinel = true}};
        index.relationship = codegen::SemanticRelationSchema{
            .kind = codegen::SemanticRelationKind::index_into,
            .target =
                codegen::TypeRef{.name = "@entity_type", .suffix = {}, .nested = std::nullopt},
            .unit = std::nullopt};
    }
    codegen::PackedFieldSchema entity_type{};
    entity_type.name = "entity_type";
    entity_type.type.name = "@entity_type";
    entity_type.bits = 8;
    entity_type.kind = codegen::PackedFieldKind::enumeration;
    packed_value.segments.emplace_back(std::move(index));
    if (include_reserved) {
        packed_value.segments.emplace_back(
            codegen::PackedReservedBitsSchema{.name = "future", .bits = 4});
    }
    packed_value.segments.emplace_back(std::move(entity_type));
    packed_value.invalid_value = 0xffffffff;
    packed_value.byte_order = byte_order;
    packed_value.bit_order = bit_order;
    packed.declarations.push_back(std::move(packed_value));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.types.emplace("entity_type", codegen::CppType{"project::EntityType"});
    manifest.modules = {std::move(enums), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("entity_ids", "EntityUniqueId")};
    return {std::move(types), type};
}

inline auto signed_delta_type(bool const constrained = false, bool const full_range = false)
    -> TypeFixture {
    codegen::NormalModuleSchema packed{};
    packed.settings.name = "signed_values";
    packed.settings.header = "SignedValues.h";
    codegen::PackedValueSchema value{};
    value.name = "SignedDelta";
    value.storage_type.name = full_range ? "std::uint64_t" : "std::uint32_t";
    auto field{codegen::PackedFieldSchema{
        .name = "delta",
        .type = codegen::TypeRef{.name = full_range ? "std::int64_t" : "std::int32_t",
                                 .suffix = {},
                                 .nested = std::nullopt},
        .bits = constrained || full_range ? std::optional<int>{} : std::optional<int>{17},
        .kind = codegen::PackedFieldKind::signed_integer,
        .range_helper = false,
        .minimum_value = full_range  ? std::optional<codegen::PackedIntegerValue>{(
                                          std::numeric_limits<std::int64_t>::min)()}
                       : constrained ? std::optional<codegen::PackedIntegerValue>{-100}
                                     : std::nullopt,
        .maximum_value = full_range  ? std::optional<codegen::PackedIntegerValue>{(
                                          std::numeric_limits<std::int64_t>::max)()}
                       : constrained ? std::optional<codegen::PackedIntegerValue>{100}
                                     : std::nullopt,
        .named_codes = constrained ? std::vector<codegen::PackedNamedCodeSchema>{{.name = "Unknown",
                                                                                  .value = -128,
                                                                                  .sentinel = true}}
                                   : std::vector<codegen::PackedNamedCodeSchema>{},
        .relationship = std::nullopt}};
    value.segments.emplace_back(std::move(field));
    if (!full_range) {
        value.segments.emplace_back(
            codegen::PackedReservedBitsSchema{.name = "future", .bits = constrained ? 24 : 15});
    }
    packed.declarations.push_back(std::move(value));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("signed_values", "SignedDelta")};
    return {std::move(types), type};
}

inline auto packed_integer_scalar_type() -> TypeFixture {
    codegen::IntegerScalarSchema scalar{};
    scalar.name = "Health";
    scalar.minimum_value = 0;
    scalar.maximum_value = 1000;
    scalar.named_codes = {{.name = "Invalid", .value = 4095, .sentinel = true}};

    codegen::NormalModuleSchema domains{};
    domains.settings.name = "domains";
    domains.settings.header = "Domains.h";
    domains.declarations.push_back(std::move(scalar));

    codegen::PackedFieldSchema health{};
    health.name = "health";
    health.type.name = "Health";
    health.bits.reset();

    codegen::PackedValueSchema status{};
    status.name = "Status";
    status.storage_type.name = "std::uint16_t";
    status.segments.emplace_back(std::move(health));

    codegen::NormalModuleSchema packed{};
    packed.settings.name = "packed";
    packed.settings.header = "Packed.h";
    packed.declarations.push_back(std::move(status));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(domains), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("packed", "Status")};
    return {std::move(types), type};
}

inline auto packed_linear_quantized_type() -> TypeFixture {
    codegen::NormalModuleSchema domains{};
    domains.settings.name = "domains";
    domains.settings.header = "Domains.h";
    domains.settings.namespace_name = "project";
    codegen::IntegerScalarSchema health{};
    health.name = "Health";
    health.minimum_value = 0;
    health.maximum_value = 1000;
    domains.declarations.push_back(std::move(health));

    codegen::NormalModuleSchema representations{};
    representations.settings.name = "representations";
    representations.settings.header = "Representations.h";
    representations.settings.namespace_name = "project";
    codegen::LinearQuantizedSchema health_q8{};
    health_q8.name = "HealthQ8";
    health_q8.source.name = "project::Health";
    health_q8.bit_width = 8;
    health_q8.reserved_codes = 2;
    health_q8.clipping = codegen::QuantizationClipping::clamp;
    representations.declarations.push_back(std::move(health_q8));

    codegen::NormalModuleSchema packed{};
    packed.settings.name = "packed";
    packed.settings.header = "Packed.h";
    packed.settings.namespace_name = "project";
    codegen::PackedValueSchema status{};
    status.name = "Status";
    status.storage_type.name = "std::uint16_t";
    codegen::PackedFieldSchema encoded_health{};
    encoded_health.name = "health";
    encoded_health.type.name = "project::HealthQ8";
    encoded_health.bits.reset();
    encoded_health.kind = codegen::PackedFieldKind::linear_quantized;
    status.segments.emplace_back(std::move(encoded_health));
    codegen::PackedFieldSchema state{};
    state.name = "state";
    state.type.name = "std::uint8_t";
    state.bits = 8;
    status.segments.emplace_back(std::move(state));
    packed.declarations.push_back(std::move(status));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(domains), std::move(representations), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("packed", "Status")};
    return {std::move(types), type};
}

inline auto packed_fixed_point_type() -> TypeFixture {
    codegen::NormalModuleSchema representations{};
    representations.settings.name = "representations";
    representations.settings.header = "Representations.h";
    representations.settings.namespace_name = "project";
    representations.declarations.push_back(
        codegen::FixedPointSchema{.name = "VelocityQ8_4",
                                  .signedness = true,
                                  .total_bits = 12,
                                  .fractional_bits = 4,
                                  .rounding = codegen::FixedPointRounding::toward_zero});

    codegen::NormalModuleSchema packed{};
    packed.settings.name = "packed";
    packed.settings.header = "Packed.h";
    packed.settings.namespace_name = "project";
    codegen::PackedValueSchema motion{};
    motion.name = "Motion";
    motion.storage_type.name = "std::uint16_t";
    codegen::PackedFieldSchema velocity{};
    velocity.name = "velocity";
    velocity.type.name = "project::VelocityQ8_4";
    velocity.bits.reset();
    velocity.kind = codegen::PackedFieldKind::fixed_point;
    motion.segments.emplace_back(std::move(velocity));
    motion.segments.emplace_back(codegen::PackedReservedBitsSchema{.name = "future", .bits = 4});
    packed.declarations.push_back(std::move(motion));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(representations), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("packed", "Motion")};
    return {std::move(types), type};
}

inline auto integer_scalar_type() -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
            .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                .header = "SemanticValues.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = std::nullopt,
                                                .include_order = {},
                                                .prelude_lines = {}},
            .declarations = {codegen::IntegerScalarSchema{
                .name = "DamageReason",
                .signedness = false,
                .minimum_value = 0,
                .maximum_value = 10,
                .bit_width = std::nullopt,
                .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false},
                                {.name = "Invalid", .value = 15, .sentinel = true}},
                .relationship = std::nullopt}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("semantic_values", "DamageReason")};
    return {std::move(types), type};
}

inline auto optional_sentinel_type() -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {codegen::IntegerScalarSchema{
                            .name = "DamageReason",
                            .signedness = false,
                            .minimum_value = 0,
                            .maximum_value = 10,
                            .bit_width = std::nullopt,
                            .named_codes = {{.name = "Invalid", .value = 15, .sentinel = true},
                                            {.name = "Pending", .value = 14, .sentinel = true}},
                            .relationship = std::nullopt}}},
                    codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {codegen::OptionalSentinelSchema{
                            .name = "OptionalDamageReason",
                            .source = codegen::TypeRef{.name = "project::DamageReason",
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                            .sentinel = "Invalid"}}}},
    };
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "OptionalDamageReason")};
    return {std::move(types), type};
}

inline auto optional_presence_bit_type(std::uint32_t const source_bits = 4) -> TypeFixture {
    auto const full_width{source_bits == 64};
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules =
            {codegen::NormalModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                     .header = "SemanticValues.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .declarations = {codegen::IntegerScalarSchema{
                     .name = "DamageReason",
                     .signedness = false,
                     .minimum_value = 0,
                     .maximum_value = full_width ? codegen::PackedIntegerValue{(
                                                       std::numeric_limits<std::uint64_t>::max)()}
                                                 : codegen::PackedIntegerValue{10},
                     .bit_width = source_bits,
                     .named_codes =
                         full_width
                             ? std::vector<codegen::PackedNamedCodeSchema>{}
                             : std::vector<codegen::PackedNamedCodeSchema>{{.name = "Invalid",
                                                                            .value = 15,
                                                                            .sentinel = true},
                                                                           {.name = "Pending",
                                                                            .value = 14,
                                                                            .sentinel = true}},
                     .relationship = std::nullopt}}},
             codegen::NormalModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "representations",
                                                     .header = "Representations.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .declarations = {codegen::OptionalPresenceBitSchema{
                     .name = "PresentDamageReason",
                     .source = codegen::TypeRef{.name = "project::DamageReason",
                                                .suffix = {},
                                                .nested = std::nullopt}}}}},
    };
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "PresentDamageReason")};
    return {std::move(types), type};
}

struct OptionalComparisonFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId source;
    lispb::schema::TypeId sentinel;
    lispb::schema::TypeId second_sentinel;
    lispb::schema::TypeId presence;
    lispb::schema::TypeId other_presence;
};

inline auto optional_comparison_types(bool const wide = false) -> OptionalComparisonFixture {
    auto const maximum{
        wide ? codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)() - 1}
             : codegen::PackedIntegerValue{10}};
    auto const invalid{
        wide ? codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()}
             : codegen::PackedIntegerValue{15}};
    auto const pending{
        wide ? codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)() - 1}
             : codegen::PackedIntegerValue{14}};
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules =
            {codegen::NormalModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                     .header = "SemanticValues.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .declarations =
                     {codegen::IntegerScalarSchema{
                          .name = "DamageReason",
                          .signedness = false,
                          .minimum_value = 0,
                          .maximum_value = maximum,
                          .bit_width = wide ? std::optional<std::uint32_t>{64}
                                            : std::optional<std::uint32_t>{4},
                          .named_codes =
                              wide ? std::vector<codegen::PackedNamedCodeSchema>{{.name = "Invalid",
                                                                                  .value = invalid,
                                                                                  .sentinel = true}}
                                   : std::vector<codegen::PackedNamedCodeSchema>{{.name = "Invalid",
                                                                                  .value = invalid,
                                                                                  .sentinel = true},
                                                                                 {.name = "Pending",
                                                                                  .value = pending,
                                                                                  .sentinel =
                                                                                      true}},
                          .relationship = std::nullopt},
                      codegen::IntegerScalarSchema{.name = "Other",
                                                   .signedness = false,
                                                   .minimum_value = 0,
                                                   .maximum_value = 3,
                                                   .bit_width = 2,
                                                   .named_codes = {},
                                                   .relationship = std::nullopt}}},
             codegen::NormalModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "representations",
                                                     .header = "Representations.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .declarations = {codegen::OptionalSentinelSchema{
                                      .name = "OptionalDamageReason",
                                      .source = codegen::TypeRef{.name = "project::DamageReason",
                                                                 .suffix = {},
                                                                 .nested = std::nullopt},
                                      .sentinel = "Invalid"},
                                  codegen::OptionalPresenceBitSchema{
                                      .name = "PresentDamageReason",
                                      .source = codegen::TypeRef{.name = "project::DamageReason",
                                                                 .suffix = {},
                                                                 .nested = std::nullopt}},
                                  codegen::OptionalPresenceBitSchema{
                                      .name = "PresentOther",
                                      .source = codegen::TypeRef{.name = "project::Other",
                                                                 .suffix = {},
                                                                 .nested = std::nullopt}}}}},
    };
    if (!wide) {
        auto& declarations{
            std::get<codegen::NormalModuleSchema>(manifest.modules.back()).declarations};
        declarations.insert(declarations.begin() + 1,
                            codegen::OptionalSentinelSchema{
                                .name = "OptionalDamageReasonPending",
                                .source = codegen::TypeRef{.name = "project::DamageReason",
                                                           .suffix = {},
                                                           .nested = std::nullopt},
                                .sentinel = "Pending"});
    }
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const source{*types.find_declared("semantic_values", "DamageReason")};
    auto const sentinel{*types.find_declared("representations", "OptionalDamageReason")};
    auto const second_sentinel{
        wide ? sentinel : *types.find_declared("representations", "OptionalDamageReasonPending")};
    auto const presence{*types.find_declared("representations", "PresentDamageReason")};
    auto const other_presence{*types.find_declared("representations", "PresentOther")};
    return {std::move(types), source, sentinel, second_sentinel, presence, other_presence};
}

inline auto linear_quantized_type(std::uint32_t const bit_width = 8,
                                  std::uint64_t const reserved_codes = 1,
                                  bool const signedness = false,
                                  codegen::PackedIntegerValue const minimum_value = 0,
                                  codegen::PackedIntegerValue const maximum_value = 1000)
    -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {codegen::IntegerScalarSchema{
                            .name = "Health",
                            .signedness = signedness,
                            .minimum_value = minimum_value,
                            .maximum_value = maximum_value,
                            .bit_width = std::nullopt,
                            .named_codes = {},
                            .relationship = std::nullopt}}},
                    codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {codegen::LinearQuantizedSchema{
                            .name = "HealthQuantized",
                            .source = codegen::TypeRef{.name = "project::Health",
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                            .bit_width = bit_width,
                            .reserved_codes = reserved_codes,
                            .clipping = codegen::QuantizationClipping::clamp}}}},
    };
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "HealthQuantized")};
    return {std::move(types), type};
}

inline auto linear_quantized_pair(bool const same_source = true,
                                  std::uint32_t const first_bits = 8,
                                  std::uint32_t const second_bits = 10,
                                  bool const signedness = false,
                                  codegen::PackedIntegerValue const minimum_value = 0,
                                  codegen::PackedIntegerValue const maximum_value = 1000)
    -> QuantizedComparisonFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {
            codegen::NormalModuleSchema{
                .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                    .header = "SemanticValues.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .declarations = {codegen::IntegerScalarSchema{.name = "Health",
                                                              .signedness = signedness,
                                                              .minimum_value = minimum_value,
                                                              .maximum_value = maximum_value,
                                                              .bit_width = std::nullopt,
                                                              .named_codes = {},
                                                              .relationship = std::nullopt},
                                 codegen::IntegerScalarSchema{.name = "Shield",
                                                              .signedness = false,
                                                              .minimum_value = 0,
                                                              .maximum_value = 2000,
                                                              .bit_width = std::nullopt,
                                                              .named_codes = {},
                                                              .relationship = std::nullopt}}},
            codegen::NormalModuleSchema{
                .settings = codegen::ModuleSettings{.name = "representations",
                                                    .header = "Representations.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .declarations = {
                    codegen::LinearQuantizedSchema{
                        .name = "HealthQ8",
                        .source = codegen::TypeRef{.name = "project::Health",
                                                   .suffix = {},
                                                   .nested = std::nullopt},
                        .bit_width = first_bits,
                        .reserved_codes = 1,
                        .clipping = codegen::QuantizationClipping::clamp},
                    codegen::LinearQuantizedSchema{
                        .name = "HealthQ10",
                        .source = codegen::TypeRef{.name = same_source ? "project::Health"
                                                                       : "project::Shield",
                                                   .suffix = {},
                                                   .nested = std::nullopt},
                        .bit_width = second_bits,
                        .reserved_codes = 2,
                        .clipping = codegen::QuantizationClipping::reject}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const first{*types.find_declared("representations", "HealthQ8")};
    auto const second{*types.find_declared("representations", "HealthQ10")};
    return {std::move(types), first, second};
}

inline auto fixed_point_type(bool const signedness,
                             std::uint32_t const total_bits,
                             std::uint32_t const fractional_bits,
                             codegen::FixedPointRounding const rounding =
                                 codegen::FixedPointRounding::nearest_even) -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
            .settings = codegen::ModuleSettings{.name = "representations",
                                                .header = "Representations.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = "project",
                                                .include_order = {},
                                                .prelude_lines = {}},
            .declarations = {codegen::FixedPointSchema{.name = "ValueFixed",
                                                       .signedness = signedness,
                                                       .total_bits = total_bits,
                                                       .fractional_bits = fractional_bits,
                                                       .rounding = rounding}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "ValueFixed")};
    return {std::move(types), type};
}

inline auto mini_float_type(std::uint32_t const sign_bits,
                            std::uint32_t const exponent_bits,
                            std::uint32_t const significand_bits,
                            std::int32_t const exponent_bias) -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
            .settings = codegen::ModuleSettings{.name = "representations",
                                                .header = "Representations.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = "project",
                                                .include_order = {},
                                                .prelude_lines = {}},
            .declarations = {codegen::MiniFloatSchema{.name = "CompactFloat",
                                                      .sign_bits = sign_bits,
                                                      .exponent_bits = exponent_bits,
                                                      .significand_bits = significand_bits,
                                                      .exponent_bias = exponent_bias}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "CompactFloat")};
    return {std::move(types), type};
}

inline auto
    integer_varint_type(bool const signedness,
                        codegen::PackedIntegerValue const minimum,
                        codegen::PackedIntegerValue const maximum,
                        codegen::IntegerVarintEncoding const encoding,
                        std::optional<codegen::PackedIntegerValue> const sentinel = std::nullopt)
        -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {
            codegen::NormalModuleSchema{
                .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                    .header = "SemanticValues.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .declarations = {codegen::IntegerScalarSchema{
                    .name = "Value",
                    .signedness = signedness,
                    .minimum_value = minimum,
                    .maximum_value = maximum,
                    .bit_width = std::nullopt,
                    .named_codes = sentinel.has_value()
                                     ? std::vector{codegen::PackedNamedCodeSchema{
                                           .name = "Invalid", .value = *sentinel, .sentinel = true}}
                                     : std::vector<codegen::PackedNamedCodeSchema>{},
                    .relationship = std::nullopt}}},
            codegen::NormalModuleSchema{.settings =
                                            codegen::ModuleSettings{.name = "representations",
                                                                    .header = "Representations.h",
                                                                    .source = std::nullopt,
                                                                    .header_include = std::nullopt,
                                                                    .namespace_name = "project",
                                                                    .include_order = {},
                                                                    .prelude_lines = {}},
                                        .declarations = {codegen::IntegerVarintSchema{
                                            .name = "ValueVarint",
                                            .source = codegen::TypeRef{.name = "project::Value",
                                                                       .suffix = {},
                                                                       .nested = std::nullopt},
                                            .encoding = encoding}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "ValueVarint")};
    return {std::move(types), type};
}

inline auto integer_varint_pair(bool const same_source = true) -> VarintComparisonFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {codegen::IntegerScalarSchema{.name = "Value",
                                                                      .signedness = true,
                                                                      .minimum_value = -65,
                                                                      .maximum_value = 64,
                                                                      .bit_width = std::nullopt,
                                                                      .named_codes = {},
                                                                      .relationship = std::nullopt},
                                         codegen::IntegerScalarSchema{.name = "Other",
                                                                      .signedness = true,
                                                                      .minimum_value = -1000,
                                                                      .maximum_value = 1000,
                                                                      .bit_width = std::nullopt,
                                                                      .named_codes = {},
                                                                      .relationship =
                                                                          std::nullopt}}},
                    codegen::NormalModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .declarations = {
                            codegen::IntegerVarintSchema{
                                .name = "ValueSigned",
                                .source = codegen::TypeRef{.name = "project::Value",
                                                           .suffix = {},
                                                           .nested = std::nullopt},
                                .encoding = codegen::IntegerVarintEncoding::signed_varint},
                            codegen::IntegerVarintSchema{
                                .name = "ValueZigZag",
                                .source = codegen::TypeRef{.name = same_source ? "project::Value"
                                                                               : "project::Other",
                                                           .suffix = {},
                                                           .nested = std::nullopt},
                                .encoding = codegen::IntegerVarintEncoding::zigzag_varint}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const first{*types.find_declared("representations", "ValueSigned")};
    auto const second{*types.find_declared("representations", "ValueZigZag")};
    return {std::move(types), first, second};
}

inline auto soa_type(std::vector<std::pair<std::string, std::string>> columns = {
                         {"min_xs", "float"},
                         {"min_ys", "float"},
                         {"min_zs", "float"},
                         {"max_xs", "float"},
                         {"max_ys", "float"},
                         {"max_zs", "float"}}) -> TypeFixture {
    std::vector<codegen::SoaMemberSchema> members;
    members.reserve(columns.size());
    for (auto& [name, type] : columns) {
        codegen::SoaMemberSchema member{};
        member.name = std::move(name);
        member.kind = codegen::SoaMemberKind::array;
        member.type.name = std::move(type);
        members.push_back(std::move(member));
    }
    codegen::NormalModuleSchema soa{};
    soa.settings.name = "soa";
    soa.settings.header = "Soa.h";
    soa.soa_backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema schema{};
    schema.name = "Columns";
    schema.members = std::move(members);
    soa.declarations.push_back(std::move(schema));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(soa)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("soa", "Columns")};
    return {std::move(types), type};
}

inline auto relationship_capacity_type(
    codegen::SemanticRelationKind const kind,
    std::uint32_t const bits,
    std::optional<std::uint64_t> const sentinel = std::nullopt,
    std::optional<std::uint64_t> const second_sentinel = std::nullopt,
    codegen::SemanticRelationUnit const offset_unit = codegen::SemanticRelationUnit::elements)
    -> RelationshipCapacityFixture {
    codegen::NormalModuleSchema soa{};
    soa.settings.name = "entities";
    soa.settings.header = "Entities.h";
    soa.soa_backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema entities{};
    entities.name = "Entities";
    entities.members = {codegen::SoaMemberSchema{
        .name = "values",
        .kind = codegen::SoaMemberKind::array,
        .type = codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
        .fixed_schema = std::nullopt,
        .nested_schema = std::nullopt,
        .mask_field = false,
        .mask_dimensions = {},
        .relationship = std::nullopt}};
    soa.declarations.push_back(std::move(entities));

    codegen::NormalModuleSchema packed{};
    packed.settings.name = "handles";
    packed.settings.header = "Handles.h";
    std::vector<codegen::PackedNamedCodeSchema> named_codes;
    if (sentinel.has_value()) {
        named_codes.push_back({.name = "Invalid", .value = *sentinel, .sentinel = true});
    }
    if (second_sentinel.has_value()) {
        named_codes.push_back({.name = "Pending", .value = *second_sentinel, .sentinel = true});
    }
    codegen::PackedFieldSchema value{
        .name = "value",
        .type = codegen::TypeRef{.name = "std::uint64_t", .suffix = {}, .nested = std::nullopt},
        .bits = static_cast<int>(bits),
        .kind = codegen::PackedFieldKind::unsigned_integer,
        .range_helper = false,
        .minimum_value =
            sentinel.has_value() ? std::optional<codegen::PackedIntegerValue>{0} : std::nullopt,
        .maximum_value = sentinel.has_value()
                           ? std::optional<codegen::PackedIntegerValue>{*sentinel - 1}
                           : std::nullopt,
        .named_codes = std::move(named_codes),
        .relationship = codegen::SemanticRelationSchema{
            .kind = kind,
            .target = codegen::TypeRef{.name = "Entities", .suffix = {}, .nested = std::nullopt},
            .unit = kind == codegen::SemanticRelationKind::offset_into ? std::optional{offset_unit}
                                                                       : std::nullopt}};
    packed.declarations = {codegen::PackedValueSchema{
        .name = "EntityValue",
        .storage_type =
            codegen::TypeRef{.name = "std::uint64_t", .suffix = {}, .nested = std::nullopt},
        .segments = {std::move(value)},
        .invalid_value = std::nullopt,
        .export_specifier = std::nullopt,
        .mutable_value = false,
        .byte_order = std::nullopt,
        .bit_order = std::nullopt}};

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(soa), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const packed_type{*types.find_declared("handles", "EntityValue")};
    auto const target{*types.find_declared("entities", "Entities")};
    return {std::move(types), packed_type, target};
}

inline auto relationship_capacity_scalar_type(
    codegen::SemanticRelationKind const kind,
    std::uint32_t const bits,
    std::uint64_t const maximum,
    std::optional<std::uint64_t> const sentinel = std::nullopt,
    codegen::SemanticRelationUnit const offset_unit = codegen::SemanticRelationUnit::elements)
    -> RelationshipCapacityFixture {
    codegen::NormalModuleSchema soa{};
    soa.settings.name = "entities";
    soa.settings.header = "Entities.h";
    soa.soa_backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema entities{};
    entities.name = "Entities";
    entities.members = {codegen::SoaMemberSchema{
        .name = "values",
        .kind = codegen::SoaMemberKind::array,
        .type = codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
        .fixed_schema = std::nullopt,
        .nested_schema = std::nullopt,
        .mask_field = false,
        .mask_dimensions = {},
        .relationship = std::nullopt}};
    soa.declarations.push_back(std::move(entities));

    codegen::NormalModuleSchema scalars{};
    scalars.settings.name = "handles";
    scalars.settings.header = "Handles.h";
    scalars.declarations = {codegen::IntegerScalarSchema{
        .name = "EntityValue",
        .signedness = false,
        .minimum_value = 0,
        .maximum_value = maximum,
        .bit_width = bits,
        .named_codes = sentinel.has_value()
                         ? std::vector<codegen::PackedNamedCodeSchema>{{.name = "Invalid",
                                                                        .value = *sentinel,
                                                                        .sentinel = true}}
                         : std::vector<codegen::PackedNamedCodeSchema>{},
        .relationship = codegen::SemanticRelationSchema{
            .kind = kind,
            .target = codegen::TypeRef{.name = "Entities", .suffix = {}, .nested = std::nullopt},
            .unit = kind == codegen::SemanticRelationKind::offset_into ? std::optional{offset_unit}
                                                                       : std::nullopt}}};

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(soa), std::move(scalars)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const scalar_type{*types.find_declared("handles", "EntityValue")};
    auto const target{*types.find_declared("entities", "Entities")};
    return {std::move(types), scalar_type, target};
}

inline auto enum_domain_type(std::vector<codegen::EnumeratorSchema> values,
                             std::optional<std::string> count = std::nullopt,
                             std::optional<std::string> underlying = "std::uint8_t",
                             std::optional<std::uint32_t> bit_width = std::nullopt,
                             std::optional<bool> signedness = std::nullopt) -> TypeFixture {
    codegen::NormalModuleSchema module{};
    module.settings.name = "domains";
    module.settings.header = "Domains.h";
    codegen::EnumSchema enumeration{};
    enumeration.name = "Domain";
    enumeration.underlying_type =
        underlying.has_value()
            ? std::optional{codegen::TypeRef{
                  .name = std::move(*underlying), .suffix = {}, .nested = std::nullopt}}
            : std::nullopt;
    enumeration.bit_width = bit_width;
    enumeration.signedness = signedness;
    enumeration.values = std::move(values);
    enumeration.count = std::move(count);
    module.declarations.push_back(std::move(enumeration));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("domains", "Domain")};
    return {std::move(types), type};
}

inline auto enum_value(std::string name,
                       std::optional<std::string> initializer,
                       bool const sentinel = false) -> codegen::EnumeratorSchema {
    return {.name = std::move(name),
            .initializer = std::move(initializer),
            .display_name = std::nullopt,
            .hidden = false,
            .serialized_name = std::nullopt,
            .sentinel = sentinel};
}

inline auto record_member(std::string name,
                          std::string type,
                          std::optional<std::uint64_t> count = std::nullopt)
    -> codegen::RecordMemberSchema {
    return {.name = std::move(name),
            .type = codegen::TypeRef{.name = std::move(type), .suffix = {}, .nested = std::nullopt},
            .count = count,
            .relationship = std::nullopt};
}

inline auto record_type(std::vector<codegen::RecordSchema> records, std::string selected = "Record")
    -> TypeFixture {
    codegen::NormalModuleSchema module{};
    module.settings.name = "records";
    module.settings.header = "Records.h";
    module.declarations.assign(std::make_move_iterator(records.begin()),
                               std::make_move_iterator(records.end()));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("records", selected)};
    return {std::move(types), type};
}

inline auto union_alternative(std::string name,
                              std::string type,
                              std::optional<std::uint64_t> count = std::nullopt)
    -> codegen::UnionAlternativeSchema {
    return {.name = std::move(name),
            .type = codegen::TypeRef{.name = std::move(type), .suffix = {}, .nested = std::nullopt},
            .count = count};
}

inline auto union_type(std::vector<codegen::UnionSchema> unions, std::string selected = "Union")
    -> TypeFixture {
    codegen::NormalModuleSchema module{};
    module.settings.name = "unions";
    module.settings.header = "Unions.h";
    module.declarations.assign(std::make_move_iterator(unions.begin()),
                               std::make_move_iterator(unions.end()));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("unions", selected)};
    return {std::move(types), type};
}

inline auto tagged_union_type() -> TypeFixture {
    codegen::NormalModuleSchema enums{};
    enums.settings.name = "events";
    enums.settings.header = "Events.h";
    enums.settings.namespace_name = "events";
    codegen::EnumSchema kind{};
    kind.name = "Kind";
    kind.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    codegen::EnumeratorSchema small_tag{};
    small_tag.name = "Small";
    codegen::EnumeratorSchema bytes_tag{};
    bytes_tag.name = "Bytes";
    codegen::EnumeratorSchema spare_tag{};
    spare_tag.name = "Spare";
    codegen::EnumeratorSchema invalid_tag{};
    invalid_tag.name = "Invalid";
    invalid_tag.sentinel = true;
    codegen::EnumeratorSchema count_tag{};
    count_tag.name = "COUNT";
    kind.values = {std::move(small_tag),
                   std::move(bytes_tag),
                   std::move(spare_tag),
                   std::move(invalid_tag),
                   std::move(count_tag)};
    kind.count = "COUNT";
    enums.declarations.push_back(std::move(kind));

    codegen::NormalModuleSchema unions{};
    unions.settings.name = "payloads";
    unions.settings.header = "Payloads.h";
    unions.settings.namespace_name = "payloads";
    codegen::TaggedUnionSchema event{};
    event.name = "Event";
    event.discriminant.name = "events::Kind";
    codegen::TaggedUnionAlternativeSchema small{};
    small.name = "small";
    small.type.name = "std::uint32_t";
    small.tag = "Small";
    codegen::TaggedUnionAlternativeSchema bytes{};
    bytes.name = "bytes";
    bytes.type.name = "std::uint8_t";
    bytes.count = 10;
    bytes.tag = "Bytes";
    event.alternatives = {std::move(small), std::move(bytes)};
    unions.declarations.push_back(std::move(event));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(enums), std::move(unions)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("payloads", "Event")};
    return {std::move(types), type};
}

} // namespace
} // namespace ioj::layout
