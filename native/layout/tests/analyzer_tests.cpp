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

auto entity_id_type(bool const include_reserved = false,
                    bool const constrain_index = false,
                    std::optional<codegen::PackedByteOrder> const byte_order = std::nullopt,
                    std::optional<codegen::PackedBitOrder> const bit_order = std::nullopt)
    -> TypeFixture {
    codegen::EnumModuleSchema enums{};
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
    enums.enums.push_back(std::move(enumeration));

    codegen::PackedValueModuleSchema packed{};
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
    packed.values.push_back(std::move(packed_value));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.types.emplace("entity_type", codegen::CppType{"project::EntityType"});
    manifest.modules = {std::move(enums), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("entity_ids", "EntityUniqueId")};
    return {std::move(types), type};
}

auto signed_delta_type(bool const constrained = false, bool const full_range = false)
    -> TypeFixture {
    codegen::PackedValueModuleSchema packed{};
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
    packed.values.push_back(std::move(value));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("signed_values", "SignedDelta")};
    return {std::move(types), type};
}

auto packed_integer_scalar_type() -> TypeFixture {
    codegen::IntegerScalarSchema scalar{};
    scalar.name = "Health";
    scalar.minimum_value = 0;
    scalar.maximum_value = 1000;
    scalar.named_codes = {{.name = "Invalid", .value = 4095, .sentinel = true}};

    codegen::ScalarModuleSchema domains{};
    domains.settings.name = "domains";
    domains.settings.header = "Domains.h";
    domains.scalars.push_back(std::move(scalar));

    codegen::PackedFieldSchema health{};
    health.name = "health";
    health.type.name = "Health";
    health.bits.reset();

    codegen::PackedValueSchema status{};
    status.name = "Status";
    status.storage_type.name = "std::uint16_t";
    status.segments.emplace_back(std::move(health));

    codegen::PackedValueModuleSchema packed{};
    packed.settings.name = "packed";
    packed.settings.header = "Packed.h";
    packed.values.push_back(std::move(status));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(domains), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("packed", "Status")};
    return {std::move(types), type};
}

auto packed_linear_quantized_type() -> TypeFixture {
    codegen::ScalarModuleSchema domains{};
    domains.settings.name = "domains";
    domains.settings.header = "Domains.h";
    domains.settings.namespace_name = "project";
    codegen::IntegerScalarSchema health{};
    health.name = "Health";
    health.minimum_value = 0;
    health.maximum_value = 1000;
    domains.scalars.push_back(std::move(health));

    codegen::RepresentationModuleSchema representations{};
    representations.settings.name = "representations";
    representations.settings.header = "Representations.h";
    representations.settings.namespace_name = "project";
    codegen::LinearQuantizedSchema health_q8{};
    health_q8.name = "HealthQ8";
    health_q8.source.name = "project::Health";
    health_q8.bit_width = 8;
    health_q8.reserved_codes = 2;
    health_q8.clipping = codegen::QuantizationClipping::clamp;
    representations.linear_quantized.push_back(std::move(health_q8));

    codegen::PackedValueModuleSchema packed{};
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
    packed.values.push_back(std::move(status));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(domains), std::move(representations), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("packed", "Status")};
    return {std::move(types), type};
}

auto integer_scalar_type() -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::ScalarModuleSchema{
            .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                .header = "SemanticValues.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = std::nullopt,
                                                .include_order = {},
                                                .prelude_lines = {}},
            .scalars = {codegen::IntegerScalarSchema{
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

auto optional_sentinel_type() -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::ScalarModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .scalars = {codegen::IntegerScalarSchema{
                            .name = "DamageReason",
                            .signedness = false,
                            .minimum_value = 0,
                            .maximum_value = 10,
                            .bit_width = std::nullopt,
                            .named_codes = {{.name = "Invalid", .value = 15, .sentinel = true},
                                            {.name = "Pending", .value = 14, .sentinel = true}},
                            .relationship = std::nullopt}}},
                    codegen::RepresentationModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .linear_quantized = {},
                        .integer_varints = {},
                        .fixed_points = {},
                        .optional_sentinels = {codegen::OptionalSentinelSchema{
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

auto optional_presence_bit_type(std::uint32_t const source_bits = 4) -> TypeFixture {
    auto const full_width{source_bits == 64};
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules =
            {codegen::ScalarModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                     .header = "SemanticValues.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .scalars = {codegen::IntegerScalarSchema{
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
             codegen::RepresentationModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "representations",
                                                     .header = "Representations.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .linear_quantized = {},
                 .integer_varints = {},
                 .fixed_points = {},
                 .optional_sentinels = {},
                 .optional_presence_bits = {codegen::OptionalPresenceBitSchema{
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

auto optional_comparison_types(bool const wide = false) -> OptionalComparisonFixture {
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
            {codegen::ScalarModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                     .header = "SemanticValues.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .scalars =
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
             codegen::RepresentationModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "representations",
                                                     .header = "Representations.h",
                                                     .source = std::nullopt,
                                                     .header_include = std::nullopt,
                                                     .namespace_name = "project",
                                                     .include_order = {},
                                                     .prelude_lines = {}},
                 .linear_quantized = {},
                 .integer_varints = {},
                 .fixed_points = {},
                 .optional_sentinels =
                     wide
                         ? std::vector<codegen::OptionalSentinelSchema>{{.name =
                                                                             "OptionalDamageReason",
                                                                         .source = codegen::
                                                                             TypeRef{.name = "p"
                                                                                             "r"
                                                                                             "o"
                                                                                             "j"
                                                                                             "e"
                                                                                             "c"
                                                                                             "t"
                                                                                             ":"
                                                                                             ":"
                                                                                             "D"
                                                                                             "a"
                                                                                             "m"
                                                                                             "a"
                                                                                             "g"
                                                                                             "e"
                                                                                             "R"
                                                                                             "e"
                                                                                             "a"
                                                                                             "s"
                                                                                             "o"
                                                                                             "n",
                                                                                     .suffix = {},
                                                                                     .nested = std::
                                                                                         nullopt},
                                                                         .sentinel = "Invalid"}}
                         : std::
                               vector<codegen::OptionalSentinelSchema>{{.name =
                                                                            "OptionalDamageReason",
                                                                        .source =
                                                                            codegen::TypeRef{
                                                                                .name = "project::"
                                                                                        "DamageReas"
                                                                                        "on",
                                                                                .suffix = {},
                                                                                .nested =
                                                                                    std::nullopt},
                                                                        .sentinel = "Invalid"},
                                                                       {.name = "OptionalDamageReas"
                                                                                "onPending",
                                                                        .source =
                                                                            codegen::TypeRef{
                                                                                .name = "project::"
                                                                                        "DamageReas"
                                                                                        "on",
                                                                                .suffix = {},
                                                                                .nested =
                                                                                    std::nullopt},
                                                                        .sentinel = "Pending"}},
                 .optional_presence_bits =
                     {codegen::OptionalPresenceBitSchema{
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
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const source{*types.find_declared("semantic_values", "DamageReason")};
    auto const sentinel{*types.find_declared("representations", "OptionalDamageReason")};
    auto const second_sentinel{
        wide ? sentinel : *types.find_declared("representations", "OptionalDamageReasonPending")};
    auto const presence{*types.find_declared("representations", "PresentDamageReason")};
    auto const other_presence{*types.find_declared("representations", "PresentOther")};
    return {std::move(types), source, sentinel, second_sentinel, presence, other_presence};
}

auto linear_quantized_type(std::uint32_t const bit_width = 8,
                           std::uint64_t const reserved_codes = 1,
                           bool const signedness = false,
                           codegen::PackedIntegerValue const minimum_value = 0,
                           codegen::PackedIntegerValue const maximum_value = 1000) -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::ScalarModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .scalars = {codegen::IntegerScalarSchema{.name = "Health",
                                                                 .signedness = signedness,
                                                                 .minimum_value = minimum_value,
                                                                 .maximum_value = maximum_value,
                                                                 .bit_width = std::nullopt,
                                                                 .named_codes = {},
                                                                 .relationship = std::nullopt}}},
                    codegen::RepresentationModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .linear_quantized = {codegen::LinearQuantizedSchema{
                            .name = "HealthQuantized",
                            .source = codegen::TypeRef{.name = "project::Health",
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                            .bit_width = bit_width,
                            .reserved_codes = reserved_codes,
                            .clipping = codegen::QuantizationClipping::clamp}},
                        .integer_varints = {},
                        .fixed_points = {},
                        .optional_sentinels = {}}},
    };
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "HealthQuantized")};
    return {std::move(types), type};
}

auto linear_quantized_pair(bool const same_source = true,
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
            codegen::ScalarModuleSchema{
                .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                    .header = "SemanticValues.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .scalars = {codegen::IntegerScalarSchema{.name = "Health",
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
            codegen::RepresentationModuleSchema{
                .settings = codegen::ModuleSettings{.name = "representations",
                                                    .header = "Representations.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .linear_quantized = {codegen::LinearQuantizedSchema{
                                         .name = "HealthQ8",
                                         .source = codegen::TypeRef{.name = "project::Health",
                                                                    .suffix = {},
                                                                    .nested = std::nullopt},
                                         .bit_width = first_bits,
                                         .reserved_codes = 1,
                                         .clipping = codegen::QuantizationClipping::clamp},
                                     codegen::LinearQuantizedSchema{
                                         .name = "HealthQ10",
                                         .source = codegen::TypeRef{.name = same_source
                                                                              ? "project::Health"
                                                                              : "project::Shield",
                                                                    .suffix = {},
                                                                    .nested = std::nullopt},
                                         .bit_width = second_bits,
                                         .reserved_codes = 2,
                                         .clipping = codegen::QuantizationClipping::reject}},
                .integer_varints = {},
                .fixed_points = {},
                .optional_sentinels = {}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const first{*types.find_declared("representations", "HealthQ8")};
    auto const second{*types.find_declared("representations", "HealthQ10")};
    return {std::move(types), first, second};
}

auto fixed_point_type(bool const signedness,
                      std::uint32_t const total_bits,
                      std::uint32_t const fractional_bits,
                      codegen::FixedPointRounding const rounding =
                          codegen::FixedPointRounding::nearest_even) -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::RepresentationModuleSchema{
            .settings = codegen::ModuleSettings{.name = "representations",
                                                .header = "Representations.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = "project",
                                                .include_order = {},
                                                .prelude_lines = {}},
            .linear_quantized = {},
            .integer_varints = {},
            .fixed_points = {codegen::FixedPointSchema{.name = "ValueFixed",
                                                       .signedness = signedness,
                                                       .total_bits = total_bits,
                                                       .fractional_bits = fractional_bits,
                                                       .rounding = rounding}},
            .optional_sentinels = {}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "ValueFixed")};
    return {std::move(types), type};
}

auto mini_float_type(std::uint32_t const sign_bits,
                     std::uint32_t const exponent_bits,
                     std::uint32_t const significand_bits,
                     std::int32_t const exponent_bias) -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::RepresentationModuleSchema{
            .settings = codegen::ModuleSettings{.name = "representations",
                                                .header = "Representations.h",
                                                .source = std::nullopt,
                                                .header_include = std::nullopt,
                                                .namespace_name = "project",
                                                .include_order = {},
                                                .prelude_lines = {}},
            .linear_quantized = {},
            .integer_varints = {},
            .fixed_points = {},
            .optional_sentinels = {},
            .optional_presence_bits = {},
            .mini_floats = {codegen::MiniFloatSchema{.name = "CompactFloat",
                                                     .sign_bits = sign_bits,
                                                     .exponent_bits = exponent_bits,
                                                     .significand_bits = significand_bits,
                                                     .exponent_bias = exponent_bias}}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "CompactFloat")};
    return {std::move(types), type};
}

auto integer_varint_type(bool const signedness,
                         codegen::PackedIntegerValue const minimum,
                         codegen::PackedIntegerValue const maximum,
                         codegen::IntegerVarintEncoding const encoding,
                         std::optional<codegen::PackedIntegerValue> const sentinel = std::nullopt)
    -> TypeFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {
            codegen::ScalarModuleSchema{
                .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                    .header = "SemanticValues.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .scalars = {codegen::IntegerScalarSchema{
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
            codegen::RepresentationModuleSchema{
                .settings = codegen::ModuleSettings{.name = "representations",
                                                    .header = "Representations.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = "project",
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .linear_quantized = {},
                .integer_varints = {codegen::IntegerVarintSchema{
                    .name = "ValueVarint",
                    .source = codegen::TypeRef{.name = "project::Value",
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .encoding = encoding}},
                .fixed_points = {},
                .optional_sentinels = {}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("representations", "ValueVarint")};
    return {std::move(types), type};
}

auto integer_varint_pair(bool const same_source = true) -> VarintComparisonFixture {
    codegen::Manifest manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {},
        .modules = {codegen::ScalarModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                            .header = "SemanticValues.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .scalars = {codegen::IntegerScalarSchema{.name = "Value",
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
                                                                 .relationship = std::nullopt}}},
                    codegen::RepresentationModuleSchema{
                        .settings = codegen::ModuleSettings{.name = "representations",
                                                            .header = "Representations.h",
                                                            .source = std::nullopt,
                                                            .header_include = std::nullopt,
                                                            .namespace_name = "project",
                                                            .include_order = {},
                                                            .prelude_lines = {}},
                        .linear_quantized = {},
                        .integer_varints =
                            {codegen::IntegerVarintSchema{
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
                                 .encoding = codegen::IntegerVarintEncoding::zigzag_varint}},
                        .fixed_points = {},
                        .optional_sentinels = {}}}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const first{*types.find_declared("representations", "ValueSigned")};
    auto const second{*types.find_declared("representations", "ValueZigZag")};
    return {std::move(types), first, second};
}

auto soa_type(std::vector<std::pair<std::string, std::string>> columns = {{"min_xs", "float"},
                                                                          {"min_ys", "float"},
                                                                          {"min_zs", "float"},
                                                                          {"max_xs", "float"},
                                                                          {"max_ys", "float"},
                                                                          {"max_zs", "float"}})
    -> TypeFixture {
    std::vector<codegen::SoaMemberSchema> members;
    members.reserve(columns.size());
    for (auto& [name, type] : columns) {
        codegen::SoaMemberSchema member{};
        member.name = std::move(name);
        member.kind = codegen::SoaMemberKind::array;
        member.type.name = std::move(type);
        members.push_back(std::move(member));
    }
    codegen::SoaModuleSchema soa{};
    soa.settings.name = "soa";
    soa.settings.header = "Soa.h";
    soa.backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema schema{};
    schema.name = "Columns";
    schema.members = std::move(members);
    soa.structs.push_back(std::move(schema));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(soa)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("soa", "Columns")};
    return {std::move(types), type};
}

auto relationship_capacity_type(
    codegen::SemanticRelationKind const kind,
    std::uint32_t const bits,
    std::optional<std::uint64_t> const sentinel = std::nullopt,
    std::optional<std::uint64_t> const second_sentinel = std::nullopt,
    codegen::SemanticRelationUnit const offset_unit = codegen::SemanticRelationUnit::elements)
    -> RelationshipCapacityFixture {
    codegen::SoaModuleSchema soa{};
    soa.settings.name = "entities";
    soa.settings.header = "Entities.h";
    soa.backend = codegen::SoaBackend::standard_library;
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
    soa.structs.push_back(std::move(entities));

    codegen::PackedValueModuleSchema packed{};
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
    packed.values = {codegen::PackedValueSchema{
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

auto relationship_capacity_scalar_type(
    codegen::SemanticRelationKind const kind,
    std::uint32_t const bits,
    std::uint64_t const maximum,
    std::optional<std::uint64_t> const sentinel = std::nullopt,
    codegen::SemanticRelationUnit const offset_unit = codegen::SemanticRelationUnit::elements)
    -> RelationshipCapacityFixture {
    codegen::SoaModuleSchema soa{};
    soa.settings.name = "entities";
    soa.settings.header = "Entities.h";
    soa.backend = codegen::SoaBackend::standard_library;
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
    soa.structs.push_back(std::move(entities));

    codegen::ScalarModuleSchema scalars{};
    scalars.settings.name = "handles";
    scalars.settings.header = "Handles.h";
    scalars.scalars = {codegen::IntegerScalarSchema{
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

auto enum_domain_type(std::vector<codegen::EnumeratorSchema> values,
                      std::optional<std::string> count = std::nullopt,
                      std::optional<std::string> underlying = "std::uint8_t",
                      std::optional<std::uint32_t> bit_width = std::nullopt,
                      std::optional<bool> signedness = std::nullopt) -> TypeFixture {
    codegen::EnumModuleSchema module{};
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
    module.enums.push_back(std::move(enumeration));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("domains", "Domain")};
    return {std::move(types), type};
}

auto enum_value(std::string name,
                std::optional<std::string> initializer,
                bool const sentinel = false) -> codegen::EnumeratorSchema {
    return {.name = std::move(name),
            .initializer = std::move(initializer),
            .display_name = std::nullopt,
            .hidden = false,
            .serialized_name = std::nullopt,
            .sentinel = sentinel};
}

auto record_member(std::string name,
                   std::string type,
                   std::optional<std::uint64_t> count = std::nullopt)
    -> codegen::RecordMemberSchema {
    return {.name = std::move(name),
            .type = codegen::TypeRef{.name = std::move(type), .suffix = {}, .nested = std::nullopt},
            .count = count,
            .relationship = std::nullopt};
}

auto record_type(std::vector<codegen::RecordSchema> records, std::string selected = "Record")
    -> TypeFixture {
    codegen::RecordModuleSchema module{};
    module.settings.name = "records";
    module.settings.header = "Records.h";
    module.records = std::move(records);
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("records", selected)};
    return {std::move(types), type};
}

auto union_alternative(std::string name,
                       std::string type,
                       std::optional<std::uint64_t> count = std::nullopt)
    -> codegen::UnionAlternativeSchema {
    return {.name = std::move(name),
            .type = codegen::TypeRef{.name = std::move(type), .suffix = {}, .nested = std::nullopt},
            .count = count};
}

auto union_type(std::vector<codegen::UnionSchema> unions, std::string selected = "Union")
    -> TypeFixture {
    codegen::UnionModuleSchema module{};
    module.settings.name = "unions";
    module.settings.header = "Unions.h";
    module.unions = std::move(unions);
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("unions", selected)};
    return {std::move(types), type};
}

auto tagged_union_type() -> TypeFixture {
    codegen::EnumModuleSchema enums{};
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
    enums.enums.push_back(std::move(kind));

    codegen::UnionModuleSchema unions{};
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
    unions.tagged_unions.push_back(std::move(event));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(enums), std::move(unions)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("payloads", "Event")};
    return {std::move(types), type};
}

TEST(EnumAnalyzer, DerivesUnsignedImplicitValuesAndCountSentinelWidth) {
    auto const fixture{enum_domain_type({enum_value("First", "0"),
                                         enum_value("Second", std::nullopt),
                                         enum_value("Seventh", "7"),
                                         enum_value("Count", std::nullopt)},
                                        "Count")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.live_value_count, 3);
    EXPECT_EQ(analysis.reserved_value_count, 1);
    ASSERT_EQ(analysis.enumerators.size(), 4U);
    EXPECT_EQ(analysis.enumerators[1].code, (EnumCodeValue{.negative = false, .magnitude = 1}));
    EXPECT_EQ(analysis.enumerators[3].code, (EnumCodeValue{.negative = false, .magnitude = 8}));
    EXPECT_EQ(analysis.minimum_value, (EnumCodeValue{.negative = false, .magnitude = 0}));
    EXPECT_EQ(analysis.maximum_value, (EnumCodeValue{.negative = false, .magnitude = 8}));
    EXPECT_EQ(analysis.signed_domain, false);
    EXPECT_EQ(analysis.minimum_required_bits, 4);
    EXPECT_FALSE(analysis.declared_bit_width.has_value());
    EXPECT_EQ(analysis.effective_bit_width, 4);
    EXPECT_EQ(analysis.semantic_width_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_semantic_codes, 12);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_backing_codes, 252);
    EXPECT_EQ(analysis.aggregate.element_count, 1);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 1);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 64);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 4'096);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ReportsExplicitSemanticWidthSeparatelyFromPhysicalBacking) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.minimum_required_bits, 3);
    EXPECT_EQ(analysis.declared_bit_width, 4);
    EXPECT_EQ(analysis.effective_bit_width, 4);
    EXPECT_EQ(analysis.semantic_width_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_semantic_codes, 14);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.unused_backing_codes, 254);
}

TEST(EnumAnalyzer, ReportsDerivedCppBackingSeparatelyFromSemanticWidth) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        std::nullopt,
                                        12,
                                        false)};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.declared_bit_width, 12);
    EXPECT_EQ(analysis.effective_bit_width, 12);
    EXPECT_EQ(analysis.backing_type, "std::uint16_t");
    EXPECT_EQ(analysis.backing_bits, 16);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, SeparatesNamedSentinelsFromCountSentinel) {
    auto const fixture{enum_domain_type({enum_value("Ready", "0"),
                                         enum_value("Invalid", "14", true),
                                         enum_value("Pending", "15", true),
                                         enum_value("Count", "16")},
                                        "Count")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.live_value_count, 1);
    EXPECT_EQ(analysis.reserved_value_count, 3);
    EXPECT_EQ(analysis.minimum_required_bits, 5);
    ASSERT_EQ(analysis.enumerators.size(), 4U);
    EXPECT_TRUE(analysis.enumerators[1].sentinel);
    EXPECT_FALSE(analysis.enumerators[1].count_sentinel);
    EXPECT_TRUE(analysis.enumerators[2].sentinel);
    EXPECT_FALSE(analysis.enumerators[2].count_sentinel);
    EXPECT_FALSE(analysis.enumerators[3].sentinel);
    EXPECT_TRUE(analysis.enumerators[3].count_sentinel);
}

TEST(EnumAnalyzer, DerivesSignedTwosComplementWidth) {
    auto const fixture{enum_domain_type(
        {enum_value("Low", "-4"), enum_value("High", "+3")}, std::nullopt, "std::int8_t")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.minimum_value, (EnumCodeValue{.negative = true, .magnitude = 4}));
    EXPECT_EQ(analysis.maximum_value, (EnumCodeValue{.negative = false, .magnitude = 3}));
    EXPECT_EQ(analysis.signed_domain, true);
    EXPECT_EQ(analysis.minimum_required_bits, 3);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_backing_codes, 254);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ExplicitSignednessConstrainsSemanticWidth) {
    auto const signed_fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Seven", "7")},
                                               std::nullopt,
                                               "std::uint8_t",
                                               std::nullopt,
                                               true)};
    auto const signed_analysis{Analyzer::analyze_enum(
        signed_fixture.types, signed_fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(signed_analysis.declared_signedness, true);
    EXPECT_EQ(signed_analysis.signed_domain, true);
    EXPECT_EQ(signed_analysis.minimum_required_bits, 4);
}

TEST(EnumAnalyzer, KeepsExpressionAndDependentImplicitValuesUnknown) {
    auto const fixture{
        enum_domain_type({enum_value("Mask", "1 << 3"), enum_value("Next", std::nullopt)})};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_FALSE(analysis.enumerators[0].code.has_value());
    EXPECT_FALSE(analysis.enumerators[1].code.has_value());
    EXPECT_FALSE(analysis.minimum_required_bits.has_value());
    EXPECT_FALSE(analysis.unused_backing_codes.has_value());
    EXPECT_EQ(analysis.diagnostics.size(), 2U);
}

TEST(EnumAnalyzer, AcceptsFullUnsignedWidthAndDiagnosesImplicitOverflow) {
    auto const fixture{enum_domain_type(
        {enum_value("Maximum", "0xffffffffffffffffULL"), enum_value("Overflow", std::nullopt)},
        std::nullopt,
        "std::uint64_t")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(
        analysis.enumerators[0].code,
        (EnumCodeValue{.negative = false, .magnitude = std::numeric_limits<std::uint64_t>::max()}));
    EXPECT_FALSE(analysis.enumerators[1].code.has_value());
    EXPECT_FALSE(analysis.minimum_required_bits.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, DiagnosesDomainsThatDoNotFitBackingSignedness) {
    auto target{AbiProfile{"Narrow enum target"}};
    target.set("NarrowSigned",
               TypeFacts{.size_bytes = 1,
                         .alignment_bytes = 1,
                         .integer_signed = true,
                         .unsigned_value_bits = std::nullopt,
                         .provenance = "Test target"});
    target.set("NarrowUnsigned",
               TypeFacts{.size_bytes = 1,
                         .alignment_bytes = 1,
                         .integer_signed = false,
                         .unsigned_value_bits = 8,
                         .provenance = "Test target"});

    auto const signed_fixture{
        enum_domain_type({enum_value("Value", "200")}, std::nullopt, "NarrowSigned")};
    auto const signed_analysis{
        Analyzer::analyze_enum(signed_fixture.types, signed_fixture.type, target)};

    EXPECT_EQ(signed_analysis.minimum_required_bits, 8);
    EXPECT_EQ(signed_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(signed_analysis.diagnostics.empty());

    auto const unsigned_fixture{
        enum_domain_type({enum_value("Value", "-1")}, std::nullopt, "NarrowUnsigned")};
    auto const unsigned_analysis{
        Analyzer::analyze_enum(unsigned_fixture.types, unsigned_fixture.type, target)};

    EXPECT_EQ(unsigned_analysis.minimum_required_bits, 1);
    EXPECT_EQ(unsigned_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(unsigned_analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ComparesExplicitBackingAcrossTargetsWithoutChangingSemanticWidth) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto wider_target{AbiProfile::host_common()};
    wider_target.set("std::uint8_t",
                     TypeFacts{.size_bytes = 2,
                               .alignment_bytes = 2,
                               .integer_signed = false,
                               .unsigned_value_bits = 16,
                               .provenance = "Test wider target"});
    wider_target.set_memory_facts({.cache_line_bytes = 128,
                                   .page_bytes = 8'192,
                                   .l1_data_cache_bytes = 150,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test wider memory"});
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 150,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});

    auto const first{Analyzer::analyze_enum(fixture.types, fixture.type, first_target, 100)};
    auto const second{Analyzer::analyze_enum(fixture.types, fixture.type, wider_target, 100)};
    auto const comparison{Analyzer::compare_enum_targets(first, second)};

    EXPECT_EQ(comparison.first.effective_bit_width, 4);
    EXPECT_EQ(comparison.second.effective_bit_width, 4);
    ASSERT_TRUE(comparison.backing_size_delta.has_value());
    EXPECT_EQ(comparison.backing_size_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.backing_size_delta->magnitude, 1);
    ASSERT_TRUE(comparison.backing_alignment_delta.has_value());
    EXPECT_EQ(comparison.backing_alignment_delta->magnitude, 1);
    ASSERT_TRUE(comparison.backing_value_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_value_bit_delta->magnitude, 8);
    ASSERT_TRUE(comparison.backing_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_bit_delta->magnitude, 8);
    EXPECT_EQ(comparison.first.unused_backing_codes, 254);
    EXPECT_EQ(comparison.second.unused_backing_codes, 65'534);
    ASSERT_TRUE(comparison.unused_backing_code_delta.has_value());
    EXPECT_EQ(comparison.unused_backing_code_delta->magnitude, 65'280);
    EXPECT_EQ(comparison.backing_fit_changed, false);
    EXPECT_EQ(comparison.first.aggregate.total_storage_bytes, 100);
    EXPECT_EQ(comparison.second.aggregate.total_storage_bytes, 200);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 100);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.first.aggregate.cache_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.aggregate.cache_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(EnumAnalyzer, ComparesDerivedBackingStorageSeparatelyFromValueCapacity) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        std::nullopt,
                                        12,
                                        false)};
    auto padded_target{AbiProfile::host_common()};
    padded_target.set("std::uint16_t",
                      TypeFacts{.size_bytes = 4,
                                .alignment_bytes = 4,
                                .integer_signed = false,
                                .unsigned_value_bits = 16,
                                .provenance = "Test padded target"});

    auto const first{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto const second{Analyzer::analyze_enum(fixture.types, fixture.type, padded_target)};
    auto const comparison{Analyzer::compare_enum_targets(first, second)};
    auto const identical{Analyzer::compare_enum_targets(first, first)};

    EXPECT_EQ(comparison.first.backing_type, "std::uint16_t");
    EXPECT_EQ(comparison.second.backing_type, "std::uint16_t");
    ASSERT_TRUE(comparison.backing_size_delta.has_value());
    EXPECT_EQ(comparison.backing_size_delta->magnitude, 2);
    ASSERT_TRUE(comparison.backing_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_bit_delta->magnitude, 16);
    ASSERT_TRUE(comparison.backing_value_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_value_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.unused_backing_codes, comparison.second.unused_backing_codes);
    ASSERT_TRUE(comparison.unused_backing_code_delta.has_value());
    EXPECT_EQ(comparison.unused_backing_code_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_size_delta.has_value());
    EXPECT_EQ(identical.backing_size_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_alignment_delta.has_value());
    EXPECT_EQ(identical.backing_alignment_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_fit_changed.has_value());
    EXPECT_FALSE(*identical.backing_fit_changed);
}

TEST(EnumAnalyzer, KeepsUnknownTargetBackingFactsUnknownInComparison) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto const known{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto const unknown{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile{"Unknown target"})};

    auto const comparison{Analyzer::compare_enum_targets(known, unknown)};

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_FALSE(comparison.backing_alignment_delta.has_value());
    EXPECT_FALSE(comparison.backing_value_bit_delta.has_value());
    EXPECT_FALSE(comparison.backing_bit_delta.has_value());
    EXPECT_FALSE(comparison.unused_backing_code_delta.has_value());
    EXPECT_FALSE(comparison.backing_fit_changed.has_value());
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.minimum_page_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target enum:");
    }));
}

TEST(EnumAnalyzer, RejectsMismatchedSemanticEnumAnalysesTransactionally) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto const first{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto second{first};
    second.minimum_required_bits = 4;

    auto comparison{Analyzer::compare_enum_targets(first, second)};

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("same semantic domain"),
              std::string::npos);

    second = first;
    second.enumerators[0].code = EnumCodeValue{.negative = false, .magnitude = 1};
    comparison = Analyzer::compare_enum_targets(first, second);

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("same ordered enumerators"),
              std::string::npos);

    second = first;
    second.aggregate.element_count = 2;
    comparison = Analyzer::compare_enum_targets(first, second);

    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);
}

