#include "test_utilities.h"

#include <codegen/ast/indices.h>
#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_origin.h>
#include <lispb/schema/union_alternatives.h>
#include <lispb/schema/union_types.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::schema {
namespace {

template <typename Id>
auto text(CompilerStorage const& storage, Id id) -> std::string_view {
    return storage.strings.view(id);
}
template <typename Id>
auto text(CompilerStorage const& storage, std::optional<Id> id) -> std::optional<std::string_view> {
    return id ? std::optional{storage.strings.view(*id)} : std::nullopt;
}

auto production_graph(CompilerStorage& storage) -> TypeGraph {
    auto const project_root{
        std::filesystem::path{IOJ_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{lispb::load_project(storage.syntax, project_root / "lispb/project.lispb")};
    auto const target{*project.find_target("sandbox-code")};
    auto const project_sources{project.sources(target)};
    std::vector<std::filesystem::path> sources;
    sources.reserve(project_sources.size());
    for (auto const& source : project_sources) {
        sources.push_back(project.root / source);
    }
    auto manifest{
        lispb::codegen::load_sources(storage, project.root / project.types(target), sources)};
    return resolve_type_graph(manifest);
}

TEST(SemanticTypeGraph, SimulationModelCompositionUsesDeclaredNodesAndStrongScalarDomain) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto& strings{storage.strings};
    auto const graph{production_graph(storage)};
    auto const& records{graph.definitions().records};
    auto const& record_members{graph.definitions().record_members};
    auto const& integer_scalars{graph.definitions().integer_scalars};
    auto const& soas{graph.definitions().soas};
    auto const& soa_columns{graph.definitions().soa_columns};
    auto const& occurrences{graph.occurrences()};
    auto const loadout{lispb::codegen::find_test_type(
        graph, "native_player_flight_model_data", "FlightModelLoadout")};
    ASSERT_TRUE(loadout.has_value());
    auto current{*loadout};
    for (auto const& [member_name, expected] :
         {std::pair{"profiles", "FlightModelProfile"},
          std::pair{"config", "FlightModelConfig"},
          std::pair{"translation", "TranslationAxesConfig"},
          std::pair{"forward", "TranslationAxisConfig"},
          std::pair{"manual", "TranslationChannelConfig"},
          std::pair{"response", "ResponseConfig"},
          std::pair{"second_order", "SecondOrderResponseConfig"}}) {
        auto const& record{graph.definition(current)};
        auto const member{std::ranges::find(records.members_of(record), member_name, [&](auto id) {
            return text(storage, record_members.name[record_members.index(id)]);
        })};
        ASSERT_NE(member, records.members_of(record).end());
        auto const next{occurrences.type[occurrences.index(
            record_members.semantic_type[record_members.index(*member)])]};
        EXPECT_EQ(graph.origin(next), TypeOrigin::declaration);
        EXPECT_EQ(text(storage, graph.name(next)), expected);
        EXPECT_NE(std::ranges::find(graph.dependencies_of(current), next),
                  graph.dependencies_of(current).end());
        current = next;
    }
    auto const& response{graph.definition(current)};
    EXPECT_EQ(
        text(storage,
             record_members.initializer[record_members.index(records.members_of(response)[0])]),
        "3.f");
    EXPECT_EQ(
        text(storage,
             record_members.initializer[record_members.index(records.members_of(response)[1])]),
        "0.5f");
    EXPECT_EQ(graph.find_registered(strings.find("laser_source")),
              lispb::codegen::find_test_type(graph, "native_laser_source", "LaserSource"));
    EXPECT_EQ(graph.find_registered(strings.find("native_index_span")),
              lispb::codegen::find_test_type(graph, "native_index_span", "IndexSpan"));
    EXPECT_EQ(graph.find_registered(strings.find("native_trace_hit")),
              lispb::codegen::find_test_type(graph, "query_results", "TraceHit"));
    auto const frame_index{
        lispb::codegen::find_test_type(graph, "entity_instance_handle", "EntityFrameIndex")};
    ASSERT_TRUE(frame_index.has_value());
    auto const& integer{graph.definition(*frame_index)};
    EXPECT_FALSE(integer_scalars.signedness[integer_scalars.index(integer)]);
    EXPECT_EQ(integer_scalars.bit_width[integer_scalars.index(integer)], 18U);
    EXPECT_EQ(integer_scalars.minimum_value[integer_scalars.index(integer)],
              lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(integer_scalars.maximum_value[integer_scalars.index(integer)],
              lispb::codegen::PackedIntegerValue{262143});
    EXPECT_FALSE(graph.find_registered(strings.find("native_health_index")).has_value());
    for (auto const& [module, name, column_name, registration] :
         {std::tuple{"native_trace_hits", "TraceHits", "hits", "native_trace_hit"}}) {
        auto const owner{lispb::codegen::find_test_type(graph, module, name)};
        ASSERT_TRUE(owner.has_value()) << module;
        auto const& soa{graph.definition(*owner)};
        auto const column{std::ranges::find(soas.columns_of(soa), column_name, [&](auto id) {
            return text(storage, soa_columns.name[soa_columns.index(id)]);
        })};
        ASSERT_NE(column, soas.columns_of(soa).end());
        EXPECT_EQ(
            occurrences
                .type[occurrences.index(soa_columns.semantic_type[soa_columns.index(*column)])],
            *graph.find_registered(strings.find(registration)));
    }
}

TEST(SemanticTypeGraph, PreservesEntityTypeEnumSemantics) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
    auto const& enums{graph.definitions().enums};
    auto const& occurrences{graph.occurrences()};
    auto const id{lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    ASSERT_TRUE(id.has_value());

    auto const& node{*id};
    EXPECT_EQ(text(storage, graph.namespace_name(node)), "ioj::sim");
    auto const type{
        ((graph.definition(node)).kind() == lispb::schema::TypeDefinitionKind::enumeration
             ? std::optional{graph.definition(node)}
             : std::nullopt)};
    ASSERT_TRUE(type.has_value());
    ASSERT_TRUE(lispb::schema::is_valid(enums.underlying_type[enums.index(*type)]));
    EXPECT_EQ(
        text(storage,
             graph.cpp_spelling(
                 occurrences.type[occurrences.index(enums.underlying_type[enums.index(*type)])])),
        "std::uint8_t");
    EXPECT_NE(std::ranges::find(
                  graph.dependencies_of(*id),
                  occurrences.type[occurrences.index(enums.underlying_type[enums.index(*type)])]),
              graph.dependencies_of(*id).end());
    EXPECT_FALSE(enums.count[enums.index(*type)].has_value());
    EXPECT_TRUE(enums.enum_array[enums.index(*type)]);

    auto const player_ship{std::ranges::find(
        graph.enum_domains().values(enums.domain[enums.index(*type)]),
        std::string{"PlayerShip"},
        [&](auto id) {
            return text(
                storage,
                graph.declarations().enumerators.name[graph.declarations().enumerators.index(id)]);
        })};
    ASSERT_NE(player_ship, graph.enum_domains().values(enums.domain[enums.index(*type)]).end());
    EXPECT_EQ(
        text(storage,
             graph.declarations()
                 .enumerators.initializer[graph.declarations().enumerators.index(*player_ship)]),
        "0");
    EXPECT_EQ(
        text(storage,
             graph.declarations()
                 .enumerators.display_name[graph.declarations().enumerators.index(*player_ship)]),
        "Player Ship");
    EXPECT_EQ(
        text(
            storage,
            graph.declarations()
                .enumerators.serialized_name[graph.declarations().enumerators.index(*player_ship)]),
        "player_ship");

    auto const count{std::ranges::find(
        graph.enum_domains().values(enums.domain[enums.index(*type)]),
        std::string{"COUNT"},
        [&](auto id) {
            return text(
                storage,
                graph.declarations().enumerators.name[graph.declarations().enumerators.index(id)]);
        })};
    EXPECT_EQ(count, graph.enum_domains().values(enums.domain[enums.index(*type)]).end());
}

TEST(SemanticTypeGraph, PreservesExplicitEnumSemanticWidth) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};
    auto& enumerators_schemas{declarations.enumerators};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("states");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("States.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{enums_schemas.append()};
                    auto const new_enums_row{enums_schemas.index(new_enums)};
                    enums_schemas.name[new_enums_row] = strings.intern("State");
                    enums_schemas.underlying_type[new_enums_row] =
                        lispb::codegen::make_test_type_ref(context, "std::uint8_t");
                    enums_schemas.values[new_enums_row] =
                        declarations.range<lispb::codegen::EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{enumerators_schemas.append()};
                                 auto const new_enumerators_row{
                                     enumerators_schemas.index(new_enumerators)};
                                 enumerators_schemas.name[new_enumerators_row] =
                                     strings.intern("Idle");
                                 enumerators_schemas.initializer[new_enumerators_row] =
                                     strings.intern("0");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{enumerators_schemas.append()};
                                 auto const new_enumerators_row{
                                     enumerators_schemas.index(new_enumerators)};
                                 enumerators_schemas.name[new_enumerators_row] =
                                     strings.intern("Active");
                                 enumerators_schemas.initializer[new_enumerators_row] =
                                     strings.intern("7");
                                 enumerators_schemas.sentinel[new_enumerators_row] = true;
                                 return new_enumerators;
                             }()});
                    enums_schemas.bit_width[new_enums_row] = 3;
                    enums_schemas.signedness[new_enums_row] =
                        lispb::schema::EnumSignedness::unsigned_;
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& enums{graph.definitions().enums};
    auto const state{lispb::codegen::find_test_type(graph, "states", "State")};
    ASSERT_TRUE(state.has_value());
    auto const& type{graph.definition(*state)};
    EXPECT_EQ(enums.bit_width[enums.index(type)], 3);
    EXPECT_EQ(enums.signedness[enums.index(type)], EnumSignedness::unsigned_);
    ASSERT_EQ(graph.enum_domains().values(enums.domain[enums.index(type)]).size(), 2U);
    EXPECT_FALSE(graph.declarations().enumerators.sentinel[graph.declarations().enumerators.index(
        graph.enum_domains().values(enums.domain[enums.index(type)])[0])]);
    EXPECT_TRUE(graph.declarations().enumerators.sentinel[graph.declarations().enumerators.index(
        graph.enum_domains().values(enums.domain[enums.index(type)])[1])]);
    EXPECT_NE(graph.enum_domains()
                  .count_value[graph.enum_domains().index(enums.domain[enums.index(type)])],
              graph.enum_domains().values(enums.domain[enums.index(type)])[1]);
}

