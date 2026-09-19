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
    ASSERT_EQ(packed->fields.size(), 2U);
    EXPECT_EQ(packed->fields[0].name, "index");
    EXPECT_EQ(packed->fields[0].bit_width, 24U);
    EXPECT_EQ(packed->fields[1].name, "entity_type");
    EXPECT_EQ(packed->fields[1].bit_width, 8U);
    EXPECT_EQ(packed->fields[1].semantic_type.type, *entity_type);

    EXPECT_NE(std::ranges::find(graph.dependencies_of(*unique_id), *entity_type),
              graph.dependencies_of(*unique_id).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*entity_type), *unique_id),
              graph.users_of(*entity_type).end());
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
    ASSERT_EQ(vector_type.columns.size(), 3U);
    for (auto const& column : vector_type.columns) {
        EXPECT_EQ(graph.type(column.semantic_type.type).cpp_spelling, "float");
    }
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
