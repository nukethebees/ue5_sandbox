#include <lispb/schema/type_graph.h>

#include <codegen/source_loader.h>
#include <lispb/project.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace lispb::schema {
namespace {

auto production_graph() -> TypeGraph {
    auto const project_root{
        std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{lispb::load_project(project_root / "lispb/project.lispb")};
    auto const& target{std::get<lispb::CppSchemaTarget>(project.targets.at("sandbox-code"))};
    std::vector<std::filesystem::path> sources;
    sources.reserve(target.sources.size());
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{codegen::load_sources(project.root / target.types, sources)};
    return resolve_type_graph(manifest);
}

TEST(SemanticTypeGraph, PreservesEntityTypeEnumSemantics) {
    auto const graph{production_graph()};
    auto const id{graph.find_declared("native_entity_type", "EntityType")};
    ASSERT_TRUE(id.has_value());

    auto const& node{graph.type(*id)};
    EXPECT_EQ(node.identity.namespace_name, "ioj::sim");
    auto const* type{std::get_if<EnumType>(&node.definition)};
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(graph.type(type->underlying_type.type).cpp_spelling, "std::uint8_t");
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*id), type->underlying_type.type),
              graph.dependencies_of(*id).end());
    EXPECT_EQ(type->count, "COUNT");

    auto const player_ship{
        std::ranges::find(type->enumerators, std::string{"PlayerShip"}, &Enumerator::name)};
    ASSERT_NE(player_ship, type->enumerators.end());
    EXPECT_EQ(player_ship->explicit_value, "0");
    EXPECT_EQ(player_ship->display_name, "Player Ship");
    EXPECT_EQ(player_ship->serialized_name, "player_ship");

    auto const count{std::ranges::find(type->enumerators, std::string{"COUNT"}, &Enumerator::name)};
    ASSERT_NE(count, type->enumerators.end());
    EXPECT_TRUE(count->hidden);
    EXPECT_TRUE(count->count_sentinel);
}

TEST(SemanticTypeGraph, PreservesExplicitEnumSemanticWidth) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::EnumModuleSchema{
            .settings = codegen::ModuleSettings{.name = "states", .header = "States.h"},
            .enums = {codegen::EnumSchema{
                .name = "State",
                .underlying_type = codegen::TypeRef{"std::uint8_t"},
                .bit_width = 3,
                .signedness = false,
                .values = {codegen::EnumeratorSchema{"Idle", "0"},
                           codegen::EnumeratorSchema{
                               .name = "Active", .initializer = "7", .sentinel = true}},
            }},
        }},
    };

    auto const graph{resolve_type_graph(manifest)};
    auto const state{graph.find_declared("states", "State")};
    ASSERT_TRUE(state.has_value());
    auto const& type{std::get<EnumType>(graph.type(*state).definition)};
    EXPECT_EQ(type.bit_width, 3);
    EXPECT_EQ(type.signedness, false);
    ASSERT_EQ(type.enumerators.size(), 2U);
    EXPECT_FALSE(type.enumerators[0].sentinel);
    EXPECT_TRUE(type.enumerators[1].sentinel);
    EXPECT_FALSE(type.enumerators[1].count_sentinel);
}

TEST(SemanticTypeGraph, ResolvesStandaloneIntegerScalarWithoutPhysicalFacts) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::ScalarModuleSchema{
            .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                .header = "SemanticValues.h",
                                                .namespace_name = "project"},
            .scalars = {codegen::IntegerScalarSchema{
                .name = "DamageReason",
                .signedness = false,
                .minimum_value = 0,
                .maximum_value = 10,
                .bit_width = std::nullopt,
                .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false},
                                {.name = "Invalid", .value = 15, .sentinel = true}}}}}}};

    auto const graph{resolve_type_graph(manifest)};
    auto const scalar_id{graph.find_declared("semantic_values", "DamageReason")};
    ASSERT_TRUE(scalar_id.has_value());
    auto const& node{graph.type(*scalar_id)};
    EXPECT_EQ(node.identity.namespace_name, "project");
    auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)};
    ASSERT_NE(scalar, nullptr);
    EXPECT_FALSE(scalar->signedness);
    EXPECT_EQ(scalar->minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(scalar->maximum_value, codegen::PackedIntegerValue{10});
    EXPECT_TRUE(scalar->bit_width_auto);
    EXPECT_EQ(scalar->bit_width, 4U);
    ASSERT_EQ(scalar->named_codes.size(), 2U);
    EXPECT_TRUE(scalar->named_codes[1].sentinel);
    EXPECT_TRUE(node.dependencies.empty());
}