TEST(SemanticTypeGraph, KeepsDerivedEnumBackingOutOfSemanticDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("states");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("States.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{enums_schemas.append()};
                    auto const new_enums_row{enums_schemas.index(new_enums)};
                    enums_schemas.name[new_enums_row] = strings.intern("State");
                    enums_schemas.underlying_type[new_enums_row] = std::nullopt;
                    enums_schemas.values[new_enums_row] =
                        declarations.range<lispb::codegen::EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[new_enumerators_row] = strings.intern("Idle");
                                 enumerators.initializer[new_enumerators_row] = strings.intern("0");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[new_enumerators_row] = strings.intern("Active");
                                 enumerators.initializer[new_enumerators_row] = strings.intern("7");
                                 return new_enumerators;
                             }()});
                    enums_schemas.bit_width[new_enums_row] = 3;
                    enums_schemas.signedness[new_enums_row] =
                        lispb::schema::EnumSignedness::unsigned_;
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& enums{graph.definitions().enums};
    auto const state{lispb::codegen::find_test_type(graph, "states", "State")};
    ASSERT_TRUE(state.has_value());
    auto const& type{graph.definition(*state)};
    EXPECT_FALSE(lispb::schema::is_valid(enums.underlying_type[enums.index(type)]));
    EXPECT_TRUE(graph.dependencies_of(*state).empty());
}

TEST(SemanticTypeGraph, ResolvesStandaloneIntegerScalarWithoutPhysicalFacts) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& semantic_relations{declarations.semantic_relations};

    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("semantic_values");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("SemanticValues.h");
                module_settings_store.namespace_name[new_module_settings_store_row] =
                    strings.intern("project");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] = declarations.range<
                lispb::codegen::DeclarationHandle>(
                {[&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("DamageReason");
                     integer_scalars.signedness[new_integer_scalars_row] = false;
                     integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                     integer_scalars.maximum_value[new_integer_scalars_row] = 10;
                     integer_scalars.bit_width[new_integer_scalars_row] = std::nullopt;
                     integer_scalars.named_codes[new_integer_scalars_row] =
                         declarations.range<lispb::codegen::PackedNamedCodeId>(
                             {[&] {
                                  auto const new_packed_named_codes{packed_named_codes.append()};
                                  packed_named_codes
                                      .name[packed_named_codes.index(new_packed_named_codes)] =
                                      strings.intern("Unknown");
                                  packed_named_codes
                                      .value[packed_named_codes.index(new_packed_named_codes)] = 0;
                                  packed_named_codes
                                      .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                      false;
                                  return new_packed_named_codes;
                              }(),
                              [&] {
                                  auto const new_packed_named_codes{packed_named_codes.append()};
                                  packed_named_codes
                                      .name[packed_named_codes.index(new_packed_named_codes)] =
                                      strings.intern("Invalid");
                                  packed_named_codes
                                      .value[packed_named_codes.index(new_packed_named_codes)] = 15;
                                  packed_named_codes
                                      .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                      true;
                                  return new_packed_named_codes;
                              }()});
                     integer_scalars.relationship[new_integer_scalars_row] = [&] {
                         auto const new_semantic_relations{semantic_relations.append()};
                         auto const new_semantic_relations_row{
                             semantic_relations.index(new_semantic_relations)};
                         semantic_relations.kind[new_semantic_relations_row] =
                             lispb::codegen::SemanticRelationKind::offset_into;
                         semantic_relations.target[new_semantic_relations_row] =
                             lispb::codegen::make_test_type_ref(context, "project::EntityTable");
                         semantic_relations.unit[new_semantic_relations_row] =
                             lispb::codegen::SemanticRelationUnit::bytes;
                         return new_semantic_relations;
                     }();
                     return new_integer_scalars;
                 }(),
                 [&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("EntityTable");
                     integer_scalars.signedness[new_integer_scalars_row] = false;
                     integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                     integer_scalars.maximum_value[new_integer_scalars_row] = 1'023;
                     context.storage.declarations.integer_scalars
                         .bit_width[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 10;
                     context.storage.declarations.integer_scalars
                         .named_codes[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = {};
                     return new_integer_scalars;
                 }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const scalar_id{lispb::codegen::find_test_type(graph, "semantic_values", "DamageReason")};
    ASSERT_TRUE(scalar_id.has_value());
    auto const& node{*scalar_id};
    EXPECT_EQ(text(storage, graph.namespace_name(node)), "project");
    auto const scalar{graph.definition(node).kind() == TypeDefinitionKind::integer_scalar
                          ? std::optional{graph.definition(node)}
                          : std::nullopt};
    ASSERT_TRUE(scalar.has_value());
    auto const& scalars{graph.definitions().integer_scalars};
    auto const row{scalars.index(*scalar)};
    auto const relationship{scalars.relationship[row]};
    auto const& relationships{graph.relationships()};
    auto const& occurrences{graph.occurrences()};

    EXPECT_FALSE(scalars.signedness[row]);
    EXPECT_EQ(scalars.minimum_value[row], lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(scalars.maximum_value[row], lispb::codegen::PackedIntegerValue{10});
    EXPECT_TRUE(scalars.bit_width_auto[row]);
    EXPECT_EQ(scalars.bit_width[row], 4U);
    ASSERT_EQ(scalars.named_codes[row].size(), 2U);
    EXPECT_TRUE(packed_named_codes.sentinel[declarations.items(scalars.named_codes[row])[1]]);
    ASSERT_TRUE(lispb::schema::is_valid(relationship));
    EXPECT_EQ(relationships.kind[relationships.index(relationship)],
              lispb::codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(relationships.unit[relationships.index(relationship)],
              lispb::codegen::SemanticRelationUnit::bytes);
    auto const target{lispb::codegen::find_test_type(graph, "semantic_values", "EntityTable")};
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(occurrences
                  .type[occurrences.index(relationships.target[relationships.index(relationship)])],
              *target);
    ASSERT_EQ(graph.dependencies_of(*scalar_id).size(), 1U);
    EXPECT_EQ(graph.dependencies_of(*scalar_id).front(), *target);
    ASSERT_EQ(graph.users_of(*target).size(), 1U);
    EXPECT_EQ(graph.users_of(*target).front(), *scalar_id);
}

TEST(SemanticTypeGraph, RejectsIntegerScalarRelationshipToExternalLeaf) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto& modules{storage.declarations.normal_modules};
    auto& modules_settings{storage.declarations.module_settings_store};
    auto& integer_scalars{storage.declarations.integer_scalars};
    auto& semantic_relations{storage.declarations.semantic_relations};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"external_table",
              storage.cpp_types.add(
                  "project::ExternalTable",
                  {storage.cpp_types.dependency("project::ExternalTable", "ExternalTable.h")})}}),
        storage.declarations,
        {[&] {
            auto const new_normal_modules{modules.append()};
            modules.settings[new_normal_modules] = [&] {
                auto const new_module_settings_store{modules_settings.append()};
                modules_settings.name[new_module_settings_store] =
                    storage.strings.intern("semantic_values");
                modules_settings.header[new_module_settings_store] =
                    storage.strings.intern("SemanticValues.h");
                modules_settings.namespace_name[new_module_settings_store] =
                    storage.strings.intern("project");
                return new_module_settings_store;
            }();
            modules.declarations[new_normal_modules] =
                storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_integer_scalars{integer_scalars.append()};
                    auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                    integer_scalars.name[new_integer_scalars_row] =
                        storage.strings.intern("EntityIndex");
                    integer_scalars.signedness[new_integer_scalars_row] = false;
                    integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                    integer_scalars.maximum_value[new_integer_scalars_row] = 1'023;
                    integer_scalars.bit_width[new_integer_scalars_row] = 10;
                    integer_scalars.named_codes[new_integer_scalars_row] = {};
                    integer_scalars.relationship[new_integer_scalars_row] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            lispb::codegen::SemanticRelationKind::index_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            lispb::codegen::make_test_type_ref(context, "@external_table");
                        semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return new_integer_scalars;
                }()});
            return new_normal_modules;
        }()}};

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesPhysicalRepresentationsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& linear_quantizeds{declarations.linear_quantizeds};

    auto& integer_varints_schemas{declarations.integer_varints};
    auto& fixed_points_schemas{declarations.fixed_points};
    auto& optional_sentinels_schemas{declarations.optional_sentinels};
    auto& optional_presence_bits_schemas{declarations.optional_presence_bits};
    auto& mini_floats_schemas{declarations.mini_floats};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("semantic_values");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("SemanticValues.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("Health");
                     integer_scalars.signedness[new_integer_scalars_row] = false;
                     integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                     integer_scalars.maximum_value[new_integer_scalars_row] = 1000;
                     integer_scalars.bit_width[new_integer_scalars_row] = std::nullopt;
                     integer_scalars.named_codes[new_integer_scalars_row] =
                         declarations.range<lispb::codegen::PackedNamedCodeId>({[&] {
                             auto const new_packed_named_codes{packed_named_codes.append()};
                             auto const new_packed_named_codes_row{
                                 packed_named_codes.index(new_packed_named_codes)};
                             packed_named_codes.name[new_packed_named_codes_row] =
                                 strings.intern("Invalid");
                             packed_named_codes.value[new_packed_named_codes_row] = 1023;
                             packed_named_codes
                                 .sentinel[packed_named_codes.index(new_packed_named_codes)] = true;
                             return new_packed_named_codes;
                         }()});
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("representations");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Representations.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>(
                     {[&] {
                          auto const new_linear_quantizeds{linear_quantizeds.append()};
                          auto const new_linear_quantizeds_row{
                              linear_quantizeds.index(new_linear_quantizeds)};
                          linear_quantizeds.name[new_linear_quantizeds_row] =
                              strings.intern("HealthQ8");
                          linear_quantizeds.source[new_linear_quantizeds_row] =
                              lispb::codegen::make_test_type_ref(context, "project::Health");
                          linear_quantizeds.bit_width[new_linear_quantizeds_row] = 8;
                          linear_quantizeds.reserved_codes[new_linear_quantizeds_row] = 1;
                          linear_quantizeds.clipping[new_linear_quantizeds_row] =
                              lispb::codegen::QuantizationClipping::clamp;
                          return new_linear_quantizeds;
                      }(),
                      [&] {
                          auto const new_integer_varints{integer_varints_schemas.append()};
                          auto const new_integer_varints_row{
                              integer_varints_schemas.index(new_integer_varints)};
                          integer_varints_schemas.name[new_integer_varints_row] =
                              strings.intern("HealthVarint");
                          integer_varints_schemas.source[new_integer_varints_row] =
                              lispb::codegen::make_test_type_ref(context, "project::Health");
                          integer_varints_schemas.encoding[new_integer_varints_row] =
                              lispb::codegen::IntegerVarintEncoding::unsigned_varint;
                          return new_integer_varints;
                      }(),
                      [&] {
                          auto const new_fixed_points{fixed_points_schemas.append()};
                          auto const new_fixed_points_row{
                              fixed_points_schemas.index(new_fixed_points)};
                          fixed_points_schemas.name[new_fixed_points_row] =
                              strings.intern("VelocityQ12_4");
                          fixed_points_schemas.signedness[new_fixed_points_row] = true;
                          fixed_points_schemas.total_bits[new_fixed_points_row] = 16;
                          fixed_points_schemas.fractional_bits[new_fixed_points_row] = 4;
                          fixed_points_schemas.rounding[new_fixed_points_row] =
                              lispb::codegen::FixedPointRounding::toward_zero;
                          return new_fixed_points;
                      }(),
                      [&] {
                          auto const new_optional_sentinels{optional_sentinels_schemas.append()};
                          auto const new_optional_sentinels_row{
                              optional_sentinels_schemas.index(new_optional_sentinels)};
                          optional_sentinels_schemas.name[new_optional_sentinels_row] =
                              strings.intern("OptionalHealth");
                          optional_sentinels_schemas.source[new_optional_sentinels_row] =
                              lispb::codegen::make_test_type_ref(context, "project::Health");
                          optional_sentinels_schemas.sentinel[new_optional_sentinels_row] =
                              strings.intern("Invalid");
                          return new_optional_sentinels;
                      }(),
                      [&] {
                          auto const new_optional_presence_bits{
                              optional_presence_bits_schemas.append()};
                          auto const new_optional_presence_bits_row{
                              optional_presence_bits_schemas.index(new_optional_presence_bits)};
                          optional_presence_bits_schemas.name[new_optional_presence_bits_row] =
                              strings.intern("PresentHealth");
                          optional_presence_bits_schemas.source[new_optional_presence_bits_row] =
                              lispb::codegen::make_test_type_ref(context, "project::Health");
                          return new_optional_presence_bits;
                      }(),
                      [&] {
                          auto const new_mini_floats{mini_floats_schemas.append()};
                          auto const new_mini_floats_row{
                              mini_floats_schemas.index(new_mini_floats)};
                          mini_floats_schemas.name[new_mini_floats_row] =
                              strings.intern("CompactFloat");
                          mini_floats_schemas.sign_bits[new_mini_floats_row] = 1;
                          mini_floats_schemas.exponent_bits[new_mini_floats_row] = 5;
                          mini_floats_schemas.significand_bits[new_mini_floats_row] = 10;
                          mini_floats_schemas.exponent_bias[new_mini_floats_row] = 15;
                          return new_mini_floats;
                      }()});
             return new_normal_modules;
         }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& linear_quantized{graph.definitions().linear_quantized};
    auto const& integer_varints{graph.definitions().integer_varints};
    auto const& fixed_points{graph.definitions().fixed_points};
    auto const& optional_sentinels{graph.definitions().optional_sentinels};
    auto const& optional_presence_bits{graph.definitions().optional_presence_bits};
    auto const& mini_floats{graph.definitions().mini_floats};
    auto const& occurrences{graph.occurrences()};
    auto const source{lispb::codegen::find_test_type(graph, "semantic_values", "Health")};
    auto const representation{lispb::codegen::find_test_type(graph, "representations", "HealthQ8")};
    auto const varint{lispb::codegen::find_test_type(graph, "representations", "HealthVarint")};
    auto const fixed_point{
        lispb::codegen::find_test_type(graph, "representations", "VelocityQ12_4")};
    auto const optional{lispb::codegen::find_test_type(graph, "representations", "OptionalHealth")};
    auto const presence{lispb::codegen::find_test_type(graph, "representations", "PresentHealth")};
    auto const mini_float{lispb::codegen::find_test_type(graph, "representations", "CompactFloat")};
    ASSERT_TRUE(source.has_value());
    ASSERT_TRUE(representation.has_value());
    ASSERT_TRUE(varint.has_value());
    ASSERT_TRUE(fixed_point.has_value());
    ASSERT_TRUE(optional.has_value());
    ASSERT_TRUE(presence.has_value());
    ASSERT_TRUE(mini_float.has_value());

    auto const& type{graph.definition(*representation)};
    EXPECT_EQ(
        occurrences.type[occurrences.index(linear_quantized.source[linear_quantized.index(type)])],
        *source);
    EXPECT_EQ(linear_quantized.bit_width[linear_quantized.index(type)], 8U);
    EXPECT_EQ(linear_quantized.reserved_codes[linear_quantized.index(type)], 1U);
    EXPECT_EQ(linear_quantized.clipping[linear_quantized.index(type)],
              lispb::codegen::QuantizationClipping::clamp);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*representation), *source),
              graph.dependencies_of(*representation).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *representation),
              graph.users_of(*source).end());

    auto const& varint_type{graph.definition(*varint)};
    EXPECT_EQ(
        occurrences
            .type[occurrences.index(integer_varints.source[integer_varints.index(varint_type)])],
        *source);
    EXPECT_EQ(integer_varints.encoding[integer_varints.index(varint_type)],
              lispb::codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*varint), *source),
              graph.dependencies_of(*varint).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *varint), graph.users_of(*source).end());

    auto const& fixed_point_type{graph.definition(*fixed_point)};
    EXPECT_TRUE(fixed_points.signedness[fixed_points.index(fixed_point_type)]);
    EXPECT_EQ(fixed_points.total_bits[fixed_points.index(fixed_point_type)], 16U);
    EXPECT_EQ(fixed_points.fractional_bits[fixed_points.index(fixed_point_type)], 4U);
    EXPECT_EQ(fixed_points.rounding[fixed_points.index(fixed_point_type)],
              lispb::codegen::FixedPointRounding::toward_zero);
    EXPECT_TRUE(graph.dependencies_of(*fixed_point).empty());

    auto const& optional_type{graph.definition(*optional)};
    EXPECT_EQ(occurrences.type[occurrences.index(
                  optional_sentinels.source[optional_sentinels.index(optional_type)])],
              *source);
    EXPECT_EQ(storage.strings.view(
                  optional_sentinels.sentinel_name[optional_sentinels.index(optional_type)]),
              "Invalid");
    EXPECT_EQ(optional_sentinels.sentinel_value[optional_sentinels.index(optional_type)],
              lispb::codegen::PackedIntegerValue{1023});
    EXPECT_EQ(optional_sentinels.bit_width[optional_sentinels.index(optional_type)], 10U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*optional), *source),
              graph.dependencies_of(*optional).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *optional), graph.users_of(*source).end());

    auto const& presence_type{graph.definition(*presence)};
    EXPECT_EQ(occurrences.type[occurrences.index(
                  optional_presence_bits.source[optional_presence_bits.index(presence_type)])],
              *source);
    EXPECT_EQ(optional_presence_bits.payload_bits[optional_presence_bits.index(presence_type)],
              10U);
    EXPECT_EQ(optional_presence_bits.encoded_bits[optional_presence_bits.index(presence_type)],
              11U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*presence), *source),
              graph.dependencies_of(*presence).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *presence), graph.users_of(*source).end());

    auto const& mini_float_type{graph.definition(*mini_float)};
    EXPECT_EQ(mini_floats.sign_bits[mini_floats.index(mini_float_type)], 1U);
    EXPECT_EQ(mini_floats.exponent_bits[mini_floats.index(mini_float_type)], 5U);
    EXPECT_EQ(mini_floats.significand_bits[mini_floats.index(mini_float_type)], 10U);
    EXPECT_EQ(mini_floats.exponent_bias[mini_floats.index(mini_float_type)], 15);
    EXPECT_TRUE(graph.dependencies_of(*mini_float).empty());
}