TEST(EnumAnalyzer, KeepsStandaloneBackingAggregateOverflowUnknown) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        "std::uint16_t",
                                        12,
                                        false)};

    auto const analysis{Analyzer::analyze_enum(fixture.types,
                                               fixture.type,
                                               AbiProfile::host_common(),
                                               std::numeric_limits<std::uint64_t>::max())};

    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_capacity.working_set_bytes.has_value());
    EXPECT_TRUE(std::ranges::any_of(analysis.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.find("aggregate storage overflows") != std::string::npos;
    }));
}

TEST(RecordAnalyzer, ReportsOffsetsInternalAndTailPadding) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[0].offset_bytes, 0);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[1].padding_before_bytes, 3);
    EXPECT_EQ(analysis.members[2].offset_bytes, 8);
    EXPECT_EQ(analysis.payload_bytes, 7);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 2);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_EQ(analysis.aggregate.element_count, 1);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 12);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 7);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 5);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 5);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto first_target{AbiProfile::host_common()};
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "synthetic wide target"});

    auto const first{Analyzer::analyze_record(fixture.types, fixture.type, first_target, 100)};
    auto const second{Analyzer::analyze_record(fixture.types, fixture.type, second_target, 100)};
    auto const comparison{Analyzer::compare_record_targets(first, second)};

    ASSERT_EQ(comparison.members.size(), 3U);
    EXPECT_EQ(comparison.first.size_bytes, 12);
    EXPECT_EQ(comparison.second.size_bytes, 24);
    ASSERT_TRUE(comparison.size_delta.has_value());
    EXPECT_EQ(comparison.size_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.size_delta->magnitude, 12);
    ASSERT_TRUE(comparison.alignment_delta.has_value());
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    ASSERT_TRUE(comparison.payload_delta.has_value());
    EXPECT_EQ(comparison.payload_delta->magnitude, 4);
    ASSERT_TRUE(comparison.internal_padding_delta.has_value());
    EXPECT_EQ(comparison.internal_padding_delta->magnitude, 4);
    ASSERT_TRUE(comparison.tail_padding_delta.has_value());
    EXPECT_EQ(comparison.tail_padding_delta->magnitude, 4);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.total_padding_delta.has_value());
    EXPECT_EQ(comparison.total_padding_delta->magnitude, 800);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 19);

    auto const& wide{comparison.members[1]};
    EXPECT_EQ(wide.name, "wide");
    EXPECT_EQ(wide.first.offset_bytes, 4);
    EXPECT_EQ(wide.second.offset_bytes, 8);
    ASSERT_TRUE(wide.element_size_delta.has_value());
    EXPECT_EQ(wide.element_size_delta->magnitude, 4);
    ASSERT_TRUE(wide.element_alignment_delta.has_value());
    EXPECT_EQ(wide.element_alignment_delta->magnitude, 4);
    ASSERT_TRUE(wide.offset_delta.has_value());
    EXPECT_EQ(wide.offset_delta->magnitude, 4);
    ASSERT_TRUE(wide.extent_delta.has_value());
    EXPECT_EQ(wide.extent_delta->magnitude, 4);
    ASSERT_TRUE(wide.padding_before_delta.has_value());
    EXPECT_EQ(wide.padding_before_delta->magnitude, 4);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_record_targets(first, first)};
    ASSERT_TRUE(identical.size_delta.has_value());
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.size_delta->magnitude, 0);
}

TEST(RecordAnalyzer, KeepsUnknownTargetFactsUnknownDuringComparison) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("value", "std::uint32_t")},
                                           .export_specifier = std::nullopt}})};
    auto const known{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto const comparison{Analyzer::compare_record_targets(known, unknown)};

    ASSERT_EQ(comparison.members.size(), 1U);
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.members[0].element_size_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target record:");
    }));
}

TEST(RecordAnalyzer, RejectsMismatchedTargetComparisonInputs) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("first", "std::uint32_t"),
                                                       record_member("second", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto const first{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 11)};

    auto comparison{Analyzer::compare_record_targets(first, second)};
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.members[1].element_count = 2;
    comparison = Analyzer::compare_record_targets(first, second);
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible semantic identity"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_record_targets(first, second);
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different records"), std::string::npos);
}

TEST(UnionAnalyzer, ReportsMaximumExtentAlignmentAndAlternativeSlack) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("small", "std::uint8_t"),
                                               union_alternative("wide", "std::uint32_t"),
                                               union_alternative("bytes", "std::uint8_t", 6)},
                              .export_specifier = std::nullopt}})};

    auto const analysis{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 100)};

    ASSERT_EQ(analysis.alternatives.size(), 3U);
    EXPECT_EQ(analysis.alternatives[0].extent_bytes, 1);
    EXPECT_EQ(analysis.alternatives[0].slack_bytes, 7);
    EXPECT_EQ(analysis.alternatives[0].total_slack_bytes, 700);
    EXPECT_EQ(analysis.alternatives[1].extent_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].slack_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].total_slack_bytes, 400);
    EXPECT_EQ(analysis.alternatives[2].extent_bytes, 6);
    EXPECT_EQ(analysis.alternatives[2].slack_bytes, 2);
    EXPECT_EQ(analysis.alternatives[2].total_slack_bytes, 200);
    EXPECT_EQ(analysis.largest_alternative_bytes, 6);
    EXPECT_EQ(analysis.tail_padding_bytes, 2);
    EXPECT_EQ(analysis.size_bytes, 8);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 200);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 13);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 8);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 512);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("small", "std::uint8_t"),
                                               union_alternative("wide", "std::uint32_t"),
                                               union_alternative("bytes", "std::uint8_t", 10)},
                              .export_specifier = std::nullopt}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});

    auto const first{Analyzer::analyze_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second{Analyzer::analyze_union(fixture.types, fixture.type, second_profile, 100)};
    auto const comparison{Analyzer::compare_union_targets(first, second)};

    ASSERT_EQ(comparison.alternatives.size(), 3);
    EXPECT_EQ(comparison.first.size_bytes, 12);
    EXPECT_EQ(comparison.second.size_bytes, 16);
    EXPECT_EQ(comparison.largest_alternative_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.tail_padding_delta->magnitude, 4);
    EXPECT_EQ(comparison.size_delta->magnitude, 4);
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 400);
    EXPECT_EQ(comparison.total_tail_padding_delta->magnitude, 400);
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.complete_elements_per_cache_line_delta->magnitude, 1);
    EXPECT_EQ(comparison.cache_line_straddling_delta->magnitude, 12);

    auto const& wide{comparison.alternatives[1]};
    EXPECT_EQ(wide.name, "wide");
    EXPECT_EQ(wide.first.extent_bytes, 4);
    EXPECT_EQ(wide.second.extent_bytes, 8);
    EXPECT_EQ(wide.element_size_delta->magnitude, 4);
    EXPECT_EQ(wide.element_alignment_delta->magnitude, 4);
    EXPECT_EQ(wide.extent_delta->magnitude, 4);
    EXPECT_EQ(wide.slack_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(wide.total_slack_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_union_targets(first, first)};
    ASSERT_EQ(identical.alternatives.size(), 3);
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_storage_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(UnionAnalyzer, PreservesUnknownAndOverflowedTargetFactsDuringComparison) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("value", "std::uint32_t")},
                              .export_specifier = std::nullopt}})};
    auto const known{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto comparison{Analyzer::compare_union_targets(known, unknown)};
    ASSERT_EQ(comparison.alternatives.size(), 1);
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.alternatives[0].element_size_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target raw union:");
    }));

    auto const overflow_first{Analyzer::analyze_union(fixture.types,
                                                      fixture.type,
                                                      AbiProfile::host_common(),
                                                      std::numeric_limits<std::uint64_t>::max())};
    auto const overflow_second{Analyzer::analyze_union(fixture.types,
                                                       fixture.type,
                                                       AbiProfile::host_common(),
                                                       std::numeric_limits<std::uint64_t>::max())};
    comparison = Analyzer::compare_union_targets(overflow_first, overflow_second);
    ASSERT_EQ(comparison.alternatives.size(), 1);
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(UnionAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialAlternatives) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("first", "std::uint32_t"),
                                               union_alternative("second", "std::uint16_t")},
                              .export_specifier = std::nullopt}})};
    auto const first{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 11)};

    auto comparison{Analyzer::compare_union_targets(first, second)};
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.alternatives.back().element_count = 2;
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible semantic identity"),
              std::string::npos);

    second = first;
    second.alternatives.back().name = "renamed";
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different raw unions"),
              std::string::npos);
}

TEST(UnionAnalyzer, ReportsSelectedCountOverflowWithoutLosingPerObjectLayout) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("value", "std::uint64_t")},
                              .export_specifier = std::nullopt}})};

    auto const analysis{Analyzer::analyze_union(fixture.types,
                                                fixture.type,
                                                AbiProfile::host_common(),
                                                std::numeric_limits<std::uint64_t>::max())};

    EXPECT_EQ(analysis.size_bytes, 8);
    EXPECT_EQ(analysis.alternatives[0].slack_bytes, 0);
    EXPECT_EQ(analysis.alternatives[0].total_slack_bytes, 0);
    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, HandlesNestedAggregatesAndUnknownOrOverflowedAlternatives) {
    codegen::RecordModuleSchema records{
        .settings = codegen::ModuleSettings{.name = "records",
                                            .header = "Records.h",
                                            .source = std::nullopt,
                                            .header_include = std::nullopt,
                                            .namespace_name = "records",
                                            .include_order = {},
                                            .prelude_lines = {}},
        .records = {codegen::RecordSchema{.name = "Record",
                                          .members = {record_member("small", "std::uint8_t"),
                                                      record_member("wide", "std::uint32_t")},
                                          .export_specifier = std::nullopt}}};
    codegen::UnionModuleSchema unions{
        .settings = codegen::ModuleSettings{.name = "unions",
                                            .header = "Unions.h",
                                            .source = std::nullopt,
                                            .header_include = std::nullopt,
                                            .namespace_name = "unions",
                                            .include_order = {},
                                            .prelude_lines = {}},
        .unions = {codegen::UnionSchema{
            .name = "Payload",
            .alternatives = {union_alternative("record", "records::Record"),
                             union_alternative("words", "std::uint16_t", 6)},
            .export_specifier = std::nullopt}},
        .tagged_unions = {}};
    codegen::Manifest manifest{.schema_version = codegen::manifest_schema_version,
                               .types = {},
                               .modules = {std::move(records), std::move(unions)}};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const payload{*types.find_declared("unions", "Payload")};
    auto analysis{Analyzer::analyze_union(types, payload, AbiProfile::host_common())};
    EXPECT_EQ(analysis.alternatives[0].element_facts->size_bytes, 8);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const unknown{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("value", "UnknownType")},
                              .export_specifier = std::nullopt}})};
    analysis = Analyzer::analyze_union(unknown.types, unknown.type, AbiProfile::host_common());
    EXPECT_FALSE(analysis.size_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());

    auto const overflow{union_type({codegen::UnionSchema{
        .name = "Union",
        .alternatives = {union_alternative(
            "values", "std::uint64_t", std::numeric_limits<std::uint64_t>::max())},
        .export_specifier = std::nullopt}})};
    analysis = Analyzer::analyze_union(overflow.types, overflow.type, AbiProfile::host_common());
    EXPECT_FALSE(analysis.size_bytes.has_value());
    EXPECT_FALSE(analysis.alternatives[0].extent_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, AnalyzesExplicitAlternativeDistribution) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("small", "std::uint8_t"),
                                               union_alternative("wide", "std::uint32_t"),
                                               union_alternative("bytes", "std::uint8_t", 6)},
                              .export_specifier = std::nullopt}})};
    auto const layout{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    constexpr std::array workload{UnionDistributionEntry{.alternative_name = "small", .weight = 2},
                                  UnionDistributionEntry{.alternative_name = "wide", .weight = 1}};

    auto distribution{Analyzer::analyze_union_distribution(layout, workload, 10)};

    ASSERT_EQ(distribution.entries.size(), 2);
    EXPECT_EQ(distribution.valid_entry_count, 2);
    EXPECT_EQ(distribution.total_weight, 3);
    EXPECT_EQ(distribution.total_extent_bytes, 6);
    EXPECT_EQ(distribution.total_slack_bytes, 18);
    EXPECT_EQ(distribution.expected_extent_bytes_per_value, 2.0L);
    EXPECT_EQ(distribution.expected_slack_bytes_per_value, 6.0L);
    EXPECT_EQ(distribution.expected_selected_extent_bytes, 20.0L);
    EXPECT_EQ(distribution.expected_selected_slack_bytes, 60.0L);
    EXPECT_EQ(distribution.entries[0].weighted_extent_bytes, 2);
    EXPECT_EQ(distribution.entries[0].weighted_slack_bytes, 14);
    EXPECT_TRUE(distribution.diagnostics.empty());

    constexpr std::array invalid_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = 1},
        UnionDistributionEntry{.alternative_name = "small", .weight = 2},
        UnionDistributionEntry{.alternative_name = "missing", .weight = 1}};
    distribution = Analyzer::analyze_union_distribution(layout, invalid_workload, 10);
    EXPECT_EQ(distribution.valid_entry_count, 1);
    EXPECT_EQ(distribution.entries.size(), 1);
    EXPECT_EQ(distribution.diagnostics.size(), 2);

    constexpr std::array zero_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = 0}};
    distribution = Analyzer::analyze_union_distribution(layout, zero_workload, 10);
    EXPECT_EQ(distribution.total_weight, 0);
    EXPECT_FALSE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    auto const unknown_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};
    distribution = Analyzer::analyze_union_distribution(unknown_layout, workload, 10);
    ASSERT_EQ(distribution.entries.size(), 2);
    EXPECT_FALSE(distribution.total_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_slack_bytes.has_value());
    EXPECT_FALSE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflowing_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = maximum},
        UnionDistributionEntry{.alternative_name = "wide", .weight = maximum}};
    distribution = Analyzer::analyze_union_distribution(layout, overflowing_workload, maximum);
    EXPECT_FALSE(distribution.total_weight.has_value());
    EXPECT_FALSE(distribution.total_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_slack_bytes.has_value());
    EXPECT_FALSE(distribution.entries[1].weighted_extent_bytes.has_value());
    EXPECT_TRUE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());
}

TEST(UnionAnalyzer, ComparesExplicitAlternativeDistributionAcrossTargetProfiles) {
    auto const fixture{union_type(
        {codegen::UnionSchema{.name = "Union",
                              .alternatives = {union_alternative("small", "std::uint8_t"),
                                               union_alternative("wide", "std::uint32_t"),
                                               union_alternative("bytes", "std::uint8_t", 6)},
                              .export_specifier = std::nullopt}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});
    auto const first_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, first_profile, 10)};
    auto const second_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, second_profile, 10)};
    constexpr std::array workload{UnionDistributionEntry{.alternative_name = "small", .weight = 2},
                                  UnionDistributionEntry{.alternative_name = "wide", .weight = 1}};
    auto const first{Analyzer::analyze_union_distribution(first_layout, workload, 10)};
    auto const second{Analyzer::analyze_union_distribution(second_layout, workload, 10)};

    auto comparison{Analyzer::compare_union_distributions(first, second)};

    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_EQ(comparison.total_weight_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.total_extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_slack_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_slack_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_NEAR(*comparison.expected_extent_per_value_delta, 4.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_slack_per_value_delta, -4.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_selected_extent_delta, 40.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_selected_slack_delta, -40.0L / 3.0L, 1e-12L);
    EXPECT_EQ(comparison.entries[1].extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[1].slack_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[1].slack_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_union_distributions(first, first)};
    ASSERT_EQ(identical.entries.size(), 2);
    EXPECT_EQ(identical.total_extent_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.expected_extent_per_value_delta, 0.0L);

    auto mismatched{second};
    mismatched.entries.back().weight = 2;
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible weight"), std::string::npos);

    mismatched = second;
    mismatched.selected_element_count = 11;
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different types or selected counts"),
              std::string::npos);

    mismatched = second;
    mismatched.entries.back().alternative_name = "renamed";
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives"),
              std::string::npos);

    mismatched = second;
    mismatched.type = lispb::schema::TypeId{second.type.value + 1};
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflow_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = maximum},
        UnionDistributionEntry{.alternative_name = "wide", .weight = maximum}};
    auto const overflow{
        Analyzer::analyze_union_distribution(first_layout, overflow_workload, maximum)};
    comparison = Analyzer::compare_union_distributions(overflow, overflow);
    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_FALSE(comparison.total_weight_delta.has_value());
    EXPECT_FALSE(comparison.total_extent_delta.has_value());
    EXPECT_FALSE(comparison.entries[1].weighted_extent_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, LaysOutDiscriminantBeforeAlignedPayloadUnion) {
    codegen::EnumModuleSchema enums{};
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
    enums.enums.push_back(std::move(kind));

    codegen::UnionModuleSchema unions{};
    unions.settings.name = "payloads";
    unions.settings.header = "Payloads.h";
    unions.settings.namespace_name = "payloads";
    codegen::TaggedUnionSchema event_schema{};
    event_schema.name = "Event";
    event_schema.discriminant.name = "events::Kind";
    codegen::TaggedUnionAlternativeSchema small{};
    small.name = "small";
    small.type.name = "std::uint32_t";
    small.tag = "Small";
    codegen::TaggedUnionAlternativeSchema bytes{};
    bytes.name = "bytes";
    bytes.type.name = "std::uint8_t";
    bytes.count = 6;
    bytes.tag = "Bytes";
    event_schema.alternatives = {std::move(small), std::move(bytes)};
    unions.tagged_unions.push_back(std::move(event_schema));

    codegen::RecordModuleSchema records{};
    records.settings.name = "records";
    records.settings.header = "Records.h";
    records.records = {codegen::RecordSchema{.name = "Envelope",
                                             .members = {record_member("event", "payloads::Event"),
                                                         record_member("suffix", "std::uint8_t")},
                                             .export_specifier = std::nullopt}};
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(enums), std::move(unions), std::move(records)};
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const event{*types.find_declared("payloads", "Event")};
    auto const analysis{
        Analyzer::analyze_tagged_union(types, event, AbiProfile::host_common(), 100)};

    ASSERT_TRUE(analysis.discriminant_facts.has_value());
    EXPECT_EQ(analysis.discriminant_facts->size_bytes, 1);
    EXPECT_EQ(analysis.largest_alternative_bytes, 6);
    EXPECT_EQ(analysis.payload_size_bytes, 8);
    EXPECT_EQ(analysis.payload_alignment_bytes, 4);
    EXPECT_EQ(analysis.payload_offset_bytes, 4);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 0);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    ASSERT_EQ(analysis.alternatives.size(), 2U);
    EXPECT_EQ(analysis.alternatives[0].payload_slack_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].payload_slack_bytes, 2);
    EXPECT_EQ(analysis.mapped_live_tags, (std::vector<std::string>{"Small", "Bytes"}));
    EXPECT_EQ(analysis.unmapped_live_tags, (std::vector<std::string>{"Spare"}));
    EXPECT_EQ(analysis.sentinel_tags, (std::vector<std::string>{"Invalid"}));
    EXPECT_EQ(analysis.count_sentinel_tag, "COUNT");
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 1'200);
    EXPECT_EQ(analysis.aggregate.total_discriminant_bytes, 100);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_internal_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 0);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 19);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 5);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 12);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 341);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_EQ(analysis.alternatives[0].total_payload_slack_bytes, 400);
    EXPECT_EQ(analysis.alternatives[1].total_payload_slack_bytes, 200);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const envelope{*types.find_declared("records", "Envelope")};
    auto const record{Analyzer::analyze_record(types, envelope, AbiProfile::host_common())};
    EXPECT_EQ(record.members[0].element_facts->size_bytes, 12);
    EXPECT_EQ(record.size_bytes, 16);
    EXPECT_TRUE(record.diagnostics.empty());

    AbiProfile unknown_memory{"unknown-memory"};
    unknown_memory.set("std::uint8_t",
                       {.size_bytes = 1,
                        .alignment_bytes = 1,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    unknown_memory.set("std::uint32_t",
                       {.size_bytes = 4,
                        .alignment_bytes = 4,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const unknown_memory_analysis{
        Analyzer::analyze_tagged_union(types, event, unknown_memory, 100)};
    EXPECT_EQ(unknown_memory_analysis.aggregate.total_storage_bytes, 1'200);
    EXPECT_FALSE(unknown_memory_analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(unknown_memory_analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(unknown_memory_analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_tagged_union(
        types, event, AbiProfile::host_common(), std::numeric_limits<std::uint64_t>::max())};
    EXPECT_FALSE(overflow.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(overflow.aggregate.total_payload_bytes.has_value());
    EXPECT_FALSE(overflow.aggregate.total_internal_padding_bytes.has_value());
    EXPECT_FALSE(overflow.alternatives[0].total_payload_slack_bytes.has_value());
    EXPECT_FALSE(overflow.diagnostics.empty());

    constexpr std::array workload{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                  TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto distribution{Analyzer::analyze_tagged_union_distribution(analysis, workload, 100)};
    EXPECT_EQ(distribution.valid_entry_count, 2);
    EXPECT_EQ(distribution.total_weight, 4);
    EXPECT_EQ(distribution.total_payload_extent_bytes, 18);
    EXPECT_EQ(distribution.total_payload_slack_bytes, 14);
    EXPECT_EQ(distribution.expected_payload_extent_bytes_per_value, 4.5L);
    EXPECT_EQ(distribution.expected_payload_slack_bytes_per_value, 3.5L);
    EXPECT_EQ(distribution.expected_selected_payload_extent_bytes, 450.0L);
    EXPECT_EQ(distribution.expected_selected_payload_slack_bytes, 350.0L);
    ASSERT_EQ(distribution.entries.size(), 2U);
    EXPECT_EQ(distribution.entries[0].weighted_payload_extent_bytes, 12);
    EXPECT_EQ(distribution.entries[1].weighted_payload_slack_bytes, 2);
    EXPECT_TRUE(distribution.diagnostics.empty());

    constexpr std::array invalid_workload{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Small", .weight = 2},
        TaggedUnionDistributionEntry{.tag = "Spare", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Invalid", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "COUNT", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Missing", .weight = 1}};
    distribution = Analyzer::analyze_tagged_union_distribution(analysis, invalid_workload, 100);
    EXPECT_EQ(distribution.valid_entry_count, 1);
    EXPECT_EQ(distribution.entries.size(), 1U);
    EXPECT_GE(distribution.diagnostics.size(), 5U);

    constexpr std::array zero_workload{TaggedUnionDistributionEntry{.tag = "Small", .weight = 0}};
    distribution = Analyzer::analyze_tagged_union_distribution(analysis, zero_workload, 100);
    EXPECT_EQ(distribution.total_weight, 0);
    EXPECT_FALSE(distribution.expected_payload_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflowing_workload{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = maximum},
        TaggedUnionDistributionEntry{.tag = "Bytes", .weight = maximum}};
    distribution =
        Analyzer::analyze_tagged_union_distribution(analysis, overflowing_workload, maximum);
    EXPECT_FALSE(distribution.total_weight.has_value());
    EXPECT_FALSE(distribution.total_payload_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_payload_slack_bytes.has_value());
    EXPECT_FALSE(distribution.entries[0].weighted_payload_extent_bytes.has_value());
    EXPECT_TRUE(distribution.expected_payload_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{tagged_union_type()};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint8_t",
                       {.size_bytes = 2,
                        .alignment_bytes = 2,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = "synthetic wide target"});
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});
    auto const first{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, second_profile, 100)};

    auto const comparison{Analyzer::compare_tagged_union_targets(first, second)};

    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_EQ(comparison.discriminant_size_delta->magnitude, 1);
    EXPECT_EQ(comparison.discriminant_alignment_delta->magnitude, 1);
    EXPECT_EQ(comparison.largest_alternative_delta->magnitude, 10);
    EXPECT_EQ(comparison.payload_size_delta->magnitude, 12);
    EXPECT_EQ(comparison.payload_alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.payload_offset_delta->magnitude, 4);
    EXPECT_EQ(comparison.internal_padding_delta->magnitude, 3);
    EXPECT_EQ(comparison.tail_padding_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.size_delta->magnitude, 16);
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 1'600);
    EXPECT_EQ(comparison.total_discriminant_delta->magnitude, 100);
    EXPECT_EQ(comparison.total_payload_delta->magnitude, 1'200);
    EXPECT_EQ(comparison.total_internal_padding_delta->magnitude, 300);
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 25);
    EXPECT_EQ(comparison.complete_elements_per_cache_line_delta->magnitude, 2);
    EXPECT_EQ(comparison.first.mapped_live_tags, comparison.second.mapped_live_tags);
    EXPECT_EQ(comparison.first.unmapped_live_tags, comparison.second.unmapped_live_tags);
    EXPECT_EQ(comparison.first.sentinel_tags, comparison.second.sentinel_tags);

    auto const& small{comparison.alternatives[0]};
    EXPECT_EQ(small.first.tag, "Small");
    EXPECT_EQ(small.second.tag, "Small");
    EXPECT_EQ(small.element_size_delta->magnitude, 4);
    EXPECT_EQ(small.element_alignment_delta->magnitude, 4);
    EXPECT_EQ(small.extent_delta->magnitude, 4);
    EXPECT_EQ(small.payload_slack_delta->magnitude, 8);
    EXPECT_EQ(small.total_payload_slack_delta->magnitude, 800);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_tagged_union_targets(first, first)};
    ASSERT_EQ(identical.alternatives.size(), 2);
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_storage_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(TaggedUnionAnalyzer, PreservesUnknownAndOverflowedTargetFactsDuringComparison) {
    auto const fixture{tagged_union_type()};
    auto const known{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto comparison{Analyzer::compare_tagged_union_targets(known, unknown)};
    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_FALSE(comparison.discriminant_size_delta.has_value());
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.alternatives[0].element_size_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target tagged union:");
    }));

    auto const overflow_first{
        Analyzer::analyze_tagged_union(fixture.types,
                                       fixture.type,
                                       AbiProfile::host_common(),
                                       std::numeric_limits<std::uint64_t>::max())};
    auto const overflow_second{overflow_first};
    comparison = Analyzer::compare_tagged_union_targets(overflow_first, overflow_second);
    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.total_payload_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, RejectsMismatchedTargetComparisonWithoutPartialAlternatives) {
    auto const fixture{tagged_union_type()};
    auto const first{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{first};
    second.aggregate.element_count = 11;

    auto comparison{Analyzer::compare_tagged_union_targets(first, second)};
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.unmapped_live_tags.push_back("Other");
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("tag coverage semantics"),
              std::string::npos);

    second = first;
    second.alternatives.back().tag = "Small";
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives and tags"),
              std::string::npos);

    second = first;
    second.alternatives.back().element_count = 11;
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible tag"), std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different tagged unions"),
              std::string::npos);
}