TEST(SemanticTypeGraph, ResolvesPhysicalRepresentationsAndDependencies) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules =
            {codegen::ScalarModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "semantic_values",
                                                     .header = "SemanticValues.h",
                                                     .namespace_name = "project"},
                 .scalars = {codegen::IntegerScalarSchema{
                     .name = "Health",
                     .signedness = false,
                     .minimum_value = 0,
                     .maximum_value = 1000,
                     .bit_width = std::nullopt,
                     .named_codes = {{.name = "Invalid", .value = 1023, .sentinel = true}}}}},
             codegen::RepresentationModuleSchema{
                 .settings = codegen::ModuleSettings{.name = "representations",
                                                     .header = "Representations.h",
                                                     .namespace_name = "project"},
                 .linear_quantized = {codegen::LinearQuantizedSchema{
                     .name = "HealthQ8",
                     .source = codegen::TypeRef{"project::Health"},
                     .bit_width = 8,
                     .reserved_codes = 1,
                     .clipping = codegen::QuantizationClipping::clamp}},
                 .integer_varints = {codegen::IntegerVarintSchema{
                     .name = "HealthVarint",
                     .source = codegen::TypeRef{"project::Health"},
                     .encoding = codegen::IntegerVarintEncoding::unsigned_varint}},
                 .fixed_points = {codegen::FixedPointSchema{
                     .name = "VelocityQ12_4",
                     .signedness = true,
                     .total_bits = 16,
                     .fractional_bits = 4,
                     .rounding = codegen::FixedPointRounding::toward_zero}},
                 .optional_sentinels = {codegen::OptionalSentinelSchema{
                     .name = "OptionalHealth",
                     .source = codegen::TypeRef{"project::Health"},
                     .sentinel = "Invalid"}},
                 .optional_presence_bits = {codegen::OptionalPresenceBitSchema{
                     .name = "PresentHealth", .source = codegen::TypeRef{"project::Health"}}},
                 .mini_floats = {codegen::MiniFloatSchema{.name = "CompactFloat",
                                                          .sign_bits = 1,
                                                          .exponent_bits = 5,
                                                          .significand_bits = 10,
                                                          .exponent_bias = 15}}}},
    };

    auto const graph{resolve_type_graph(manifest)};
    auto const source{graph.find_declared("semantic_values", "Health")};
    auto const representation{graph.find_declared("representations", "HealthQ8")};
    auto const varint{graph.find_declared("representations", "HealthVarint")};
    auto const fixed_point{graph.find_declared("representations", "VelocityQ12_4")};
    auto const optional{graph.find_declared("representations", "OptionalHealth")};
    auto const presence{graph.find_declared("representations", "PresentHealth")};
    auto const mini_float{graph.find_declared("representations", "CompactFloat")};
    ASSERT_TRUE(source.has_value());
    ASSERT_TRUE(representation.has_value());
    ASSERT_TRUE(varint.has_value());
    ASSERT_TRUE(fixed_point.has_value());
    ASSERT_TRUE(optional.has_value());
    ASSERT_TRUE(presence.has_value());
    ASSERT_TRUE(mini_float.has_value());

    auto const& type{std::get<LinearQuantizedType>(graph.type(*representation).definition)};
    EXPECT_EQ(type.source.type, *source);
    EXPECT_EQ(type.bit_width, 8U);
    EXPECT_EQ(type.reserved_codes, 1U);
    EXPECT_EQ(type.clipping, codegen::QuantizationClipping::clamp);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*representation), *source),
              graph.dependencies_of(*representation).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *representation),
              graph.users_of(*source).end());

    auto const& varint_type{std::get<IntegerVarintType>(graph.type(*varint).definition)};
    EXPECT_EQ(varint_type.source.type, *source);
    EXPECT_EQ(varint_type.encoding, codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*varint), *source),
              graph.dependencies_of(*varint).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *varint), graph.users_of(*source).end());

    auto const& fixed_point_type{std::get<FixedPointType>(graph.type(*fixed_point).definition)};
    EXPECT_TRUE(fixed_point_type.signedness);
    EXPECT_EQ(fixed_point_type.total_bits, 16U);
    EXPECT_EQ(fixed_point_type.fractional_bits, 4U);
    EXPECT_EQ(fixed_point_type.rounding, codegen::FixedPointRounding::toward_zero);
    EXPECT_TRUE(graph.dependencies_of(*fixed_point).empty());

    auto const& optional_type{std::get<OptionalSentinelType>(graph.type(*optional).definition)};
    EXPECT_EQ(optional_type.source.type, *source);
    EXPECT_EQ(optional_type.sentinel_name, "Invalid");
    EXPECT_EQ(optional_type.sentinel_value, codegen::PackedIntegerValue{1023});
    EXPECT_EQ(optional_type.bit_width, 10U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*optional), *source),
              graph.dependencies_of(*optional).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *optional), graph.users_of(*source).end());

    auto const& presence_type{std::get<OptionalPresenceBitType>(graph.type(*presence).definition)};
    EXPECT_EQ(presence_type.source.type, *source);
    EXPECT_EQ(presence_type.payload_bits, 10U);
    EXPECT_EQ(presence_type.encoded_bits, 11U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*presence), *source),
              graph.dependencies_of(*presence).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *presence), graph.users_of(*source).end());

    auto const& mini_float_type{std::get<MiniFloatType>(graph.type(*mini_float).definition)};
    EXPECT_EQ(mini_float_type.sign_bits, 1U);
    EXPECT_EQ(mini_float_type.exponent_bits, 5U);
    EXPECT_EQ(mini_float_type.significand_bits, 10U);
    EXPECT_EQ(mini_float_type.exponent_bias, 15);
    EXPECT_TRUE(graph.dependencies_of(*mini_float).empty());
}