TEST(SemanticTypeGraph, ResolvesPackedFieldsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
    auto const& semantic_packed{graph.definitions().packed};
    auto const& semantic_packed_fields{graph.definitions().packed_fields};
    auto const& occurrences{graph.occurrences()};
    auto const entity_type{
        lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    auto const unique_id{lispb::codegen::find_test_type(graph, "native_entity_id", "EntityId")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(unique_id.has_value());

    auto const packed{
        ((graph.definition(*unique_id)).kind() == lispb::schema::TypeDefinitionKind::packed
             ? std::optional{graph.definition(*unique_id)}
             : std::nullopt)};
    ASSERT_TRUE(packed.has_value());
    EXPECT_EQ(text(storage,
                   graph.cpp_spelling(occurrences.type[occurrences.index(
                       semantic_packed.storage_type[semantic_packed.index(*packed)])])),
              "std::uint32_t");
    EXPECT_EQ(semantic_packed.invalid_raw_value[semantic_packed.index(*packed)], 0xffffffffU);
    ASSERT_EQ(semantic_packed.segments_of(*packed).size(), 2U);
    auto const& index{semantic_packed.segments_of(*packed)[0]};
    auto const& entity_type_field{semantic_packed.segments_of(*packed)[1]};
    EXPECT_EQ(text(storage, semantic_packed_fields.name[semantic_packed_fields.index(index)]),
              "index");
    EXPECT_EQ(semantic_packed_fields.bit_width[semantic_packed_fields.index(index)], 24U);
    EXPECT_EQ(
        text(storage, semantic_packed_fields.name[semantic_packed_fields.index(entity_type_field)]),
        "entity_type");
    EXPECT_EQ(semantic_packed_fields.bit_width[semantic_packed_fields.index(entity_type_field)],
              8U);
    EXPECT_EQ(
        occurrences.type[occurrences.index(
            semantic_packed_fields.semantic_type[semantic_packed_fields.index(entity_type_field)])],
        *entity_type);

    EXPECT_NE(std::ranges::find(graph.dependencies_of(*unique_id), *entity_type),
              graph.dependencies_of(*unique_id).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*entity_type), *unique_id),
              graph.users_of(*entity_type).end());
}