TEST(TaggedUnionAnalyzer, ComparesExplicitDistributionAcrossTargetProfiles) {
    auto const fixture{tagged_union_type()};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint8_t",
                       {.size_bytes = 2,
                        .alignment_bytes = 2,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const first_layout{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second_layout{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, second_profile, 100)};
    constexpr std::array distribution{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                      TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto const first{Analyzer::analyze_tagged_union_distribution(first_layout, distribution, 100)};
    auto const second{
        Analyzer::analyze_tagged_union_distribution(second_layout, distribution, 100)};

    auto const comparison{Analyzer::compare_tagged_union_distributions(first, second)};

    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_EQ(comparison.total_weight_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.total_payload_extent_delta->magnitude, 22);
    EXPECT_EQ(comparison.total_payload_slack_delta->magnitude, 26);
    EXPECT_EQ(comparison.expected_payload_extent_per_value_delta, 5.5L);
    EXPECT_EQ(comparison.expected_payload_slack_per_value_delta, 6.5L);
    EXPECT_EQ(comparison.expected_selected_payload_extent_delta, 550.0L);
    EXPECT_EQ(comparison.expected_selected_payload_slack_delta, 650.0L);
    EXPECT_EQ(comparison.entries[0].payload_extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[0].payload_slack_delta->magnitude, 8);
    EXPECT_EQ(comparison.entries[0].weighted_payload_extent_delta->magnitude, 12);
    EXPECT_EQ(comparison.entries[0].weighted_payload_slack_delta->magnitude, 24);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_tagged_union_distributions(first, first)};
    ASSERT_EQ(identical.entries.size(), 2);
    EXPECT_EQ(identical.total_payload_extent_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.expected_payload_extent_per_value_delta, 0.0L);
}

TEST(TaggedUnionAnalyzer, RejectsMismatchedOrOverflowedDistributionComparison) {
    auto const fixture{tagged_union_type()};
    auto const layout{Analyzer::analyze_tagged_union(
        fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    constexpr std::array distribution{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                      TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto const first{Analyzer::analyze_tagged_union_distribution(layout, distribution, 100)};
    auto second{first};
    second.entries.back().weight = 2;

    auto comparison{Analyzer::compare_tagged_union_distributions(first, second)};
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible alternative identity"),
              std::string::npos);

    second = first;
    second.selected_element_count = 101;
    comparison = Analyzer::compare_tagged_union_distributions(first, second);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different types or selected counts"),
              std::string::npos);

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflow_distribution{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = maximum},
        TaggedUnionDistributionEntry{.tag = "Bytes", .weight = maximum}};
    auto const overflow{
        Analyzer::analyze_tagged_union_distribution(layout, overflow_distribution, maximum)};
    comparison = Analyzer::compare_tagged_union_distributions(overflow, overflow);
    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_FALSE(comparison.total_weight_delta.has_value());
    EXPECT_FALSE(comparison.total_payload_extent_delta.has_value());
    EXPECT_FALSE(comparison.entries[0].weighted_payload_extent_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(RecordAnalyzer, HandlesFixedArraysAndNestedRecords) {
    auto const fixture{record_type(
        {codegen::RecordSchema{.name = "Inner",
                               .members = {record_member("tag", "std::uint8_t"),
                                           record_member("value", "std::uint32_t")},
                               .export_specifier = std::nullopt},
         codegen::RecordSchema{.name = "Record",
                               .members = {record_member("prefix", "std::uint8_t"),
                                           record_member("inner", "Inner"),
                                           record_member("samples", "std::uint16_t", 2)},
                               .export_specifier = std::nullopt}},
        "Record")};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[1].element_facts->size_bytes, 8);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[2].element_count, 2);
    EXPECT_EQ(analysis.members[2].extent_bytes, 4);
    EXPECT_EQ(analysis.members[2].offset_bytes, 12);
    EXPECT_EQ(analysis.payload_bytes, 13);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 0);
    EXPECT_EQ(analysis.size_bytes, 16);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, KeepsSemanticRelationshipsOutOfAbiLayout) {
    auto member{record_member("value", "std::uint32_t")};
    auto related_member{member};
    related_member.relationship = codegen::SemanticRelationSchema{
        .kind = codegen::SemanticRelationKind::references,
        .target = codegen::TypeRef{.name = "Target", .suffix = {}, .nested = std::nullopt},
        .unit = std::nullopt};
    auto records = [](codegen::RecordMemberSchema value_member) {
        return std::vector{codegen::RecordSchema{.name = "Target",
                                                 .members = {record_member("id", "std::uint64_t")},
                                                 .export_specifier = std::nullopt},
                           codegen::RecordSchema{.name = "Record",
                                                 .members = {std::move(value_member)},
                                                 .export_specifier = std::nullopt}};
    };
    auto const plain{record_type(records(std::move(member)))};
    auto const related{record_type(records(std::move(related_member)))};

    auto const plain_analysis{
        Analyzer::analyze_record(plain.types, plain.type, AbiProfile::host_common(), 100)};
    auto const related_analysis{
        Analyzer::analyze_record(related.types, related.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(related_analysis.size_bytes, plain_analysis.size_bytes);
    EXPECT_EQ(related_analysis.alignment_bytes, plain_analysis.alignment_bytes);
    EXPECT_EQ(related_analysis.payload_bytes, plain_analysis.payload_bytes);
    EXPECT_EQ(related_analysis.internal_padding_bytes, plain_analysis.internal_padding_bytes);
    EXPECT_EQ(related_analysis.tail_padding_bytes, plain_analysis.tail_padding_bytes);
    EXPECT_EQ(related_analysis.members[0].offset_bytes, plain_analysis.members[0].offset_bytes);
    EXPECT_EQ(related_analysis.members[0].extent_bytes, plain_analysis.members[0].extent_bytes);
    EXPECT_EQ(related_analysis.aggregate.total_storage_bytes,
              plain_analysis.aggregate.total_storage_bytes);
    EXPECT_TRUE(related_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, KeepsUnknownAndOverflowedLayoutsUnknown) {
    auto const unknown{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("value", "UnknownType")},
                                           .export_specifier = std::nullopt}})};
    auto const unknown_analysis{
        Analyzer::analyze_record(unknown.types, unknown.type, AbiProfile::host_common())};
    EXPECT_FALSE(unknown_analysis.size_bytes.has_value());
    EXPECT_FALSE(unknown_analysis.diagnostics.empty());

    auto const overflow{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member(
            "values", "std::uint64_t", std::numeric_limits<std::uint64_t>::max())},
        .export_specifier = std::nullopt}})};
    auto const overflow_analysis{
        Analyzer::analyze_record(overflow.types, overflow.type, AbiProfile::host_common())};
    EXPECT_FALSE(overflow_analysis.size_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.members[0].extent_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ScalesAggregateWasteAndDiagnosesUnknownOrOverflowedTargetFacts) {
    auto const fixture{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member("small", "std::uint8_t"), record_member("wide", "std::uint32_t")},
        .export_specifier = std::nullopt}})};

    auto analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 500);
    EXPECT_EQ(analysis.aggregate.total_internal_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 0);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 13);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    AbiProfile unknown_memory{"unknown-memory"};
    unknown_memory.set("std::uint8_t",
                       {.size_bytes = 1,
                        .alignment_bytes = 1,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    unknown_memory.set("std::uint32_t",
                       {.size_bytes = 4,
                        .alignment_bytes = 4,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    analysis = Analyzer::analyze_record(fixture.types, fixture.type, unknown_memory, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());

    analysis = Analyzer::analyze_record(fixture.types,
                                        fixture.type,
                                        AbiProfile::host_common(),
                                        std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_padding_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, CountsCacheLineAndPageStraddlingForAlignedContiguousArrays) {
    auto const periodic{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto analysis{
        Analyzer::analyze_record(periodic.types, periodic.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 12);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);

    auto const larger_than_line{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("bytes", "std::uint8_t", 65)},
                                           .export_specifier = std::nullopt}})};
    analysis = Analyzer::analyze_record(
        larger_than_line.types, larger_than_line.type, AbiProfile::host_common(), 100);
    EXPECT_EQ(analysis.size_bytes, 65);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 100);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 1);
}

TEST(RecordAnalyzer, ReportsExplicitSequentialMemberAccessTraffic) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    auto access{Analyzer::analyze_record_member_access(record, "wide", AbiProfile::host_common())};

    EXPECT_EQ(access.element_count, 100);
    ASSERT_EQ(access.accesses.size(), 1);
    EXPECT_EQ(access.accesses.front().operation, AccessOperation::read);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_EQ(access.read_useful_bytes, 400);
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_EQ(access.logical_read_useful_bytes, 400);
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.object_footprint_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.cache_bytes_touched, 1'216);
    EXPECT_EQ(access.non_selected_cache_bytes, 816);
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l2.has_value());
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l3.has_value());
    EXPECT_EQ(access.pages_touched, 1);
    EXPECT_EQ(access.page_bytes_touched, 4'096);
    EXPECT_EQ(access.non_selected_page_bytes, 3'696);
    EXPECT_TRUE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(record, "missing", AbiProfile::host_common());
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(
        record, "wide", AbiProfile::host_common(), AccessOperation::read_write, 3);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_EQ(access.logical_read_useful_bytes, 1'200);
    EXPECT_EQ(access.logical_write_useful_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.read_cache_lines_touched, 19);
    EXPECT_EQ(access.write_cache_lines_touched, 19);

    access = Analyzer::analyze_record_member_access(record,
                                                    "wide",
                                                    AbiProfile::host_common(),
                                                    AccessOperation::read,
                                                    std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(
        record, "wide", AbiProfile::host_common(), AccessOperation::read, 0);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_FALSE(access.logical_write_useful_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, UnionsMultipleSelectedMembersWithoutDoubleCounting) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    std::vector<std::string> const members{"small", "medium", "small"};
    auto const access{Analyzer::analyze_record_access(record, members, AbiProfile::host_common())};

    EXPECT_EQ(access.member_names, (std::vector<std::string>{"small", "medium"}));
    EXPECT_EQ(access.useful_bytes, 300);
    EXPECT_EQ(access.object_footprint_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.cache_bytes_touched, 1'216);
    EXPECT_EQ(access.non_selected_cache_bytes, 916);
    EXPECT_EQ(access.pages_touched, 1);
    EXPECT_TRUE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, ClassifiesIndividualMemberAccessOperations) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto const abi{AbiProfile::host_common()};
    auto const record{Analyzer::analyze_record(fixture.types, fixture.type, abi, 100)};
    std::vector<AccessIntent> const accesses{
        {.name = "small", .operation = AccessOperation::read},
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_record_access(record, accesses, abi, 3)};

    EXPECT_EQ(analysis.accesses, accesses);
    EXPECT_EQ(analysis.useful_bytes, 700);
    EXPECT_EQ(analysis.read_useful_bytes, 300);
    EXPECT_EQ(analysis.write_useful_bytes, 600);
    EXPECT_EQ(analysis.logical_read_useful_bytes, 900);
    EXPECT_EQ(analysis.logical_write_useful_bytes, 1'800);
    EXPECT_EQ(analysis.cache_lines_touched, 19);
    EXPECT_EQ(analysis.read_cache_lines_touched, 19);
    EXPECT_EQ(analysis.read_cache_bytes_touched, 1'216);
    EXPECT_EQ(analysis.write_cache_lines_touched, 19);
    EXPECT_EQ(analysis.write_cache_bytes_touched, 1'216);
    EXPECT_EQ(analysis.read_pages_touched, 1);
    EXPECT_EQ(analysis.write_pages_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto conflicting{accesses};
    conflicting.push_back({.name = "small", .operation = AccessOperation::write});
    auto const normalized{Analyzer::analyze_record_access(record, conflicting, abi, 3)};
    EXPECT_EQ(normalized.accesses, accesses);
    EXPECT_EQ(normalized.read_useful_bytes, 300);
    EXPECT_EQ(normalized.write_useful_bytes, 600);
    EXPECT_FALSE(normalized.diagnostics.empty());
}

TEST(RecordAccessComparison, ReportsTargetLayoutAndRegionConsequencesForOneWorkload) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 2'000,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "Test wide target"});
    second_target.set_memory_facts({.cache_line_bytes = 128,
                                    .page_bytes = 8'192,
                                    .l1_data_cache_bytes = 2'000,
                                    .l2_cache_bytes = std::nullopt,
                                    .l3_cache_bytes = std::nullopt,
                                    .provenance = "Test second memory"});
    std::array const accesses{AccessIntent{.name = "small", .operation = AccessOperation::read},
                              AccessIntent{.name = "wide", .operation = AccessOperation::write}};
    auto const first_record{
        Analyzer::analyze_record(fixture.types, fixture.type, first_target, 100)};
    auto const second_record{
        Analyzer::analyze_record(fixture.types, fixture.type, second_target, 100)};
    auto const first{Analyzer::analyze_record_access(first_record, accesses, first_target, 3)};
    auto const second{Analyzer::analyze_record_access(second_record, accesses, second_target, 3)};

    auto const comparison{Analyzer::compare_record_access(first, second)};
    auto const identical{Analyzer::compare_record_access(first, first)};

    EXPECT_EQ(comparison.first.type, fixture.type);
    EXPECT_EQ(comparison.first.useful_bytes, 500);
    EXPECT_EQ(comparison.second.useful_bytes, 900);
    ASSERT_TRUE(comparison.useful_byte_delta.has_value());
    EXPECT_EQ(comparison.useful_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.read_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.write_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.write_useful_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.logical_write_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.object_footprint_byte_delta.has_value());
    EXPECT_EQ(comparison.object_footprint_byte_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.cache_byte_delta.has_value());
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 1'216);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    ASSERT_TRUE(comparison.non_selected_page_byte_delta.has_value());
    EXPECT_EQ(comparison.non_selected_page_byte_delta->magnitude, 3'696);
    EXPECT_EQ(comparison.first.cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.cache_footprint_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());

    ASSERT_TRUE(identical.useful_byte_delta.has_value());
    EXPECT_EQ(identical.useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.cache_byte_delta.has_value());
    EXPECT_EQ(identical.cache_byte_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(RecordAccessComparison, PreservesUnknownAndOverflowedTargetFacts) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("value", "std::uint32_t")},
                                           .export_specifier = std::nullopt}})};
    std::array const accesses{
        AccessIntent{.name = "value", .operation = AccessOperation::read_write}};
    auto const known_record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown_record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile{"Unknown target"}, 10)};
    auto const known{
        Analyzer::analyze_record_access(known_record, accesses, AbiProfile::host_common(), 2)};
    auto const unknown{
        Analyzer::analyze_record_access(unknown_record, accesses, AbiProfile{"Unknown target"}, 2)};

    auto const unknown_comparison{Analyzer::compare_record_access(known, unknown)};

    EXPECT_FALSE(unknown_comparison.useful_byte_delta.has_value());
    EXPECT_FALSE(unknown_comparison.object_footprint_byte_delta.has_value());
    EXPECT_FALSE(unknown_comparison.cache_byte_delta.has_value());
    EXPECT_TRUE(
        std::ranges::any_of(unknown_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target record access:");
        }));

    auto const overflow_record{Analyzer::analyze_record(fixture.types,
                                                        fixture.type,
                                                        AbiProfile::host_common(),
                                                        std::numeric_limits<std::uint64_t>::max())};
    auto const overflow{
        Analyzer::analyze_record_access(overflow_record, accesses, AbiProfile::host_common(), 2)};
    auto const overflow_comparison{Analyzer::compare_record_access(overflow, overflow)};

    EXPECT_FALSE(overflow_comparison.useful_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.object_footprint_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.cache_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.diagnostics.empty());
}

TEST(RecordAccessComparison, RejectsMismatchedWorkloadsTransactionally) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("first", "std::uint32_t"),
                                                       record_member("second", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    std::array const accesses{AccessIntent{.name = "first", .operation = AccessOperation::read},
                              AccessIntent{.name = "second", .operation = AccessOperation::write}};
    auto const first{
        Analyzer::analyze_record_access(record, accesses, AbiProfile::host_common(), 2)};
    auto second{first};
    second.accesses.front().operation = AccessOperation::write;

    auto comparison{Analyzer::compare_record_access(first, second)};

    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("access classifications"),
              std::string::npos);

    second = first;
    second.member_names.pop_back();
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("complete unique selected member set"),
              std::string::npos);

    second = first;
    second.multiplicity = 3;
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different access multiplicities"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different records"), std::string::npos);
}

TEST(RecordAnalyzer, MemberAccessHandlesSpanningMembersAndOverflow) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("prefix", "std::uint8_t"),
                                                       record_member("bytes", "std::uint8_t", 65)},
                                           .export_specifier = std::nullopt}})};
    auto record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 2)};
    auto access{Analyzer::analyze_record_member_access(record, "bytes", AbiProfile::host_common())};
    EXPECT_EQ(record.size_bytes, 66);
    EXPECT_EQ(access.useful_bytes, 130);
    EXPECT_EQ(access.cache_lines_touched, 3);
    EXPECT_EQ(access.cache_bytes_touched, 192);
    EXPECT_EQ(access.non_selected_cache_bytes, 62);

    record = Analyzer::analyze_record(fixture.types,
                                      fixture.type,
                                      AbiProfile::host_common(),
                                      std::numeric_limits<std::uint64_t>::max());
    access = Analyzer::analyze_record_member_access(record, "bytes", AbiProfile::host_common());
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.cache_bytes_touched.has_value());
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, PeriodicMemberAccessMatchesBruteForceRegionUnion) {
    constexpr std::array counts{
        std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{7}, std::uint64_t{33}};
    for (std::uint64_t stride{1}; stride <= 32; ++stride) {
        for (std::uint64_t offset{}; offset < stride; ++offset) {
            for (std::uint64_t extent{1}; extent <= stride - offset; ++extent) {
                for (auto const count : counts) {
                    RecordAnalysis record{};
                    record.size_bytes = stride;
                    record.members.push_back(
                        RecordMemberAnalysis{.name = "selected",
                                             .semantic_type = {},
                                             .element_count = 1,
                                             .element_facts = std::nullopt,
                                             .offset_bytes = offset,
                                             .extent_bytes = extent,
                                             .padding_before_bytes = std::nullopt});
                    record.aggregate.element_count = count;
                    record.aggregate.total_storage_bytes = stride * count;

                    std::set<std::uint64_t> expected_lines;
                    for (std::uint64_t index{}; index < count; ++index) {
                        auto const begin{index * stride + offset};
                        auto const end{begin + extent - 1};
                        for (auto line{begin / 64}; line <= end / 64; ++line) {
                            expected_lines.insert(line);
                        }
                    }
                    auto const access{Analyzer::analyze_record_member_access(
                        record, "selected", AbiProfile::host_common())};
                    ASSERT_TRUE(access.cache_lines_touched.has_value());
                    EXPECT_EQ(*access.cache_lines_touched, expected_lines.size())
                        << "stride=" << stride << " offset=" << offset << " extent=" << extent
                        << " count=" << count;
                }
            }
        }
    }
}

TEST(RecordAnalyzer, PeriodicMultiMemberAccessMatchesBruteForceRegionUnion) {
    constexpr std::array counts{
        std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{7}, std::uint64_t{33}};
    std::vector<std::string> const selected_members{"first", "last"};
    for (std::uint64_t stride{2}; stride <= 32; ++stride) {
        for (auto const count : counts) {
            RecordAnalysis record{};
            record.size_bytes = stride;
            record.members.push_back(RecordMemberAnalysis{.name = "first",
                                                          .semantic_type = {},
                                                          .element_count = 1,
                                                          .element_facts = std::nullopt,
                                                          .offset_bytes = 0,
                                                          .extent_bytes = 1,
                                                          .padding_before_bytes = std::nullopt});
            record.members.push_back(RecordMemberAnalysis{.name = "last",
                                                          .semantic_type = {},
                                                          .element_count = 1,
                                                          .element_facts = std::nullopt,
                                                          .offset_bytes = stride - 1,
                                                          .extent_bytes = 1,
                                                          .padding_before_bytes = std::nullopt});
            record.aggregate.element_count = count;
            record.aggregate.total_storage_bytes = stride * count;

            std::set<std::uint64_t> expected_lines;
            for (std::uint64_t index{}; index < count; ++index) {
                expected_lines.insert(index * stride / 64);
                expected_lines.insert((index * stride + stride - 1) / 64);
            }
            auto const access{Analyzer::analyze_record_access(
                record, selected_members, AbiProfile::host_common())};
            ASSERT_TRUE(access.cache_lines_touched.has_value());
            EXPECT_EQ(*access.cache_lines_touched, expected_lines.size())
                << "stride=" << stride << " count=" << count;
        }
    }
}

TEST(IntegerScalarAnalyzer, ReportsDomainSentinelAndDerivedWidthWithoutPhysicalFacts) {
    auto const fixture{integer_scalar_type()};

    auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.signedness);
    EXPECT_EQ(analysis.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.maximum_value, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.live_value_count, 11U);
    EXPECT_EQ(analysis.sentinel_code_count, 1U);
    EXPECT_EQ(analysis.required_code_count, 12U);
    EXPECT_EQ(analysis.minimum_required_bits, 4U);
    EXPECT_FALSE(analysis.declared_bit_width.has_value());
    EXPECT_EQ(analysis.effective_bit_width, 4U);
    EXPECT_EQ(analysis.unused_codes, 4U);
    ASSERT_EQ(analysis.named_codes.size(), 2U);
    EXPECT_TRUE(analysis.named_codes[1].sentinel);
}