TEST(SemanticTypeGraph, ResolvesPackedFieldsAndDependencies) {
    auto const graph{production_graph()};
    auto const entity_type{graph.find_declared("native_entity_type", "EntityType")};
    auto const unique_id{graph.find_declared("native_entity_unique_id", "EntityUniqueId")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(unique_id.has_value());

    auto const* packed{std::get_if<PackedType>(&graph.type(*unique_id).definition)};
    ASSERT_NE(packed, nullptr);
    EXPECT_EQ(graph.type(packed->storage_type.type).cpp_spelling, "std::uint32_t");
    EXPECT_EQ(packed->invalid_raw_value, 0xffffffffU);
    ASSERT_EQ(packed->segments.size(), 2U);
    auto const& index{std::get<PackedField>(packed->segments[0])};
    auto const& entity_type_field{std::get<PackedField>(packed->segments[1])};
    EXPECT_EQ(index.name, "index");
    EXPECT_EQ(index.bit_width, 24U);
    EXPECT_EQ(entity_type_field.name, "entity_type");
    EXPECT_EQ(entity_type_field.bit_width, 8U);
    EXPECT_EQ(entity_type_field.semantic_type.type, *entity_type);

    EXPECT_NE(std::ranges::find(graph.dependencies_of(*unique_id), *entity_type),
              graph.dependencies_of(*unique_id).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*entity_type), *unique_id),
              graph.users_of(*entity_type).end());
}

TEST(SemanticTypeGraph, RetainsPackedPhysicalOrdering) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::PackedValueModuleSchema{
            .settings = codegen::ModuleSettings{.name = "wire", .header = "Wire.h"},
            .values = {codegen::PackedValueSchema{
                .name = "Header",
                .storage_type = codegen::TypeRef{"std::uint32_t"},
                .segments = {codegen::PackedFieldSchema{
                    "kind", codegen::TypeRef{"std::uint8_t"}, 8}},
                .byte_order = codegen::PackedByteOrder::big_endian,
                .bit_order = codegen::PackedBitOrder::most_significant_first}}}}};

    auto const graph{resolve_type_graph(manifest)};
    auto const header{graph.find_declared("wire", "Header")};
    ASSERT_TRUE(header.has_value());
    auto const& packed{std::get<PackedType>(graph.type(*header).definition)};
    EXPECT_EQ(packed.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(packed.bit_order, codegen::PackedBitOrder::most_significant_first);
}