TEST(SemanticTypeGraph, ResolvesPackedIntegerScalarDomainAndDependency) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields_schemas{declarations.packed_fields};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& semantic_declarations{storage.declarations};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("domains");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Domains.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("Health");
                     integer_scalars.signedness[new_integer_scalars_row] = false;
                     integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                     integer_scalars.maximum_value[new_integer_scalars_row] = 1000;
                     integer_scalars.bit_width[new_integer_scalars_row] = std::nullopt;
                     integer_scalars.named_codes[new_integer_scalars_row] =
                         declarations.range<lispb::codegen::PackedNamedCodeId>({[&] {
                             auto const new_packed_named_codes{packed_named_codes.append()};
                             auto const new_packed_named_codes_row{
                                 packed_named_codes.index(new_packed_named_codes)};
                             packed_named_codes.name[new_packed_named_codes_row] =
                                 strings.intern("Invalid");
                             packed_named_codes.value[new_packed_named_codes_row] = 4095;
                             packed_named_codes
                                 .sentinel[packed_named_codes.index(new_packed_named_codes)] = true;
                             return new_packed_named_codes;
                         }()});
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("packed");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Packed.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_packed_values{packed_values.append()};
                     auto const new_packed_values_row{packed_values.index(new_packed_values)};
                     packed_values.name[new_packed_values_row] = strings.intern("Status");
                     packed_values.storage_type[new_packed_values_row] =
                         lispb::codegen::make_test_type_ref(context, "std::uint16_t");
                     packed_values.segments[new_packed_values_row] =
                         declarations.range<lispb::codegen::PackedSegmentId>({[&] {
                             auto const new_packed_fields{packed_fields_schemas.append()};
                             auto const new_packed_fields_row{
                                 packed_fields_schemas.index(new_packed_fields)};
                             packed_fields_schemas.name[new_packed_fields_row] =
                                 strings.intern("healt"
                                                "h");
                             packed_fields_schemas.type[new_packed_fields_row] =
                                 lispb::codegen::make_test_type_ref(context,
                                                                    "pr"
                                                                    "oj"
                                                                    "ec"
                                                                    "t:"
                                                                    ":H"
                                                                    "ea"
                                                                    "lt"
                                                                    "h");
                             packed_fields_schemas.bits[new_packed_fields_row] = std::nullopt;
                             packed_fields_schemas.kind[new_packed_fields_row] =
                                 lispb::codegen::PackedFieldKind::unsigned_integer;
                             return new_packed_fields;
                         }()});
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& semantic_packed{graph.definitions().packed};
    auto const& semantic_packed_fields{graph.definitions().packed_fields};
    auto const& occurrences{graph.occurrences()};
    auto const health{lispb::codegen::find_test_type(graph, "domains", "Health")};
    auto const status{lispb::codegen::find_test_type(graph, "packed", "Status")};
    ASSERT_TRUE(health.has_value());
    ASSERT_TRUE(status.has_value());

    auto const& packed{graph.definition(*status)};
    ASSERT_EQ(semantic_packed.segments_of(packed).size(), 1U);
    auto const& field{semantic_packed.segments_of(packed).front()};
    EXPECT_EQ(occurrences.type[occurrences.index(
                  semantic_packed_fields.semantic_type[semantic_packed_fields.index(field)])],
              *health);
    EXPECT_EQ(semantic_packed_fields.bit_width[semantic_packed_fields.index(field)], 12U);
    EXPECT_TRUE(semantic_packed_fields.bit_width_auto[semantic_packed_fields.index(field)]);
    EXPECT_EQ(semantic_packed_fields.minimum_value[semantic_packed_fields.index(field)],
              lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(semantic_packed_fields.maximum_value[semantic_packed_fields.index(field)],
              lispb::codegen::PackedIntegerValue{1000});
    ASSERT_EQ(semantic_packed_fields.named_codes[semantic_packed_fields.index(field)].size(), 1U);
    EXPECT_EQ(
        text(storage,
             packed_named_codes.name[packed_named_codes.index(
                 semantic_declarations
                     .items(semantic_packed_fields.named_codes[semantic_packed_fields.index(field)])
                     .front())]),
        "Invalid");
    EXPECT_TRUE(packed_named_codes.sentinel[packed_named_codes.index(
        semantic_declarations
            .items(semantic_packed_fields.named_codes[semantic_packed_fields.index(field)])
            .front())]);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*status), *health),
              graph.dependencies_of(*status).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*health), *status), graph.users_of(*health).end());
}

TEST(SemanticTypeGraph, RetainsPackedPhysicalOrdering) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] = strings.intern("wire");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Wire.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_packed_values{packed_values.append()};
                    auto const new_packed_values_row{packed_values.index(new_packed_values)};
                    packed_values.name[new_packed_values_row] = strings.intern("Header");
                    packed_values.storage_type[new_packed_values_row] =
                        lispb::codegen::make_test_type_ref(context, "std::uint32_t");
                    packed_values.segments[new_packed_values_row] =
                        declarations.range<lispb::codegen::PackedSegmentId>({[&] {
                            auto const new_packed_fields{packed_fields.append()};
                            auto const new_packed_fields_row{
                                packed_fields.index(new_packed_fields)};
                            packed_fields.name[new_packed_fields_row] = strings.intern("kind");
                            packed_fields.type[new_packed_fields_row] =
                                lispb::codegen::make_test_type_ref(context, "std::uint8_t");
                            packed_fields.bits[new_packed_fields_row] = 8;
                            return new_packed_fields;
                        }()});
                    packed_values.byte_order[new_packed_values_row] =
                        lispb::codegen::PackedByteOrder::big_endian;
                    packed_values.bit_order[new_packed_values_row] =
                        lispb::codegen::PackedBitOrder::most_significant_first;
                    return new_packed_values;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& semantic_packed{graph.definitions().packed};
    auto const header{lispb::codegen::find_test_type(graph, "wire", "Header")};
    ASSERT_TRUE(header.has_value());
    auto const& packed{graph.definition(*header)};
    EXPECT_EQ(semantic_packed.byte_order[semantic_packed.index(packed)],
              lispb::codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(semantic_packed.bit_order[semantic_packed.index(packed)],
              lispb::codegen::PackedBitOrder::most_significant_first);
}

TEST(SemanticTypeGraph, ResolvesSoaColumnsAndEquivalentValues) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
    auto const& soas{graph.definitions().soas};
    auto const& soa_columns{graph.definitions().soa_columns};
    auto const& occurrences{graph.occurrences()};
    auto const entity_type{
        lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    auto const history{
        lispb::codegen::find_test_type(graph, "native_entity_history", "EntityHistory")};
    auto const vectors{lispb::codegen::find_test_type(graph, "native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(history.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& history_type{graph.definition(*history)};
    auto const entity_types{
        std::ranges::find(soas.columns_of(history_type), std::string{"entity_types"}, [&](auto id) {
            return text(storage, soa_columns.name[soa_columns.index(id)]);
        })};
    ASSERT_NE(entity_types, soas.columns_of(history_type).end());
    EXPECT_EQ(
        occurrences
            .type[occurrences.index(soa_columns.semantic_type[soa_columns.index(*entity_types)])],
        *entity_type);

    auto const& vector_type{graph.definition(*vectors)};
    auto const vector_equivalent{graph.find_registered(storage.strings.find("native_vector_3f"))};
    ASSERT_TRUE(vector_equivalent.has_value());
    ASSERT_TRUE(lispb::schema::is_valid(soas.equivalent_type[soas.index(vector_type)]));
    EXPECT_EQ(occurrences.type[occurrences.index(soas.equivalent_type[soas.index(vector_type)])],
              *vector_equivalent);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*vectors), *vector_equivalent),
              graph.dependencies_of(*vectors).end());
    ASSERT_EQ(soas.columns_of(vector_type).size(), 3U);
    for (auto const column_id : soas.columns_of(vector_type)) {
        auto const column{column_id};
        EXPECT_EQ(text(storage,
                       graph.cpp_spelling(occurrences.type[occurrences.index(
                           soa_columns.semantic_type[soa_columns.index(column)])])),
                  "float");
    }
}

TEST(SemanticTypeGraph, RejectsNestedMembersWithoutDeclaredSoaTypes) {
    for (auto const type : {"Missing", "int", "Record", "Child*", "Child const"}) {
        lispb::codegen::CompilerTestContext context;
        auto const manifest{lispb::codegen::parse_test_manifest(
            context,
            std::string{"(module test :header \"Test.h\" "
                        "(record Record (member value float)) "
                        "(struct Child (member values array float)) "
                        "(struct Parent (member child nested \""} +
                type + "\")))")};
        EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
    }
}

TEST(SemanticTypeGraph, ResolvesNestedSoaThroughRegisteredCrossModuleType) {
    lispb::codegen::CompilerTestContext context;
    auto const child_type{context.storage.cpp_types.intern("first::Child")};
    auto const manifest{lispb::codegen::parse_test_manifest(context,
                                                            R"(
        (module first :header "First.h" :namespace first
          (struct Child (member values array float)))
        (module second :header "Second.h" :namespace second
          (struct Child (member values array int))
          (struct Parent
            (member child nested @first_child)
            (function first float :const true
              :body #cpp{return $column(child.values)[0];}cpp#))))",
                                                            {{"first_child", child_type}})};
    auto const graph{resolve_type_graph(manifest)};
    auto const child{lispb::codegen::find_test_type(graph, "first", "Child").value()};
    auto const parent{lispb::codegen::find_test_type(graph, "second", "Parent").value()};
    auto const columns{graph.definitions().soas.columns_of(graph.definition(parent))};
    ASSERT_EQ(columns.size(), 1);
    auto const& members{graph.definitions().soa_columns};
    auto const row{members.index(columns.front())};
    EXPECT_EQ(members.kind[row], lispb::codegen::SoaMemberKind::nested);
    auto const& occurrences{graph.occurrences()};
    EXPECT_EQ(occurrences.type[occurrences.index(members.semantic_type[row])], child);
}

TEST(SemanticTypeGraph, ResolvesSoaColumnRelationshipsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& soas_schemas{declarations.soas};
    auto& soa_members{declarations.soa_members};

    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("tables");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Tables.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_soas{soas_schemas.append()};
                         auto const new_soas_row{soas_schemas.index(new_soas)};
                         soas_schemas.name[new_soas_row] = strings.intern("Target");
                         soas_schemas.members[new_soas_row] =
                             declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{soa_members.append()};
                                 auto const new_soa_members_row{soa_members.index(new_soa_members)};
                                 soa_members.name[new_soa_members_row] = strings.intern("values");
                                 soa_members.kind[new_soa_members_row] =
                                     lispb::codegen::SoaMemberKind::array;
                                 soa_members.type[new_soa_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "float");
                                 soa_members.relationship[new_soa_members_row] = std::nullopt;
                                 return new_soa_members;
                             }()});
                         return new_soas;
                     }(),
                     [&] {
                         auto const new_soas{soas_schemas.append()};
                         auto const new_soas_row{soas_schemas.index(new_soas)};
                         soas_schemas.name[new_soas_row] = strings.intern("User");
                         soas_schemas.members[new_soas_row] =
                             declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{soa_members.append()};
                                 auto const new_soa_members_row{soa_members.index(new_soa_members)};
                                 soa_members.name[new_soa_members_row] =
                                     strings.intern("identifiers");
                                 soa_members.kind[new_soa_members_row] =
                                     lispb::codegen::SoaMemberKind::array;
                                 soa_members.type[new_soa_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "std::uint32_t");
                                 soa_members.relationship[new_soa_members_row] = [&] {
                                     auto const new_semantic_relations{semantic_relations.append()};
                                     semantic_relations
                                         .kind[semantic_relations.index(new_semantic_relations)] =
                                         lispb::codegen::SemanticRelationKind::index_into;
                                     semantic_relations
                                         .target[semantic_relations.index(new_semantic_relations)] =
                                         lispb::codegen::make_test_type_ref(context, "Target");
                                     semantic_relations
                                         .unit[semantic_relations.index(new_semantic_relations)] =
                                         std::nullopt;
                                     return new_semantic_relations;
                                 }();
                                 return new_soa_members;
                             }()});
                         return new_soas;
                     }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& soas{graph.definitions().soas};
    auto const& soa_columns{graph.definitions().soa_columns};
    auto const& occurrences{graph.occurrences()};
    auto const target{lispb::codegen::find_test_type(graph, "tables", "Target")};
    auto const user{lispb::codegen::find_test_type(graph, "tables", "User")};
    ASSERT_TRUE(target.has_value());
    ASSERT_TRUE(user.has_value());
    auto const& soa{graph.definition(*user)};
    ASSERT_EQ(soas.columns_of(soa).size(), 1U);
    ASSERT_TRUE(lispb::schema::is_valid(
        soa_columns.relationship[soa_columns.index(soas.columns_of(soa)[0])]));
    EXPECT_EQ(graph.relationships().kind[graph.relationships().index(
                  soa_columns.relationship[soa_columns.index(soas.columns_of(soa)[0])])],
              lispb::codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(
        occurrences.type[occurrences.index(graph.relationships().target[graph.relationships().index(
            soa_columns.relationship[soa_columns.index(soas.columns_of(soa)[0])])])],
        *target);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*user), *target),
              graph.dependencies_of(*user).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*target), *user), graph.users_of(*target).end());
}