TEST(IntegerScalarAnalyzer, ReportsZeroUnusedCodesForFullSixtyFourBitDomains) {
    auto make_fixture = [](bool const signedness) {
        codegen::Manifest manifest{
            .schema_version = codegen::manifest_schema_version,
            .types = {},
            .modules = {codegen::ScalarModuleSchema{
                .settings = codegen::ModuleSettings{.name = "full_domain",
                                                    .header = "FullDomain.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = std::nullopt,
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .scalars = {codegen::IntegerScalarSchema{
                    .name = "FullDomain",
                    .signedness = signedness,
                    .minimum_value = signedness ? codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::int64_t>::min)()}
                                                : codegen::PackedIntegerValue{0},
                    .maximum_value = signedness ? codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::int64_t>::max)()}
                                                : codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::uint64_t>::max)()},
                    .bit_width = std::nullopt,
                    .named_codes = {},
                    .relationship = std::nullopt}}}}};
        auto types{lispb::schema::resolve_type_graph(manifest)};
        auto const type{*types.find_declared("full_domain", "FullDomain")};
        return TypeFixture{std::move(types), type};
    };

    for (auto const signedness : {false, true}) {
        auto const fixture{make_fixture(signedness)};
        auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.type)};

        EXPECT_EQ(analysis.signedness, signedness);
        EXPECT_EQ(analysis.effective_bit_width, 64U);
        EXPECT_FALSE(analysis.live_value_count.has_value());
        EXPECT_FALSE(analysis.required_code_count.has_value());
        EXPECT_EQ(analysis.unused_codes, 0U);
    }
}