TEST(SemanticTypeGraph, ResolvesStandardLibrarySoaColumns) {
    auto const graph{production_graph()};
    auto const entity_type{graph.find_declared("native_entity_type", "EntityType")};
    auto const history{graph.find_declared("native_entity_history", "EntityHistoryColumns")};
    auto const vectors{graph.find_declared("native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(history.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& history_type{std::get<SoaType>(graph.type(*history).definition)};
    auto const entity_types{
        std::ranges::find(history_type.columns, std::string{"entity_types"}, &SoaColumn::name)};
    ASSERT_NE(entity_types, history_type.columns.end());
    EXPECT_EQ(entity_types->semantic_type.type, *entity_type);

    auto const& vector_type{std::get<SoaType>(graph.type(*vectors).definition)};
    EXPECT_EQ(vector_type.source_kind, SoaSourceKind::vector);
    auto const vector_equivalent{graph.find_registered("native_vector_3f")};
    ASSERT_TRUE(vector_equivalent.has_value());
    ASSERT_TRUE(vector_type.equivalent_type.has_value());
    EXPECT_EQ(vector_type.equivalent_type->type, *vector_equivalent);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*vectors), *vector_equivalent),
              graph.dependencies_of(*vectors).end());
    ASSERT_EQ(vector_type.columns.size(), 3U);
    for (auto const& column : vector_type.columns) {
        EXPECT_EQ(graph.type(column.semantic_type.type).cpp_spelling, "float");
    }
}

TEST(SemanticTypeGraph, ResolvesRecordMembersFixedArraysAndDependencies) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::RecordModuleSchema{
            .settings = codegen::ModuleSettings{.name = "records", .header = "Records.h"},
            .records =
                {
                    codegen::RecordSchema{
                        .name = "Position",
                        .members = {{.name = "x", .type = codegen::TypeRef{"float"}}}},
                    codegen::RecordSchema{.name = "Path",
                                          .members = {{.name = "points",
                                                       .type = codegen::TypeRef{"Position"},
                                                       .count = 8}}},
                }}},
    };

    auto const graph{resolve_type_graph(manifest)};
    auto const position{graph.find_declared("records", "Position")};
    auto const path{graph.find_declared("records", "Path")};
    ASSERT_TRUE(position.has_value());
    ASSERT_TRUE(path.has_value());
    auto const& record{std::get<RecordType>(graph.type(*path).definition)};
    ASSERT_EQ(record.members.size(), 1U);
    EXPECT_EQ(record.members[0].semantic_type.type, *position);
    EXPECT_EQ(record.members[0].count, 8);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*path), *position),
              graph.dependencies_of(*path).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*position), *path), graph.users_of(*position).end());
}

TEST(SemanticTypeGraph, RejectsDirectAndIndirectByValueRecordCycles) {
    auto module{codegen::RecordModuleSchema{
        .settings = codegen::ModuleSettings{.name = "records", .header = "Records.h"},
        .records = {codegen::RecordSchema{
            .name = "Record", .members = {{.name = "self", .type = codegen::TypeRef{"Record"}}}}},
    }};
    auto manifest{
        codegen::Manifest{.schema_version = codegen::manifest_schema_version, .modules = {module}}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    module.records = {
        codegen::RecordSchema{.name = "First",
                              .members = {{.name = "second", .type = codegen::TypeRef{"Second"}}}},
        codegen::RecordSchema{.name = "Second",
                              .members = {{.name = "first", .type = codegen::TypeRef{"First"}}}},
    };
    manifest.modules = {module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesRawUnionAlternativesAndDependencies) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::UnionModuleSchema{
            .settings = codegen::ModuleSettings{.name = "payloads", .header = "Payloads.h"},
            .unions = {codegen::UnionSchema{
                .name = "Payload",
                .alternatives =
                    {{.name = "identifier", .type = codegen::TypeRef{"std::uint32_t"}},
                     {.name = "bytes", .type = codegen::TypeRef{"std::uint8_t"}, .count = 12}},
                .export_specifier = std::nullopt}}}}};

    auto const graph{resolve_type_graph(manifest)};
    auto const payload{graph.find_declared("payloads", "Payload")};
    ASSERT_TRUE(payload.has_value());
    auto const& union_type{std::get<UnionType>(graph.type(*payload).definition)};
    ASSERT_EQ(union_type.alternatives.size(), 2U);
    EXPECT_EQ(union_type.alternatives[0].name, "identifier");
    EXPECT_EQ(graph.type(union_type.alternatives[0].semantic_type.type).cpp_spelling,
              "std::uint32_t");
    EXPECT_EQ(union_type.alternatives[1].count, 12);
    EXPECT_EQ(graph.dependencies_of(*payload).size(), 2U);
}