TEST(SemanticTypeGraph, ResolvesRecordMembersFixedArraysAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records_schemas{declarations.records};
    auto& record_members_schemas{declarations.record_members};

    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("records");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Records.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_records{records_schemas.append()};
                         auto const new_records_row{records_schemas.index(new_records)};
                         records_schemas.name[new_records_row] = strings.intern("Position");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                 auto const new_record_members{record_members_schemas.append()};
                                 auto const new_record_members_row{
                                     record_members_schemas.index(new_record_members)};
                                 record_members_schemas.name[new_record_members_row] =
                                     strings.intern("x");
                                 record_members_schemas.type[new_record_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "float");
                                 return new_record_members;
                             }()});
                         return new_records;
                     }(),
                     [&] {
                         auto const new_records{records_schemas.append()};
                         auto const new_records_row{records_schemas.index(new_records)};
                         records_schemas.name[new_records_row] = strings.intern("Table");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                 auto const new_record_members{record_members_schemas.append()};
                                 auto const new_record_members_row{
                                     record_members_schemas.index(new_record_members)};
                                 record_members_schemas.name[new_record_members_row] =
                                     strings.intern("value");
                                 record_members_schemas.type[new_record_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "float");
                                 return new_record_members;
                             }()});
                         return new_records;
                     }(),
                     [&] {
                         auto const new_records{records_schemas.append()};
                         auto const new_records_row{records_schemas.index(new_records)};
                         records_schemas.name[new_records_row] = strings.intern("Path");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                 auto const new_record_members{record_members_schemas.append()};
                                 auto const new_record_members_row{
                                     record_members_schemas.index(new_record_members)};
                                 record_members_schemas.name[new_record_members_row] =
                                     strings.intern("points");
                                 record_members_schemas.type[new_record_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "Position");
                                 record_members_schemas.count[new_record_members_row] = 8;
                                 record_members_schemas.relationship[record_members_schemas.index(
                                     new_record_members)] = [&] {
                                     auto const new_semantic_relations{semantic_relations.append()};
                                     semantic_relations
                                         .kind[semantic_relations.index(new_semantic_relations)] =
                                         lispb::codegen::SemanticRelationKind::member_of;
                                     semantic_relations
                                         .target[semantic_relations.index(new_semantic_relations)] =
                                         lispb::codegen::make_test_type_ref(context, "Table");
                                     semantic_relations
                                         .unit[semantic_relations.index(new_semantic_relations)] =
                                         std::nullopt;
                                     return new_semantic_relations;
                                 }();
                                 return new_record_members;
                             }()});
                         return new_records;
                     }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& records{graph.definitions().records};
    auto const& record_members{graph.definitions().record_members};
    auto const& occurrences{graph.occurrences()};
    auto const position{lispb::codegen::find_test_type(graph, "records", "Position")};
    auto const table{lispb::codegen::find_test_type(graph, "records", "Table")};
    auto const path{lispb::codegen::find_test_type(graph, "records", "Path")};
    ASSERT_TRUE(position.has_value());
    ASSERT_TRUE(table.has_value());
    ASSERT_TRUE(path.has_value());
    auto const& record{graph.definition(*path)};
    ASSERT_EQ(records.members_of(record).size(), 1U);
    EXPECT_EQ(
        occurrences.type[occurrences.index(
            record_members.semantic_type[record_members.index(records.members_of(record)[0])])],
        *position);
    EXPECT_EQ(record_members.count[record_members.index(records.members_of(record)[0])], 8);
    ASSERT_TRUE(lispb::schema::is_valid(
        record_members.relationship[record_members.index(records.members_of(record)[0])]));
    EXPECT_EQ(
        graph.relationships().kind[graph.relationships().index(
            record_members.relationship[record_members.index(records.members_of(record)[0])])],
        lispb::codegen::SemanticRelationKind::member_of);
    EXPECT_EQ(
        occurrences.type[occurrences.index(graph.relationships().target[graph.relationships().index(
            record_members.relationship[record_members.index(records.members_of(record)[0])])])],
        *table);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*path), *position),
              graph.dependencies_of(*path).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*position), *path), graph.users_of(*position).end());
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*path), *table),
              graph.dependencies_of(*path).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*table), *path), graph.users_of(*table).end());
}

TEST(SemanticTypeGraph, RejectsDirectAndIndirectByValueRecordCycles) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.record_members};

    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto module{[&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[new_normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("records");
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern("Records.h");
            return new_module_settings_store;
        }();
        normal_modules.declarations[new_normal_modules_row] =
            declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_records{records.append()};
                auto const new_records_row{records.index(new_records)};
                records.name[new_records_row] = strings.intern("Record");
                records.members[new_records_row] =
                    declarations.range<lispb::codegen::RecordMemberId>({[&] {
                        auto const new_record_members{record_members.append()};
                        auto const new_record_members_row{record_members.index(new_record_members)};
                        record_members.name[new_record_members_row] = strings.intern("self");
                        record_members.type[new_record_members_row] =
                            lispb::codegen::make_test_type_ref(context, "Record");
                        return new_record_members;
                    }()});
                return new_records;
            }()});
        return new_normal_modules;
    }()};
    auto manifest{lispb::codegen::Manifest{lispb::codegen::manifest_schema_version,
                                           lispb::codegen::make_test_registry(context),
                                           declarations,
                                           {module}}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    normal_modules.declarations[normal_modules.index(module)] = storage.declarations.range(
        {[&] {
             auto const new_records{records.append()};
             records.name[records.index(new_records)] = strings.intern("First");
             records.members[records.index(new_records)] =
                 declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{record_members.append()};
                     auto const new_record_members_row{record_members.index(new_record_members)};
                     record_members.name[new_record_members_row] = strings.intern("second");
                     record_members.type[new_record_members_row] =
                         lispb::codegen::make_test_type_ref(context, "Second");
                     return new_record_members;
                 }()});
             return new_records;
         }(),
         [&] {
             auto const new_records{records.append()};
             auto const new_records_row{records.index(new_records)};
             records.name[new_records_row] = strings.intern("Second");
             records.members[new_records_row] =
                 declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{record_members.append()};
                     auto const new_record_members_row{record_members.index(new_record_members)};
                     record_members.name[new_record_members_row] = strings.intern("first");
                     record_members.type[new_record_members_row] =
                         lispb::codegen::make_test_type_ref(context, "First");
                     return new_record_members;
                 }()});
             return new_records;
         }()});
    manifest.modules = {module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, PointerSelfReferenceIsNotByValueRecursion) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.record_members};
    auto& type_refs{declarations.type_refs};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("records");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Records.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_records{records.append()};
                    auto const new_records_row{records.index(new_records)};
                    records.name[new_records_row] = strings.intern("Node");
                    records.members[new_records_row] =
                        declarations.range<lispb::codegen::RecordMemberId>({[&] {
                            auto const new_record_members{record_members.append()};
                            auto const new_record_members_row{
                                record_members.index(new_record_members)};
                            record_members.name[new_record_members_row] = strings.intern("next");
                            record_members.type[new_record_members_row] = [&] {
                                auto const new_type_refs{type_refs.append()};
                                auto const new_type_refs_row{type_refs.index(new_type_refs)};
                                type_refs.name[new_type_refs_row] = strings.intern("Node");
                                type_refs.suffix[new_type_refs_row] = strings.intern("*");
                                return new_type_refs;
                            }();
                            return new_record_members;
                        }()});
                    return new_records;
                }()});
            return new_normal_modules;
        }()}};
    EXPECT_NO_THROW(static_cast<void>(resolve_type_graph(manifest)));
}