TEST(IntegerScalarAnalyzer, DerivesLinkedIndexCapacityWithSentinel) {
    auto const fixture{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::index_into, 12, 4'094, 4'095)};
    std::array capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};
    EXPECT_EQ(fits.relationship_kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(fits.relationship_target, "Entities");
    EXPECT_EQ(fits.relationship_target_extent, 4'000);
    EXPECT_EQ(fits.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(fits.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(fits.relationship_minimum_required_bits, 12);
    EXPECT_EQ(fits.relationship_width_sufficient, true);
    EXPECT_EQ(fits.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_capacity_headroom, 95);
    EXPECT_EQ(fits.relationship_semantic_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_sentinel_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_effective_capacity_headroom, 95);
    EXPECT_TRUE(fits.diagnostics.empty());

    capacity.front().element_capacity = 4'096;
    auto const insufficient{
        Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};
    EXPECT_EQ(insufficient.relationship_minimum_required_bits, 13);
    EXPECT_EQ(insufficient.relationship_width_sufficient, false);
    EXPECT_EQ(insufficient.relationship_code_space_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.relationship_capacity_headroom.has_value());
    EXPECT_EQ(insufficient.relationship_effective_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(insufficient.diagnostics.empty());
}

TEST(IntegerScalarAnalyzer, ComparesLinkedCapacityAcrossWidthThreshold) {
    auto const fixture{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::index_into, 12, 4'094, 4'095)};
    std::array const first_capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};
    std::array const second_capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'096, .byte_extent = std::nullopt}};

    auto const comparison{Analyzer::compare_integer_scalar_capacity(
        fixture.types, fixture.packed, first_capacity, second_capacity)};

    EXPECT_EQ(comparison.first.relationship_target_extent, 4'000);
    EXPECT_EQ(comparison.second.relationship_target_extent, 4'096);
    ASSERT_TRUE(comparison.relationship_target_extent_delta.has_value());
    EXPECT_EQ(comparison.relationship_target_extent_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.relationship_target_extent_delta->magnitude, 96);
    EXPECT_EQ(comparison.first.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(comparison.second.relationship_required_code_count,
              (ExactCodeCount{.value = 4'097, .two_to_64 = false}));
    EXPECT_EQ(comparison.first.relationship_minimum_required_bits, 12);
    EXPECT_EQ(comparison.second.relationship_minimum_required_bits, 13);
    ASSERT_TRUE(comparison.relationship_minimum_required_bit_delta.has_value());
    EXPECT_EQ(comparison.relationship_minimum_required_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.relationship_minimum_required_bit_delta->magnitude, 1);
    EXPECT_EQ(comparison.first.relationship_width_sufficient, true);
    EXPECT_EQ(comparison.second.relationship_width_sufficient, false);
    EXPECT_EQ(comparison.first.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(comparison.second.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(comparison.first.relationship_capacity_headroom, 95);
    EXPECT_FALSE(comparison.second.relationship_capacity_headroom.has_value());
    ASSERT_TRUE(comparison.relationship_code_space_capacity_limit_delta.has_value());
    EXPECT_EQ(comparison.relationship_code_space_capacity_limit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.relationship_capacity_headroom_delta.has_value());
    EXPECT_EQ(comparison.first.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(comparison.second.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(comparison.first.relationship_effective_capacity_headroom, 95);
    EXPECT_FALSE(comparison.second.relationship_effective_capacity_headroom.has_value());
    ASSERT_TRUE(comparison.relationship_effective_capacity_limit_delta.has_value());
    EXPECT_EQ(comparison.relationship_effective_capacity_limit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.relationship_effective_capacity_headroom_delta.has_value());
    EXPECT_EQ(comparison.relationship_width_fit_changed, true);
}

TEST(IntegerScalarAnalyzer, DerivesElementAndByteOffsetExtents) {
    auto const elements{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into, 12, 4'095)};
    std::array const element_facts{RelationshipTargetFacts{
        .target = elements.target, .element_capacity = 4'000, .byte_extent = 8'192}};
    auto const element_analysis{
        Analyzer::analyze_integer_scalar(elements.types, elements.packed, element_facts)};
    EXPECT_EQ(element_analysis.relationship_unit, codegen::SemanticRelationUnit::elements);
    EXPECT_EQ(element_analysis.relationship_target_extent, 4'000);
    EXPECT_EQ(element_analysis.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(element_analysis.relationship_minimum_required_bits, 12);
    EXPECT_EQ(element_analysis.relationship_width_sufficient, true);

    auto const bytes{relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into,
                                                       12,
                                                       4'095,
                                                       std::nullopt,
                                                       codegen::SemanticRelationUnit::bytes)};
    std::array const byte_facts{RelationshipTargetFacts{
        .target = bytes.target, .element_capacity = 4'000, .byte_extent = 8'192}};
    auto const byte_analysis{
        Analyzer::analyze_integer_scalar(bytes.types, bytes.packed, byte_facts)};
    EXPECT_EQ(byte_analysis.relationship_unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(byte_analysis.relationship_target_extent, 8'192);
    EXPECT_EQ(byte_analysis.relationship_live_value_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(byte_analysis.relationship_minimum_required_bits, 13);
    EXPECT_EQ(byte_analysis.relationship_width_sufficient, false);

    std::array const no_byte_extent{RelationshipTargetFacts{
        .target = bytes.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};
    auto const unknown{Analyzer::analyze_integer_scalar(bytes.types, bytes.packed, no_byte_extent)};
    EXPECT_FALSE(unknown.relationship_target_extent.has_value());
    EXPECT_FALSE(unknown.relationship_minimum_required_bits.has_value());
}

TEST(IntegerScalarAnalyzer, ComparesByteOffsetWidthThreshold) {
    auto const fixture{relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into,
                                                         12,
                                                         4'095,
                                                         std::nullopt,
                                                         codegen::SemanticRelationUnit::bytes)};
    std::array const first{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 1'000, .byte_extent = 4'096}};
    std::array const second{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 1'000, .byte_extent = 4'097}};

    auto const comparison{
        Analyzer::compare_integer_scalar_capacity(fixture.types, fixture.packed, first, second)};

    EXPECT_EQ(comparison.first.relationship_target_extent, 4'096);
    EXPECT_EQ(comparison.second.relationship_target_extent, 4'097);
    EXPECT_EQ(comparison.first.relationship_minimum_required_bits, 12);
    EXPECT_EQ(comparison.second.relationship_minimum_required_bits, 13);
    EXPECT_EQ(comparison.first.relationship_width_sufficient, true);
    EXPECT_EQ(comparison.second.relationship_width_sufficient, false);
    EXPECT_EQ(comparison.relationship_width_fit_changed, true);
}

TEST(IntegerScalarAnalyzer, HandlesLinkedCountExactAndBeyondTwoTo64) {
    auto const full{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::count_of, 64, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const capacity{
        RelationshipTargetFacts{.target = full.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};

    auto const exact{Analyzer::analyze_integer_scalar(full.types, full.packed, capacity)};
    EXPECT_EQ(exact.relationship_live_value_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(exact.relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(exact.relationship_minimum_required_bits, 64);
    EXPECT_EQ(exact.relationship_width_sufficient, true);
    EXPECT_EQ(exact.relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_capacity_headroom, 0);
    EXPECT_EQ(exact.relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_effective_capacity_headroom, 0);
    EXPECT_TRUE(exact.diagnostics.empty());

    auto const with_sentinel{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::count_of,
                                          64,
                                          (std::numeric_limits<std::uint64_t>::max)() - 1,
                                          (std::numeric_limits<std::uint64_t>::max)())};
    std::array const overflowing_capacity{
        RelationshipTargetFacts{.target = with_sentinel.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};
    auto const beyond{Analyzer::analyze_integer_scalar(
        with_sentinel.types, with_sentinel.packed, overflowing_capacity)};
    EXPECT_EQ(beyond.relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_FALSE(beyond.relationship_required_code_count.has_value());
    EXPECT_EQ(beyond.relationship_minimum_required_bits, 65);
    EXPECT_EQ(beyond.relationship_width_sufficient, false);
    EXPECT_EQ(beyond.relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(beyond.relationship_capacity_headroom.has_value());
    EXPECT_EQ(beyond.relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(beyond.relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(beyond.relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(beyond.relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(beyond.diagnostics.empty());
}

TEST(IntegerScalarAnalyzer, SeparatesSemanticSentinelAndCodeSpaceCapacityLimits) {
    auto const fixture{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::index_into, 12, 100, 200)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 50, .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};

    EXPECT_EQ(analysis.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(analysis.relationship_capacity_headroom, 4'045);
    EXPECT_EQ(analysis.relationship_semantic_capacity_limit, 101);
    EXPECT_EQ(analysis.relationship_sentinel_capacity_limit, 200);
    EXPECT_EQ(analysis.relationship_effective_capacity_limit, 101);
    EXPECT_EQ(analysis.relationship_effective_capacity_headroom, 51);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(OptionalSentinelAnalyzer, ReportsCodeSpaceAndScaledPayloadWithoutAbiFacts) {
    auto const fixture{optional_sentinel_type()};

    auto const analysis{Analyzer::analyze_optional_sentinel(fixture.types, fixture.type, 100)};

    EXPECT_EQ(analysis.sentinel_name, "Invalid");
    EXPECT_EQ(analysis.sentinel_value, codegen::PackedIntegerValue{15});
    EXPECT_FALSE(analysis.source_signedness);
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.present_value_count, 11U);
    EXPECT_EQ(analysis.absence_code_count, 1U);
    EXPECT_EQ(analysis.other_sentinel_code_count, 1U);
    EXPECT_EQ(analysis.unused_code_count, 3U);
    EXPECT_EQ(analysis.encoded_storage_bits, 4U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 16, .two_to_64 = false}));
    EXPECT_EQ(analysis.element_count, 100U);
    EXPECT_EQ(analysis.total_encoded_bits, 400U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_optional_sentinel(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.total_encoded_bits.has_value());
    ASSERT_EQ(overflow.diagnostics.size(), 1U);
    EXPECT_EQ(overflow.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(OptionalPresenceBitAnalyzer, ReportsCanonicalAbsenceAndScaledPayloadWithoutAbiFacts) {
    auto const fixture{optional_presence_bit_type()};

    auto const analysis{Analyzer::analyze_optional_presence_bit(fixture.types, fixture.type, 100)};

    EXPECT_FALSE(analysis.source_signedness);
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.present_value_count, 11U);
    EXPECT_EQ(analysis.canonical_absence_state_count, 1U);
    EXPECT_EQ(analysis.source_sentinel_code_count, 2U);
    EXPECT_EQ(analysis.source_unused_payload_codes, 3U);
    EXPECT_EQ(analysis.presence_bits, 1U);
    EXPECT_EQ(analysis.payload_bits, 4U);
    EXPECT_EQ(analysis.encoded_storage_bits, 5U);
    EXPECT_EQ(analysis.noncanonical_absence_patterns, 15U);
    EXPECT_EQ(analysis.element_count, 100U);
    EXPECT_EQ(analysis.total_encoded_bits, 500U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_optional_presence_bit(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.total_encoded_bits.has_value());
    ASSERT_EQ(overflow.diagnostics.size(), 1U);
    EXPECT_EQ(overflow.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(OptionalPresenceBitAnalyzer, PreservesHonest65BitEncodingForFullWidthSource) {
    auto const fixture{optional_presence_bit_type(64)};

    auto const analysis{Analyzer::analyze_optional_presence_bit(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.present_value_count.has_value());
    EXPECT_EQ(analysis.source_sentinel_code_count, 0U);
    EXPECT_EQ(analysis.source_unused_payload_codes, 0U);
    EXPECT_EQ(analysis.payload_bits, 64U);
    EXPECT_EQ(analysis.encoded_storage_bits, 65U);
    EXPECT_EQ(analysis.noncanonical_absence_patterns, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(analysis.total_encoded_bits, 65U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(OptionalEncodingComparison, ReportsPolicyAndScaledBitConsequences) {
    auto const fixture{optional_comparison_types()};

    auto const comparison{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.presence, 100)};

    EXPECT_TRUE(comparison.supported_encodings);
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.second.kind, OptionalEncodingKind::presence_bit);
    EXPECT_EQ(comparison.first.absence_sentinel_name, "Invalid");
    EXPECT_FALSE(comparison.second.absence_sentinel_name.has_value());
    EXPECT_EQ(comparison.first.present_value_count, 11U);
    EXPECT_EQ(comparison.second.present_value_count, 11U);
    EXPECT_EQ(comparison.first.canonical_absence_state_count, 1U);
    EXPECT_EQ(comparison.second.canonical_absence_state_count, 1U);
    EXPECT_EQ(comparison.first.source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.second.source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.first.source_sentinel_codes_used_for_absence, 1U);
    EXPECT_EQ(comparison.second.source_sentinel_codes_used_for_absence, 0U);
    EXPECT_EQ(comparison.first.remaining_source_sentinel_code_count, 1U);
    EXPECT_EQ(comparison.second.remaining_source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.first.source_unused_payload_codes, 3U);
    EXPECT_EQ(comparison.second.source_unused_payload_codes, 3U);
    EXPECT_EQ(comparison.first.noncanonical_absence_patterns, 0U);
    EXPECT_EQ(comparison.second.noncanonical_absence_patterns, 15U);
    EXPECT_EQ(comparison.first.encoded_storage_bits, 4U);
    EXPECT_EQ(comparison.second.encoded_storage_bits, 5U);
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 1U);
    EXPECT_EQ(comparison.first.total_encoded_bits, 400U);
    EXPECT_EQ(comparison.second.total_encoded_bits, 500U);
    ASSERT_TRUE(comparison.total_encoded_bit_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_bit_delta->magnitude, 100U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(OptionalEncodingComparison, SupportsSiblingSentinelPolicies) {
    auto const fixture{optional_comparison_types()};

    auto const comparison{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.second_sentinel, 100)};

    EXPECT_TRUE(comparison.supported_encodings);
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.second.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.first.absence_sentinel_name, "Invalid");
    EXPECT_EQ(comparison.second.absence_sentinel_name, "Pending");
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 0U);
}

TEST(OptionalEncodingComparison, Preserves64To65BitBoundaryAndOverflow) {
    auto const fixture{optional_comparison_types(true)};

    auto const comparison{
        Analyzer::compare_optional_encodings(fixture.types, fixture.sentinel, fixture.presence)};
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.encoded_storage_bits, 64U);
    EXPECT_EQ(comparison.second.encoded_storage_bits, 65U);
    EXPECT_EQ(comparison.first.source_unused_payload_codes, 0U);
    EXPECT_EQ(comparison.second.source_unused_payload_codes, 0U);
    EXPECT_EQ(comparison.second.noncanonical_absence_patterns,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(comparison.first.total_encoded_bits, 64U);
    EXPECT_EQ(comparison.second.total_encoded_bits, 65U);

    auto const overflow{
        Analyzer::compare_optional_encodings(fixture.types,
                                             fixture.sentinel,
                                             fixture.presence,
                                             (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.first.total_encoded_bits.has_value());
    EXPECT_FALSE(overflow.second.total_encoded_bits.has_value());
    EXPECT_FALSE(overflow.total_encoded_bit_delta.has_value());
    EXPECT_EQ(overflow.diagnostics.size(), 2U);
}

TEST(OptionalEncodingComparison, RejectsDifferentSourcesAndNonOptionalTypes) {
    auto const fixture{optional_comparison_types()};

    auto const incompatible{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.other_presence)};
    EXPECT_TRUE(incompatible.supported_encodings);
    EXPECT_FALSE(incompatible.compatible_source);
    ASSERT_EQ(incompatible.diagnostics.size(), 1U);
    EXPECT_EQ(incompatible.diagnostics.front().severity, DiagnosticSeverity::error);

    auto const unsupported{
        Analyzer::compare_optional_encodings(fixture.types, fixture.source, fixture.presence)};
    EXPECT_FALSE(unsupported.supported_encodings);
    EXPECT_FALSE(unsupported.compatible_source);
    ASSERT_EQ(unsupported.diagnostics.size(), 1U);
    EXPECT_EQ(unsupported.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(LinearQuantizedAnalyzer, ReportsCapacityResolutionAndEndpointFacts) {
    auto const fixture{linear_quantized_type()};

    auto const analysis{Analyzer::analyze_linear_quantized(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.source_span, 1000U);
    EXPECT_EQ(analysis.encoded_storage_bits, 8U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 256, .two_to_64 = false}));
    EXPECT_EQ(analysis.reserved_code_count, 1U);
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 255, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(analysis.resolution), 1000.0 / 254.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(analysis.maximum_rounding_error), 500.0 / 254.0, 1e-12);
    EXPECT_TRUE(analysis.minimum_endpoint_exact);
    EXPECT_TRUE(analysis.maximum_endpoint_exact);
    EXPECT_EQ(analysis.clipping, codegen::QuantizationClipping::clamp);
}

TEST(LinearQuantizedAnalyzer, PreservesHonestExactCapacityAt64Bits) {
    auto const full_fixture{linear_quantized_type(64, 0)};
    auto const full{Analyzer::analyze_linear_quantized(full_fixture.types, full_fixture.type)};
    EXPECT_EQ(full.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.usable_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_GT(full.resolution, 0.0L);

    auto const reserved_fixture{linear_quantized_type(64, 1)};
    auto const reserved{
        Analyzer::analyze_linear_quantized(reserved_fixture.types, reserved_fixture.type)};
    EXPECT_EQ(reserved.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(
        reserved.usable_code_count,
        (ExactCodeCount{.value = (std::numeric_limits<std::uint64_t>::max)(), .two_to_64 = false}));
    EXPECT_GT(reserved.resolution, 0.0L);
}

TEST(LinearQuantizedAnalyzer, HandlesCrossZeroAndWhollyNegativeSignedDomains) {
    auto const crossing_fixture{linear_quantized_type(8, 1, true, -100, 100)};
    auto const crossing{
        Analyzer::analyze_linear_quantized(crossing_fixture.types, crossing_fixture.type)};
    EXPECT_EQ(crossing.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(crossing.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_EQ(crossing.source_span, 200U);
    EXPECT_EQ(crossing.usable_code_count, (ExactCodeCount{.value = 255, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(crossing.resolution), 200.0 / 254.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(crossing.maximum_rounding_error), 100.0 / 254.0, 1e-12);
    EXPECT_TRUE(crossing.minimum_endpoint_exact);
    EXPECT_TRUE(crossing.maximum_endpoint_exact);

    auto const negative_fixture{linear_quantized_type(10, 2, true, -1000, -1)};
    auto const negative{
        Analyzer::analyze_linear_quantized(negative_fixture.types, negative_fixture.type)};
    EXPECT_EQ(negative.source_minimum, codegen::PackedIntegerValue{-1000});
    EXPECT_EQ(negative.source_maximum, codegen::PackedIntegerValue{-1});
    EXPECT_EQ(negative.source_span, 999U);
    EXPECT_EQ(negative.usable_code_count, (ExactCodeCount{.value = 1022, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(negative.resolution), 999.0 / 1021.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(negative.maximum_rounding_error), 999.0 / 2042.0, 1e-12);
}

TEST(LinearQuantizedAnalyzer, PreservesFullSigned64BitSpanWithoutOverflow) {
    auto const fixture{linear_quantized_type(
        64,
        0,
        true,
        codegen::PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63),
        codegen::PackedIntegerValue{(std::numeric_limits<std::int64_t>::max)()})};

    auto const analysis{Analyzer::analyze_linear_quantized(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.source_span, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.resolution, 1.0L);
    EXPECT_EQ(analysis.maximum_rounding_error, 0.5L);
}

TEST(LinearQuantizedComparison, ReportsPrecisionAndScaledPayloadConsequences) {
    auto const fixture{linear_quantized_pair()};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second, 1'000)};

    EXPECT_TRUE(comparison.compatible_source);
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 2U);
    ASSERT_TRUE(comparison.total_code_count_delta.has_value());
    EXPECT_EQ(comparison.total_code_count_delta->magnitude, 768U);
    ASSERT_TRUE(comparison.usable_code_count_delta.has_value());
    EXPECT_EQ(comparison.usable_code_count_delta->magnitude, 767U);
    ASSERT_TRUE(comparison.reserved_code_count_delta.has_value());
    EXPECT_EQ(comparison.reserved_code_count_delta->magnitude, 1U);
    EXPECT_EQ(comparison.clipping_changed, true);
    ASSERT_TRUE(comparison.resolution_delta.has_value());
    EXPECT_LT(*comparison.resolution_delta, 0.0L);
    ASSERT_TRUE(comparison.maximum_rounding_error_delta.has_value());
    EXPECT_LT(*comparison.maximum_rounding_error_delta, 0.0L);
    EXPECT_EQ(comparison.first_total_encoded_bits, 8'000U);
    EXPECT_EQ(comparison.second_total_encoded_bits, 10'000U);
    ASSERT_TRUE(comparison.total_encoded_bit_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.total_encoded_bit_delta->magnitude, 2'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(LinearQuantizedComparison, PreservesExactTwoTo64CapacityDelta) {
    auto const fixture{linear_quantized_pair(true, 8, 64)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second)};

    ASSERT_TRUE(comparison.total_code_count_delta.has_value());
    EXPECT_EQ(comparison.total_code_count_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.total_code_count_delta->magnitude,
              (std::numeric_limits<std::uint64_t>::max)() - 255U);
}

TEST(LinearQuantizedComparison, ComparesRepresentationsOfOneSignedDomain) {
    auto const fixture{linear_quantized_pair(true, 8, 10, true, -100, 100)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second, 10'000)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(comparison.second.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_EQ(comparison.first.source_span, 200U);
    EXPECT_EQ(comparison.second.source_span, 200U);
    ASSERT_TRUE(comparison.resolution_delta.has_value());
    EXPECT_LT(*comparison.resolution_delta, 0.0L);
    ASSERT_TRUE(comparison.maximum_rounding_error_delta.has_value());
    EXPECT_LT(*comparison.maximum_rounding_error_delta, 0.0L);
    EXPECT_EQ(comparison.first_total_encoded_bits, 80'000U);
    EXPECT_EQ(comparison.second_total_encoded_bits, 100'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(LinearQuantizedComparison, RejectsDifferentSemanticSources) {
    auto const fixture{linear_quantized_pair(false)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_FALSE(comparison.total_encoded_bit_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics[0].severity, DiagnosticSeverity::error);
}

TEST(LinearQuantizedComparison, ReportsScaledPayloadOverflowWithoutInventingStorage) {
    auto const fixture{linear_quantized_pair()};

    auto const comparison{Analyzer::compare_linear_quantized(
        fixture.types, fixture.first, fixture.second, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_FALSE(comparison.first_total_encoded_bits.has_value());
    EXPECT_FALSE(comparison.second_total_encoded_bits.has_value());
    EXPECT_FALSE(comparison.total_encoded_bit_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 2U);
}

TEST(FixedPointAnalyzer, ReportsSignedRangeResolutionAndRounding) {
    auto const fixture{fixed_point_type(true, 16, 4)};

    auto const analysis{Analyzer::analyze_fixed_point(fixture.types, fixture.type, 1'000)};

    EXPECT_TRUE(analysis.signedness);
    EXPECT_EQ(analysis.total_bits, 16U);
    EXPECT_EQ(analysis.fractional_bits, 4U);
    EXPECT_EQ(analysis.whole_bits, 11U);
    EXPECT_EQ(analysis.minimum_raw_value, codegen::PackedIntegerValue{-32'768});
    EXPECT_EQ(analysis.maximum_raw_value, codegen::PackedIntegerValue{32'767});
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.scale), 16.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.resolution), 0.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.minimum_value), -2'048.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_value), 2'047.9375);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 0.03125);
    EXPECT_EQ(analysis.rounding, codegen::FixedPointRounding::nearest_even);
    EXPECT_EQ(analysis.total_encoded_bits, 16'000U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(FixedPointAnalyzer, SupportsUnsignedFractionOnlyAndTowardZeroRounding) {
    auto const fixture{fixed_point_type(false, 8, 8, codegen::FixedPointRounding::toward_zero)};

    auto const analysis{Analyzer::analyze_fixed_point(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.signedness);
    EXPECT_EQ(analysis.whole_bits, 0U);
    EXPECT_EQ(analysis.minimum_raw_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.maximum_raw_value, codegen::PackedIntegerValue{255});
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.minimum_value), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_value), 255.0 / 256.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 1.0 / 256.0);
}

TEST(FixedPointAnalyzer, PreservesFullWidthRawRangeAndReportsScaledOverflow) {
    auto const fixture{fixed_point_type(false, 64, 64)};

    auto const analysis{Analyzer::analyze_fixed_point(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.maximum_raw_value,
              codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()});
    EXPECT_GT(analysis.resolution, 0.0L);
    EXPECT_FALSE(analysis.total_encoded_bits.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(MiniFloatAnalyzer, ReportsBinary16StyleCodeRolesAndNumericalRange) {
    auto const fixture{mini_float_type(1, 5, 10, 15)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type, 1'000)};

    EXPECT_EQ(analysis.sign_bits, 1U);
    EXPECT_EQ(analysis.exponent_bits, 5U);
    EXPECT_EQ(analysis.significand_bits, 10U);
    EXPECT_EQ(analysis.total_bits, 16U);
    EXPECT_EQ(analysis.exponent_bias, 15);
    EXPECT_EQ(analysis.exponent_code_count, 32U);
    EXPECT_EQ(analysis.normal_exponent_code_count, 30U);
    EXPECT_EQ(analysis.minimum_normal_exponent, -14);
    EXPECT_EQ(analysis.maximum_normal_exponent, 15);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 65'536, .two_to_64 = false}));
    EXPECT_EQ(analysis.zero_code_count, 2U);
    EXPECT_EQ(analysis.infinity_code_count, 2U);
    EXPECT_EQ(analysis.nan_code_count, 2'046U);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, 2'046U);
    ASSERT_TRUE(analysis.minimum_positive_subnormal.has_value());
    ASSERT_TRUE(analysis.minimum_positive_normal.has_value());
    ASSERT_TRUE(analysis.maximum_finite.has_value());
    ASSERT_TRUE(analysis.minimum_finite.has_value());
    ASSERT_TRUE(analysis.unit_interval_resolution.has_value());
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_subnormal),
                     std::ldexp(1.0, -24));
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_normal), std::ldexp(1.0, -14));
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.maximum_finite), 65'504.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_finite), -65'504.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.unit_interval_resolution), std::ldexp(1.0, -10));
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_relative_rounding_error),
                     std::ldexp(1.0, -11));
    EXPECT_EQ(analysis.total_encoded_bits, 16'000U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(MiniFloatAnalyzer, SupportsUnsignedFormatsWithoutFractionPayloads) {
    auto const fixture{mini_float_type(0, 3, 0, 3)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.total_bits, 3U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 8, .two_to_64 = false}));
    EXPECT_EQ(analysis.zero_code_count, 1U);
    EXPECT_EQ(analysis.infinity_code_count, 1U);
    EXPECT_EQ(analysis.nan_code_count, 0U);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, 0U);
    EXPECT_FALSE(analysis.minimum_positive_subnormal.has_value());
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_normal), 0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.maximum_finite), 8.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_finite), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.unit_interval_resolution), 1.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_relative_rounding_error), 0.5);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(MiniFloatAnalyzer, PreservesTwoTo64CodeSpaceAndReportsScaledOverflow) {
    auto const fixture{mini_float_type(1, 2, 61, 1)};

    auto const analysis{Analyzer::analyze_mini_float(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.total_bits, 64U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.nan_code_count, (std::uint64_t{1} << 62) - 2);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, (std::uint64_t{1} << 62) - 2);
    EXPECT_FALSE(analysis.total_encoded_bits.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(MiniFloatAnalyzer, KeepsExactExponentFactsWhenHostNumericsOverflow) {
    auto const fixture{mini_float_type(1, 5, 10, -32'768)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_normal_exponent, 32'769);
    EXPECT_EQ(analysis.maximum_normal_exponent, 32'798);
    EXPECT_FALSE(analysis.minimum_positive_subnormal.has_value());
    EXPECT_FALSE(analysis.minimum_positive_normal.has_value());
    EXPECT_FALSE(analysis.maximum_finite.has_value());
    EXPECT_FALSE(analysis.minimum_finite.has_value());
    EXPECT_FALSE(analysis.unit_interval_resolution.has_value());
    EXPECT_GE(analysis.diagnostics.size(), 4U);
    for (auto const& diagnostic : analysis.diagnostics) {
        EXPECT_EQ(diagnostic.severity, DiagnosticSeverity::warning);
    }
}

TEST(IntegerVarintAnalyzer, ReportsUnsignedEncodedByteRangeAndScaledBounds) {
    auto const fixture{
        integer_varint_type(false, 0, 16'384, codegen::IntegerVarintEncoding::unsigned_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type, 100)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 3U);
    EXPECT_EQ(analysis.minimum_total_bytes, 100U);
    EXPECT_EQ(analysis.maximum_total_bytes, 300U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintAnalyzer, IncludesSourceSentinelsInEncodedByteRange) {
    auto const fixture{integer_varint_type(false,
                                           0,
                                           100,
                                           codegen::IntegerVarintEncoding::unsigned_varint,
                                           (std::numeric_limits<std::uint64_t>::max)())};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 10U);
}

TEST(IntegerVarintAnalyzer, ReportsSignedLeb128RangeAcrossZero) {
    auto const fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::signed_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 2U);
}

TEST(IntegerVarintAnalyzer, HandlesFullSignedZigzagRangeAndAggregateOverflow) {
    auto const fixture{integer_varint_type(true,
                                           (std::numeric_limits<std::int64_t>::min)(),
                                           (std::numeric_limits<std::int64_t>::max)(),
                                           codegen::IntegerVarintEncoding::zigzag_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 10U);
    EXPECT_EQ(analysis.minimum_total_bytes, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(analysis.maximum_total_bytes.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
}

TEST(IntegerVarintComparison, ComparesSameSourceEncodingBounds) {
    auto const fixture{integer_varint_pair()};

    auto const comparison{
        Analyzer::compare_integer_varint(fixture.types, fixture.first, fixture.second, 1'000)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.encoding, codegen::IntegerVarintEncoding::signed_varint);
    EXPECT_EQ(comparison.second.encoding, codegen::IntegerVarintEncoding::zigzag_varint);
    ASSERT_TRUE(comparison.minimum_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.minimum_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.maximum_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.maximum_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.minimum_total_bytes, 1'000U);
    EXPECT_EQ(comparison.first.maximum_total_bytes, 2'000U);
    EXPECT_EQ(comparison.second.minimum_total_bytes, 1'000U);
    EXPECT_EQ(comparison.second.maximum_total_bytes, 2'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(IntegerVarintComparison, RejectsDifferentSemanticSources) {
    auto const fixture{integer_varint_pair(false)};

    auto const comparison{
        Analyzer::compare_integer_varint(fixture.types, fixture.first, fixture.second)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.minimum_encoded_byte_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(IntegerVarintDistribution, ComputesWeightedExpectedSizeFromExplicitValues) {
    auto const fixture{
        integer_varint_type(false, 0, 16'384, codegen::IntegerVarintEncoding::unsigned_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1},
                             IntegerVarintDistributionEntry{.value = 128, .weight = 3},
                             IntegerVarintDistributionEntry{.value = 16'384, .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries, 100)};

    EXPECT_EQ(analysis.valid_entry_count, 3U);
    EXPECT_EQ(analysis.total_weight, 5U);
    EXPECT_EQ(analysis.total_encoded_bytes, 10U);
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.0L);
    ASSERT_TRUE(analysis.expected_selected_bytes.has_value());
    EXPECT_EQ(*analysis.expected_selected_bytes, 200.0L);
    ASSERT_EQ(analysis.entries.size(), 3U);
    EXPECT_EQ(analysis.entries[0].encoded_bytes, 1U);
    EXPECT_EQ(analysis.entries[0].weighted_encoded_bytes, 1U);
    EXPECT_EQ(analysis.entries[1].encoded_bytes, 2U);
    EXPECT_EQ(analysis.entries[1].weighted_encoded_bytes, 6U);
    EXPECT_EQ(analysis.entries[2].encoded_bytes, 3U);
    EXPECT_EQ(analysis.entries[2].weighted_encoded_bytes, 3U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, ComputesSignedAndZigZagWeightedSizes) {
    auto const signed_fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::signed_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = -64, .weight = 2},
                             IntegerVarintDistributionEntry{.value = 64, .weight = 1}};

    auto const signed_analysis{Analyzer::analyze_integer_varint_distribution(
        signed_fixture.types, signed_fixture.type, entries, 3)};

    EXPECT_EQ(signed_analysis.total_weight, 3U);
    EXPECT_EQ(signed_analysis.total_encoded_bytes, 4U);
    ASSERT_TRUE(signed_analysis.expected_bytes_per_value.has_value());
    EXPECT_NEAR(static_cast<double>(*signed_analysis.expected_bytes_per_value), 4.0 / 3.0, 1e-12);
    ASSERT_TRUE(signed_analysis.expected_selected_bytes.has_value());
    EXPECT_NEAR(static_cast<double>(*signed_analysis.expected_selected_bytes), 4.0, 1e-12);

    auto const zigzag_fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::zigzag_varint)};
    auto const zigzag_analysis{Analyzer::analyze_integer_varint_distribution(
        zigzag_fixture.types, zigzag_fixture.type, entries, 3)};

    EXPECT_EQ(zigzag_analysis.total_weight, 3U);
    EXPECT_EQ(zigzag_analysis.total_encoded_bytes, 4U);
    ASSERT_TRUE(zigzag_analysis.expected_bytes_per_value.has_value());
    EXPECT_NEAR(static_cast<double>(*zigzag_analysis.expected_bytes_per_value), 4.0 / 3.0, 1e-12);
    EXPECT_TRUE(zigzag_analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, EmptyInputKeepsExpectedSizeUnknown) {
    auto const fixture{
        integer_varint_type(false, 0, 100, codegen::IntegerVarintEncoding::unsigned_varint)};

    auto const analysis{Analyzer::analyze_integer_varint_distribution(
        fixture.types, fixture.type, std::span<IntegerVarintDistributionEntry const>{})};

    EXPECT_EQ(analysis.valid_entry_count, 0U);
    EXPECT_EQ(analysis.total_weight, 0U);
    EXPECT_EQ(analysis.total_encoded_bytes, 0U);
    EXPECT_FALSE(analysis.expected_bytes_per_value.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(IntegerVarintDistribution, AcceptsSentinelsAndDuplicateEntries) {
    auto const fixture{integer_varint_type(false,
                                           0,
                                           100,
                                           codegen::IntegerVarintEncoding::unsigned_varint,
                                           (std::numeric_limits<std::uint64_t>::max)())};
    std::array const entries{
        IntegerVarintDistributionEntry{.value = 0, .weight = 2},
        IntegerVarintDistributionEntry{.value = 0, .weight = 3},
        IntegerVarintDistributionEntry{.value = (std::numeric_limits<std::uint64_t>::max)(),
                                       .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_EQ(analysis.total_weight, 6U);
    EXPECT_EQ(analysis.total_encoded_bytes, 15U);
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.5L);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, RejectsOutOfDomainAndZeroWeightOnlyInput) {
    auto const fixture{
        integer_varint_type(false, 0, 100, codegen::IntegerVarintEncoding::unsigned_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 50, .weight = 0},
                             IntegerVarintDistributionEntry{.value = 101, .weight = 4}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_EQ(analysis.valid_entry_count, 1U);
    EXPECT_EQ(analysis.total_weight, 0U);
    EXPECT_EQ(analysis.total_encoded_bytes, 0U);
    EXPECT_FALSE(analysis.expected_bytes_per_value.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 2U);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::warning);
}

TEST(IntegerVarintDistribution, KeepsNumericalExpectationWhenExactTotalsOverflow) {
    auto const fixture{integer_varint_type(true,
                                           (std::numeric_limits<std::int64_t>::min)(),
                                           (std::numeric_limits<std::int64_t>::max)(),
                                           codegen::IntegerVarintEncoding::zigzag_varint)};
    std::array const entries{IntegerVarintDistributionEntry{
                                 .value = 0, .weight = (std::numeric_limits<std::uint64_t>::max)()},
                             IntegerVarintDistributionEntry{.value = 1, .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_FALSE(analysis.total_weight.has_value());
    EXPECT_FALSE(analysis.total_encoded_bytes.has_value());
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 1.0L);
    ASSERT_EQ(analysis.diagnostics.size(), 2U);
}

TEST(IntegerVarintDistribution, RetainsEntryFactsWhenWeightedBytesOverflow) {
    auto const fixture{
        integer_varint_type(true, -64, 64, codegen::IntegerVarintEncoding::zigzag_varint)};
    std::array const entries{IntegerVarintDistributionEntry{
        .value = 64, .weight = (std::numeric_limits<std::uint64_t>::max)()}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    ASSERT_EQ(analysis.entries.size(), 1U);
    EXPECT_EQ(analysis.entries.front().encoded_bytes, 2U);
    EXPECT_FALSE(analysis.entries.front().weighted_encoded_bytes.has_value());
    EXPECT_EQ(analysis.total_weight, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(analysis.total_encoded_bytes.has_value());
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.0L);
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
}

TEST(IntegerVarintDistributionComparison, AppliesOneDistributionToBothEncodings) {
    auto const fixture{integer_varint_pair()};
    std::array const entries{IntegerVarintDistributionEntry{.value = -64, .weight = 2},
                             IntegerVarintDistributionEntry{.value = 64, .weight = 1}};

    auto const comparison{Analyzer::compare_integer_varint_distribution(
        fixture.types, fixture.first, fixture.second, entries, 3)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.total_weight, 3U);
    EXPECT_EQ(comparison.second.total_weight, 3U);
    EXPECT_EQ(comparison.first.total_encoded_bytes, 4U);
    EXPECT_EQ(comparison.second.total_encoded_bytes, 4U);
    ASSERT_TRUE(comparison.total_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.expected_bytes_per_value_delta.has_value());
    EXPECT_EQ(*comparison.expected_bytes_per_value_delta, 0.0L);
    ASSERT_TRUE(comparison.expected_selected_bytes_delta.has_value());
    EXPECT_EQ(*comparison.expected_selected_bytes_delta, 0.0L);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(IntegerVarintDistributionComparison, RejectsDifferentSemanticSources) {
    auto const fixture{integer_varint_pair(false)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1}};

    auto const comparison{Analyzer::compare_integer_varint_distribution(
        fixture.types, fixture.first, fixture.second, entries)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.total_encoded_byte_delta.has_value());
    EXPECT_FALSE(comparison.expected_bytes_per_value_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(PackedAnalyzer, ReportsEntityUniqueIdLayout) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.storage_facts->size_bytes, 4);
    EXPECT_EQ(analysis.storage_bits, 32);
    EXPECT_EQ(analysis.bits_used, 32);
    EXPECT_EQ(analysis.payload_bits, 32);
    EXPECT_EQ(analysis.reserved_bits, 0);
    EXPECT_EQ(analysis.unused_bits, 0);
    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_EQ(analysis.fields[0].least_significant_bit, 0);
    EXPECT_EQ(analysis.fields[0].most_significant_bit, 23);
    EXPECT_EQ(analysis.fields[0].maximum_unsigned_value, 16'777'215);
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 24);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 31);
    EXPECT_EQ(analysis.fields[1].maximum_unsigned_value, 255);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{entity_id_type(
        false, false, std::nullopt, codegen::PackedBitOrder::most_significant_first)};
    auto first_target{AbiProfile::host_common()};
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "synthetic wide target"});
    auto const first{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, first_target, 100)};
    auto const second{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, second_target, 100)};

    auto const comparison{Analyzer::compare_packed_targets(first, second)};

    ASSERT_EQ(comparison.fields.size(), 2U);
    EXPECT_EQ(comparison.first.storage_bits, 32U);
    EXPECT_EQ(comparison.second.storage_bits, 64U);
    ASSERT_TRUE(comparison.storage_size_delta.has_value());
    EXPECT_EQ(comparison.storage_size_delta->magnitude, 4U);
    ASSERT_TRUE(comparison.storage_alignment_delta.has_value());
    EXPECT_EQ(comparison.storage_alignment_delta->magnitude, 4U);
    ASSERT_TRUE(comparison.storage_bit_delta.has_value());
    EXPECT_EQ(comparison.storage_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.unused_bit_delta.has_value());
    EXPECT_EQ(comparison.unused_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 400U);
    ASSERT_TRUE(comparison.total_unused_bit_delta.has_value());
    EXPECT_EQ(comparison.total_unused_bit_delta->magnitude, 3'200U);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 6U);
    ASSERT_TRUE(comparison.fields[0].least_significant_bit_delta.has_value());
    EXPECT_EQ(comparison.fields[0].least_significant_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.fields[1].most_significant_bit_delta.has_value());
    EXPECT_EQ(comparison.fields[1].most_significant_bit_delta->magnitude, 32U);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_packed_targets(first, first)};
    ASSERT_TRUE(identical.storage_bit_delta.has_value());
    EXPECT_EQ(identical.storage_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.storage_bit_delta->magnitude, 0U);
}

TEST(PackedAnalyzer, PreservesUnknownAndOverflowAcrossTargetComparison) {
    auto const fixture{entity_id_type()};
    auto const known{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};
    auto const unknown{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile{"unknown"}, 10)};
    auto unknown_comparison{Analyzer::compare_packed_targets(known, unknown)};

    ASSERT_EQ(unknown_comparison.fields.size(), 2U);
    EXPECT_FALSE(unknown_comparison.storage_size_delta.has_value());
    EXPECT_FALSE(unknown_comparison.storage_bit_delta.has_value());
    EXPECT_FALSE(unknown_comparison.minimum_cache_line_delta.has_value());
    EXPECT_TRUE(
        std::ranges::any_of(unknown_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target packed layout:");
        }));

    auto narrow_target{AbiProfile::host_common()};
    narrow_target.set("std::uint32_t",
                      {.size_bytes = 2,
                       .alignment_bytes = 2,
                       .integer_signed = false,
                       .unsigned_value_bits = 16,
                       .provenance = "synthetic narrow target"});
    auto const overflow{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, narrow_target, 10)};
    auto const overflow_comparison{Analyzer::compare_packed_targets(known, overflow)};

    EXPECT_EQ(overflow_comparison.second.overflow_bits, 16U);
    ASSERT_TRUE(overflow_comparison.overflow_bit_delta.has_value());
    EXPECT_EQ(overflow_comparison.overflow_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(overflow_comparison.overflow_bit_delta->magnitude, 16U);
    EXPECT_TRUE(
        std::ranges::any_of(overflow_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target packed layout:") &&
                   diagnostic.severity == DiagnosticSeverity::error;
        }));
}

TEST(PackedAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialFields) {
    auto const fixture{entity_id_type()};
    auto const first{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};
    auto second{first};
    second.fields[1].name = "different";

    auto comparison{Analyzer::compare_packed_targets(first, second)};

    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same ordered segments"),
              std::string::npos);

    second = first;
    second.aggregate.element_count = 11;
    comparison = Analyzer::compare_packed_targets(first, second);
    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.storage_type = "std::uint64_t";
    comparison = Analyzer::compare_packed_targets(first, second);
    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same physical variant"),
              std::string::npos);
}

TEST(PackedAnalyzer, SeparatesReservedPayloadAndTrailingUnusedBits) {
    auto const fixture{entity_id_type(true)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 100)};

    EXPECT_EQ(analysis.storage_bits, 32);
    EXPECT_EQ(analysis.bits_used, 32);
    EXPECT_EQ(analysis.payload_bits, 28);
    EXPECT_EQ(analysis.reserved_bits, 4);
    EXPECT_EQ(analysis.unused_bits, 0);
    ASSERT_EQ(analysis.fields.size(), 3U);
    EXPECT_FALSE(analysis.fields[0].reserved);
    EXPECT_TRUE(analysis.fields[1].reserved);
    EXPECT_FALSE(analysis.fields[1].semantic_type.has_value());
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 20U);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 23U);
    EXPECT_FALSE(analysis.fields[1].maximum_unsigned_value.has_value());
    EXPECT_EQ(analysis.fields[2].least_significant_bit, 24U);
    EXPECT_EQ(analysis.aggregate.total_payload_bits, 2'800U);
    EXPECT_EQ(analysis.aggregate.total_reserved_bits, 400U);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 0U);
}

TEST(PackedAnalyzer, ReportsExplicitByteOrderAndMostSignificantFirstRanges) {
    auto const fixture{entity_id_type(true,
                                      false,
                                      codegen::PackedByteOrder::big_endian,
                                      codegen::PackedBitOrder::most_significant_first)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    EXPECT_EQ(analysis.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(analysis.bit_order, codegen::PackedBitOrder::most_significant_first);
    ASSERT_EQ(analysis.fields.size(), 3U);
    EXPECT_EQ(analysis.fields[0].least_significant_bit, 12U);
    EXPECT_EQ(analysis.fields[0].most_significant_bit, 31U);
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 8U);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 11U);
    EXPECT_EQ(analysis.fields[2].least_significant_bit, 0U);
    EXPECT_EQ(analysis.fields[2].most_significant_bit, 7U);

    auto compact_fixture{entity_id_type(false,
                                        false,
                                        codegen::PackedByteOrder::little_endian,
                                        codegen::PackedBitOrder::most_significant_first)};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = compact_fixture.type, .field_name = "index"}] =
        20;
    auto compact{Analyzer::analyze_packed(
        compact_fixture.types, compact_fixture.type, variant, AbiProfile::host_common())};
    EXPECT_EQ(compact.unused_bits, 4U);
    EXPECT_EQ(compact.fields[0].least_significant_bit, 12U);
    EXPECT_EQ(compact.fields[0].most_significant_bit, 31U);
    EXPECT_EQ(compact.fields[1].least_significant_bit, 4U);
    EXPECT_EQ(compact.fields[1].most_significant_bit, 11U);

    variant.overrides.packed_field_widths[{.type = compact_fixture.type, .field_name = "index"}] =
        25;
    auto overflow{Analyzer::analyze_packed(
        compact_fixture.types, compact_fixture.type, variant, AbiProfile::host_common())};
    EXPECT_EQ(overflow.overflow_bits, 1U);
    EXPECT_EQ(overflow.fields[0].least_significant_bit, 7U);
    EXPECT_EQ(overflow.fields[0].most_significant_bit, 31U);
    EXPECT_FALSE(overflow.fields[1].most_significant_bit.has_value());
}

TEST(PackedAnalyzer, ReportsPackedFieldSemanticRangeCodeSpace) {
    auto const fixture{entity_id_type(true, true)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    auto const& index{analysis.fields[0]};
    EXPECT_EQ(index.minimum_semantic_value, 0U);
    EXPECT_EQ(index.maximum_semantic_value, 1'000'000U);
    EXPECT_EQ(index.semantic_value_count, 1'000'001U);
    EXPECT_EQ(index.sentinel_code_count, 2U);
    EXPECT_EQ(index.required_code_count, 1'000'003U);
    EXPECT_EQ(index.minimum_required_bits, 20U);
    EXPECT_TRUE(index.schema_bit_width_auto);
    EXPECT_EQ(index.schema_bit_width, 20U);
    EXPECT_EQ(index.unused_codes, 48'573U);
    ASSERT_EQ(index.named_codes.size(), 3U);
    EXPECT_EQ(index.named_codes[1].name, "Invalid");
    EXPECT_TRUE(index.named_codes[1].sentinel);
    EXPECT_EQ(index.relationship_kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(index.relationship_target, "EntityType");

    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 19;
    auto const too_narrow{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};
    EXPECT_FALSE(too_narrow.fields[0].unused_codes.has_value());
    EXPECT_FALSE(too_narrow.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSharedIntegerScalarFieldDomain) {
    auto const fixture{packed_integer_scalar_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 1U);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.logical_type, "Health");
    EXPECT_EQ(field.schema_bit_width, 12U);
    EXPECT_TRUE(field.schema_bit_width_auto);
    EXPECT_EQ(field.minimum_semantic_value, 0U);
    EXPECT_EQ(field.maximum_semantic_value, 1000U);
    EXPECT_EQ(field.semantic_value_count, 1001U);
    EXPECT_EQ(field.sentinel_code_count, 1U);
    EXPECT_EQ(field.required_code_count, 1002U);
    EXPECT_EQ(field.minimum_required_bits, 12U);
    EXPECT_EQ(field.unused_codes, 3094U);
    ASSERT_EQ(field.named_codes.size(), 1U);
    EXPECT_EQ(field.named_codes.front().name, "Invalid");
    EXPECT_TRUE(field.named_codes.front().sentinel);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsPlacedLinearQuantizationFacts) {
    auto const fixture{packed_linear_quantized_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.kind, codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(field.schema_bit_width, 8U);
    EXPECT_TRUE(field.schema_bit_width_auto);
    ASSERT_TRUE(field.linear_quantized.has_value());
    EXPECT_EQ(field.linear_quantized->source_minimum, 0U);
    EXPECT_EQ(field.linear_quantized->source_maximum, 1000U);
    EXPECT_EQ(field.linear_quantized->encoded_storage_bits, 8U);
    EXPECT_EQ(field.linear_quantized->usable_code_count,
              (ExactCodeCount{.value = 254, .two_to_64 = false}));
    EXPECT_EQ(field.linear_quantized->reserved_code_count, 2U);
    EXPECT_NEAR(static_cast<double>(field.linear_quantized->resolution), 1000.0 / 253.0, 1e-12);
    EXPECT_EQ(field.linear_quantized->clipping, codegen::QuantizationClipping::clamp);
    EXPECT_TRUE(analysis.diagnostics.empty());

    Variant overridden;
    overridden.overrides.packed_field_widths[{.type = fixture.type, .field_name = "health"}] = 7;
    auto const ignored_override{Analyzer::analyze_packed(
        fixture.types, fixture.type, overridden, AbiProfile::host_common())};
    EXPECT_EQ(ignored_override.fields.front().bit_width, 8U);
    EXPECT_FALSE(ignored_override.fields.front().overridden);
    ASSERT_EQ(ignored_override.diagnostics.size(), 1U);
    EXPECT_EQ(ignored_override.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(PackedAnalyzer, DerivesIndexCapacityWidthWithSentinel) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::index_into, 12, 4'095)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    ASSERT_EQ(fits.fields.size(), 1);
    auto const& field{fits.fields.front()};
    EXPECT_EQ(field.relationship_target_extent, 4'000);
    EXPECT_EQ(field.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_minimum_required_bits, 12);
    EXPECT_EQ(field.relationship_width_sufficient, true);
    EXPECT_EQ(field.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_capacity_headroom, 95);
    EXPECT_EQ(field.relationship_semantic_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_sentinel_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_effective_capacity_headroom, 95);
    EXPECT_TRUE(fits.diagnostics.empty());

    auto overflow_capacity{capacity};
    overflow_capacity.front().element_capacity = 4'096;
    auto const insufficient{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, overflow_capacity)};
    EXPECT_EQ(insufficient.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 4'097, .two_to_64 = false}));
    EXPECT_EQ(insufficient.fields.front().relationship_minimum_required_bits, 13);
    EXPECT_EQ(insufficient.fields.front().relationship_width_sufficient, false);
    EXPECT_EQ(insufficient.fields.front().relationship_code_space_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.fields.front().relationship_capacity_headroom.has_value());
    EXPECT_FALSE(insufficient.diagnostics.empty());
}

TEST(PackedAnalyzer, DistinguishesCountCapacityAndExactTwoTo64) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::count_of, 12, 4'095)};
    std::array capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'094, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};
    EXPECT_EQ(fits.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 4'095, .two_to_64 = false}));
    EXPECT_EQ(fits.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 4'096, .two_to_64 = false}));
    EXPECT_EQ(fits.fields.front().relationship_minimum_required_bits, 12);
    EXPECT_EQ(fits.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(fits.fields.front().relationship_code_space_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_capacity_headroom, 0);
    EXPECT_EQ(fits.fields.front().relationship_semantic_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_sentinel_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_effective_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_effective_capacity_headroom, 0);
    EXPECT_TRUE(fits.diagnostics.empty());

    auto const full_fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::count_of, 64)};
    std::array const full_capacity{
        RelationshipTargetFacts{.target = full_fixture.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};
    auto const full{Analyzer::analyze_packed(full_fixture.types,
                                             full_fixture.packed,
                                             Variant{},
                                             AbiProfile::host_common(),
                                             1,
                                             full_capacity)};
    EXPECT_EQ(full.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.fields.front().relationship_minimum_required_bits, 64);
    EXPECT_EQ(full.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(full.fields.front().relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(full.fields.front().relationship_capacity_headroom, 0);
    EXPECT_FALSE(full.fields.front().relationship_semantic_capacity_limit.has_value());
    EXPECT_EQ(full.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(full.fields.front().relationship_effective_capacity_limit.has_value());
    EXPECT_FALSE(full.fields.front().relationship_effective_capacity_headroom.has_value());
    EXPECT_TRUE(full.diagnostics.empty());
}

TEST(PackedAnalyzer, KeepsMissingRelationshipTargetFactsUnknown) {
    auto const index{relationship_capacity_type(codegen::SemanticRelationKind::index_into, 1)};
    auto const missing{
        Analyzer::analyze_packed(index.types, index.packed, Variant{}, AbiProfile::host_common())};
    EXPECT_FALSE(missing.fields.front().relationship_target_extent.has_value());
    EXPECT_FALSE(missing.fields.front().relationship_minimum_required_bits.has_value());

    std::array const zero_capacity{RelationshipTargetFacts{
        .target = index.target, .element_capacity = 0, .byte_extent = std::nullopt}};
    auto const zero{Analyzer::analyze_packed(
        index.types, index.packed, Variant{}, AbiProfile::host_common(), 1, zero_capacity)};
    EXPECT_EQ(zero.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = false}));
    EXPECT_EQ(zero.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = false}));
    EXPECT_EQ(zero.fields.front().relationship_minimum_required_bits, 1);
    EXPECT_EQ(zero.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(zero.fields.front().relationship_code_space_capacity_limit, 2);
    EXPECT_EQ(zero.fields.front().relationship_capacity_headroom, 2);
    EXPECT_FALSE(zero.fields.front().relationship_semantic_capacity_limit.has_value());
    EXPECT_EQ(zero.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(zero.fields.front().relationship_effective_capacity_limit.has_value());

    auto const offset{relationship_capacity_type(codegen::SemanticRelationKind::offset_into, 12)};
    std::array const offset_facts{RelationshipTargetFacts{
        .target = offset.target, .element_capacity = std::nullopt, .byte_extent = 1'000}};
    auto const missing_element_extent{Analyzer::analyze_packed(
        offset.types, offset.packed, Variant{}, AbiProfile::host_common(), 1, offset_facts)};
    EXPECT_FALSE(missing_element_extent.fields.front().relationship_target_extent.has_value());
    EXPECT_FALSE(
        missing_element_extent.fields.front().relationship_minimum_required_bits.has_value());
}

TEST(PackedAnalyzer, DerivesByteOffsetExtentAndWidth) {
    auto const fixture{relationship_capacity_type(codegen::SemanticRelationKind::offset_into,
                                                  13,
                                                  std::nullopt,
                                                  std::nullopt,
                                                  codegen::SemanticRelationUnit::bytes)};
    auto const target{Analyzer::analyze_soa(fixture.types,
                                            fixture.target,
                                            Variant{},
                                            AbiProfile::host_common(),
                                            8'192,
                                            SoaAllocationStrategy::separate_columns)};
    ASSERT_EQ(target.total_allocation_bytes, 8'192);
    std::array const facts{RelationshipTargetFacts{.target = fixture.target,
                                                   .element_capacity = target.capacity,
                                                   .byte_extent = target.total_allocation_bytes}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, facts)};

    ASSERT_EQ(analysis.fields.size(), 1);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.relationship_unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(field.relationship_target_extent, 8'192);
    EXPECT_EQ(field.relationship_live_value_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_required_code_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_minimum_required_bits, 13);
    EXPECT_EQ(field.relationship_width_sufficient, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AccountsForMultipleSentinelsInCapacityHeadroom) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::index_into, 3, 6, 7)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4, .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    ASSERT_EQ(analysis.fields.size(), 1);
    EXPECT_EQ(analysis.fields.front().relationship_code_space_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_capacity_headroom, 2);
    EXPECT_EQ(analysis.fields.front().relationship_semantic_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_sentinel_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_headroom, 2);
    EXPECT_EQ(analysis.fields.front().relationship_width_sufficient, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, DiagnosesRelationshipRequirementBeyond64Bits) {
    auto const fixture{relationship_capacity_type(
        codegen::SemanticRelationKind::count_of, 64, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const capacity{
        RelationshipTargetFacts{.target = fixture.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    EXPECT_EQ(analysis.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_FALSE(analysis.fields.front().relationship_required_code_count.has_value());
    EXPECT_EQ(analysis.fields.front().relationship_minimum_required_bits, 65);
    EXPECT_EQ(analysis.fields.front().relationship_width_sufficient, false);
    EXPECT_EQ(analysis.fields.front().relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(analysis.fields.front().relationship_capacity_headroom.has_value());
    EXPECT_EQ(analysis.fields.front().relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(analysis.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(analysis.fields.front().relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsUnusedAndExcessBits) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;
    auto analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.bits_used, 28);
    EXPECT_EQ(analysis.unused_bits, 4);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 400);

    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 25;
    analysis =
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common());
    EXPECT_FALSE(analysis.unused_bits.has_value());
    EXPECT_EQ(analysis.overflow_bits, 1);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSchemaAndOverrideProvenance) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.schema_storage_type, "std::uint32_t");
    EXPECT_TRUE(analysis.storage_overridden);
    EXPECT_EQ(analysis.fields[0].logical_type, "std::uint32_t");
    EXPECT_EQ(analysis.fields[0].schema_bit_width, 24);
    EXPECT_EQ(analysis.fields[0].bit_width, 20);
    EXPECT_TRUE(analysis.fields[0].overridden);
    EXPECT_FALSE(analysis.fields[1].overridden);
}

TEST(PackedAnalyzer, DiagnosesZeroWidthFields) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 0;
    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_FALSE(analysis.fields[0].most_significant_bit.has_value());
    EXPECT_EQ(analysis.bits_used, 8);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AppliesExplicitVariantOverrides) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 40;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.storage_type, "std::uint64_t");
    EXPECT_EQ(analysis.storage_bits, 64);
    EXPECT_EQ(analysis.bits_used, 48);
    EXPECT_EQ(analysis.unused_bits, 16);
}

TEST(PackedAnalyzer, ReportsOverflowSafeAggregateMemoryAtSelectedScale) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 1'000)};

    EXPECT_EQ(analysis.aggregate.element_count, 1'000);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 4'000);
    EXPECT_EQ(analysis.aggregate.total_payload_bits, 32'000);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 0);
    EXPECT_EQ(analysis.aggregate.cache_line_bytes, 64);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 63);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 16);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.page_bytes, 4'096);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 1'024);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
}

TEST(PackedAnalyzer, ReportsAlignedContiguousBoundaryStraddling) {
    auto const fixture{entity_id_type()};
    auto abi{AbiProfile::host_common()};
    abi.set("uint24",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = false,
             .unsigned_value_bits = 24,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = false,
             .unsigned_value_bits = 128,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});

    Variant three_byte_variant;
    three_byte_variant.overrides.packed_storage_types[fixture.type] = "uint24";
    three_byte_variant.overrides
        .packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 16;
    auto analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, three_byte_variant, abi, 10)};
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 30);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 2);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 2);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 3);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 2);

    analysis = Analyzer::analyze_packed(fixture.types, fixture.type, three_byte_variant, abi, 1);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);

    Variant wide_variant;
    wide_variant.overrides.packed_storage_types[fixture.type] = "wide";
    analysis = Analyzer::analyze_packed(fixture.types, fixture.type, wide_variant, abi, 3);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 0);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 3);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 0);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 3);
}

TEST(PackedAnalyzer, KeepsUnknownTargetMemoryFactsUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = {}});

    auto const analysis{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};

    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 40);
    EXPECT_FALSE(analysis.aggregate.cache_line_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_line_straddling_elements.has_value());
    EXPECT_FALSE(analysis.aggregate.page_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.page_straddling_elements.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSignedArbitraryWidthRange) {
    auto const fixture{signed_delta_type()};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    EXPECT_EQ(analysis.fields[0].kind, codegen::PackedFieldKind::signed_integer);
    EXPECT_EQ(analysis.fields[0].bit_width, 17U);
    EXPECT_EQ(analysis.fields[0].minimum_signed_value, -65'536);
    EXPECT_EQ(analysis.fields[0].maximum_signed_value, 65'535);
    EXPECT_FALSE(analysis.fields[1].minimum_signed_value.has_value());
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSignedSemanticRangeSentinelAndDerivedWidth) {
    auto const fixture{signed_delta_type(true)};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    auto const& delta{analysis.fields[0]};
    EXPECT_TRUE(delta.schema_bit_width_auto);
    EXPECT_EQ(delta.bit_width, 8U);
    EXPECT_EQ(delta.minimum_semantic_value, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(delta.maximum_semantic_value, codegen::PackedIntegerValue{100});
    EXPECT_EQ(delta.semantic_value_count, 201U);
    EXPECT_EQ(delta.sentinel_code_count, 1U);
    EXPECT_EQ(delta.required_code_count, 202U);
    EXPECT_EQ(delta.minimum_required_bits, 8U);
    EXPECT_EQ(delta.unused_codes, 54U);
    ASSERT_EQ(delta.named_codes.size(), 1U);
    EXPECT_EQ(delta.named_codes[0].value, codegen::PackedIntegerValue{-128});
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsKnownZeroWasteForFullSigned64BitDomain) {
    auto const fixture{signed_delta_type(false, true)};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 1U);
    auto const& delta{analysis.fields[0]};
    EXPECT_TRUE(delta.schema_bit_width_auto);
    EXPECT_EQ(delta.bit_width, 64U);
    EXPECT_FALSE(delta.semantic_value_count.has_value());
    EXPECT_FALSE(delta.required_code_count.has_value());
    EXPECT_EQ(delta.minimum_required_bits, 64U);
    EXPECT_EQ(delta.unused_codes, 0U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, DiagnosesAggregateOverflow) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(fixture.types,
                                                 fixture.type,
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 std::numeric_limits<std::uint64_t>::max())};

    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_payload_bits.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_line_straddling_elements.has_value());
    EXPECT_FALSE(analysis.aggregate.page_straddling_elements.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, SeparatesSelectedBitsFromWholeStorageAndRegionFootprints) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::array const accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read},
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi, 3)};

    EXPECT_EQ(analysis.field_names, (std::vector<std::string>{"index", "entity_type"}));
    EXPECT_EQ(analysis.element_count, 100);
    EXPECT_EQ(analysis.multiplicity, 3);
    EXPECT_EQ(analysis.useful_bits, 3'200);
    EXPECT_EQ(analysis.read_useful_bits, 2'400);
    EXPECT_EQ(analysis.write_useful_bits, 800);
    EXPECT_EQ(analysis.logical_read_useful_bits, 7'200);
    EXPECT_EQ(analysis.logical_write_useful_bits, 2'400);
    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_EQ(analysis.fields[0].name, "index");
    EXPECT_EQ(analysis.fields[0].operation, AccessOperation::read);
    EXPECT_EQ(analysis.fields[0].bit_width, 24);
    EXPECT_EQ(analysis.fields[0].useful_bits, 2'400);
    EXPECT_EQ(analysis.fields[0].read_useful_bits, 2'400);
    EXPECT_EQ(analysis.fields[0].write_useful_bits, 0);
    EXPECT_EQ(analysis.fields[0].logical_read_useful_bits, 7'200);
    EXPECT_EQ(analysis.fields[0].logical_write_useful_bits, 0);
    EXPECT_EQ(analysis.fields[1].name, "entity_type");
    EXPECT_EQ(analysis.fields[1].operation, AccessOperation::write);
    EXPECT_EQ(analysis.fields[1].bit_width, 8);
    EXPECT_EQ(analysis.fields[1].read_useful_bits, 0);
    EXPECT_EQ(analysis.fields[1].write_useful_bits, 800);
    EXPECT_EQ(analysis.fields[1].logical_write_useful_bits, 2'400);
    EXPECT_EQ(analysis.storage_footprint_bytes, 400);
    EXPECT_EQ(analysis.storage_footprint_bits, 3'200);
    EXPECT_EQ(analysis.non_useful_storage_bits, 0);
    EXPECT_EQ(analysis.unselected_field_bits, 0);
    EXPECT_EQ(analysis.reserved_region_bits, 0);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 0);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 7);
    EXPECT_EQ(analysis.minimum_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.read_cache_lines_touched, 7);
    EXPECT_EQ(analysis.read_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.write_cache_lines_touched, 7);
    EXPECT_EQ(analysis.write_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.minimum_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_page_bytes_touched, 4'096);
    EXPECT_EQ(analysis.read_pages_touched, 1);
    EXPECT_EQ(analysis.write_pages_touched, 1);
    EXPECT_EQ(analysis.cache_footprint_capacity.working_set_bytes, 448);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, ReportsUnselectedAndUnusedStorageBitsAsNonUseful) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, variant, abi, 10)};
    std::array const accesses{
        AccessIntent{.name = "entity_type", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.useful_bits, 80);
    EXPECT_EQ(analysis.read_useful_bits, 80);
    EXPECT_EQ(analysis.write_useful_bits, 80);
    EXPECT_EQ(analysis.storage_footprint_bits, 320);
    EXPECT_EQ(analysis.non_useful_storage_bits, 240);
    EXPECT_EQ(analysis.unselected_field_bits, 200);
    EXPECT_EQ(analysis.reserved_region_bits, 0);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 40);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 1);
    EXPECT_EQ(analysis.read_cache_lines_touched, 1);
    EXPECT_EQ(analysis.write_cache_lines_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, RejectsReservedMissingAndConflictingSelections) {
    auto const fixture{entity_id_type(true)};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 2)};
    std::array const accesses{AccessIntent{.name = "future", .operation = AccessOperation::read},
                              AccessIntent{.name = "missing", .operation = AccessOperation::read},
                              AccessIntent{.name = "index", .operation = AccessOperation::read},
                              AccessIntent{.name = "index", .operation = AccessOperation::write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.field_names, (std::vector<std::string>{"index"}));
    EXPECT_EQ(analysis.useful_bits, 40);
    EXPECT_EQ(analysis.non_useful_storage_bits, 24);
    EXPECT_EQ(analysis.unselected_field_bits, 16);
    EXPECT_EQ(analysis.reserved_region_bits, 8);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 0);
    ASSERT_EQ(analysis.diagnostics.size(), 3);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[2].severity, DiagnosticSeverity::error);
}

TEST(PackedAccessAnalyzer, KeepsUnknownMemoryFactsUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.useful_bits, 240);
    EXPECT_EQ(analysis.storage_footprint_bytes, 40);
    EXPECT_EQ(analysis.non_useful_storage_bits, 80);
    EXPECT_FALSE(analysis.cache_line_bytes.has_value());
    EXPECT_FALSE(analysis.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(analysis.minimum_cache_bytes_touched.has_value());
    EXPECT_FALSE(analysis.page_bytes.has_value());
    EXPECT_FALSE(analysis.minimum_pages_touched.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 2);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::warning);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::warning);
}

TEST(PackedAccessAnalyzer, DiagnosesZeroMultiplicityAndCheckedOverflow) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, abi, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi, 0)};

    EXPECT_FALSE(analysis.useful_bits.has_value());
    EXPECT_FALSE(analysis.storage_footprint_bytes.has_value());
    EXPECT_FALSE(analysis.logical_read_useful_bits.has_value());
    EXPECT_FALSE(analysis.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(analysis.minimum_pages_touched.has_value());
    ASSERT_EQ(analysis.fields.size(), 1);
    EXPECT_FALSE(analysis.fields[0].useful_bits.has_value());
    EXPECT_FALSE(analysis.fields[0].logical_read_useful_bits.has_value());
    EXPECT_FALSE(analysis.unselected_field_bits.has_value());
    EXPECT_FALSE(analysis.reserved_region_bits.has_value());
    EXPECT_FALSE(analysis.physically_unused_storage_bits.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAccessComparison, ReportsWidthAndStorageConsequencesForOneWorkload) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const baseline_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 100)};
    Variant narrow_variant;
    narrow_variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] =
        20;
    auto const narrow_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, narrow_variant, abi, 100)};
    auto const baseline_access{Analyzer::analyze_packed_access(baseline_packed, accesses, abi, 2)};
    auto const narrow_access{Analyzer::analyze_packed_access(narrow_packed, accesses, abi, 2)};

    auto const width_comparison{Analyzer::compare_packed_access(baseline_access, narrow_access)};

    EXPECT_TRUE(width_comparison.diagnostics.empty());
    EXPECT_EQ(width_comparison.field_names, (std::vector<std::string>{"index"}));
    EXPECT_EQ(width_comparison.element_count, 100);
    EXPECT_EQ(width_comparison.multiplicity, 2);
    ASSERT_TRUE(width_comparison.useful_bit_delta.has_value());
    EXPECT_EQ(width_comparison.useful_bit_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(width_comparison.useful_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.logical_read_useful_bit_delta->magnitude, 800);
    ASSERT_EQ(width_comparison.fields.size(), 1);
    EXPECT_EQ(width_comparison.fields[0].name, "index");
    EXPECT_EQ(width_comparison.fields[0].first.bit_width, 24);
    EXPECT_EQ(width_comparison.fields[0].second.bit_width, 20);
    EXPECT_EQ(width_comparison.fields[0].bit_width_delta->direction,
              NumericDeltaDirection::decreased);
    EXPECT_EQ(width_comparison.fields[0].bit_width_delta->magnitude, 4);
    EXPECT_EQ(width_comparison.fields[0].useful_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.fields[0].logical_read_useful_bit_delta->magnitude, 800);
    EXPECT_EQ(width_comparison.storage_footprint_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.non_useful_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(width_comparison.non_useful_storage_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.unselected_field_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.reserved_region_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.physically_unused_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(width_comparison.physically_unused_storage_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.cache_line_delta->direction, NumericDeltaDirection::unchanged);

    Variant wide_storage_variant;
    wide_storage_variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    auto const wide_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, wide_storage_variant, abi, 100)};
    auto const wide_access{Analyzer::analyze_packed_access(wide_packed, accesses, abi, 2)};
    auto const storage_comparison{Analyzer::compare_packed_access(baseline_access, wide_access)};

    EXPECT_EQ(storage_comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(storage_comparison.storage_footprint_byte_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.storage_footprint_byte_delta->magnitude, 400);
    EXPECT_EQ(storage_comparison.unselected_field_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(storage_comparison.physically_unused_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.physically_unused_storage_bit_delta->magnitude, 3'200);
    EXPECT_EQ(storage_comparison.cache_line_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.cache_line_delta->magnitude, 6);
    EXPECT_EQ(storage_comparison.page_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, ReportsTargetStorageAndRegionConsequencesForOneWorkload) {
    auto const fixture{entity_id_type()};
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 500,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "Test wide target"});
    second_target.set_memory_facts({.cache_line_bytes = 128,
                                    .page_bytes = 8'192,
                                    .l1_data_cache_bytes = 500,
                                    .l2_cache_bytes = std::nullopt,
                                    .l3_cache_bytes = std::nullopt,
                                    .provenance = "Test second memory"});
    std::array const accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read_write}};
    auto const first_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, first_target, 100)};
    auto const second_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, second_target, 100)};
    auto const first{Analyzer::analyze_packed_access(first_packed, accesses, first_target, 2)};
    auto const second{Analyzer::analyze_packed_access(second_packed, accesses, second_target, 2)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};
    auto const identical{Analyzer::compare_packed_access(first, first)};

    EXPECT_EQ(comparison.first.type, fixture.type);
    EXPECT_EQ(comparison.first.useful_bits, 2'400);
    EXPECT_EQ(comparison.second.useful_bits, 2'400);
    ASSERT_TRUE(comparison.useful_bit_delta.has_value());
    EXPECT_EQ(comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.logical_read_useful_bit_delta.has_value());
    EXPECT_EQ(comparison.logical_read_useful_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.storage_footprint_byte_delta.has_value());
    EXPECT_EQ(comparison.storage_footprint_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.non_useful_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.non_useful_storage_bit_delta->magnitude, 3'200);
    ASSERT_TRUE(comparison.unselected_field_bit_delta.has_value());
    EXPECT_EQ(comparison.unselected_field_bit_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.physically_unused_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.physically_unused_storage_bit_delta->magnitude, 3'200);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.cache_byte_delta.has_value());
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 448);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    ASSERT_TRUE(comparison.page_byte_delta.has_value());
    EXPECT_EQ(comparison.page_byte_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.first.cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.cache_footprint_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());

    ASSERT_TRUE(identical.storage_footprint_byte_delta.has_value());
    EXPECT_EQ(identical.storage_footprint_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.cache_byte_delta.has_value());
    EXPECT_EQ(identical.cache_byte_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, MatchesFieldDetailsIndependentOfIntentOrder) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const first_accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read},
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write}};
    std::array const second_accesses{
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write},
        AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const first{Analyzer::analyze_packed_access(packed, first_accesses, abi, 3)};
    auto const second{Analyzer::analyze_packed_access(packed, second_accesses, abi, 3)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_TRUE(comparison.diagnostics.empty());
    ASSERT_EQ(comparison.fields.size(), 2);
    EXPECT_EQ(comparison.fields[0].name, "index");
    EXPECT_EQ(comparison.fields[1].name, "entity_type");
    EXPECT_EQ(comparison.fields[0].useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.fields[1].logical_write_useful_bit_delta->direction,
              NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, DiagnosesMissingFieldDetails) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto second{first};
    second.fields.clear();

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.fields.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
}

TEST(PackedAccessComparison, RejectsMismatchedWorkloads) {
    auto first{PackedAccessAnalysis{}};
    first.field_names = {"index"};
    first.accesses = {{.name = "index", .operation = AccessOperation::read}};
    first.element_count = 10;
    first.multiplicity = 1;
    first.useful_bits = 100;
    auto second{first};
    second.accesses.front().operation = AccessOperation::write;

    auto comparison{Analyzer::compare_packed_access(first, second)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());

    second.accesses = first.accesses;
    second.element_count = 11;
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.storage_footprint_byte_delta.has_value());

    second.element_count = first.element_count;
    second.multiplicity = 2;
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_bit_delta.has_value());

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different packed values"),
              std::string::npos);
}

TEST(PackedAccessComparison, KeepsUnknownPhysicalDeltasUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto const second{Analyzer::analyze_packed_access(packed, accesses, abi)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_EQ(comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.storage_footprint_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.cache_line_delta.has_value());
    EXPECT_FALSE(comparison.cache_byte_delta.has_value());
    EXPECT_FALSE(comparison.page_delta.has_value());
    EXPECT_FALSE(comparison.page_byte_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(PackedAccessComparison, KeepsOverflowedDeltasUnknown) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const packed{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, abi, (std::numeric_limits<std::uint64_t>::max)())};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto const second{Analyzer::analyze_packed_access(packed, accesses, abi)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
    EXPECT_FALSE(comparison.storage_footprint_byte_delta.has_value());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());
    EXPECT_FALSE(comparison.page_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsSixFloatPayloadAcrossCapacities) {
    auto const fixture{soa_type()};
    auto const abi{AbiProfile::host_common()};
    for (auto const capacity : {std::uint64_t{0},
                                std::uint64_t{1},
                                std::uint64_t{4'096},
                                std::uint64_t{16'384},
                                std::uint64_t{65'536}}) {
        auto const analysis{
            Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, capacity)};
        EXPECT_EQ(analysis.bytes_per_logical_element, 24);
        EXPECT_EQ(analysis.total_payload_bytes, 24 * capacity);
        ASSERT_EQ(analysis.columns.size(), 6);
        EXPECT_EQ(analysis.columns[0].total_bytes, 4 * capacity);
        EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 16);
        EXPECT_EQ(analysis.columns[0].minimum_cache_lines,
                  (4 * capacity) / 64 + ((4 * capacity) % 64 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.columns[0].complete_elements_per_page, 1'024);
        EXPECT_EQ(analysis.columns[0].minimum_pages,
                  (4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.minimum_pages,
                  6 * ((4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1)));
    }
}

TEST(SoaAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});

    auto const first{Analyzer::analyze_soa(fixture.types,
                                           fixture.type,
                                           Variant{},
                                           first_profile,
                                           3,
                                           SoaAllocationStrategy::aligned_contiguous)};
    auto const second{Analyzer::analyze_soa(fixture.types,
                                            fixture.type,
                                            Variant{},
                                            second_profile,
                                            3,
                                            SoaAllocationStrategy::aligned_contiguous)};
    auto const comparison{Analyzer::compare_soa_targets(first, second)};

    EXPECT_TRUE(comparison.compatible);
    ASSERT_EQ(comparison.columns.size(), 2);
    EXPECT_EQ(comparison.columns[0].element_size_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[0].allocation_offset_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[1].name, "wide");
    EXPECT_EQ(comparison.columns[1].element_size_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].element_alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].total_byte_delta->magnitude, 12);
    EXPECT_EQ(comparison.columns[1].first.allocation_offset_bytes, 4);
    EXPECT_EQ(comparison.columns[1].second.allocation_offset_bytes, 8);
    EXPECT_EQ(comparison.columns[1].allocation_offset_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].first.padding_before_bytes, 1);
    EXPECT_EQ(comparison.columns[1].second.padding_before_bytes, 5);
    EXPECT_EQ(comparison.columns[1].padding_before_delta->magnitude, 4);
    EXPECT_EQ(comparison.bytes_per_logical_element_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_payload_delta->magnitude, 12);
    EXPECT_EQ(comparison.total_allocation_delta->magnitude, 16);
    EXPECT_EQ(comparison.total_alignment_padding_delta->magnitude, 4);
    EXPECT_EQ(comparison.allocation_alignment_delta->magnitude, 4);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_soa_targets(first, first)};
    EXPECT_TRUE(identical.compatible);
    ASSERT_EQ(identical.columns.size(), 2);
    EXPECT_EQ(identical.columns[1].element_size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_allocation_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(SoaAnalyzer, PreservesUnknownTargetFactsAndSideDiagnostics) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const known{Analyzer::analyze_soa(fixture.types,
                                           fixture.type,
                                           Variant{},
                                           AbiProfile::host_common(),
                                           3,
                                           SoaAllocationStrategy::aligned_contiguous)};
    auto const unknown{Analyzer::analyze_soa(fixture.types,
                                             fixture.type,
                                             Variant{},
                                             AbiProfile{"unknown target"},
                                             3,
                                             SoaAllocationStrategy::aligned_contiguous)};

    auto const comparison{Analyzer::compare_soa_targets(known, unknown)};

    EXPECT_TRUE(comparison.compatible);
    ASSERT_EQ(comparison.columns.size(), 2);
    EXPECT_FALSE(comparison.columns[1].element_size_delta.has_value());
    EXPECT_FALSE(comparison.columns[1].allocation_offset_delta.has_value());
    EXPECT_FALSE(comparison.total_allocation_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target SoA layout:");
    }));
}

TEST(SoaAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialColumns) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const analysis{Analyzer::analyze_soa(fixture.types,
                                              fixture.type,
                                              Variant{},
                                              AbiProfile::host_common(),
                                              3,
                                              SoaAllocationStrategy::aligned_contiguous)};
    auto second{analysis};
    second.columns.back().physical_type = "std::uint64_t";

    auto comparison{Analyzer::compare_soa_targets(analysis, second)};
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique ordered columns"),
              std::string::npos);

    second = analysis;
    second.capacity = 4;
    comparison = Analyzer::compare_soa_targets(analysis, second);
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different capacities"),
              std::string::npos);

    second = analysis;
    second.allocation_strategy = SoaAllocationStrategy::separate_columns;
    comparison = Analyzer::compare_soa_targets(analysis, second);
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different allocation strategies"),
              std::string::npos);
}

TEST(SoaAnalyzer, AppliesCapacityAndColumnTypeOverrides) {
    auto const fixture{soa_type()};
    Variant variant;
    variant.overrides.capacities[fixture.type] = 4'096;
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "min_xs"}] = "double";

    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 65'536)};

    EXPECT_EQ(analysis.capacity, 4'096);
    EXPECT_TRUE(analysis.capacity_overridden);
    EXPECT_EQ(analysis.bytes_per_logical_element, 28);
    EXPECT_EQ(analysis.total_payload_bytes, 28 * 4'096);
    EXPECT_EQ(analysis.columns[0].type_facts->size_bytes, 8);
    EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 8);
}

TEST(SoaAnalyzer, ModelsSeparateAndAlignedContiguousAllocations) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}, {"medium", "std::uint16_t"}})};
    auto const abi{AbiProfile::host_common()};

    auto const separate{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::separate_columns)};
    EXPECT_EQ(separate.allocation_count, 3);
    EXPECT_EQ(separate.total_payload_bytes, 21);
    EXPECT_EQ(separate.total_allocation_bytes, 21);
    EXPECT_EQ(separate.total_alignment_padding_bytes, 0);
    EXPECT_FALSE(separate.allocation_alignment_bytes.has_value());
    EXPECT_TRUE(std::ranges::all_of(separate.columns, [](SoaColumnAnalysis const& column) {
        return !column.allocation_offset_bytes.has_value() &&
               !column.padding_before_bytes.has_value();
    }));

    auto const contiguous{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_EQ(contiguous.allocation_count, 1);
    EXPECT_EQ(contiguous.total_payload_bytes, 21);
    EXPECT_EQ(contiguous.total_allocation_bytes, 22);
    EXPECT_EQ(contiguous.total_alignment_padding_bytes, 1);
    EXPECT_EQ(contiguous.allocation_alignment_bytes, 4);
    EXPECT_EQ(contiguous.cache_line_bytes, 64);
    EXPECT_EQ(contiguous.page_bytes, 4'096);
    ASSERT_EQ(contiguous.columns.size(), 3);
    EXPECT_EQ(contiguous.columns[0].allocation_offset_bytes, 0);
    EXPECT_EQ(contiguous.columns[0].padding_before_bytes, 0);
    EXPECT_EQ(contiguous.columns[1].allocation_offset_bytes, 4);
    EXPECT_EQ(contiguous.columns[1].padding_before_bytes, 1);
    EXPECT_EQ(contiguous.columns[2].allocation_offset_bytes, 16);
    EXPECT_EQ(contiguous.columns[2].padding_before_bytes, 0);

    auto const empty_capacity{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 0, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_EQ(empty_capacity.allocation_count, 0);
    EXPECT_EQ(empty_capacity.total_allocation_bytes, 0);
    EXPECT_EQ(empty_capacity.total_alignment_padding_bytes, 0);
}

TEST(SoaAnalyzer, DerivesRelationshipTargetFactsPerVariantAndAllocationStrategy) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint64_t"}})};
    auto variant{Variant{}};
    variant.overrides.capacities[fixture.type] = 3;

    auto const separate{
        Analyzer::derive_relationship_target_facts(fixture.types,
                                                   variant,
                                                   AbiProfile::host_common(),
                                                   100,
                                                   SoaAllocationStrategy::separate_columns)};
    auto const contiguous{
        Analyzer::derive_relationship_target_facts(fixture.types,
                                                   variant,
                                                   AbiProfile::host_common(),
                                                   100,
                                                   SoaAllocationStrategy::aligned_contiguous)};

    ASSERT_EQ(separate.size(), 1);
    ASSERT_EQ(contiguous.size(), 1);
    EXPECT_EQ(separate.front().target, fixture.type);
    EXPECT_EQ(separate.front().element_capacity, 3);
    EXPECT_EQ(separate.front().byte_extent, 27);
    EXPECT_EQ(contiguous.front().target, fixture.type);
    EXPECT_EQ(contiguous.front().element_capacity, 3);
    EXPECT_EQ(contiguous.front().byte_extent, 32);
}

TEST(SoaAnalyzer, KeepsUnknownRelationshipTargetByteExtentUnknown) {
    auto const fixture{soa_type({{"values", "std::uint8_t"}})};
    auto const facts{Analyzer::derive_relationship_target_facts(
        fixture.types, Variant{}, AbiProfile{"unknown physical facts"}, 64)};

    ASSERT_EQ(facts.size(), 1);
    EXPECT_EQ(facts.front().target, fixture.type);
    EXPECT_EQ(facts.front().element_capacity, 64);
    EXPECT_FALSE(facts.front().byte_extent.has_value());
}

TEST(SoaAnalyzer, KeepsIncompleteContiguousAllocationUnknown) {
    auto const fixture{soa_type({{"unknown", "UnknownUserType"}})};
    auto const abi{AbiProfile::host_common()};
    auto const unknown{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_FALSE(unknown.total_allocation_bytes.has_value());
    EXPECT_FALSE(unknown.total_alignment_padding_bytes.has_value());
    EXPECT_FALSE(unknown.allocation_alignment_bytes.has_value());
    EXPECT_FALSE(unknown.diagnostics.empty());

    auto const overflow{soa_type({{"wide", "std::uint64_t"}})};
    auto const overflowed{Analyzer::analyze_soa(overflow.types,
                                                overflow.type,
                                                Variant{},
                                                abi,
                                                std::numeric_limits<std::uint64_t>::max(),
                                                SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_FALSE(overflowed.total_allocation_bytes.has_value());
    EXPECT_FALSE(overflowed.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsCacheLineTilingForNonDivisibleAndOversizedElements) {
    auto const fixture{soa_type({{"three", "three_bytes"}, {"wide_values", "wide"}})};
    AbiProfile abi{"test"};
    abi.set("three_bytes",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});
    abi.set("wide",
            {.size_bytes = 80,
             .alignment_bytes = 16,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test profile"});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1)};

    ASSERT_TRUE(analysis.columns[0].cache_line_tiling.has_value());
    auto const& three_bytes{*analysis.columns[0].cache_line_tiling};
    EXPECT_FALSE(three_bytes.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(three_bytes.complete_elements_from_line_start, 21);
    EXPECT_EQ(three_bytes.boundary_fragment_bytes, 1);
    EXPECT_EQ(three_bytes.minimum_cache_lines_per_element, 1);

    ASSERT_TRUE(analysis.columns[1].cache_line_tiling.has_value());
    auto const& wide{*analysis.columns[1].cache_line_tiling};
    EXPECT_FALSE(wide.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(wide.complete_elements_from_line_start, 0);
    EXPECT_EQ(wide.boundary_fragment_bytes, 0);
    EXPECT_EQ(wide.minimum_cache_lines_per_element, 2);
}

TEST(Analyzer, ReportsFactualNumericDeltas) {
    auto const increased{numeric_delta(100, 150)};
    ASSERT_TRUE(increased.has_value());
    EXPECT_EQ(increased->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(increased->magnitude, 50);
    EXPECT_DOUBLE_EQ(*increased->percentage, 50.0);

    auto const decreased{numeric_delta(150, 100)};
    ASSERT_TRUE(decreased.has_value());
    EXPECT_EQ(decreased->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(decreased->magnitude, 50);

    auto const zero_baseline{numeric_delta(0, 1)};
    ASSERT_TRUE(zero_baseline.has_value());
    EXPECT_FALSE(zero_baseline->percentage.has_value());
    EXPECT_FALSE(numeric_delta(std::nullopt, 1).has_value());
}

TEST(AbiProfile, ResolvesSchemaRepresentationsWithoutGuessingCycles) {
    auto abi{AbiProfile::host_common()};
    abi.set_representation("EntityUniqueId", "std::uint32_t");
    abi.set_representation("CycleA", "CycleB");
    abi.set_representation("CycleB", "CycleA");

    EXPECT_EQ(abi.find("EntityUniqueId")->size_bytes, 4);
    EXPECT_FALSE(abi.find("CycleA").has_value());
    EXPECT_FALSE(abi.find("Unknown").has_value());
}

TEST(AbiProfile, ExposesExplicitX86MemoryFactsWithProvenance) {
    auto const abi{AbiProfile::host_common()};

    EXPECT_FALSE(abi.name().empty());
    EXPECT_TRUE(abi.identity().platform.has_value());
    EXPECT_TRUE(abi.identity().architecture.has_value());
    EXPECT_FALSE(abi.identity().abi.has_value());
    EXPECT_TRUE(abi.identity().compiler.has_value());
    EXPECT_TRUE(abi.identity().build_configuration.has_value());
    EXPECT_EQ(abi.memory_facts().cache_line_bytes, 64);
    EXPECT_EQ(abi.memory_facts().page_bytes, 4'096);
    EXPECT_FALSE(abi.memory_facts().provenance.empty());
    EXPECT_FALSE(abi.find("std::uint8_t")->provenance.empty());
    EXPECT_EQ(abi.find("std::uint8_t")->integer_signed, false);
    EXPECT_EQ(abi.find("std::int8_t")->integer_signed, true);
    EXPECT_FALSE(abi.find("float")->integer_signed.has_value());
}

TEST(AbiProfile, RetainsFullyDescribedIdentitySeparatelyFromFactProvenance) {
    AbiProfile abi{"Windows shipping",
                   {.platform = "Windows",
                    .architecture = "x86-64",
                    .abi = "Microsoft x64",
                    .compiler = "MSVC 19.44",
                    .build_configuration = "Shipping"}};
    abi.set("word",
            {.size_bytes = 8,
             .alignment_bytes = 8,
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = "generated probe output"});
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "selected machine profile"});

    EXPECT_EQ(abi.identity().platform, "Windows");
    EXPECT_EQ(abi.identity().architecture, "x86-64");
    EXPECT_EQ(abi.identity().abi, "Microsoft x64");
    EXPECT_EQ(abi.identity().compiler, "MSVC 19.44");
    EXPECT_EQ(abi.identity().build_configuration, "Shipping");
    EXPECT_EQ(abi.find("word")->provenance, "generated probe output");
    EXPECT_EQ(abi.memory_facts().provenance, "selected machine profile");
}

TEST(AbiProfile, LeavesUnspecifiedIdentityAndFactsUnknown) {
    AbiProfileIdentity identity;
    identity.architecture = "x86-64";
    AbiProfile abi{"partial", std::move(identity)};
    abi.set("word",
            {.size_bytes = 8,
             .alignment_bytes = 8,
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = {}});

    EXPECT_FALSE(abi.identity().platform.has_value());
    EXPECT_EQ(abi.identity().architecture, "x86-64");
    EXPECT_FALSE(abi.identity().abi.has_value());
    EXPECT_FALSE(abi.identity().compiler.has_value());
    EXPECT_FALSE(abi.identity().build_configuration.has_value());
    EXPECT_TRUE(abi.find("word")->provenance.empty());
    EXPECT_FALSE(abi.memory_facts().cache_line_bytes.has_value());
    EXPECT_FALSE(abi.memory_facts().page_bytes.has_value());
    EXPECT_TRUE(abi.memory_facts().provenance.empty());
}

TEST(AbiProfile, ParsesAndSerializesGeneratedTargetFactsDeterministically) {
    auto const source{R"profile(ioj-layout-profile 1
name "Windows x64 Debug"
identity platform "Windows"
identity architecture "AMD64"
identity abi "Microsoft x64"
identity compiler "MSVC 19.44"
identity build-configuration "Debug"
type "float" 4 4 non-integer unknown "probe float"
type "std::uint32_t" 4 4 unsigned 32 "probe uint32"
representation "EntityId" "ProjectWord"
representation "ProjectWord" "std::uint32_t"
memory cache-line 64
memory page 4096
memory l1-data 32768
memory l3 0
memory-provenance "machine profile"
)profile"};

    auto const parsed{parse_abi_profile(source)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->name(), "Windows x64 Debug");
    EXPECT_EQ(parsed->identity().platform, "Windows");
    EXPECT_EQ(parsed->identity().architecture, "AMD64");
    EXPECT_EQ(parsed->identity().abi, "Microsoft x64");
    EXPECT_EQ(parsed->identity().compiler, "MSVC 19.44");
    EXPECT_EQ(parsed->identity().build_configuration, "Debug");
    ASSERT_TRUE(parsed->find("EntityId").has_value());
    EXPECT_EQ(parsed->find("EntityId")->size_bytes, 4);
    EXPECT_EQ(parsed->find("EntityId")->alignment_bytes, 4);
    EXPECT_EQ(parsed->find("EntityId")->integer_signed, false);
    EXPECT_EQ(parsed->find("EntityId")->unsigned_value_bits, 32);
    EXPECT_EQ(parsed->find("EntityId")->provenance, "probe uint32");
    EXPECT_EQ(parsed->find("float")->integer_signed, std::nullopt);
    EXPECT_EQ(parsed->memory_facts().cache_line_bytes, 64);
    EXPECT_EQ(parsed->memory_facts().page_bytes, 4'096);
    EXPECT_EQ(parsed->memory_facts().l1_data_cache_bytes, 32'768);
    EXPECT_EQ(parsed->memory_facts().l2_cache_bytes, std::nullopt);
    EXPECT_EQ(parsed->memory_facts().l3_cache_bytes, 0);
    EXPECT_EQ(parsed->memory_facts().provenance, "machine profile");

    auto const serialized{serialize_abi_profile(*parsed)};
    auto const reparsed{parse_abi_profile(serialized)};
    ASSERT_TRUE(reparsed.has_value()) << reparsed.error().message;
    EXPECT_EQ(reparsed->name(), parsed->name());
    EXPECT_EQ(reparsed->identity(), parsed->identity());
    EXPECT_EQ(reparsed->types(), parsed->types());
    EXPECT_EQ(reparsed->representations(), parsed->representations());
    EXPECT_EQ(reparsed->memory_facts(), parsed->memory_facts());
    EXPECT_EQ(serialize_abi_profile(*reparsed), serialized);
}

TEST(AbiProfile, KeepsOmittedGeneratedFactsUnknown) {
    auto const parsed{parse_abi_profile(R"profile(ioj-layout-profile 1
name "Partial target"
identity architecture "arm64"
type "word" 8 8 signed unknown ""
memory-provenance ""
)profile")};

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_FALSE(parsed->identity().platform.has_value());
    EXPECT_EQ(parsed->identity().architecture, "arm64");
    EXPECT_FALSE(parsed->identity().abi.has_value());
    EXPECT_FALSE(parsed->memory_facts().cache_line_bytes.has_value());
    EXPECT_FALSE(parsed->memory_facts().page_bytes.has_value());
    EXPECT_TRUE(parsed->memory_facts().provenance.empty());
    ASSERT_TRUE(parsed->find("word").has_value());
    EXPECT_EQ(parsed->find("word")->integer_signed, true);
    EXPECT_FALSE(parsed->find("word")->unsigned_value_bits.has_value());
    EXPECT_TRUE(parsed->find("word")->provenance.empty());
    EXPECT_FALSE(parsed->find("unknown").has_value());
}

TEST(AbiProfile, RejectsMalformedOrContradictoryGeneratedFactsWithoutPublishing) {
    std::array const invalid_profiles{
        "name \"missing header\"\n",
        "ioj-layout-profile 1\nname \"zero size\"\ntype \"word\" 0 1 unsigned 1 \"probe\"\n",
        "ioj-layout-profile 1\nname \"bad alignment\"\ntype \"word\" 4 3 unsigned 32 \"probe\"\n",
        "ioj-layout-profile 1\nname \"bad signed bits\"\ntype \"word\" 4 4 signed 31 \"probe\"\n",
        "ioj-layout-profile 1\nname \"wide bits\"\ntype \"word\" 1 1 unsigned 9 \"probe\"\n",
        "ioj-layout-profile 1\nname \"duplicate\"\ntype \"word\" 4 4 unsigned 32 \"a\"\ntype "
        "\"word\" 4 4 unsigned 32 \"b\"\n",
        "ioj-layout-profile 1\nname \"duplicate identity\"\nidentity abi \"one\"\nidentity abi "
        "\"two\"\n",
        "ioj-layout-profile 1\nname \"cycle\"\nrepresentation \"A\" \"B\"\nrepresentation \"B\" "
        "\"A\"\n",
        "ioj-layout-profile 1\nname \"zero memory\"\nmemory cache-line 0\n",
        "ioj-layout-profile 1\nname \"unknown directive\"\nmystery 1\n"};

    for (auto const* const source : invalid_profiles) {
        SCOPED_TRACE(source);
        auto const parsed{parse_abi_profile(source)};
        ASSERT_FALSE(parsed.has_value());
        EXPECT_FALSE(parsed.error().message.empty());
    }
}

TEST(Analyzer, ReportsAggregateWorkingSetFitForExplicitCacheCapacities) {
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 4'096,
                          .l2_cache_bytes = 8'192,
                          .l3_cache_bytes = 16'384,
                          .provenance = "synthetic cache profile"});

    auto const packed_fixture{entity_id_type()};
    auto const packed{
        Analyzer::analyze_packed(packed_fixture.types, packed_fixture.type, Variant{}, abi, 1'024)};
    EXPECT_EQ(packed.aggregate.cache_capacity.working_set_bytes, 4'096);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l1_data, true);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l2, true);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l3, true);

    auto const record_fixture{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member("small", "std::uint8_t"), record_member("wide", "std::uint32_t")},
        .export_specifier = std::nullopt}})};
    auto const record{
        Analyzer::analyze_record(record_fixture.types, record_fixture.type, abi, 1'025)};
    EXPECT_EQ(record.aggregate.cache_capacity.working_set_bytes, 8'200);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l1_data, false);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l2, false);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l3, true);

    auto const soa_fixture{soa_type()};
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    EXPECT_EQ(soa.cache_capacity.working_set_bytes, 24'000);
    EXPECT_EQ(soa.cache_capacity.fits_l1_data, false);
    EXPECT_EQ(soa.cache_capacity.fits_l2, false);
    EXPECT_EQ(soa.cache_capacity.fits_l3, false);
}

TEST(Analyzer, LeavesCacheFitUnknownWhenCapacityOrWorkingSetIsUnknown) {
    auto const packed_fixture{entity_id_type()};
    auto const packed{Analyzer::analyze_packed(
        packed_fixture.types, packed_fixture.type, Variant{}, AbiProfile::host_common(), 1)};
    EXPECT_FALSE(packed.aggregate.cache_capacity.l1_data_capacity_bytes.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l2.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l3.has_value());

    auto const soa_fixture{soa_type({{"unknown", "UnknownUserType"}})};
    AbiProfile abi{"unknown working set"};
    abi.set_memory_facts({.cache_line_bytes = std::nullopt,
                          .page_bytes = std::nullopt,
                          .l1_data_cache_bytes = 32'768,
                          .l2_cache_bytes = 1'048'576,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "partial synthetic profile"});
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    EXPECT_FALSE(soa.cache_capacity.working_set_bytes.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l2.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l3.has_value());
}

TEST(SoaAnalyzer, LeavesCacheStatisticsUnknownWithoutTargetFact) {
    auto const fixture{soa_type({{"values", "four"}})};
    AbiProfile abi{"unknown memory"};
    abi.set("four",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};

    EXPECT_EQ(analysis.columns[0].total_bytes, 400);
    EXPECT_FALSE(analysis.columns[0].minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.columns[0].cache_line_tiling.has_value());
    EXPECT_FALSE(analysis.columns[0].minimum_pages.has_value());
    EXPECT_FALSE(analysis.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, UnknownTypesRemainUnknown) {
    auto const fixture{soa_type({{"unknown", "UnknownUserType"}})};
    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};

    EXPECT_FALSE(analysis.columns[0].type_facts.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsIntegerOverflow) {
    auto const fixture{soa_type({{"values", "huge"}})};
    AbiProfile abi{"test"};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = {}});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 2)};

    EXPECT_FALSE(analysis.columns[0].total_bytes.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsSelectedColumnAccessFootprints) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1'000)};
    std::vector<std::string> const columns{"wide", "small", "wide"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 100)};

    EXPECT_EQ(access.column_names, (std::vector<std::string>{"wide", "small"}));
    ASSERT_EQ(access.columns.size(), 2);
    EXPECT_EQ(access.columns[0].name, "wide");
    EXPECT_EQ(access.columns[0].physical_type, "std::uint64_t");
    EXPECT_EQ(access.columns[0].element_bytes, 8);
    EXPECT_EQ(access.columns[0].useful_bytes, 800);
    EXPECT_EQ(access.columns[0].read_useful_bytes, 800);
    EXPECT_EQ(access.columns[0].write_useful_bytes, 0);
    EXPECT_EQ(access.columns[0].minimum_cache_lines, 13);
    EXPECT_EQ(access.columns[0].minimum_cache_bytes, 832);
    EXPECT_EQ(access.columns[0].non_payload_cache_bytes, 32);
    EXPECT_EQ(access.columns[0].minimum_pages, 1);
    EXPECT_EQ(access.columns[0].minimum_page_bytes, 4'096);
    EXPECT_EQ(access.columns[0].non_payload_page_bytes, 3'296);
    EXPECT_EQ(access.columns[0].allocated_capacity_payload_bytes, 8'000);
    EXPECT_EQ(access.columns[0].capacity_slack_payload_bytes, 7'200);
    EXPECT_EQ(access.columns[1].name, "small");
    EXPECT_EQ(access.columns[1].element_bytes, 1);
    EXPECT_EQ(access.columns[1].useful_bytes, 100);
    EXPECT_EQ(access.columns[1].read_useful_bytes, 100);
    EXPECT_EQ(access.columns[1].write_useful_bytes, 0);
    EXPECT_EQ(access.columns[1].minimum_cache_lines, 2);
    EXPECT_EQ(access.columns[1].minimum_cache_bytes, 128);
    EXPECT_EQ(access.columns[1].non_payload_cache_bytes, 28);
    EXPECT_EQ(access.columns[1].minimum_pages, 1);
    EXPECT_EQ(access.columns[1].minimum_page_bytes, 4'096);
    EXPECT_EQ(access.columns[1].non_payload_page_bytes, 3'996);
    EXPECT_EQ(access.columns[1].allocated_capacity_payload_bytes, 1'000);
    EXPECT_EQ(access.columns[1].capacity_slack_payload_bytes, 900);
    EXPECT_EQ(access.element_count, 100);
    ASSERT_EQ(access.accesses.size(), 2);
    EXPECT_EQ(access.accesses.front().operation, AccessOperation::read);
    EXPECT_EQ(access.useful_bytes, 900);
    EXPECT_EQ(access.read_useful_bytes, 900);
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_EQ(access.logical_read_useful_bytes, 900);
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.full_logical_payload_bytes, 1'300);
    EXPECT_EQ(access.unselected_payload_bytes, 400);
    EXPECT_EQ(access.allocated_capacity_payload_bytes, 13'000);
    EXPECT_EQ(access.capacity_slack_payload_bytes, 11'700);
    EXPECT_EQ(access.cache_line_bytes, 64);
    EXPECT_EQ(access.minimum_cache_lines_touched, 15);
    EXPECT_EQ(access.minimum_cache_bytes_touched, 960);
    EXPECT_EQ(access.non_payload_cache_bytes, 60);
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l2.has_value());
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l3.has_value());
    EXPECT_EQ(access.page_bytes, 4'096);
    EXPECT_EQ(access.minimum_pages_touched, 2);
    EXPECT_EQ(access.minimum_page_bytes_touched, 8'192);
    EXPECT_EQ(access.non_payload_page_bytes, 7'292);
    EXPECT_TRUE(access.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsAlignedSelectedColumnBoundaryCrossings) {
    auto const fixture{soa_type({{"even", "even"}, {"odd", "odd"}, {"wide", "wide"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("even",
            {.size_bytes = 2,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("odd",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 20)};
    std::vector<std::string> const columns{"even", "odd", "wide"};

    auto access{Analyzer::analyze_soa_access(soa, columns, abi, 10)};

    ASSERT_EQ(access.columns.size(), 3);
    EXPECT_EQ(access.columns[0].aligned_cache_line_straddling_elements, 0);
    EXPECT_EQ(access.columns[0].aligned_page_straddling_elements, 0);
    EXPECT_EQ(access.columns[1].aligned_cache_line_straddling_elements, 2);
    EXPECT_EQ(access.columns[1].aligned_page_straddling_elements, 2);
    EXPECT_EQ(access.columns[2].aligned_cache_line_straddling_elements, 10);
    EXPECT_EQ(access.columns[2].aligned_page_straddling_elements, 10);

    access = Analyzer::analyze_soa_access(soa, columns, abi, 0);
    for (auto const& column : access.columns) {
        EXPECT_EQ(column.aligned_cache_line_straddling_elements, 0);
        EXPECT_EQ(column.aligned_page_straddling_elements, 0);
    }

    abi.set_memory_facts({.cache_line_bytes = std::nullopt,
                          .page_bytes = std::nullopt,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test unknown"});
    access = Analyzer::analyze_soa_access(soa, columns, abi, 10);
    for (auto const& column : access.columns) {
        EXPECT_FALSE(column.aligned_cache_line_straddling_elements.has_value());
        EXPECT_FALSE(column.aligned_page_straddling_elements.has_value());
    }
}

TEST(SoaAnalyzer, ComparesAlignedSelectedColumnBoundaryCrossingsAcrossVariants) {
    auto const fixture{soa_type({{"value", "odd"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("odd",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});
    auto const baseline{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 20)};
    auto variant{Variant{}};
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "value"}] = "wide";
    auto const overridden{Analyzer::analyze_soa(fixture.types, fixture.type, variant, abi, 20)};
    std::vector<std::string> const columns{"value"};
    auto const first{Analyzer::analyze_soa_access(baseline, columns, abi, 10)};
    auto const second{Analyzer::analyze_soa_access(overridden, columns, abi, 10)};

    auto const comparison{Analyzer::compare_soa_access(first, second)};

    ASSERT_EQ(comparison.columns.size(), 1);
    auto const& column{comparison.columns.front()};
    EXPECT_EQ(column.first.aligned_cache_line_straddling_elements, 2);
    EXPECT_EQ(column.second.aligned_cache_line_straddling_elements, 10);
    EXPECT_EQ(column.aligned_cache_line_straddling_element_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(column.aligned_cache_line_straddling_element_delta->magnitude, 8);
    EXPECT_EQ(column.first.aligned_page_straddling_elements, 2);
    EXPECT_EQ(column.second.aligned_page_straddling_elements, 10);
    EXPECT_EQ(column.aligned_page_straddling_element_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(column.aligned_page_straddling_element_delta->magnitude, 8);
}

TEST(SoaAnalyzer, KeepsIncompleteSelectedColumnAccessUnknown) {
    auto const fixture{soa_type({{"known", "std::uint32_t"}, {"unknown", "UnknownUserType"}})};
    AbiProfile abi{"partial"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::vector<std::string> const columns{"known", "unknown", "missing"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 10)};

    EXPECT_EQ(access.column_names, (std::vector<std::string>{"known", "unknown"}));
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.read_useful_bytes.has_value());
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_FALSE(access.full_logical_payload_bytes.has_value());
    EXPECT_FALSE(access.unselected_payload_bytes.has_value());
    EXPECT_FALSE(access.capacity_slack_payload_bytes.has_value());
    EXPECT_FALSE(access.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(access.minimum_pages_touched.has_value());
    EXPECT_FALSE(access.minimum_read_cache_lines_touched.has_value());
    EXPECT_EQ(access.minimum_write_cache_lines_touched, 0);
    EXPECT_FALSE(access.minimum_read_pages_touched.has_value());
    EXPECT_EQ(access.minimum_write_pages_touched, 0);
    EXPECT_FALSE(access.cache_line_bytes.has_value());
    EXPECT_FALSE(access.page_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    auto const empty{Analyzer::analyze_soa_access(soa, std::span<std::string const>{}, abi, 10)};
    EXPECT_TRUE(empty.column_names.empty());
    EXPECT_FALSE(empty.useful_bytes.has_value());
    EXPECT_FALSE(empty.diagnostics.empty());
}

TEST(SoaAnalyzer, DiagnosesSelectedColumnAccessOverflowAndExcessCapacity) {
    auto const fixture{soa_type({{"huge", "huge"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1)};
    std::vector<std::string> const columns{"huge"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 2)};

    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.read_useful_bytes.has_value());
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_FALSE(access.full_logical_payload_bytes.has_value());
    EXPECT_FALSE(access.capacity_slack_payload_bytes.has_value());
    EXPECT_FALSE(access.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(access.minimum_pages_touched.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    auto const write_access{
        Analyzer::analyze_soa_access(soa, columns, abi, 2, AccessOperation::write)};
    EXPECT_EQ(write_access.read_useful_bytes, 0);
    EXPECT_FALSE(write_access.write_useful_bytes.has_value());
}

TEST(SoaAnalyzer, ScalesLogicalAccessMultiplicityWithoutChangingFootprint) {
    auto const fixture{soa_type({{"wide", "std::uint64_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::vector<std::string> const columns{"wide"};

    auto access{
        Analyzer::analyze_soa_access(soa, columns, abi, 10, AccessOperation::read_write, 3)};
    EXPECT_EQ(access.multiplicity, 3);
    EXPECT_EQ(access.useful_bytes, 80);
    EXPECT_EQ(access.logical_read_useful_bytes, 240);
    EXPECT_EQ(access.logical_write_useful_bytes, 240);
    EXPECT_EQ(access.minimum_cache_lines_touched, 2);
    EXPECT_EQ(access.minimum_read_cache_lines_touched, 2);
    EXPECT_EQ(access.minimum_write_cache_lines_touched, 2);

    access = Analyzer::analyze_soa_access(
        soa, columns, abi, 1, AccessOperation::read, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(access.useful_bytes, 8);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.minimum_cache_lines_touched, 1);
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_soa_access(soa, columns, abi, 10, AccessOperation::read, 0);
    EXPECT_EQ(access.useful_bytes, 80);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_FALSE(access.logical_write_useful_bytes.has_value());
    EXPECT_EQ(access.minimum_cache_lines_touched, 2);
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(SoaAnalyzer, ClassifiesIndividualColumnAccessOperations) {
    auto const fixture{soa_type(
        {{"wide", "std::uint64_t"}, {"small", "std::uint8_t"}, {"medium", "std::uint32_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::vector<AccessIntent> const accesses{
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "small", .operation = AccessOperation::read},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 2)};

    EXPECT_EQ(analysis.accesses, accesses);
    EXPECT_EQ(analysis.useful_bytes, 1'300);
    EXPECT_EQ(analysis.read_useful_bytes, 500);
    EXPECT_EQ(analysis.write_useful_bytes, 1'200);
    EXPECT_EQ(analysis.logical_read_useful_bytes, 1'000);
    EXPECT_EQ(analysis.logical_write_useful_bytes, 2'400);
    ASSERT_EQ(analysis.columns.size(), 3);
    EXPECT_EQ(analysis.columns[0].operation, AccessOperation::write);
    EXPECT_EQ(analysis.columns[1].operation, AccessOperation::read);
    EXPECT_EQ(analysis.columns[2].operation, AccessOperation::read_write);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 22);
    EXPECT_EQ(analysis.minimum_read_cache_lines_touched, 9);
    EXPECT_EQ(analysis.minimum_read_cache_bytes_touched, 576);
    EXPECT_EQ(analysis.minimum_write_cache_lines_touched, 20);
    EXPECT_EQ(analysis.minimum_write_cache_bytes_touched, 1'280);
    EXPECT_EQ(analysis.minimum_pages_touched, 3);
    EXPECT_EQ(analysis.minimum_read_pages_touched, 2);
    EXPECT_EQ(analysis.minimum_write_pages_touched, 2);
    EXPECT_EQ(analysis.minimum_read_page_bytes_touched, 8'192);
    EXPECT_EQ(analysis.minimum_write_page_bytes_touched, 8'192);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto same{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 2)};
    auto comparison{Analyzer::compare_soa_access(analysis, same)};
    EXPECT_TRUE(comparison.diagnostics.empty());
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    same.accesses.front().operation = AccessOperation::read;
    comparison = Analyzer::compare_soa_access(analysis, same);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());

    auto conflicting{accesses};
    conflicting.push_back({.name = "wide", .operation = AccessOperation::read});
    auto const normalized{Analyzer::analyze_soa_access(soa, conflicting, abi, 100, 2)};
    EXPECT_EQ(normalized.accesses, accesses);
    EXPECT_EQ(normalized.read_useful_bytes, 500);
    EXPECT_EQ(normalized.write_useful_bytes, 1'200);
    EXPECT_FALSE(normalized.diagnostics.empty());
}

TEST(SoaAnalyzer, UnionsSelectedAccessInAlignedContiguousBlock) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"wide", "std::uint64_t"}, {"medium", "std::uint32_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types,
                                         fixture.type,
                                         Variant{},
                                         abi,
                                         10,
                                         SoaAllocationStrategy::aligned_contiguous)};
    std::vector<AccessIntent> const accesses{
        {.name = "small", .operation = AccessOperation::read},
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_soa_access(soa, accesses, abi, 2)};

    EXPECT_EQ(analysis.allocation_strategy, SoaAllocationStrategy::aligned_contiguous);
    EXPECT_TRUE(analysis.footprint_exact);
    EXPECT_EQ(analysis.allocation_count, 1);
    EXPECT_EQ(analysis.total_allocation_bytes, 136);
    EXPECT_EQ(analysis.alignment_padding_bytes, 6);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_cache_bytes_touched, 128);
    EXPECT_EQ(analysis.minimum_read_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_write_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_read_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_write_pages_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto variant{Variant{}};
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "wide"}] =
        "std::uint32_t";
    auto const compact_soa{Analyzer::analyze_soa(
        fixture.types, fixture.type, variant, abi, 10, SoaAllocationStrategy::aligned_contiguous)};
    auto const compact_access{Analyzer::analyze_soa_access(compact_soa, accesses, abi, 2)};
    auto const comparison{Analyzer::compare_soa_access(analysis, compact_access)};
    EXPECT_EQ(comparison.allocation_strategy, SoaAllocationStrategy::aligned_contiguous);
    EXPECT_TRUE(comparison.footprint_exact);
    EXPECT_EQ(comparison.first_allocation_count, 1);
    EXPECT_EQ(comparison.second_allocation_count, 1);
    EXPECT_EQ(comparison.first_total_allocation_bytes, 136);
    EXPECT_EQ(comparison.second_total_allocation_bytes, 92);
    EXPECT_EQ(comparison.total_allocation_byte_delta->magnitude, 44);
    EXPECT_EQ(comparison.alignment_padding_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 1);

    auto const excess{Analyzer::analyze_soa_access(soa, accesses, abi, 11)};
    EXPECT_FALSE(excess.footprint_exact);
    EXPECT_FALSE(excess.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(excess.minimum_pages_touched.has_value());
    EXPECT_FALSE(excess.diagnostics.empty());
}

TEST(SoaAnalyzer, UsesContiguousColumnOffsetsForBoundaryCrossings) {
    auto const fixture{soa_type({{"lead", "Lead"}, {"odd", "Odd"}})};
    auto abi{AbiProfile{"Offset target"}};
    abi.set("Lead",
            TypeFacts{.size_bytes = 8,
                      .alignment_bytes = 4,
                      .integer_signed = std::nullopt,
                      .unsigned_value_bits = std::nullopt,
                      .provenance = "Test target"});
    abi.set("Odd",
            TypeFacts{.size_bytes = 12,
                      .alignment_bytes = 4,
                      .integer_signed = std::nullopt,
                      .unsigned_value_bits = std::nullopt,
                      .provenance = "Test target"});
    abi.set_memory_facts({.cache_line_bytes = 16,
                          .page_bytes = 32,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "Test target"});
    std::array<std::string, 1> const selected{"odd"};

    auto const separate{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 1, SoaAllocationStrategy::separate_columns)};
    auto const separate_access{Analyzer::analyze_soa_access(separate, selected, abi, 1)};
    ASSERT_EQ(separate_access.columns.size(), 1);
    EXPECT_EQ(separate_access.columns.front().aligned_cache_line_straddling_elements, 0);

    auto const contiguous{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 1, SoaAllocationStrategy::aligned_contiguous)};
    ASSERT_EQ(contiguous.columns[1].allocation_offset_bytes, 8);
    auto const contiguous_access{Analyzer::analyze_soa_access(contiguous, selected, abi, 1)};
    ASSERT_EQ(contiguous_access.columns.size(), 1);
    EXPECT_EQ(contiguous_access.columns.front().aligned_cache_line_straddling_elements, 1);
    EXPECT_EQ(contiguous_access.columns.front().aligned_page_straddling_elements, 0);
}

TEST(SoaAnalyzer, ComparesEquivalentRecordAndSoaAccessFacts) {
    auto const record_fixture{
        record_type({codegen::RecordSchema{.name = "Row",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("medium", "std::uint32_t"),
                                                       record_member("wide", "std::uint64_t")},
                                           .export_specifier = std::nullopt}},
                    "Row")};
    auto const soa_fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 1'000,
                          .l2_cache_bytes = 960,
                          .l3_cache_bytes = 1'600,
                          .provenance = "test profile"});
    auto const record{
        Analyzer::analyze_record(record_fixture.types, record_fixture.type, abi, 100)};
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    std::vector<std::string> const members{"small", "wide"};
    std::vector<AccessIntent> const accesses{{.name = "small", .operation = AccessOperation::read},
                                             {.name = "wide", .operation = AccessOperation::write}};
    auto const record_access{Analyzer::analyze_record_access(record, accesses, abi, 3)};
    auto const soa_access{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 3)};

    auto const comparison{Analyzer::compare_record_soa_access(record_access, soa_access)};

    EXPECT_EQ(comparison.member_names, members);
    EXPECT_EQ(comparison.element_count, 100);
    EXPECT_EQ(comparison.accesses, accesses);
    EXPECT_EQ(comparison.multiplicity, 3);
    EXPECT_EQ(comparison.record.useful_bytes, 900);
    EXPECT_EQ(comparison.soa.useful_bytes, 900);
    EXPECT_EQ(comparison.record.read_useful_bytes, 100);
    EXPECT_EQ(comparison.soa.read_useful_bytes, 100);
    EXPECT_EQ(comparison.record.write_useful_bytes, 800);
    EXPECT_EQ(comparison.soa.write_useful_bytes, 800);
    EXPECT_EQ(comparison.record.logical_read_useful_bytes, 300);
    EXPECT_EQ(comparison.soa.logical_read_useful_bytes, 300);
    EXPECT_EQ(comparison.record.logical_write_useful_bytes, 2'400);
    EXPECT_EQ(comparison.soa.logical_write_useful_bytes, 2'400);
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.write_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.record.cache_lines, 25);
    EXPECT_EQ(comparison.record.cache_bytes, 1'600);
    EXPECT_EQ(comparison.soa.cache_lines, 15);
    EXPECT_EQ(comparison.soa.cache_bytes, 960);
    EXPECT_EQ(comparison.record.read_cache_bytes, 1'600);
    EXPECT_EQ(comparison.record.write_cache_bytes, 1'600);
    EXPECT_EQ(comparison.soa.read_cache_bytes, 128);
    EXPECT_EQ(comparison.soa.write_cache_bytes, 832);
    EXPECT_EQ(comparison.read_cache_byte_delta->magnitude, 1'472);
    EXPECT_EQ(comparison.write_cache_byte_delta->magnitude, 768);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.working_set_bytes, 1'600);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.working_set_bytes, 960);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l1_data, false);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l2, false);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l2, true);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.record.pages, 1);
    EXPECT_EQ(comparison.record.page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.pages, 2);
    EXPECT_EQ(comparison.soa.page_bytes, 8'192);
    EXPECT_EQ(comparison.record.read_page_bytes, 4'096);
    EXPECT_EQ(comparison.record.write_page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.read_page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.write_page_bytes, 4'096);
    EXPECT_EQ(comparison.record_non_useful_cache_bytes, 700);
    EXPECT_EQ(comparison.soa_non_useful_cache_bytes, 60);
    EXPECT_EQ(comparison.record_non_useful_page_bytes, 3'196);
    EXPECT_EQ(comparison.soa_non_useful_page_bytes, 7'292);
    EXPECT_EQ(comparison.useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.cache_line_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 10);
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 640);
    EXPECT_EQ(comparison.page_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.page_delta->magnitude, 1);
    EXPECT_EQ(comparison.page_byte_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.non_useful_cache_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.non_useful_cache_byte_delta->magnitude, 640);
    EXPECT_EQ(comparison.non_useful_page_byte_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.non_useful_page_byte_delta->magnitude, 4'096);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, RejectsIncompatibleRecordAndSoaAccessSets) {
    RecordAccessAnalysis record;
    record.member_names = {"first"};
    record.element_count = 10;
    SoaAccessAnalysis soa;
    soa.column_names = {"second"};
    soa.element_count = 10;

    auto comparison{Analyzer::compare_record_soa_access(record, soa)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());

    soa.column_names = {"first"};
    soa.element_count = 11;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    record.element_count = 11;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.record_non_useful_cache_bytes.has_value());
    EXPECT_FALSE(comparison.soa_non_useful_cache_bytes.has_value());
    EXPECT_FALSE(comparison.non_useful_cache_byte_delta.has_value());
    EXPECT_FALSE(comparison.record_non_useful_page_bytes.has_value());
    EXPECT_FALSE(comparison.soa_non_useful_page_bytes.has_value());
    EXPECT_FALSE(comparison.non_useful_page_byte_delta.has_value());

    record.accesses = {{.name = "first", .operation = AccessOperation::read}};
    soa.accesses = {{.name = "first", .operation = AccessOperation::write}};
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.write_useful_byte_delta.has_value());

    soa.accesses.front().operation = AccessOperation::read;
    soa.multiplicity = 2;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.logical_write_useful_byte_delta.has_value());
}

TEST(SoaAnalyzer, ComparesSelectedAccessAcrossPhysicalVariants) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 500,
                          .l2_cache_bytes = 448,
                          .l3_cache_bytes = 832,
                          .provenance = "test profile"});
    auto variant{Variant{}};
    variant.overrides.capacities[fixture.type] = 500;
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "wide"}] =
        "std::uint32_t";
    auto const first{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1'000)};
    auto const second{Analyzer::analyze_soa(fixture.types, fixture.type, variant, abi, 1'000)};
    std::vector<std::string> const columns{"wide"};
    auto const first_access{
        Analyzer::analyze_soa_access(first, columns, abi, 100, AccessOperation::write, 5)};
    auto const second_access{
        Analyzer::analyze_soa_access(second, columns, abi, 100, AccessOperation::write, 5)};

    auto const comparison{Analyzer::compare_soa_access(first_access, second_access)};

    EXPECT_EQ(comparison.column_names, columns);
    ASSERT_EQ(comparison.columns.size(), 1);
    EXPECT_EQ(comparison.columns[0].name, "wide");
    EXPECT_EQ(comparison.columns[0].first.physical_type, "std::uint64_t");
    EXPECT_EQ(comparison.columns[0].second.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].first.element_bytes, 8);
    EXPECT_EQ(comparison.columns[0].second.element_bytes, 4);
    EXPECT_EQ(comparison.columns[0].element_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[0].useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.columns[0].cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.columns[0].cache_byte_delta->magnitude, 384);
    EXPECT_EQ(comparison.columns[0].first.non_payload_cache_bytes, 32);
    EXPECT_EQ(comparison.columns[0].second.non_payload_cache_bytes, 48);
    EXPECT_EQ(comparison.columns[0].non_payload_cache_byte_delta->magnitude, 16);
    EXPECT_EQ(comparison.columns[0].page_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[0].first.non_payload_page_bytes, 3'296);
    EXPECT_EQ(comparison.columns[0].second.non_payload_page_bytes, 3'696);
    EXPECT_EQ(comparison.columns[0].non_payload_page_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.columns[0].allocated_capacity_payload_delta->magnitude, 6'000);
    EXPECT_EQ(comparison.columns[0].capacity_slack_payload_delta->magnitude, 5'600);
    EXPECT_EQ(comparison.element_count, 100);
    ASSERT_EQ(comparison.accesses.size(), 1);
    EXPECT_EQ(comparison.accesses.front().operation, AccessOperation::write);
    EXPECT_EQ(comparison.multiplicity, 5);
    EXPECT_EQ(comparison.first.useful_bytes, 800);
    EXPECT_EQ(comparison.second.useful_bytes, 400);
    EXPECT_EQ(comparison.first.read_useful_bytes, 0);
    EXPECT_EQ(comparison.second.read_useful_bytes, 0);
    EXPECT_EQ(comparison.first.write_useful_bytes, 800);
    EXPECT_EQ(comparison.second.write_useful_bytes, 400);
    EXPECT_EQ(comparison.first.logical_read_useful_bytes, 0);
    EXPECT_EQ(comparison.second.logical_read_useful_bytes, 0);
    EXPECT_EQ(comparison.first.logical_write_useful_bytes, 4'000);
    EXPECT_EQ(comparison.second.logical_write_useful_bytes, 2'000);
    EXPECT_EQ(comparison.first.read_cache_bytes, 0);
    EXPECT_EQ(comparison.second.read_cache_bytes, 0);
    EXPECT_EQ(comparison.first.write_cache_bytes, 832);
    EXPECT_EQ(comparison.second.write_cache_bytes, 448);
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.write_useful_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.write_useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->direction,
              NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->magnitude, 2'000);
    EXPECT_EQ(comparison.first_full_logical_payload_bytes, 1'300);
    EXPECT_EQ(comparison.second_full_logical_payload_bytes, 900);
    EXPECT_EQ(comparison.first_unselected_payload_bytes, 500);
    EXPECT_EQ(comparison.second_unselected_payload_bytes, 500);
    EXPECT_EQ(comparison.first.cache_lines, 13);
    EXPECT_EQ(comparison.second.cache_lines, 7);
    EXPECT_EQ(comparison.first.cache_bytes, 832);
    EXPECT_EQ(comparison.second.cache_bytes, 448);
    EXPECT_EQ(comparison.first.pages, 1);
    EXPECT_EQ(comparison.second.pages, 1);
    EXPECT_EQ(comparison.first_allocated_capacity_payload_bytes, 13'000);
    EXPECT_EQ(comparison.second_allocated_capacity_payload_bytes, 4'500);
    EXPECT_EQ(comparison.first_capacity_slack_payload_bytes, 11'700);
    EXPECT_EQ(comparison.second_capacity_slack_payload_bytes, 3'600);
    EXPECT_EQ(comparison.useful_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 384);
    EXPECT_EQ(comparison.first_non_payload_cache_bytes, 32);
    EXPECT_EQ(comparison.second_non_payload_cache_bytes, 48);
    EXPECT_EQ(comparison.non_payload_cache_byte_delta->magnitude, 16);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.working_set_bytes, 832);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.working_set_bytes, 448);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l1_data, false);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l2, false);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l2, true);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.page_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first_non_payload_page_bytes, 3'296);
    EXPECT_EQ(comparison.second_non_payload_page_bytes, 3'696);
    EXPECT_EQ(comparison.non_payload_page_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.full_logical_payload_delta->magnitude, 400);
    EXPECT_EQ(comparison.unselected_payload_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.allocated_capacity_payload_delta->magnitude, 8'500);
    EXPECT_EQ(comparison.capacity_slack_payload_delta->magnitude, 8'100);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, ComparesSelectedAccessAcrossTargetProfiles) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const first_layout{Analyzer::analyze_soa(fixture.types,
                                                  fixture.type,
                                                  Variant{},
                                                  first_profile,
                                                  100,
                                                  SoaAllocationStrategy::separate_columns)};
    auto const second_layout{Analyzer::analyze_soa(fixture.types,
                                                   fixture.type,
                                                   Variant{},
                                                   second_profile,
                                                   100,
                                                   SoaAllocationStrategy::separate_columns)};
    std::array const accesses{AccessIntent{.name = "wide", .operation = AccessOperation::write}};
    auto const first_access{
        Analyzer::analyze_soa_access(first_layout, accesses, first_profile, 10, 3)};
    auto const second_access{
        Analyzer::analyze_soa_access(second_layout, accesses, second_profile, 10, 3)};

    auto const comparison{Analyzer::compare_soa_access(first_access, second_access)};

    ASSERT_EQ(comparison.columns.size(), 1);
    EXPECT_EQ(comparison.columns[0].first.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].second.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].element_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[0].useful_byte_delta->magnitude, 40);
    EXPECT_EQ(comparison.columns[0].logical_write_useful_byte_delta->magnitude, 120);
    EXPECT_EQ(comparison.columns[0].cache_line_delta->magnitude, 1);
    EXPECT_EQ(comparison.first_total_allocation_bytes, 500);
    EXPECT_EQ(comparison.second_total_allocation_bytes, 900);
    EXPECT_EQ(comparison.total_allocation_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.first_alignment_padding_bytes, 0);
    EXPECT_EQ(comparison.second_alignment_padding_bytes, 0);
    EXPECT_EQ(comparison.alignment_padding_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.useful_bytes, 40);
    EXPECT_EQ(comparison.second.useful_bytes, 80);
    EXPECT_EQ(comparison.first.write_cache_bytes, 64);
    EXPECT_EQ(comparison.second.write_cache_bytes, 128);
    EXPECT_EQ(comparison.write_cache_byte_delta->magnitude, 64);
    EXPECT_EQ(comparison.first_allocated_capacity_payload_bytes, 500);
    EXPECT_EQ(comparison.second_allocated_capacity_payload_bytes, 900);
    EXPECT_EQ(comparison.allocated_capacity_payload_delta->magnitude, 400);
    EXPECT_EQ(comparison.first_capacity_slack_payload_bytes, 450);
    EXPECT_EQ(comparison.second_capacity_slack_payload_bytes, 810);
    EXPECT_EQ(comparison.capacity_slack_payload_delta->magnitude, 360);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, RejectsIncompatibleSelectedAccessComparisons) {
    SoaAccessAnalysis first;
    first.column_names = {"first"};
    first.element_count = 10;
    auto second{first};
    second.column_names = {"second"};

    auto comparison{Analyzer::compare_soa_access(first, second)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());

    second.column_names = first.column_names;
    second.element_count = 11;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    second.element_count = first.element_count;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    second.columns = first.columns;
    first.accesses = {{.name = "first", .operation = AccessOperation::read}};
    second.accesses = {{.name = "first", .operation = AccessOperation::write}};
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.write_useful_byte_delta.has_value());

    second.accesses.front().operation = AccessOperation::read;
    second.allocation_strategy = SoaAllocationStrategy::aligned_contiguous;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.total_allocation_byte_delta.has_value());

    second.allocation_strategy = SoaAllocationStrategy::separate_columns;
    second.multiplicity = 2;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.logical_write_useful_byte_delta.has_value());

    first.column_names = {"first", "second"};
    second = first;
    SoaColumnAccessAnalysis first_column;
    first_column.name = "first";
    SoaColumnAccessAnalysis second_column;
    second_column.name = "second";
    first.columns = {first_column, second_column};
    second.columns = {first_column};
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.columns.empty());
}

} // namespace
} // namespace ioj::layout