TEST(SemanticTypeGraph, RejectsUnionAndMixedAggregateCycles) {
    codegen::UnionSchema union_schema{
        .name = "Payload",
        .alternatives = {{.name = "self", .type = codegen::TypeRef{"Payload"}}},
        .export_specifier = std::nullopt};
    codegen::UnionModuleSchema union_module{
        .settings = codegen::ModuleSettings{.name = "payloads",
                                            .header = "Payloads.h",
                                            .namespace_name = "payloads"},
        .unions = {union_schema}};
    codegen::Manifest manifest{.schema_version = codegen::manifest_schema_version,
                               .modules = {union_module}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    union_module.unions.front().alternatives = {
        {.name = "record", .type = codegen::TypeRef{"records::Record"}}};
    codegen::RecordSchema record_schema{
        .name = "Record",
        .members = {{.name = "payload", .type = codegen::TypeRef{"payloads::Payload"}}},
        .export_specifier = std::nullopt};
    codegen::RecordModuleSchema record_module{
        .settings = codegen::ModuleSettings{.name = "records",
                                            .header = "Records.h",
                                            .namespace_name = "records"},
        .records = {record_schema}};
    manifest.modules = {record_module, union_module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesTaggedUnionDiscriminantMappingsAndDependencies) {
    codegen::EnumModuleSchema enums{
        .settings = codegen::ModuleSettings{.name = "events",
                                            .header = "Events.h",
                                            .namespace_name = "events"},
        .enums = {codegen::EnumSchema{
            .name = "EventKind",
            .underlying_type = codegen::TypeRef{"std::uint8_t"},
            .values = {codegen::EnumeratorSchema{"Spawn"},
                       codegen::EnumeratorSchema{"Damage"},
                       codegen::EnumeratorSchema{.name = "Invalid", .sentinel = true}}}}};
    codegen::UnionModuleSchema unions{
        .settings = codegen::ModuleSettings{.name = "payloads",
                                            .header = "Payloads.h",
                                            .namespace_name = "payloads"},
        .unions = {},
        .tagged_unions = {codegen::TaggedUnionSchema{
            .name = "Event",
            .discriminant = codegen::TypeRef{"events::EventKind"},
            .alternatives = {
                {.name = "spawn", .type = codegen::TypeRef{"std::uint32_t"}, .tag = "Spawn"},
                {.name = "damage",
                 .type = codegen::TypeRef{"std::uint16_t"},
                 .count = 4,
                 .tag = "Damage"}}}}};
    codegen::Manifest manifest{.schema_version = codegen::manifest_schema_version,
                               .modules = {enums, unions}};

    auto graph{resolve_type_graph(manifest)};
    auto const event{graph.find_declared("payloads", "Event")};
    auto const kind{graph.find_declared("events", "EventKind")};
    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(kind.has_value());
    auto const& tagged{std::get<TaggedUnionType>(graph.type(*event).definition)};
    EXPECT_EQ(tagged.discriminant.type, *kind);
    ASSERT_EQ(tagged.alternatives.size(), 2U);
    EXPECT_EQ(tagged.alternatives[0].tag, "Spawn");
    EXPECT_EQ(tagged.alternatives[1].count, 4);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*event), *kind),
              graph.dependencies_of(*event).end());
    EXPECT_EQ(graph.users_of(*kind).front(), *event);

    manifest.modules = {
        enums,
        codegen::UnionModuleSchema{.settings = unions.settings,
                                   .unions = {},
                                   .tagged_unions = {codegen::TaggedUnionSchema{
                                       .name = "Event",
                                       .discriminant = codegen::TypeRef{"events::EventKind"},
                                       .alternatives = {{.name = "missing",
                                                         .type = codegen::TypeRef{"std::uint32_t"},
                                                         .tag = "Missing"}}}}}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    std::get<codegen::UnionModuleSchema>(manifest.modules[1])
        .tagged_unions.front()
        .alternatives.front()
        .tag = "Invalid";
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    std::get<codegen::UnionModuleSchema>(manifest.modules[1]).tagged_unions.front().discriminant =
        codegen::TypeRef{"std::uint8_t"};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, KeepsUnknownRegisteredTypesAsExternalLeaves) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {{"native_handle", codegen::CppType{"NativeHandle", "native/handle.h"}}},
        .modules = {codegen::EnumModuleSchema{
            .settings = codegen::ModuleSettings{.name = "mode", .header = "Mode.h"},
            .enums = {codegen::EnumSchema{
                .name = "Mode",
                .underlying_type = codegen::TypeRef{"uint8"},
                .values = {codegen::EnumeratorSchema{"Value"}},
            }},
        }},
    };
    auto const graph{resolve_type_graph(manifest)};
    auto const handle{graph.find_registered("native_handle")};
    ASSERT_TRUE(handle.has_value());
    auto const* external{std::get_if<ExternalType>(&graph.type(*handle).definition)};
    ASSERT_NE(external, nullptr);
    EXPECT_EQ(external->cpp_type.spelling, "NativeHandle");
    ASSERT_FALSE(external->cpp_type.dependencies.empty());
    EXPECT_EQ(*external->cpp_type.dependencies.front().header, "native/handle.h");
    EXPECT_TRUE(graph.dependencies_of(*handle).empty());
}