TEST(SemanticTypeGraph, ClassifiesBoundedUsesWithoutDiscardingSpelling) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& type_refs{declarations.type_refs};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& semantic_strings{storage.strings};
    auto const pointer{lispb::codegen::resolve_type_use(
        [&] {
            auto const new_type_refs{type_refs.append()};
            auto const new_type_refs_row{type_refs.index(new_type_refs)};
            type_refs.name[new_type_refs_row] = strings.intern("float");
            type_refs.suffix[new_type_refs_row] = strings.intern(" const* const*");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_EQ(storage.cpp_types.spelling(pointer.cpp_type), "float const* const*");
    EXPECT_EQ(pointer.physical.form, lispb::codegen::PhysicalTypeForm::object_pointer);
    EXPECT_EQ(semantic_strings.view(pointer.physical.object_spelling), "float");
    EXPECT_FALSE(pointer.physical.contains_value());
    auto const value{lispb::codegen::resolve_type_use(
        [&] {
            auto const new_type_refs{type_refs.append()};
            auto const new_type_refs_row{type_refs.index(new_type_refs)};
            type_refs.name[new_type_refs_row] = strings.intern("float");
            type_refs.suffix[new_type_refs_row] = strings.intern(" const");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_TRUE(value.physical.contains_value());
    for (auto const suffix : {"&", "&&", "* const&"}) {
        auto const reference{lispb::codegen::resolve_type_use(
            [&] {
                auto const new_type_refs{type_refs.append()};
                auto const new_type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[new_type_refs_row] = strings.intern("float");
                type_refs.suffix[new_type_refs_row] = strings.intern(suffix);
                return new_type_refs;
            }(),
            lispb::codegen::make_test_registry(context))};
        EXPECT_FALSE(reference.physical.contains_value());
        EXPECT_NE(reference.physical.form, lispb::codegen::PhysicalTypeForm::value);
    }
    for (auto const suffix : {"[4]", "(*)()", " Owner::*", "*[]"}) {
        auto const unsupported{lispb::codegen::resolve_type_use(
            [&] {
                auto const new_type_refs{type_refs.append()};
                auto const new_type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[new_type_refs_row] = strings.intern("float");
                type_refs.suffix[new_type_refs_row] = strings.intern(suffix);
                return new_type_refs;
            }(),
            lispb::codegen::make_test_registry(context))};
        EXPECT_EQ(unsupported.physical.form, lispb::codegen::PhysicalTypeForm::unsupported)
            << suffix;
        EXPECT_FALSE(
            lispb::codegen::physical_type_diagnostic(storage.cpp_types, unsupported.cpp_type)
                .empty());
    }
    auto const nested{lispb::codegen::resolve_type_use(
        [&] {
            auto const new_type_refs{type_refs.append()};
            auto const new_type_refs_row{type_refs.index(new_type_refs)};
            type_refs.name[new_type_refs_row] = strings.intern("Owner");
            type_refs.suffix[new_type_refs_row] = strings.intern("");
            type_refs.nested[new_type_refs_row] = strings.intern("Inner");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_EQ(semantic_strings.view(nested.physical.object_spelling), "Owner::Inner");
    EXPECT_FALSE(nested.physical.contains_value());
}

TEST(SemanticTypeGraph, ResolvesRawUnionAlternativesAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& unions_schemas{declarations.unions};
    auto& union_alternatives_schemas{declarations.union_alternatives};

    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("payloads");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Payloads.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_unions{unions_schemas.append()};
                    auto const new_unions_row{unions_schemas.index(new_unions)};
                    unions_schemas.name[new_unions_row] = strings.intern("Payload");
                    unions_schemas.alternatives[new_unions_row] =
                        declarations.range<lispb::codegen::UnionAlternativeId>(
                            {[&] {
                                 auto const new_union_alternatives{
                                     union_alternatives_schemas.append()};
                                 auto const new_union_alternatives_row{
                                     union_alternatives_schemas.index(new_union_alternatives)};
                                 union_alternatives_schemas.name[new_union_alternatives_row] =
                                     strings.intern("identifier");
                                 union_alternatives_schemas.type[new_union_alternatives_row] =
                                     lispb::codegen::make_test_type_ref(context, "std::uint32_t");
                                 return new_union_alternatives;
                             }(),
                             [&] {
                                 auto const new_union_alternatives{
                                     union_alternatives_schemas.append()};
                                 auto const new_union_alternatives_row{
                                     union_alternatives_schemas.index(new_union_alternatives)};
                                 union_alternatives_schemas.name[new_union_alternatives_row] =
                                     strings.intern("bytes");
                                 union_alternatives_schemas.type[new_union_alternatives_row] =
                                     lispb::codegen::make_test_type_ref(context, "std::uint8_t");
                                 union_alternatives_schemas.count[new_union_alternatives_row] = 12;
                                 return new_union_alternatives;
                             }()});
                    unions_schemas.export_specifier[new_unions_row] = {};
                    return new_unions;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const& unions{graph.definitions().unions};
    auto const& union_alternatives{graph.definitions().union_alternatives};
    auto const& occurrences{graph.occurrences()};
    auto const payload{lispb::codegen::find_test_type(graph, "payloads", "Payload")};
    ASSERT_TRUE(payload.has_value());
    auto const& union_type{graph.definition(*payload)};
    ASSERT_EQ(unions.alternatives_of(union_type).size(), 2U);
    EXPECT_EQ(text(storage,
                   union_alternatives
                       .name[union_alternatives.index(unions.alternatives_of(union_type)[0])]),
              "identifier");
    EXPECT_EQ(text(storage,
                   graph.cpp_spelling(occurrences.type[occurrences.index(
                       union_alternatives.semantic_type[union_alternatives.index(
                           unions.alternatives_of(union_type)[0])])])),
              "std::uint32_t");
    EXPECT_EQ(
        union_alternatives.count[union_alternatives.index(unions.alternatives_of(union_type)[1])],
        12);
    EXPECT_EQ(graph.dependencies_of(*payload).size(), 2U);
}

TEST(SemanticTypeGraph, RejectsUnionAndMixedAggregateCycles) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& unions{declarations.unions};
    auto& union_alternatives{declarations.union_alternatives};

    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.record_members};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& semantic_declarations{storage.declarations};
    auto& semantic_strings{storage.strings};
    auto const union_schema{unions.append()};
    auto const union_schema_row{unions.index(union_schema)};
    unions.name[union_schema_row] = strings.intern("Payload");
    unions.alternatives[union_schema_row] =
        declarations.range<lispb::codegen::UnionAlternativeId>({[&] {
            auto const new_union_alternatives{union_alternatives.append()};
            auto const new_union_alternatives_row{union_alternatives.index(new_union_alternatives)};
            union_alternatives.name[new_union_alternatives_row] = strings.intern("self");
            union_alternatives.type[new_union_alternatives_row] =
                lispb::codegen::make_test_type_ref(context, "Payload");
            return new_union_alternatives;
        }()});
    unions.export_specifier[union_schema_row] = {};
    auto const union_module{normal_modules.append()};
    auto const union_module_row{normal_modules.index(union_module)};
    normal_modules.settings[union_module_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("payloads");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Payloads.h");
        module_settings_store.namespace_name[new_module_settings_store_row] =
            strings.intern("payloads");
        return new_module_settings_store;
    }();
    normal_modules.declarations[union_module_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({union_schema});
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::make_test_registry(context),
                                      declarations,
                                      {union_module}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    unions.alternatives[union_schema_row] = semantic_declarations.range({[&] {
        auto const new_union_alternatives{union_alternatives.append()};
        union_alternatives.name[union_alternatives.index(new_union_alternatives)] =
            semantic_strings.intern("record");
        union_alternatives.type[union_alternatives.index(new_union_alternatives)] =
            lispb::codegen::make_test_type_ref(context, "records::Record");
        return new_union_alternatives;
    }()});
    normal_modules.declarations[union_module_row] = semantic_declarations.range({union_schema});
    auto const record_schema{records.append()};
    auto const record_schema_row{records.index(record_schema)};
    records.name[record_schema_row] = strings.intern("Record");
    records.members[record_schema_row] = declarations.range<lispb::codegen::RecordMemberId>({[&] {
        auto const new_record_members{record_members.append()};
        auto const new_record_members_row{record_members.index(new_record_members)};
        record_members.name[new_record_members_row] = strings.intern("payload");
        record_members.type[new_record_members_row] =
            lispb::codegen::make_test_type_ref(context, "payloads::Payload");
        return new_record_members;
    }()});
    records.export_specifier[record_schema_row] = {};
    auto const record_module{normal_modules.append()};
    normal_modules.settings[normal_modules.index(record_module)] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("records");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Records.h");
        module_settings_store.namespace_name[new_module_settings_store_row] =
            strings.intern("records");
        return new_module_settings_store;
    }();
    normal_modules.declarations[normal_modules.index(record_module)] =
        declarations.range<lispb::codegen::DeclarationHandle>({record_schema});
    manifest.modules = {record_module, union_module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesTaggedUnionDiscriminantMappingsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& tagged_unions_schemas{declarations.tagged_unions};
    auto& tagged_union_alternatives_schemas{declarations.tagged_union_alternatives};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto const enums{normal_modules.append()};
    auto const enums_row{normal_modules.index(enums)};
    normal_modules.settings[enums_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("events");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Events.h");
        module_settings_store.namespace_name[new_module_settings_store_row] =
            strings.intern("events");
        return new_module_settings_store;
    }();
    normal_modules.declarations[enums_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_enums{enums_schemas.append()};
            auto const new_enums_row{enums_schemas.index(new_enums)};
            enums_schemas.name[new_enums_row] = strings.intern("EventKind");
            enums_schemas.underlying_type[new_enums_row] =
                lispb::codegen::make_test_type_ref(context, "std::uint8_t");
            enums_schemas.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     enumerators.name[enumerators.index(new_enumerators)] = strings.intern("Spawn");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     enumerators.name[enumerators.index(new_enumerators)] =
                         strings.intern("Damage");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Invalid");
                     enumerators.sentinel[new_enumerators_row] = true;
                     return new_enumerators;
                 }()});
            return new_enums;
        }()});
    auto const unions{normal_modules.append()};
    auto const unions_row{normal_modules.index(unions)};
    normal_modules.settings[unions_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("payloads");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Payloads.h");
        module_settings_store.namespace_name[new_module_settings_store_row] =
            strings.intern("payloads");
        return new_module_settings_store;
    }();
    normal_modules
        .declarations[unions_row] = declarations.range<lispb::codegen::DeclarationHandle>({[&] {
        auto const new_tagged_unions{tagged_unions_schemas.append()};
        auto const new_tagged_unions_row{tagged_unions_schemas.index(new_tagged_unions)};
        tagged_unions_schemas.name[new_tagged_unions_row] = strings.intern("Event");
        tagged_unions_schemas.discriminant[new_tagged_unions_row] =
            lispb::codegen::make_test_type_ref(context, "events::EventKind");
        tagged_unions_schemas.alternatives[new_tagged_unions_row] =
            declarations.range<lispb::codegen::TaggedUnionAlternativeId>(
                {[&] {
                     auto const new_tagged_union_alternatives{
                         tagged_union_alternatives_schemas.append()};
                     auto const new_tagged_union_alternatives_row{
                         tagged_union_alternatives_schemas.index(new_tagged_union_alternatives)};
                     tagged_union_alternatives_schemas.name[new_tagged_union_alternatives_row] =
                         strings.intern("spawn");
                     tagged_union_alternatives_schemas.type[new_tagged_union_alternatives_row] =
                         lispb::codegen::make_test_type_ref(context, "std::uint32_t");
                     tagged_union_alternatives_schemas.tag[new_tagged_union_alternatives_row] =
                         strings.intern("Spawn");
                     return new_tagged_union_alternatives;
                 }(),
                 [&] {
                     auto const new_tagged_union_alternatives{
                         tagged_union_alternatives_schemas.append()};
                     auto const new_tagged_union_alternatives_row{
                         tagged_union_alternatives_schemas.index(new_tagged_union_alternatives)};
                     tagged_union_alternatives_schemas.name[new_tagged_union_alternatives_row] =
                         strings.intern("damage");
                     tagged_union_alternatives_schemas.type[new_tagged_union_alternatives_row] =
                         lispb::codegen::make_test_type_ref(context, "std::uint16_t");
                     tagged_union_alternatives_schemas.count[new_tagged_union_alternatives_row] = 4;
                     tagged_union_alternatives_schemas.tag[new_tagged_union_alternatives_row] =
                         strings.intern("Damage");
                     return new_tagged_union_alternatives;
                 }()});
        return new_tagged_unions;
    }()});
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::make_test_registry(context),
                                      declarations,
                                      {enums, unions}};

    auto graph{resolve_type_graph(manifest)};
    auto const event{lispb::codegen::find_test_type(graph, "payloads", "Event")};
    auto const kind{lispb::codegen::find_test_type(graph, "events", "EventKind")};
    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(kind.has_value());
    auto const& tagged{graph.definition(*event)};
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .tagged_unions.discriminant[graph.definitions().tagged_unions.index(tagged)])],
        *kind);
    ASSERT_EQ(graph.definitions().tagged_unions.alternatives_of(tagged).size(), 2U);
    EXPECT_EQ(
        text(storage,
             graph.definitions()
                 .tagged_union_alternatives.tag[graph.definitions().tagged_union_alternatives.index(
                     graph.definitions().tagged_unions.alternatives_of(tagged)[0])]),
        "Spawn");
    EXPECT_EQ(
        graph.definitions()
            .tagged_union_alternatives.count[graph.definitions().tagged_union_alternatives.index(
                graph.definitions().tagged_unions.alternatives_of(tagged)[1])],
        4);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*event), *kind),
              graph.dependencies_of(*event).end());
    EXPECT_EQ(graph.users_of(*kind).front(), *event);

    for (auto const tag : {"Missing", "Invalid"}) {
        auto invalid{lispb::codegen::parse_test_manifest(
            context,
            "(module payloads :header \"Payloads.h\" :namespace payloads (tagged-union Event "
            ":discriminant events::EventKind (alternative missing std::uint32_t :tag " +
                std::string{tag} + ")))")};
        manifest.modules = {enums, invalid.modules.front()};
        EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
    }
    auto invalid{lispb::codegen::parse_test_manifest(context, R"(
        (module payloads :header "Payloads.h" :namespace payloads
          (tagged-union Event :discriminant std::uint8_t
            (alternative missing std::uint32_t :tag Invalid))))")};
    manifest.modules = {enums, invalid.modules.front()};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, KeepsUnknownRegisteredTypesAsExternalLeaves) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"native_handle",
              storage.cpp_types.add(
                  "NativeHandle",
                  {storage.cpp_types.dependency("NativeHandle", "native/handle.h")})}}),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] = strings.intern("mode");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Mode.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{enums.append()};
                    auto const new_enums_row{enums.index(new_enums)};
                    enums.name[new_enums_row] = strings.intern("Mode");
                    enums.underlying_type[new_enums_row] =
                        lispb::codegen::make_test_type_ref(context, "uint8");
                    enums.values[new_enums_row] =
                        declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{declarations.enumerators.append()};
                            declarations.enumerators
                                .name[declarations.enumerators.index(new_enumerators)] =
                                strings.intern("Value");
                            return new_enumerators;
                        }()});
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const& externals{graph.definitions().externals};
    auto const handle{graph.find_registered(storage.strings.find("native_handle"))};
    ASSERT_TRUE(handle.has_value());
    auto const external{
        ((graph.definition(*handle)).kind() == lispb::schema::TypeDefinitionKind::external
             ? std::optional{graph.definition(*handle)}
             : std::nullopt)};
    ASSERT_TRUE(external.has_value());
    EXPECT_EQ(storage.cpp_types.spelling(externals.cpp_type[externals.index(*external)]),
              "NativeHandle");
    ASSERT_FALSE(
        storage.cpp_types.dependencies(externals.cpp_type[externals.index(*external)]).empty());
    EXPECT_EQ(
        *storage.cpp_types.dependency_header(
            storage.cpp_types.dependencies(externals.cpp_type[externals.index(*external)]).front()),
        "native/handle.h");
    EXPECT_TRUE(graph.dependencies_of(*handle).empty());
}

TEST(SemanticTypeGraph, RejectsAmbiguousRegisteredDeclarationBindings) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto enum_module = [&](std::string module_name, std::string const& header) {
        return [&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern(std::move(module_name));
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern(header);
                module_settings_store.namespace_name[new_module_settings_store_row] =
                    strings.intern("project");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{enums.append()};
                    auto const new_enums_row{enums.index(new_enums)};
                    enums.name[new_enums_row] = strings.intern("Mode");
                    enums.underlying_type[new_enums_row] =
                        lispb::codegen::make_test_type_ref(context, "uint8");
                    enums.values[new_enums_row] =
                        declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{declarations.enumerators.append()};
                            declarations.enumerators
                                .name[declarations.enumerators.index(new_enumerators)] =
                                strings.intern("Value");
                            return new_enumerators;
                        }()});
                    return new_enums;
                }()});
            return new_normal_modules;
        }();
    };
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context,
                                           {{"mode", storage.cpp_types.add("project::Mode")}}),
        declarations,
        {enum_module("first", "First.h"), enum_module("second", "Second.h")}};

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsInvalidAndDuplicateDeclaredIdentities) {
    lispb::codegen::CompilerTestContext context;
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& normal_modules_schemas{storage.declarations.normal_modules};
    auto& module_settings_store_schemas{storage.declarations.module_settings_store};
    auto& enums_schemas{storage.declarations.enums};

    auto& enumerators_schemas{storage.declarations.enumerators};
    auto& semantic_declarations{storage.declarations};
    auto const make_module = [&](std::string name,
                                 std::vector<lispb::codegen::DeclarationHandle> const& enums) {
        std::vector<lispb::codegen::DeclarationHandle> declarations;
        for (auto const& value : enums) {
            declarations.push_back(value);
        }
        return [&] {
            auto const new_normal_modules{normal_modules_schemas.append()};
            normal_modules_schemas.settings[normal_modules_schemas.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{module_settings_store_schemas.append()};
                    module_settings_store_schemas
                        .name[module_settings_store_schemas.index(new_module_settings_store)] =
                        strings.intern(std::move(name));
                    module_settings_store_schemas
                        .header[module_settings_store_schemas.index(new_module_settings_store)] =
                        strings.intern("Types.h");
                    return new_module_settings_store;
                }();
            normal_modules_schemas.declarations[normal_modules_schemas.index(new_normal_modules)] =
                semantic_declarations.range(declarations);
            return new_normal_modules;
        }();
    };
    auto const make_enum = [&](std::string name) {
        return [&] {
            auto const new_enums{enums_schemas.append()};
            enums_schemas.name[enums_schemas.index(new_enums)] = strings.intern(std::move(name));
            enums_schemas.underlying_type[enums_schemas.index(new_enums)] =
                lispb::codegen::make_test_type_ref(context, "uint8");
            enums_schemas.values[enums_schemas.index(new_enums)] =
                semantic_declarations.range<lispb::codegen::EnumeratorId>({[&] {
                    auto const new_enumerators{enumerators_schemas.append()};
                    enumerators_schemas.name[enumerators_schemas.index(new_enumerators)] =
                        strings.intern("Value");
                    return new_enumerators;
                }()});
            return new_enums;
        }();
    };

    lispb::codegen::Manifest invalid{lispb::codegen::manifest_schema_version,
                                     lispb::codegen::make_test_registry(context),
                                     semantic_declarations,
                                     {make_module("types", {make_enum("")})}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(invalid)), std::invalid_argument);

    lispb::codegen::Manifest duplicate{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        semantic_declarations,
        {make_module("types", {make_enum("Mode"), make_enum("Mode")})}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(duplicate)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsUnresolvedRegisteredReferences) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] = strings.intern("mode");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Mode.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{enums.append()};
                    auto const new_enums_row{enums.index(new_enums)};
                    enums.name[new_enums_row] = strings.intern("Mode");
                    enums.underlying_type[new_enums_row] =
                        lispb::codegen::make_test_type_ref(context, "@missing");
                    enums.values[new_enums_row] =
                        declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{declarations.enumerators.append()};
                            declarations.enumerators
                                .name[declarations.enumerators.index(new_enumerators)] =
                                strings.intern("Value");
                            return new_enumerators;
                        }()});
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ReferenceLookupMatchesResolverBindings) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records_schemas{declarations.records};
    auto& record_members_schemas{declarations.record_members};
    auto& type_refs{declarations.type_refs};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};

    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context, {{"bar", storage.cpp_types.add("game::Bar")}}),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules_schemas.append()};
             auto const new_normal_modules_row{normal_modules_schemas.index(new_normal_modules)};
             normal_modules_schemas.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] = strings.intern("A");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("A.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("game");
                 return new_module_settings_store;
             }();
             normal_modules_schemas.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>(
                     {[&] {
                          auto const new_records{records_schemas.append()};
                          auto const new_records_row{records_schemas.index(new_records)};
                          records_schemas.name[new_records_row] = strings.intern("Foo");
                          records_schemas.members[new_records_row] =
                              declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                  auto const new_record_members{record_members_schemas.append()};
                                  auto const new_record_members_row{
                                      record_members_schemas.index(new_record_members)};
                                  record_members_schemas.name[new_record_members_row] =
                                      strings.intern("value");
                                  record_members_schemas.type[new_record_members_row] =
                                      lispb::codegen::make_test_type_ref(context, "int32");
                                  return new_record_members;
                              }()});
                          return new_records;
                      }(),
                      [&] {
                          auto const new_records{records_schemas.append()};
                          auto const new_records_row{records_schemas.index(new_records)};
                          records_schemas.name[new_records_row] = strings.intern("Bar");
                          records_schemas.members[new_records_row] =
                              declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                  auto const new_record_members{record_members_schemas.append()};
                                  auto const new_record_members_row{
                                      record_members_schemas.index(new_record_members)};
                                  record_members_schemas.name[new_record_members_row] =
                                      strings.intern("value");
                                  record_members_schemas.type[new_record_members_row] =
                                      lispb::codegen::make_test_type_ref(context, "int32");
                                  return new_record_members;
                              }()});
                          return new_records;
                      }(),
                      [&] {
                          auto const new_records{records_schemas.append()};
                          auto const new_records_row{records_schemas.index(new_records)};
                          records_schemas.name[new_records_row] = strings.intern("User");
                          records_schemas.members[new_records_row] = declarations.range<
                              lispb::codegen::RecordMemberId>(
                              {[&] {
                                   auto const new_record_members{record_members_schemas.append()};
                                   auto const new_record_members_row{
                                       record_members_schemas.index(new_record_members)};
                                   record_members_schemas.name[new_record_members_row] =
                                       strings.intern("local");
                                   record_members_schemas.type[new_record_members_row] =
                                       lispb::codegen::make_test_type_ref(context, "Foo");
                                   return new_record_members;
                               }(),
                               [&] {
                                   auto const new_record_members{record_members_schemas.append()};
                                   auto const new_record_members_row{
                                       record_members_schemas.index(new_record_members)};
                                   record_members_schemas.name[new_record_members_row] =
                                       strings.intern("qualified");
                                   record_members_schemas.type[new_record_members_row] =
                                       lispb::codegen::make_test_type_ref(context, "game::Bar");
                                   return new_record_members;
                               }(),
                               [&] {
                                   auto const new_record_members{record_members_schemas.append()};
                                   auto const new_record_members_row{
                                       record_members_schemas.index(new_record_members)};
                                   record_members_schemas.name[new_record_members_row] =
                                       strings.intern("registered");
                                   record_members_schemas.type[new_record_members_row] =
                                       lispb::codegen::make_test_type_ref(context, "@bar");
                                   return new_record_members;
                               }(),
                               [&] {
                                   auto const new_record_members{record_members_schemas.append()};
                                   auto const new_record_members_row{
                                       record_members_schemas.index(new_record_members)};
                                   record_members_schemas.name[new_record_members_row] =
                                       strings.intern("suffixed");
                                   record_members_schemas.type[new_record_members_row] = [&] {
                                       auto const new_type_refs{type_refs.append()};
                                       auto const new_type_refs_row{type_refs.index(new_type_refs)};
                                       type_refs.name[new_type_refs_row] = strings.intern("Foo");
                                       type_refs.suffix[new_type_refs_row] = strings.intern("*");
                                       return new_type_refs;
                                   }();
                                   return new_record_members;
                               }(),
                               [&] {
                                   auto const new_record_members{record_members_schemas.append()};
                                   auto const new_record_members_row{
                                       record_members_schemas.index(new_record_members)};
                                   record_members_schemas.name[new_record_members_row] =
                                       strings.intern("ambiguous");
                                   record_members_schemas.type[new_record_members_row] =
                                       lispb::codegen::make_test_type_ref(context, "game::Foo");
                                   return new_record_members;
                               }()});
                          return new_records;
                      }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules_schemas.append()};
             auto const new_normal_modules_row{normal_modules_schemas.index(new_normal_modules)};
             normal_modules_schemas.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] = strings.intern("B");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("B.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("game");
                 return new_module_settings_store;
             }();
             normal_modules_schemas.declarations[new_normal_modules_row] =
                 declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_records{records_schemas.append()};
                     auto const new_records_row{records_schemas.index(new_records)};
                     records_schemas.name[new_records_row] = strings.intern("Foo");
                     records_schemas.members[new_records_row] =
                         declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{record_members_schemas.append()};
                             auto const new_record_members_row{
                                 record_members_schemas.index(new_record_members)};
                             record_members_schemas.name[new_record_members_row] =
                                 strings.intern("value");
                             record_members_schemas.type[new_record_members_row] =
                                 lispb::codegen::make_test_type_ref(context, "int32");
                             return new_record_members;
                         }()});
                     return new_records;
                 }()});
             return new_normal_modules;
         }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const& semantic_record_members{graph.definitions().record_members};
    auto const& semantic_records{graph.definitions().records};
    auto const& occurrences{graph.occurrences()};
    auto const& store{storage.declarations};
    auto const module{manifest.modules.front()};
    auto const schema{
        store.items(store.normal_modules.declarations[store.normal_modules.index(module)]).back()};
    auto const& user{graph.definition(*lispb::codegen::find_test_type(graph, "A", "User"))};
    auto const bound_member_count{store.records.members[store.records.index(schema)].size() - 1};
    for (std::size_t index{}; index < bound_member_count; ++index) {
        EXPECT_EQ(
            graph.find_reference(store.record_members.type[store.record_members.index(store.items(
                                     store.records.members[store.records.index(schema)])[index])],
                                 module),
            occurrences.type[occurrences.index(
                semantic_record_members.semantic_type[semantic_record_members.index(
                    semantic_records.members_of(user)[index])])]);
    }
    EXPECT_FALSE(
        graph.find_reference(lispb::codegen::make_test_type_ref(context, "game::Foo"), module)
            .has_value());
    EXPECT_EQ(graph.origin(occurrences.type[occurrences.index(
                  semantic_record_members.semantic_type[semantic_record_members.index(
                      semantic_records.members_of(user).back())])]),
              TypeOrigin::cpp_spelling);
    EXPECT_FALSE(graph
                     .find_reference(lispb::codegen::make_test_type_ref(context, "Foo"),
                                     lispb::codegen::ModuleHandle{})
                     .has_value());
    EXPECT_FALSE(
        graph.find_reference(lispb::codegen::make_test_type_ref(context, "@missing"), module)
            .has_value());
}