TEST(SemanticTypeGraph, RejectsAmbiguousRegisteredDeclarationBindings) {
    auto enum_module = [](std::string module_name, std::string header) {
        return codegen::EnumModuleSchema{
            .settings = codegen::ModuleSettings{.name = std::move(module_name),
                                                .header = std::move(header),
                                                .namespace_name = "project"},
            .enums = {codegen::EnumSchema{
                .name = "Mode",
                .underlying_type = codegen::TypeRef{"uint8"},
                .values = {codegen::EnumeratorSchema{"Value"}},
            }},
        };
    };
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .types = {{"mode", codegen::CppType{"project::Mode"}}},
        .modules = {enum_module("first", "First.h"), enum_module("second", "Second.h")},
    };

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsInvalidAndDuplicateDeclaredIdentities) {
    auto const make_module = [](std::string name, std::vector<codegen::EnumSchema> enums) {
        return codegen::EnumModuleSchema{
            .settings = codegen::ModuleSettings{.name = std::move(name), .header = "Types.h"},
            .enums = std::move(enums),
        };
    };
    auto const make_enum = [](std::string name) {
        return codegen::EnumSchema{
            .name = std::move(name),
            .underlying_type = codegen::TypeRef{"uint8"},
            .values = {codegen::EnumeratorSchema{"Value"}},
        };
    };

    codegen::Manifest const invalid{
        .schema_version = codegen::manifest_schema_version,
        .modules = {make_module("types", {make_enum("")})},
    };
    EXPECT_THROW(static_cast<void>(resolve_type_graph(invalid)), std::invalid_argument);

    codegen::Manifest const duplicate{
        .schema_version = codegen::manifest_schema_version,
        .modules = {make_module("types", {make_enum("Mode"), make_enum("Mode")})},
    };
    EXPECT_THROW(static_cast<void>(resolve_type_graph(duplicate)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsUnresolvedRegisteredReferences) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::EnumModuleSchema{
            .settings = codegen::ModuleSettings{.name = "mode", .header = "Mode.h"},
            .enums = {codegen::EnumSchema{
                .name = "Mode",
                .underlying_type = codegen::TypeRef{"@missing"},
                .values = {codegen::EnumeratorSchema{"Value"}},
            }},
        }},
    };

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, TreatsRawCppSpellingsAsExplicitExternalLeaves) {
    codegen::Manifest const manifest{
        .schema_version = codegen::manifest_schema_version,
        .modules = {codegen::SoaModuleSchema{
            .settings = codegen::ModuleSettings{.name = "values", .header = "Values.h"},
            .structs = {codegen::SoaSchema{
                .name = "Values",
                .members = {codegen::SoaMemberSchema{
                    "items", codegen::SoaMemberKind::array, codegen::TypeRef{"ExternalValue"}}},
            }},
            .backend = codegen::SoaBackend::standard_library,
        }},
    };
    auto const graph{resolve_type_graph(manifest)};
    auto const values{graph.find_declared("values", "Values")};
    ASSERT_TRUE(values.has_value());
    auto const& soa{std::get<SoaType>(graph.type(*values).definition)};
    ASSERT_EQ(soa.columns.size(), 1U);
    EXPECT_EQ(graph.type(soa.columns.front().semantic_type.type).identity.origin,
              TypeOrigin::cpp_spelling);
}

} // namespace
} // namespace lispb::schema