TEST(SemanticTypeGraph, TreatsRawCppSpellingsAsExplicitExternalLeaves) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& soas_schemas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("values");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Values.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_soas{soas_schemas.append()};
                    auto const new_soas_row{soas_schemas.index(new_soas)};
                    soas_schemas.name[new_soas_row] = strings.intern("Values");
                    soas_schemas.members[new_soas_row] =
                        declarations.range<lispb::codegen::SoaMemberId>({[&] {
                            auto const new_soa_members{soa_members.append()};
                            auto const new_soa_members_row{soa_members.index(new_soa_members)};
                            soa_members.name[new_soa_members_row] = strings.intern("items");
                            soa_members.kind[new_soa_members_row] =
                                lispb::codegen::SoaMemberKind::array;
                            soa_members.type[new_soa_members_row] =
                                lispb::codegen::make_test_type_ref(context, "ExternalValue");
                            return new_soa_members;
                        }()});
                    return new_soas;
                }()});
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const& soas{graph.definitions().soas};
    auto const& soa_columns{graph.definitions().soa_columns};
    auto const& occurrences{graph.occurrences()};
    auto const values{lispb::codegen::find_test_type(graph, "values", "Values")};
    ASSERT_TRUE(values.has_value());
    auto const& soa{graph.definition(*values)};
    ASSERT_EQ(soas.columns_of(soa).size(), 1U);
    EXPECT_EQ(graph.origin(occurrences.type[occurrences.index(
                  soa_columns.semantic_type[soa_columns.index(soas.columns_of(soa).front())])]),
              TypeOrigin::cpp_spelling);
}

TEST(SemanticTypeGraph, SoaOwnerUsesItsLogicalIdentity) {
    lispb::codegen::CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& soas_schemas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& type_refs{declarations.type_refs};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records_schemas{declarations.records};
    auto& record_members_schemas{declarations.record_members};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& semantic_strings{storage.strings};
    auto const rows{soas_schemas.append()};
    auto const rows_row{soas_schemas.index(rows)};
    soas_schemas.name[rows_row] = strings.intern("LogicalRows");
    soas_schemas.members[rows_row] = declarations.range<lispb::codegen::SoaMemberId>({[&] {
        auto const new_soa_members{soa_members.append()};
        auto const new_soa_members_row{soa_members.index(new_soa_members)};
        soa_members.name[new_soa_members_row] = strings.intern("values");
        soa_members.kind[new_soa_members_row] = lispb::codegen::SoaMemberKind::array;
        soa_members.type[new_soa_members_row] =
            lispb::codegen::make_test_type_ref(context, "float");
        return new_soa_members;
    }()});
    soas_schemas.equivalent_type[rows_row] = lispb::codegen::make_test_type_ref(context, "Row");
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"logical", storage.cpp_types.add("model::LogicalRows")},
             {"physical", storage.cpp_types.add("model::LogicalRows")}}),
        declarations,
        {[&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] = strings.intern("model");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Model.h");
                module_settings_store.namespace_name[new_module_settings_store_row] =
                    strings.intern("model");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>(
                    {rows,
                     [&] {
                         auto const new_soas{soas_schemas.append()};
                         auto const new_soas_row{soas_schemas.index(new_soas)};
                         soas_schemas.name[new_soas_row] = strings.intern("Layout");
                         soas_schemas.members[new_soas_row] =
                             declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{soa_members.append()};
                                 auto const new_soa_members_row{soa_members.index(new_soa_members)};
                                 soa_members.name[new_soa_members_row] = strings.intern("xs");
                                 soa_members.kind[new_soa_members_row] =
                                     lispb::codegen::SoaMemberKind::array;
                                 soa_members.type[new_soa_members_row] =
                                     lispb::codegen::make_test_type_ref(context, "float");
                                 return new_soa_members;
                             }()});
                         soas_schemas.layout_only[new_soas_row] = true;
                         return new_soas;
                     }(),
                     [&] {
                         auto const new_records{records_schemas.append()};
                         auto const new_records_row{records_schemas.index(new_records)};
                         records_schemas.name[new_records_row] = strings.intern("Reference");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::RecordMemberId>({[&] {
                                 auto const new_record_members{record_members_schemas.append()};
                                 auto const new_record_members_row{
                                     record_members_schemas.index(new_record_members)};
                                 record_members_schemas.name[new_record_members_row] =
                                     strings.intern("owner");
                                 record_members_schemas.type[new_record_members_row] = [&] {
                                     auto const new_type_refs{type_refs.append()};
                                     auto const new_type_refs_row{type_refs.index(new_type_refs)};
                                     type_refs.name[new_type_refs_row] =
                                         strings.intern("LogicalRows");
                                     type_refs.suffix[new_type_refs_row] = strings.intern("*");
                                     return new_type_refs;
                                 }();
                                 return new_record_members;
                             }()});
                         return new_records;
                     }()});
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const& soas{graph.definitions().soas};
    auto const& record_members{graph.definitions().record_members};
    auto const& records{graph.definitions().records};
    auto const& occurrences{graph.occurrences()};
    auto const id{lispb::codegen::find_test_type(graph, "model", "LogicalRows")};
    ASSERT_TRUE(id);
    EXPECT_EQ(text(storage, graph.name(*id)), "LogicalRows");
    EXPECT_EQ(text(storage, graph.cpp_spelling(*id)), "model::LogicalRows");
    EXPECT_EQ(graph.find_registered(semantic_strings.find("logical")), id);
    EXPECT_EQ(graph.find_registered(semantic_strings.find("physical")), id);
    EXPECT_EQ(graph.find_reference(lispb::codegen::make_test_type_ref(context, "LogicalRows"),
                                   manifest.modules.front()),
              id);
    auto const& soa{graph.definition(*id)};
    ASSERT_TRUE(lispb::schema::is_valid(soas.equivalent_type[soas.index(soa)]));
    EXPECT_EQ(text(storage,
                   graph.name(
                       occurrences.type[occurrences.index(soas.equivalent_type[soas.index(soa)])])),
              "Row");
    EXPECT_TRUE(
        text(storage, graph.cpp_spelling(*lispb::codegen::find_test_type(graph, "model", "Layout")))
            .empty());
    auto const& reference{
        graph.definition(*lispb::codegen::find_test_type(graph, "model", "Reference"))};
    EXPECT_EQ(occurrences.type[occurrences.index(record_members.semantic_type[record_members.index(
                  records.members_of(reference).front())])],
              *id);
    EXPECT_EQ(
        storage.cpp_types.spelling(occurrences.cpp_type[occurrences.index(
            record_members
                .semantic_type[record_members.index(records.members_of(reference).front())])]),
        "LogicalRows*");
}

} // namespace
} // namespace lispb::schema
