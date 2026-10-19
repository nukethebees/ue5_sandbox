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
#include <codegen/schema/soa_backend.h>
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
#include <lispb/schema/soa_source_kind.h>
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
    auto const graph{production_graph(storage)};
    auto const loadout{lispb::codegen::find_test_type(
        graph, "native_player_flight_model_data", "FlightModelLoadout")};
    ASSERT_TRUE(loadout.has_value());
    auto current{*loadout};
    for (auto const& [member_name, expected] :
         {std::pair{"up", "FlightModelProfile"},
          std::pair{"config", "FlightModelConfig"},
          std::pair{"translation", "TranslationAxesConfig"},
          std::pair{"forward", "TranslationAxisConfig"},
          std::pair{"manual", "TranslationChannelConfig"},
          std::pair{"response", "ResponseConfig"},
          std::pair{"second_order", "SecondOrderResponseConfig"}}) {
        auto const& record{graph.definition(current)};
        auto const member{std::ranges::find(
            graph.definitions().records.members_of(record), member_name, [&](auto id) {
                return text(storage,
                            graph.definitions()
                                .record_members.name[graph.definitions().record_members.index(id)]);
            })};
        ASSERT_NE(member, graph.definitions().records.members_of(record).end());
        auto const next{graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .record_members.semantic_type[graph.definitions().record_members.index(*member)])]};
        EXPECT_EQ(graph.origin(next), TypeOrigin::declaration);
        EXPECT_EQ(text(storage, graph.name(next)), expected);
        EXPECT_NE(std::ranges::find(graph.dependencies_of(current), next),
                  graph.dependencies_of(current).end());
        current = next;
    }
    auto const& response{graph.definition(current)};
    EXPECT_EQ(
        text(
            storage,
            graph.definitions().record_members.initializer[graph.definitions().record_members.index(
                graph.definitions().records.members_of(response)[0])]),
        "3.f");
    EXPECT_EQ(
        text(
            storage,
            graph.definitions().record_members.initializer[graph.definitions().record_members.index(
                graph.definitions().records.members_of(response)[1])]),
        "0.5f");
    EXPECT_EQ(graph.find_registered(storage.strings.find("laser_source")),
              lispb::codegen::find_test_type(graph, "native_laser_source", "LaserSource"));
    EXPECT_EQ(graph.find_registered(storage.strings.find("native_index_span")),
              lispb::codegen::find_test_type(graph, "native_index_span", "IndexSpan"));
    EXPECT_EQ(graph.find_registered(storage.strings.find("native_trace_hit")),
              lispb::codegen::find_test_type(graph, "query_results", "TraceHit"));
    auto const frame_index{
        lispb::codegen::find_test_type(graph, "entity_instance_handle", "EntityFrameIndex")};
    ASSERT_TRUE(frame_index.has_value());
    auto const& integer{graph.definition(*frame_index)};
    EXPECT_FALSE(
        graph.definitions()
            .integer_scalars.signedness[graph.definitions().integer_scalars.index(integer)]);
    EXPECT_EQ(graph.definitions()
                  .integer_scalars.bit_width[graph.definitions().integer_scalars.index(integer)],
              18U);
    EXPECT_EQ(
        graph.definitions()
            .integer_scalars.minimum_value[graph.definitions().integer_scalars.index(integer)],
        lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(
        graph.definitions()
            .integer_scalars.maximum_value[graph.definitions().integer_scalars.index(integer)],
        lispb::codegen::PackedIntegerValue{262143});
    EXPECT_FALSE(graph.find_registered(storage.strings.find("native_health_index")).has_value());
    for (auto const& [module, name, column_name, registration] :
         {std::tuple{"native_trace_hits", "TraceHits", "hits", "native_trace_hit"}}) {
        auto const owner{lispb::codegen::find_test_type(graph, module, name)};
        ASSERT_TRUE(owner.has_value()) << module;
        auto const& soa{graph.definition(*owner)};
        auto const column{
            std::ranges::find(graph.definitions().soas.columns_of(soa), column_name, [&](auto id) {
                return text(storage,
                            graph.definitions()
                                .soa_columns.name[graph.definitions().soa_columns.index(id)]);
            })};
        ASSERT_NE(column, graph.definitions().soas.columns_of(soa).end());
        EXPECT_EQ(
            graph.occurrences().type[graph.occurrences().index(
                graph.definitions()
                    .soa_columns.semantic_type[graph.definitions().soa_columns.index(*column)])],
            *graph.find_registered(storage.strings.find(registration)));
    }
}

TEST(SemanticTypeGraph, PreservesEntityTypeEnumSemantics) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
    auto const id{lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    ASSERT_TRUE(id.has_value());

    auto const& node{*id};
    EXPECT_EQ(text(storage, graph.namespace_name(node)), "ioj::sim");
    auto const type{
        ((graph.definition(node)).kind() == lispb::schema::TypeDefinitionKind::enumeration
             ? std::optional{graph.definition(node)}
             : std::nullopt)};
    ASSERT_TRUE(type.has_value());
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions().enums.underlying_type[graph.definitions().enums.index(*type)]));
    EXPECT_EQ(text(storage,
                   graph.cpp_spelling(graph.occurrences().type[graph.occurrences().index(
                       graph.definitions()
                           .enums.underlying_type[graph.definitions().enums.index(*type)])])),
              "std::uint8_t");
    EXPECT_NE(
        std::ranges::find(graph.dependencies_of(*id),
                          graph.occurrences().type[graph.occurrences().index(
                              graph.definitions()
                                  .enums.underlying_type[graph.definitions().enums.index(*type)])]),
        graph.dependencies_of(*id).end());
    EXPECT_FALSE(
        graph.definitions().enums.count[graph.definitions().enums.index(*type)].has_value());
    EXPECT_TRUE(graph.definitions().enums.enum_array[graph.definitions().enums.index(*type)]);

    auto const player_ship{std::ranges::find(
        graph.enum_domains().values(
            graph.definitions().enums.domain[graph.definitions().enums.index(*type)]),
        std::string{"PlayerShip"},
        [&](auto id) {
            return text(
                storage,
                graph.declarations().enumerators.name[graph.declarations().enumerators.index(id)]);
        })};
    ASSERT_NE(player_ship,
              graph.enum_domains()
                  .values(graph.definitions().enums.domain[graph.definitions().enums.index(*type)])
                  .end());
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
        graph.enum_domains().values(
            graph.definitions().enums.domain[graph.definitions().enums.index(*type)]),
        std::string{"COUNT"},
        [&](auto id) {
            return text(
                storage,
                graph.declarations().enumerators.name[graph.declarations().enumerators.index(id)]);
        })};
    EXPECT_EQ(count,
              graph.enum_domains()
                  .values(graph.definitions().enums.domain[graph.definitions().enums.index(*type)])
                  .end());
}

TEST(SemanticTypeGraph, PreservesExplicitEnumSemanticWidth) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("states");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("States.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{context.storage.declarations.enums.append()};
                    context.storage.declarations.enums
                        .name[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.strings.intern("State");
                    context.storage.declarations.enums
                        .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("std::uint8_t");
                            return new_type_refs;
                        }();
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{
                                     context.storage.declarations.enumerators.append()};
                                 context.storage.declarations.enumerators
                                     .name[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.intern("Idle");
                                 context.storage.declarations.enumerators
                                     .initializer[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.add("0");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{
                                     context.storage.declarations.enumerators.append()};
                                 context.storage.declarations.enumerators
                                     .name[context.storage.declarations.enumerators.index(
                                         new_enumerators)] =
                                     context.storage.strings.intern("Active");
                                 context.storage.declarations.enumerators
                                     .initializer[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.add("7");
                                 context.storage.declarations.enumerators
                                     .sentinel[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = true;
                                 return new_enumerators;
                             }()});
                    context.storage.declarations.enums
                        .bit_width[context.storage.declarations.enums.index(new_enums)] = 3;
                    context.storage.declarations.enums
                        .signedness[context.storage.declarations.enums.index(new_enums)] =
                        lispb::schema::EnumSignedness::unsigned_;
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const state{lispb::codegen::find_test_type(graph, "states", "State")};
    ASSERT_TRUE(state.has_value());
    auto const& type{graph.definition(*state)};
    EXPECT_EQ(graph.definitions().enums.bit_width[graph.definitions().enums.index(type)], 3);
    EXPECT_EQ(graph.definitions().enums.signedness[graph.definitions().enums.index(type)],
              EnumSignedness::unsigned_);
    ASSERT_EQ(graph.enum_domains()
                  .values(graph.definitions().enums.domain[graph.definitions().enums.index(type)])
                  .size(),
              2U);
    EXPECT_FALSE(graph.declarations().enumerators.sentinel[graph.declarations().enumerators.index(
        graph.enum_domains().values(
            graph.definitions().enums.domain[graph.definitions().enums.index(type)])[0])]);
    EXPECT_TRUE(graph.declarations().enumerators.sentinel[graph.declarations().enumerators.index(
        graph.enum_domains().values(
            graph.definitions().enums.domain[graph.definitions().enums.index(type)])[1])]);
    EXPECT_NE(graph.enum_domains().count_value[graph.enum_domains().index(
                  graph.definitions().enums.domain[graph.definitions().enums.index(type)])],
              graph.enum_domains().values(
                  graph.definitions().enums.domain[graph.definitions().enums.index(type)])[1]);
}

TEST(SemanticTypeGraph, KeepsDerivedEnumBackingOutOfSemanticDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("states");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("States.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{context.storage.declarations.enums.append()};
                    context.storage.declarations.enums
                        .name[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.strings.intern("State");
                    context.storage.declarations.enums
                        .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                        std::nullopt;
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{
                                     context.storage.declarations.enumerators.append()};
                                 context.storage.declarations.enumerators
                                     .name[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.intern("Idle");
                                 context.storage.declarations.enumerators
                                     .initializer[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.add("0");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{
                                     context.storage.declarations.enumerators.append()};
                                 context.storage.declarations.enumerators
                                     .name[context.storage.declarations.enumerators.index(
                                         new_enumerators)] =
                                     context.storage.strings.intern("Active");
                                 context.storage.declarations.enumerators
                                     .initializer[context.storage.declarations.enumerators.index(
                                         new_enumerators)] = context.storage.strings.add("7");
                                 return new_enumerators;
                             }()});
                    context.storage.declarations.enums
                        .bit_width[context.storage.declarations.enums.index(new_enums)] = 3;
                    context.storage.declarations.enums
                        .signedness[context.storage.declarations.enums.index(new_enums)] =
                        lispb::schema::EnumSignedness::unsigned_;
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const state{lispb::codegen::find_test_type(graph, "states", "State")};
    ASSERT_TRUE(state.has_value());
    auto const& type{graph.definition(*state)};
    EXPECT_FALSE(lispb::schema::is_valid(
        graph.definitions().enums.underlying_type[graph.definitions().enums.index(type)]));
    EXPECT_TRUE(graph.dependencies_of(*state).empty());
}

TEST(SemanticTypeGraph, ResolvesStandaloneIntegerScalarWithoutPhysicalFacts) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("semantic_values");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.add("SemanticValues.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("project");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_integer_scalars{
                             context.storage.declarations.integer_scalars.append()};
                         context.storage.declarations.integer_scalars
                             .name[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] =
                             context.storage.strings.intern("DamageReason");
                         context.storage.declarations.integer_scalars
                             .signedness[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = false;
                         context.storage.declarations.integer_scalars
                             .minimum_value[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = 0;
                         context.storage.declarations.integer_scalars
                             .maximum_value[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = 10;
                         context.storage.declarations.integer_scalars
                             .bit_width[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = std::nullopt;
                         context.storage.declarations.integer_scalars
                             .named_codes[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] =
                             context.storage.declarations.range<lispb::codegen::PackedNamedCodeId>(
                                 {[&] {
                                      auto const new_packed_named_codes{
                                          context.storage.declarations.packed_named_codes.append()};
                                      context.storage.declarations.packed_named_codes
                                          .name[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                          context.storage.strings.intern("Unknown");
                                      context.storage.declarations.packed_named_codes
                                          .value[context.storage.declarations.packed_named_codes
                                                     .index(new_packed_named_codes)] = 0;
                                      context.storage.declarations.packed_named_codes
                                          .sentinel[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] = false;
                                      return new_packed_named_codes;
                                  }(),
                                  [&] {
                                      auto const new_packed_named_codes{
                                          context.storage.declarations.packed_named_codes.append()};
                                      context.storage.declarations.packed_named_codes
                                          .name[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                          context.storage.strings.intern("Invalid");
                                      context.storage.declarations.packed_named_codes
                                          .value[context.storage.declarations.packed_named_codes
                                                     .index(new_packed_named_codes)] = 15;
                                      context.storage.declarations.packed_named_codes
                                          .sentinel[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] = true;
                                      return new_packed_named_codes;
                                  }()});
                         context.storage.declarations.integer_scalars
                             .relationship[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = [&] {
                             auto const new_semantic_relations{
                                 context.storage.declarations.semantic_relations.append()};
                             context.storage.declarations.semantic_relations
                                 .kind[context.storage.declarations.semantic_relations.index(
                                     new_semantic_relations)] =
                                 lispb::codegen::SemanticRelationKind::offset_into;
                             context.storage.declarations.semantic_relations
                                 .target[context.storage.declarations.semantic_relations.index(
                                     new_semantic_relations)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("project::EntityTable");
                                 return new_type_refs;
                             }();
                             context.storage.declarations.semantic_relations
                                 .unit[context.storage.declarations.semantic_relations.index(
                                     new_semantic_relations)] =
                                 lispb::codegen::SemanticRelationUnit::bytes;
                             return new_semantic_relations;
                         }();
                         return new_integer_scalars;
                     }(),
                     [&] {
                         auto const new_integer_scalars{
                             context.storage.declarations.integer_scalars.append()};
                         context.storage.declarations.integer_scalars
                             .name[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] =
                             context.storage.strings.intern("EntityTable");
                         context.storage.declarations.integer_scalars
                             .signedness[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = false;
                         context.storage.declarations.integer_scalars
                             .minimum_value[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = 0;
                         context.storage.declarations.integer_scalars
                             .maximum_value[context.storage.declarations.integer_scalars.index(
                                 new_integer_scalars)] = 1'023;
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
    EXPECT_FALSE(
        graph.definitions()
            .integer_scalars.signedness[graph.definitions().integer_scalars.index(*scalar)]);
    EXPECT_EQ(
        graph.definitions()
            .integer_scalars.minimum_value[graph.definitions().integer_scalars.index(*scalar)],
        lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(
        graph.definitions()
            .integer_scalars.maximum_value[graph.definitions().integer_scalars.index(*scalar)],
        lispb::codegen::PackedIntegerValue{10});
    EXPECT_TRUE(
        graph.definitions()
            .integer_scalars.bit_width_auto[graph.definitions().integer_scalars.index(*scalar)]);
    EXPECT_EQ(graph.definitions()
                  .integer_scalars.bit_width[graph.definitions().integer_scalars.index(*scalar)],
              4U);
    ASSERT_EQ(graph.definitions()
                  .integer_scalars.named_codes[graph.definitions().integer_scalars.index(*scalar)]
                  .size(),
              2U);
    EXPECT_TRUE(
        storage.declarations.packed_named_codes
            .sentinel[storage.declarations.packed_named_codes.index(storage.declarations.items(
                graph.definitions().integer_scalars.named_codes
                    [graph.definitions().integer_scalars.index(*scalar)])[1])]);
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions()
            .integer_scalars.relationship[graph.definitions().integer_scalars.index(*scalar)]));
    EXPECT_EQ(
        graph.relationships().kind[graph.relationships().index(
            graph.definitions()
                .integer_scalars.relationship[graph.definitions().integer_scalars.index(*scalar)])],
        lispb::codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(
        graph.relationships().unit[graph.relationships().index(
            graph.definitions()
                .integer_scalars.relationship[graph.definitions().integer_scalars.index(*scalar)])],
        lispb::codegen::SemanticRelationUnit::bytes);
    auto const target{lispb::codegen::find_test_type(graph, "semantic_values", "EntityTable")};
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.relationships().target[graph.relationships().index(
                      graph.definitions().integer_scalars.relationship
                          [graph.definitions().integer_scalars.index(*scalar)])])],
              *target);
    ASSERT_EQ(graph.dependencies_of(*scalar_id).size(), 1U);
    EXPECT_EQ(graph.dependencies_of(*scalar_id).front(), *target);
    ASSERT_EQ(graph.users_of(*target).size(), 1U);
    EXPECT_EQ(graph.users_of(*target).front(), *scalar_id);
}

TEST(SemanticTypeGraph, RejectsIntegerScalarRelationshipToExternalLeaf) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"external_table",
              storage.cpp_types.add(
                  "project::ExternalTable",
                  {storage.cpp_types.dependency("project::ExternalTable", "ExternalTable.h")})}}),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("semantic_values");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.add("SemanticValues.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("project");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_integer_scalars{
                        context.storage.declarations.integer_scalars.append()};
                    context.storage.declarations.integer_scalars
                        .name[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = context.storage.strings.intern("EntityIndex");
                    context.storage.declarations.integer_scalars
                        .signedness[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = false;
                    context.storage.declarations.integer_scalars
                        .minimum_value[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = 0;
                    context.storage.declarations.integer_scalars
                        .maximum_value[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = 1'023;
                    context.storage.declarations.integer_scalars
                        .bit_width[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = 10;
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = {};
                    context.storage.declarations.integer_scalars
                        .relationship[context.storage.declarations.integer_scalars.index(
                            new_integer_scalars)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] =
                            lispb::codegen::SemanticRelationKind::index_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("@external_table");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = std::nullopt;
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
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("semantic_values");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.add("SemanticValues.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_integer_scalars{
                         context.storage.declarations.integer_scalars.append()};
                     context.storage.declarations.integer_scalars
                         .name[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = context.storage.strings.intern("Health");
                     context.storage.declarations.integer_scalars
                         .signedness[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = false;
                     context.storage.declarations.integer_scalars
                         .minimum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 0;
                     context.storage.declarations.integer_scalars
                         .maximum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 1000;
                     context.storage.declarations.integer_scalars
                         .bit_width[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = std::nullopt;
                     context.storage.declarations.integer_scalars
                         .named_codes[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] =
                         context.storage.declarations.range<lispb::codegen::PackedNamedCodeId>(
                             {[&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Invalid");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 1023;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }()});
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("representations");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.add("Representations.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                     {[&] {
                          auto const new_linear_quantizeds{
                              context.storage.declarations.linear_quantizeds.append()};
                          context.storage.declarations.linear_quantizeds
                              .name[context.storage.declarations.linear_quantizeds.index(
                                  new_linear_quantizeds)] =
                              context.storage.strings.intern("HealthQ8");
                          context.storage.declarations.linear_quantizeds
                              .source[context.storage.declarations.linear_quantizeds.index(
                                  new_linear_quantizeds)] = [&] {
                              auto const new_type_refs{
                                  context.storage.declarations.type_refs.append()};
                              context.storage.declarations.type_refs
                                  .name[context.storage.declarations.type_refs.index(
                                      new_type_refs)] =
                                  context.storage.strings.intern("project::Health");
                              return new_type_refs;
                          }();
                          context.storage.declarations.linear_quantizeds
                              .bit_width[context.storage.declarations.linear_quantizeds.index(
                                  new_linear_quantizeds)] = 8;
                          context.storage.declarations.linear_quantizeds
                              .reserved_codes[context.storage.declarations.linear_quantizeds.index(
                                  new_linear_quantizeds)] = 1;
                          context.storage.declarations.linear_quantizeds
                              .clipping[context.storage.declarations.linear_quantizeds.index(
                                  new_linear_quantizeds)] =
                              lispb::codegen::QuantizationClipping::clamp;
                          return new_linear_quantizeds;
                      }(),
                      [&] {
                          auto const new_integer_varints{
                              context.storage.declarations.integer_varints.append()};
                          context.storage.declarations.integer_varints
                              .name[context.storage.declarations.integer_varints.index(
                                  new_integer_varints)] =
                              context.storage.strings.intern("HealthVarint");
                          context.storage.declarations.integer_varints
                              .source[context.storage.declarations.integer_varints.index(
                                  new_integer_varints)] = [&] {
                              auto const new_type_refs{
                                  context.storage.declarations.type_refs.append()};
                              context.storage.declarations.type_refs
                                  .name[context.storage.declarations.type_refs.index(
                                      new_type_refs)] =
                                  context.storage.strings.intern("project::Health");
                              return new_type_refs;
                          }();
                          context.storage.declarations.integer_varints
                              .encoding[context.storage.declarations.integer_varints.index(
                                  new_integer_varints)] =
                              lispb::codegen::IntegerVarintEncoding::unsigned_varint;
                          return new_integer_varints;
                      }(),
                      [&] {
                          auto const new_fixed_points{
                              context.storage.declarations.fixed_points.append()};
                          context.storage.declarations.fixed_points
                              .name[context.storage.declarations.fixed_points.index(
                                  new_fixed_points)] =
                              context.storage.strings.intern("VelocityQ12_4");
                          context.storage.declarations.fixed_points
                              .signedness[context.storage.declarations.fixed_points.index(
                                  new_fixed_points)] = true;
                          context.storage.declarations.fixed_points
                              .total_bits[context.storage.declarations.fixed_points.index(
                                  new_fixed_points)] = 16;
                          context.storage.declarations.fixed_points
                              .fractional_bits[context.storage.declarations.fixed_points.index(
                                  new_fixed_points)] = 4;
                          context.storage.declarations.fixed_points
                              .rounding[context.storage.declarations.fixed_points.index(
                                  new_fixed_points)] =
                              lispb::codegen::FixedPointRounding::toward_zero;
                          return new_fixed_points;
                      }(),
                      [&] {
                          auto const new_optional_sentinels{
                              context.storage.declarations.optional_sentinels.append()};
                          context.storage.declarations.optional_sentinels
                              .name[context.storage.declarations.optional_sentinels.index(
                                  new_optional_sentinels)] =
                              context.storage.strings.intern("OptionalHealth");
                          context.storage.declarations.optional_sentinels
                              .source[context.storage.declarations.optional_sentinels.index(
                                  new_optional_sentinels)] = [&] {
                              auto const new_type_refs{
                                  context.storage.declarations.type_refs.append()};
                              context.storage.declarations.type_refs
                                  .name[context.storage.declarations.type_refs.index(
                                      new_type_refs)] =
                                  context.storage.strings.intern("project::Health");
                              return new_type_refs;
                          }();
                          context.storage.declarations.optional_sentinels
                              .sentinel[context.storage.declarations.optional_sentinels.index(
                                  new_optional_sentinels)] =
                              context.storage.strings.intern("Invalid");
                          return new_optional_sentinels;
                      }(),
                      [&] {
                          auto const new_optional_presence_bits{
                              context.storage.declarations.optional_presence_bits.append()};
                          context.storage.declarations.optional_presence_bits
                              .name[context.storage.declarations.optional_presence_bits.index(
                                  new_optional_presence_bits)] =
                              context.storage.strings.intern("PresentHealth");
                          context.storage.declarations.optional_presence_bits
                              .source[context.storage.declarations.optional_presence_bits.index(
                                  new_optional_presence_bits)] = [&] {
                              auto const new_type_refs{
                                  context.storage.declarations.type_refs.append()};
                              context.storage.declarations.type_refs
                                  .name[context.storage.declarations.type_refs.index(
                                      new_type_refs)] =
                                  context.storage.strings.intern("project::Health");
                              return new_type_refs;
                          }();
                          return new_optional_presence_bits;
                      }(),
                      [&] {
                          auto const new_mini_floats{
                              context.storage.declarations.mini_floats.append()};
                          context.storage.declarations.mini_floats
                              .name[context.storage.declarations.mini_floats.index(
                                  new_mini_floats)] =
                              context.storage.strings.intern("CompactFloat");
                          context.storage.declarations.mini_floats
                              .sign_bits[context.storage.declarations.mini_floats.index(
                                  new_mini_floats)] = 1;
                          context.storage.declarations.mini_floats
                              .exponent_bits[context.storage.declarations.mini_floats.index(
                                  new_mini_floats)] = 5;
                          context.storage.declarations.mini_floats
                              .significand_bits[context.storage.declarations.mini_floats.index(
                                  new_mini_floats)] = 10;
                          context.storage.declarations.mini_floats
                              .exponent_bias[context.storage.declarations.mini_floats.index(
                                  new_mini_floats)] = 15;
                          return new_mini_floats;
                      }()});
             return new_normal_modules;
         }()}};

    auto const graph{resolve_type_graph(manifest)};
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
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .linear_quantized.source[graph.definitions().linear_quantized.index(type)])],
              *source);
    EXPECT_EQ(graph.definitions()
                  .linear_quantized.bit_width[graph.definitions().linear_quantized.index(type)],
              8U);
    EXPECT_EQ(
        graph.definitions()
            .linear_quantized.reserved_codes[graph.definitions().linear_quantized.index(type)],
        1U);
    EXPECT_EQ(graph.definitions()
                  .linear_quantized.clipping[graph.definitions().linear_quantized.index(type)],
              lispb::codegen::QuantizationClipping::clamp);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*representation), *source),
              graph.dependencies_of(*representation).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *representation),
              graph.users_of(*source).end());

    auto const& varint_type{graph.definition(*varint)};
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .integer_varints.source[graph.definitions().integer_varints.index(varint_type)])],
        *source);
    EXPECT_EQ(graph.definitions()
                  .integer_varints.encoding[graph.definitions().integer_varints.index(varint_type)],
              lispb::codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*varint), *source),
              graph.dependencies_of(*varint).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *varint), graph.users_of(*source).end());

    auto const& fixed_point_type{graph.definition(*fixed_point)};
    EXPECT_TRUE(
        graph.definitions()
            .fixed_points.signedness[graph.definitions().fixed_points.index(fixed_point_type)]);
    EXPECT_EQ(
        graph.definitions()
            .fixed_points.total_bits[graph.definitions().fixed_points.index(fixed_point_type)],
        16U);
    EXPECT_EQ(
        graph.definitions()
            .fixed_points.fractional_bits[graph.definitions().fixed_points.index(fixed_point_type)],
        4U);
    EXPECT_EQ(graph.definitions()
                  .fixed_points.rounding[graph.definitions().fixed_points.index(fixed_point_type)],
              lispb::codegen::FixedPointRounding::toward_zero);
    EXPECT_TRUE(graph.dependencies_of(*fixed_point).empty());

    auto const& optional_type{graph.definition(*optional)};
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions().optional_sentinels.source
                      [graph.definitions().optional_sentinels.index(optional_type)])],
              *source);
    EXPECT_EQ(storage.strings.view(
                  graph.definitions()
                      .optional_sentinels
                      .sentinel_name[graph.definitions().optional_sentinels.index(optional_type)]),
              "Invalid");
    EXPECT_EQ(graph.definitions()
                  .optional_sentinels
                  .sentinel_value[graph.definitions().optional_sentinels.index(optional_type)],
              lispb::codegen::PackedIntegerValue{1023});
    EXPECT_EQ(graph.definitions()
                  .optional_sentinels
                  .bit_width[graph.definitions().optional_sentinels.index(optional_type)],
              10U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*optional), *source),
              graph.dependencies_of(*optional).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *optional), graph.users_of(*source).end());

    auto const& presence_type{graph.definition(*presence)};
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions().optional_presence_bits.source
                      [graph.definitions().optional_presence_bits.index(presence_type)])],
              *source);
    EXPECT_EQ(graph.definitions()
                  .optional_presence_bits
                  .payload_bits[graph.definitions().optional_presence_bits.index(presence_type)],
              10U);
    EXPECT_EQ(graph.definitions()
                  .optional_presence_bits
                  .encoded_bits[graph.definitions().optional_presence_bits.index(presence_type)],
              11U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*presence), *source),
              graph.dependencies_of(*presence).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*source), *presence), graph.users_of(*source).end());

    auto const& mini_float_type{graph.definition(*mini_float)};
    EXPECT_EQ(graph.definitions()
                  .mini_floats.sign_bits[graph.definitions().mini_floats.index(mini_float_type)],
              1U);
    EXPECT_EQ(
        graph.definitions()
            .mini_floats.exponent_bits[graph.definitions().mini_floats.index(mini_float_type)],
        5U);
    EXPECT_EQ(
        graph.definitions()
            .mini_floats.significand_bits[graph.definitions().mini_floats.index(mini_float_type)],
        10U);
    EXPECT_EQ(
        graph.definitions()
            .mini_floats.exponent_bias[graph.definitions().mini_floats.index(mini_float_type)],
        15);
    EXPECT_TRUE(graph.dependencies_of(*mini_float).empty());
}

TEST(SemanticTypeGraph, ResolvesPackedFieldsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
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
                   graph.cpp_spelling(graph.occurrences().type[graph.occurrences().index(
                       graph.definitions()
                           .packed.storage_type[graph.definitions().packed.index(*packed)])])),
              "std::uint32_t");
    EXPECT_EQ(
        graph.definitions().packed.invalid_raw_value[graph.definitions().packed.index(*packed)],
        0xffffffffU);
    ASSERT_EQ(graph.definitions().packed.segments_of(*packed).size(), 2U);
    auto const& index{graph.definitions().packed.segments_of(*packed)[0]};
    auto const& entity_type_field{graph.definitions().packed.segments_of(*packed)[1]};
    EXPECT_EQ(
        text(
            storage,
            graph.definitions().packed_fields.name[graph.definitions().packed_fields.index(index)]),
        "index");
    EXPECT_EQ(
        graph.definitions().packed_fields.bit_width[graph.definitions().packed_fields.index(index)],
        24U);
    EXPECT_EQ(
        text(storage,
             graph.definitions()
                 .packed_fields.name[graph.definitions().packed_fields.index(entity_type_field)]),
        "entity_type");
    EXPECT_EQ(
        graph.definitions()
            .packed_fields.bit_width[graph.definitions().packed_fields.index(entity_type_field)],
        8U);
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions().packed_fields.semantic_type[graph.definitions().packed_fields.index(
                entity_type_field)])],
        *entity_type);

    EXPECT_NE(std::ranges::find(graph.dependencies_of(*unique_id), *entity_type),
              graph.dependencies_of(*unique_id).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*entity_type), *unique_id),
              graph.users_of(*entity_type).end());
}

TEST(SemanticTypeGraph, ResolvesPackedIntegerScalarDomainAndDependency) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("domains");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Domains.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_integer_scalars{
                         context.storage.declarations.integer_scalars.append()};
                     context.storage.declarations.integer_scalars
                         .name[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = context.storage.strings.intern("Health");
                     context.storage.declarations.integer_scalars
                         .signedness[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = false;
                     context.storage.declarations.integer_scalars
                         .minimum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 0;
                     context.storage.declarations.integer_scalars
                         .maximum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 1000;
                     context.storage.declarations.integer_scalars
                         .bit_width[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = std::nullopt;
                     context.storage.declarations.integer_scalars
                         .named_codes[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] =
                         context.storage.declarations.range<lispb::codegen::PackedNamedCodeId>(
                             {[&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Invalid");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 4095;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }()});
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("packed");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Packed.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_packed_values{
                         context.storage.declarations.packed_values.append()};
                     context.storage.declarations.packed_values
                         .name[context.storage.declarations.packed_values.index(
                             new_packed_values)] = context.storage.strings.intern("Status");
                     context.storage.declarations.packed_values
                         .storage_type[context.storage.declarations.packed_values.index(
                             new_packed_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint16_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_values
                         .segments[context.storage.declarations.packed_values.index(
                             new_packed_values)] =
                         context.storage.declarations.range<lispb::codegen::PackedSegmentId>({[&] {
                             auto const new_packed_fields{
                                 context.storage.declarations.packed_fields.append()};
                             context.storage.declarations.packed_fields
                                 .name[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = context.storage.strings.intern("healt"
                                                                                          "h");
                             context.storage.declarations.packed_fields
                                 .type[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] = context.storage.strings.intern("pr"
                                                                                          "oj"
                                                                                          "ec"
                                                                                          "t:"
                                                                                          ":H"
                                                                                          "ea"
                                                                                          "lt"
                                                                                          "h");
                                 return new_type_refs;
                             }();
                             context.storage.declarations.packed_fields
                                 .bits[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = std::nullopt;
                             context.storage.declarations.packed_fields
                                 .kind[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] =
                                 lispb::codegen::PackedFieldKind::unsigned_integer;
                             return new_packed_fields;
                         }()});
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const health{lispb::codegen::find_test_type(graph, "domains", "Health")};
    auto const status{lispb::codegen::find_test_type(graph, "packed", "Status")};
    ASSERT_TRUE(health.has_value());
    ASSERT_TRUE(status.has_value());

    auto const& packed{graph.definition(*status)};
    ASSERT_EQ(graph.definitions().packed.segments_of(packed).size(), 1U);
    auto const& field{graph.definitions().packed.segments_of(packed).front()};
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .packed_fields.semantic_type[graph.definitions().packed_fields.index(field)])],
        *health);
    EXPECT_EQ(
        graph.definitions().packed_fields.bit_width[graph.definitions().packed_fields.index(field)],
        12U);
    EXPECT_TRUE(graph.definitions()
                    .packed_fields.bit_width_auto[graph.definitions().packed_fields.index(field)]);
    EXPECT_EQ(graph.definitions()
                  .packed_fields.minimum_value[graph.definitions().packed_fields.index(field)],
              lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(graph.definitions()
                  .packed_fields.maximum_value[graph.definitions().packed_fields.index(field)],
              lispb::codegen::PackedIntegerValue{1000});
    ASSERT_EQ(graph.definitions()
                  .packed_fields.named_codes[graph.definitions().packed_fields.index(field)]
                  .size(),
              1U);
    EXPECT_EQ(text(storage,
                   storage.declarations.packed_named_codes
                       .name[storage.declarations.packed_named_codes.index(
                           storage.declarations
                               .items(graph.definitions().packed_fields.named_codes
                                          [graph.definitions().packed_fields.index(field)])
                               .front())]),
              "Invalid");
    EXPECT_TRUE(storage.declarations.packed_named_codes
                    .sentinel[storage.declarations.packed_named_codes.index(
                        storage.declarations
                            .items(graph.definitions().packed_fields.named_codes
                                       [graph.definitions().packed_fields.index(field)])
                            .front())]);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*status), *health),
              graph.dependencies_of(*status).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*health), *status), graph.users_of(*health).end());
}

TEST(SemanticTypeGraph, RetainsPackedPhysicalOrdering) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("wire");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Wire.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_packed_values{
                        context.storage.declarations.packed_values.append()};
                    context.storage.declarations.packed_values
                        .name[context.storage.declarations.packed_values.index(new_packed_values)] =
                        context.storage.strings.intern("Header");
                    context.storage.declarations.packed_values
                        .storage_type[context.storage.declarations.packed_values.index(
                            new_packed_values)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("std::uint32_t");
                        return new_type_refs;
                    }();
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(
                            new_packed_values)] =
                        context.storage.declarations.range<lispb::codegen::PackedSegmentId>({[&] {
                            auto const new_packed_fields{
                                context.storage.declarations.packed_fields.append()};
                            context.storage.declarations.packed_fields
                                .name[context.storage.declarations.packed_fields.index(
                                    new_packed_fields)] = context.storage.strings.intern("kind");
                            context.storage.declarations.packed_fields
                                .type[context.storage.declarations.packed_fields.index(
                                    new_packed_fields)] = [&] {
                                auto const new_type_refs{
                                    context.storage.declarations.type_refs.append()};
                                context.storage.declarations.type_refs
                                    .name[context.storage.declarations.type_refs.index(
                                        new_type_refs)] =
                                    context.storage.strings.intern("std::uint8_t");
                                return new_type_refs;
                            }();
                            context.storage.declarations.packed_fields
                                .bits[context.storage.declarations.packed_fields.index(
                                    new_packed_fields)] = 8;
                            return new_packed_fields;
                        }()});
                    context.storage.declarations.packed_values
                        .byte_order[context.storage.declarations.packed_values.index(
                            new_packed_values)] = lispb::codegen::PackedByteOrder::big_endian;
                    context.storage.declarations.packed_values
                        .bit_order[context.storage.declarations.packed_values.index(
                            new_packed_values)] =
                        lispb::codegen::PackedBitOrder::most_significant_first;
                    return new_packed_values;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const header{lispb::codegen::find_test_type(graph, "wire", "Header")};
    ASSERT_TRUE(header.has_value());
    auto const& packed{graph.definition(*header)};
    EXPECT_EQ(graph.definitions().packed.byte_order[graph.definitions().packed.index(packed)],
              lispb::codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(graph.definitions().packed.bit_order[graph.definitions().packed.index(packed)],
              lispb::codegen::PackedBitOrder::most_significant_first);
}

TEST(SemanticTypeGraph, ResolvesStandardLibrarySoaColumns) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const graph{production_graph(storage)};
    auto const entity_type{
        lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    auto const history{
        lispb::codegen::find_test_type(graph, "native_entity_history", "EntityHistoryColumns")};
    auto const vectors{lispb::codegen::find_test_type(graph, "native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(history.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& history_type{graph.definition(*history)};
    auto const entity_types{std::ranges::find(
        graph.definitions().soas.columns_of(history_type),
        std::string{"entity_types"},
        [&](auto id) {
            return text(
                storage,
                graph.definitions().soa_columns.name[graph.definitions().soa_columns.index(id)]);
        })};
    ASSERT_NE(entity_types, graph.definitions().soas.columns_of(history_type).end());
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .soa_columns.semantic_type[graph.definitions().soa_columns.index(*entity_types)])],
        *entity_type);

    auto const& vector_type{graph.definition(*vectors)};
    EXPECT_EQ(graph.definitions().soas.source_kind[graph.definitions().soas.index(vector_type)],
              SoaSourceKind::vector);
    auto const vector_equivalent{graph.find_registered(storage.strings.find("native_vector_3f"))};
    ASSERT_TRUE(vector_equivalent.has_value());
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions().soas.equivalent_type[graph.definitions().soas.index(vector_type)]));
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions().soas.equivalent_type[graph.definitions().soas.index(vector_type)])],
        *vector_equivalent);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*vectors), *vector_equivalent),
              graph.dependencies_of(*vectors).end());
    ASSERT_EQ(graph.definitions().soas.columns_of(vector_type).size(), 3U);
    EXPECT_TRUE(std::ranges::equal(graph.definitions().soas.vector_components_of(vector_type),
                                   std::vector<IdentifierId>{storage.strings.find("xs"),
                                                             storage.strings.find("ys"),
                                                             storage.strings.find("zs")}));
    for (auto const column_id : graph.definitions().soas.columns_of(vector_type)) {
        auto const column{column_id};
        EXPECT_EQ(text(storage,
                       graph.cpp_spelling(graph.occurrences().type[graph.occurrences().index(
                           graph.definitions().soa_columns.semantic_type
                               [graph.definitions().soa_columns.index(column)])])),
                  "float");
    }
    auto const local_vectors{
        lispb::codegen::find_test_type(graph, "lasers_soa_entities", "Vectors3f")};
    ASSERT_TRUE(local_vectors.has_value());
    auto const& local{graph.definition(*local_vectors)};
    EXPECT_EQ(graph.definitions().soas.source_kind[graph.definitions().soas.index(local)],
              SoaSourceKind::structure);
    EXPECT_TRUE(std::ranges::equal(graph.definitions().soas.vector_components_of(local),
                                   graph.definitions().soas.vector_components_of(vector_type)));
    EXPECT_FALSE(graph.definitions()
                     .soas.equivalent_constructor[graph.definitions().soas.index(vector_type)]);
    EXPECT_EQ(
        graph.definitions().soas.equivalent_constructor[graph.definitions().soas.index(local)],
        graph.definitions()
            .soas.equivalent_constructor[graph.definitions().soas.index(vector_type)]);
}

TEST(SemanticTypeGraph, RejectsVectorComponentsThatDisagreeWithDeclaredEquivalent) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const project_root{
        std::filesystem::path{IOJ_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{lispb::load_project(storage.syntax, project_root / "lispb/project.lispb")};
    auto const target{*project.find_target("sandbox-code")};
    std::vector<std::filesystem::path> sources;
    for (auto const& source : project.sources(target)) {
        sources.push_back(project.root / source);
    }
    auto manifest{
        lispb::codegen::load_sources(storage, project.root / project.types(target), sources)};
    auto& store{storage.declarations};
    for (auto& module_id : manifest.modules) {
        auto module{((module_id).kind() == lispb::codegen::ModuleKind::normal
                         ? std::optional{module_id}
                         : std::nullopt)};
        if (!module ||
            text(storage,
                 store.module_settings_store.name[store.module_settings_store.index(
                     store.normal_modules.settings[store.normal_modules.index(*module)])]) !=
                "lasers_soa_entities") {
            continue;
        }
        auto const declarations{
            store.items(store.normal_modules.declarations[store.normal_modules.index(*module)])};
        std::vector<lispb::codegen::DeclarationHandle> updated{declarations.begin(),
                                                               declarations.end()};
        auto vectors{updated.front()};
        std::vector<lispb::codegen::SoaMemberId> members;
        for (auto member_id : store.items(store.soas.members[store.soas.index(vectors)])) {
            auto member{member_id};
            store.soa_members.type[store.soa_members.index(member)] = [&] {
                auto const new_type_refs{store.type_refs.append()};
                store.type_refs.name[store.type_refs.index(new_type_refs)] =
                    storage.strings.intern("double");
                return new_type_refs;
            }();
            members.push_back(member);
        }
        store.soas.members[store.soas.index(vectors)] = store.range(members);
        updated.front() = vectors;
        store.normal_modules.declarations[store.normal_modules.index(*module)] =
            store.range(updated);
        module_id = *module;
    }
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsNestedSoaTypeThatDisagreesWithResolvedSchema) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("tables");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Tables.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_soas{context.storage.declarations.soas.append()};
                         context.storage.declarations.soas
                             .name[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("Child");
                         context.storage.declarations.soas
                             .members[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{
                                     context.storage.declarations.soa_members.append()};
                                 context.storage.declarations.soa_members
                                     .name[context.storage.declarations.soa_members.index(
                                         new_soa_members)] =
                                     context.storage.strings.intern("values");
                                 context.storage.declarations.soa_members
                                     .kind[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = lispb::codegen::SoaMemberKind::array;
                                 context.storage.declarations.soa_members
                                     .type[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("float");
                                     return new_type_refs;
                                 }();
                                 return new_soa_members;
                             }()});
                         return new_soas;
                     }(),
                     [&] {
                         auto const new_soas{context.storage.declarations.soas.append()};
                         context.storage.declarations.soas
                             .name[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("Parent");
                         context.storage.declarations.soas
                             .members[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{
                                     context.storage.declarations.soa_members.append()};
                                 context.storage.declarations.soa_members
                                     .name[context.storage.declarations.soa_members.index(
                                         new_soa_members)] =
                                     context.storage.strings.intern("child");
                                 context.storage.declarations.soa_members
                                     .kind[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = lispb::codegen::SoaMemberKind::nested;
                                 context.storage.declarations.soa_members
                                     .type[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("Other");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.soa_members
                                     .nested_schema[context.storage.declarations.soa_members.index(
                                         new_soa_members)] =
                                     context.storage.strings.intern("Child");
                                 return new_soa_members;
                             }()});
                         context.storage.declarations.soas
                             .single_allocation[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("SingleParent");
                         return new_soas;
                     }()});
            return new_normal_modules;
        }()}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesSoaColumnRelationshipsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("tables");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Tables.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_soas{context.storage.declarations.soas.append()};
                         context.storage.declarations.soas
                             .name[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("Target");
                         context.storage.declarations.soas
                             .members[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{
                                     context.storage.declarations.soa_members.append()};
                                 context.storage.declarations.soa_members
                                     .name[context.storage.declarations.soa_members.index(
                                         new_soa_members)] =
                                     context.storage.strings.intern("values");
                                 context.storage.declarations.soa_members
                                     .kind[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = lispb::codegen::SoaMemberKind::array;
                                 context.storage.declarations.soa_members
                                     .type[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("float");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.soa_members
                                     .relationship[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = std::nullopt;
                                 return new_soa_members;
                             }()});
                         return new_soas;
                     }(),
                     [&] {
                         auto const new_soas{context.storage.declarations.soas.append()};
                         context.storage.declarations.soas
                             .name[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("User");
                         context.storage.declarations.soas
                             .members[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{
                                     context.storage.declarations.soa_members.append()};
                                 context.storage.declarations.soa_members
                                     .name[context.storage.declarations.soa_members.index(
                                         new_soa_members)] =
                                     context.storage.strings.intern("identifiers");
                                 context.storage.declarations.soa_members
                                     .kind[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = lispb::codegen::SoaMemberKind::array;
                                 context.storage.declarations.soa_members
                                     .type[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("std::uint32_t");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.soa_members
                                     .relationship[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_semantic_relations{
                                         context.storage.declarations.semantic_relations.append()};
                                     context.storage.declarations.semantic_relations
                                         .kind[context.storage.declarations.semantic_relations
                                                   .index(new_semantic_relations)] =
                                         lispb::codegen::SemanticRelationKind::index_into;
                                     context.storage.declarations.semantic_relations
                                         .target[context.storage.declarations.semantic_relations
                                                     .index(new_semantic_relations)] = [&] {
                                         auto const new_type_refs{
                                             context.storage.declarations.type_refs.append()};
                                         context.storage.declarations.type_refs
                                             .name[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] =
                                             context.storage.strings.intern("Target");
                                         return new_type_refs;
                                     }();
                                     context.storage.declarations.semantic_relations
                                         .unit[context.storage.declarations.semantic_relations
                                                   .index(new_semantic_relations)] = std::nullopt;
                                     return new_semantic_relations;
                                 }();
                                 return new_soa_members;
                             }()});
                         return new_soas;
                     }()});
            context.storage.declarations.normal_modules
                .soa_backend[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] = lispb::codegen::SoaBackend::standard_library;
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const target{lispb::codegen::find_test_type(graph, "tables", "Target")};
    auto const user{lispb::codegen::find_test_type(graph, "tables", "User")};
    ASSERT_TRUE(target.has_value());
    ASSERT_TRUE(user.has_value());
    auto const& soa{graph.definition(*user)};
    ASSERT_EQ(graph.definitions().soas.columns_of(soa).size(), 1U);
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions().soa_columns.relationship[graph.definitions().soa_columns.index(
            graph.definitions().soas.columns_of(soa)[0])]));
    EXPECT_EQ(
        graph.relationships().kind[graph.relationships().index(
            graph.definitions().soa_columns.relationship[graph.definitions().soa_columns.index(
                graph.definitions().soas.columns_of(soa)[0])])],
        lispb::codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.relationships().target[graph.relationships().index(
                graph.definitions().soa_columns.relationship[graph.definitions().soa_columns.index(
                    graph.definitions().soas.columns_of(soa)[0])])])],
        *target);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(*user), *target),
              graph.dependencies_of(*user).end());
    EXPECT_NE(std::ranges::find(graph.users_of(*target), *user), graph.users_of(*target).end());
}

TEST(SemanticTypeGraph, ResolvesRecordMembersFixedArraysAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("records");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Records.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                    {[&] {
                         auto const new_records{context.storage.declarations.records.append()};
                         context.storage.declarations.records
                             .name[context.storage.declarations.records.index(new_records)] =
                             context.storage.strings.intern("Position");
                         context.storage.declarations.records
                             .members[context.storage.declarations.records.index(new_records)] =
                             context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                 {[&] {
                                     auto const new_record_members{
                                         context.storage.declarations.record_members.append()};
                                     context.storage.declarations.record_members
                                         .name[context.storage.declarations.record_members.index(
                                             new_record_members)] =
                                         context.storage.strings.intern("x");
                                     context.storage.declarations.record_members
                                         .type[context.storage.declarations.record_members.index(
                                             new_record_members)] = [&] {
                                         auto const new_type_refs{
                                             context.storage.declarations.type_refs.append()};
                                         context.storage.declarations.type_refs
                                             .name[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] =
                                             context.storage.strings.intern("float");
                                         return new_type_refs;
                                     }();
                                     return new_record_members;
                                 }()});
                         return new_records;
                     }(),
                     [&] {
                         auto const new_records{context.storage.declarations.records.append()};
                         context.storage.declarations.records
                             .name[context.storage.declarations.records.index(new_records)] =
                             context.storage.strings.intern("Table");
                         context.storage.declarations.records
                             .members[context.storage.declarations.records.index(new_records)] =
                             context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                 {[&] {
                                     auto const new_record_members{
                                         context.storage.declarations.record_members.append()};
                                     context.storage.declarations.record_members
                                         .name[context.storage.declarations.record_members.index(
                                             new_record_members)] =
                                         context.storage.strings.intern("value");
                                     context.storage.declarations.record_members
                                         .type[context.storage.declarations.record_members.index(
                                             new_record_members)] = [&] {
                                         auto const new_type_refs{
                                             context.storage.declarations.type_refs.append()};
                                         context.storage.declarations.type_refs
                                             .name[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] =
                                             context.storage.strings.intern("float");
                                         return new_type_refs;
                                     }();
                                     return new_record_members;
                                 }()});
                         return new_records;
                     }(),
                     [&] {
                         auto const new_records{context.storage.declarations.records.append()};
                         context.storage.declarations.records
                             .name[context.storage.declarations.records.index(new_records)] =
                             context.storage.strings.intern("Path");
                         context.storage.declarations.records
                             .members[context.storage.declarations.records.index(new_records)] =
                             context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                 {[&] {
                                     auto const new_record_members{
                                         context.storage.declarations.record_members.append()};
                                     context.storage.declarations.record_members
                                         .name[context.storage.declarations.record_members.index(
                                             new_record_members)] =
                                         context.storage.strings.intern("points");
                                     context.storage.declarations.record_members
                                         .type[context.storage.declarations.record_members.index(
                                             new_record_members)] = [&] {
                                         auto const new_type_refs{
                                             context.storage.declarations.type_refs.append()};
                                         context.storage.declarations.type_refs
                                             .name[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] =
                                             context.storage.strings.intern("Position");
                                         return new_type_refs;
                                     }();
                                     context.storage.declarations.record_members
                                         .count[context.storage.declarations.record_members.index(
                                             new_record_members)] = 8;
                                     context.storage.declarations.record_members
                                         .relationship[context.storage.declarations.record_members
                                                           .index(new_record_members)] = [&] {
                                         auto const new_semantic_relations{
                                             context.storage.declarations.semantic_relations
                                                 .append()};
                                         context.storage.declarations.semantic_relations
                                             .kind[context.storage.declarations.semantic_relations
                                                       .index(new_semantic_relations)] =
                                             lispb::codegen::SemanticRelationKind::member_of;
                                         context.storage.declarations.semantic_relations
                                             .target[context.storage.declarations.semantic_relations
                                                         .index(new_semantic_relations)] = [&] {
                                             auto const new_type_refs{
                                                 context.storage.declarations.type_refs.append()};
                                             context.storage.declarations.type_refs
                                                 .name[context.storage.declarations.type_refs.index(
                                                     new_type_refs)] =
                                                 context.storage.strings.intern("Table");
                                             return new_type_refs;
                                         }();
                                         context.storage.declarations.semantic_relations
                                             .unit[context.storage.declarations.semantic_relations
                                                       .index(new_semantic_relations)] =
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
    auto const position{lispb::codegen::find_test_type(graph, "records", "Position")};
    auto const table{lispb::codegen::find_test_type(graph, "records", "Table")};
    auto const path{lispb::codegen::find_test_type(graph, "records", "Path")};
    ASSERT_TRUE(position.has_value());
    ASSERT_TRUE(table.has_value());
    ASSERT_TRUE(path.has_value());
    auto const& record{graph.definition(*path)};
    ASSERT_EQ(graph.definitions().records.members_of(record).size(), 1U);
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(record)[0])])],
              *position);
    EXPECT_EQ(graph.definitions().record_members.count[graph.definitions().record_members.index(
                  graph.definitions().records.members_of(record)[0])],
              8);
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions().record_members.relationship[graph.definitions().record_members.index(
            graph.definitions().records.members_of(record)[0])]));
    EXPECT_EQ(graph.relationships().kind[graph.relationships().index(
                  graph.definitions()
                      .record_members.relationship[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(record)[0])])],
              lispb::codegen::SemanticRelationKind::member_of);
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.relationships().target[graph.relationships().index(
                      graph.definitions()
                          .record_members.relationship[graph.definitions().record_members.index(
                              graph.definitions().records.members_of(record)[0])])])],
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
    [[maybe_unused]] auto& storage{context.storage};
    auto module{[&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("records");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Records.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_records{context.storage.declarations.records.append()};
                context.storage.declarations.records
                    .name[context.storage.declarations.records.index(new_records)] =
                    context.storage.strings.intern("Record");
                context.storage.declarations.records
                    .members[context.storage.declarations.records.index(new_records)] =
                    context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                        auto const new_record_members{
                            context.storage.declarations.record_members.append()};
                        context.storage.declarations.record_members
                            .name[context.storage.declarations.record_members.index(
                                new_record_members)] = context.storage.strings.intern("self");
                        context.storage.declarations.record_members
                            .type[context.storage.declarations.record_members.index(
                                new_record_members)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Record");
                            return new_type_refs;
                        }();
                        return new_record_members;
                    }()});
                return new_records;
            }()});
        return new_normal_modules;
    }()};
    auto manifest{lispb::codegen::Manifest{lispb::codegen::manifest_schema_version,
                                           lispb::codegen::make_test_registry(context),
                                           context.storage.declarations,
                                           {module}}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    storage.declarations.normal_modules.declarations
        [storage.declarations.normal_modules.index(module)] = storage.declarations.range(
        {[&] {
             auto const new_records{storage.declarations.records.append()};
             storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("First");
             storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("second");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("Second");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }(),
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("Second");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("first");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("First");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()});
    manifest.modules = {module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, PointerSelfReferenceIsNotByValueRecursion) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("records");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Records.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_records{context.storage.declarations.records.append()};
                    context.storage.declarations.records
                        .name[context.storage.declarations.records.index(new_records)] =
                        context.storage.strings.intern("Node");
                    context.storage.declarations.records
                        .members[context.storage.declarations.records.index(new_records)] =
                        context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                            auto const new_record_members{
                                context.storage.declarations.record_members.append()};
                            context.storage.declarations.record_members
                                .name[context.storage.declarations.record_members.index(
                                    new_record_members)] = context.storage.strings.intern("next");
                            context.storage.declarations.record_members
                                .type[context.storage.declarations.record_members.index(
                                    new_record_members)] = [&] {
                                auto const new_type_refs{
                                    context.storage.declarations.type_refs.append()};
                                context.storage.declarations.type_refs
                                    .name[context.storage.declarations.type_refs.index(
                                        new_type_refs)] = context.storage.strings.intern("Node");
                                context.storage.declarations.type_refs
                                    .suffix[context.storage.declarations.type_refs.index(
                                        new_type_refs)] = context.storage.strings.add("*");
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
    [[maybe_unused]] auto& storage{context.storage};
    auto const pointer{lispb::codegen::resolve_type_use(
        [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("float");
            context.storage.declarations.type_refs
                .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.add(" const* const*");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_EQ(storage.cpp_types.spelling(pointer.cpp_type), "float const* const*");
    EXPECT_EQ(pointer.physical.form, lispb::codegen::PhysicalTypeForm::object_pointer);
    EXPECT_EQ(storage.strings.view(pointer.physical.object_spelling), "float");
    EXPECT_FALSE(pointer.physical.contains_value());
    auto const value{lispb::codegen::resolve_type_use(
        [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("float");
            context.storage.declarations.type_refs
                .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.add(" const");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_TRUE(value.physical.contains_value());
    for (auto const suffix : {"&", "&&", "* const&"}) {
        auto const reference{lispb::codegen::resolve_type_use(
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("float");
                context.storage.declarations.type_refs
                    .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.add(suffix);
                return new_type_refs;
            }(),
            lispb::codegen::make_test_registry(context))};
        EXPECT_FALSE(reference.physical.contains_value());
        EXPECT_NE(reference.physical.form, lispb::codegen::PhysicalTypeForm::value);
    }
    for (auto const suffix : {"[4]", "(*)()", " Owner::*", "*[]"}) {
        auto const unsupported{lispb::codegen::resolve_type_use(
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("float");
                context.storage.declarations.type_refs
                    .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.add(suffix);
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
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("Owner");
            context.storage.declarations.type_refs
                .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.add("");
            context.storage.declarations.type_refs
                .nested[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.add("Inner");
            return new_type_refs;
        }(),
        lispb::codegen::make_test_registry(context))};
    EXPECT_EQ(storage.strings.view(nested.physical.object_spelling), "Owner::Inner");
    EXPECT_FALSE(nested.physical.contains_value());
}

TEST(SemanticTypeGraph, ResolvesRawUnionAlternativesAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("payloads");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Payloads.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_unions{context.storage.declarations.unions.append()};
                    context.storage.declarations.unions
                        .name[context.storage.declarations.unions.index(new_unions)] =
                        context.storage.strings.intern("Payload");
                    context.storage.declarations.unions
                        .alternatives[context.storage.declarations.unions.index(new_unions)] =
                        context.storage.declarations.range<lispb::codegen::UnionAlternativeId>(
                            {[&] {
                                 auto const new_union_alternatives{
                                     context.storage.declarations.union_alternatives.append()};
                                 context.storage.declarations.union_alternatives
                                     .name[context.storage.declarations.union_alternatives.index(
                                         new_union_alternatives)] =
                                     context.storage.strings.intern("identifier");
                                 context.storage.declarations.union_alternatives
                                     .type[context.storage.declarations.union_alternatives.index(
                                         new_union_alternatives)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("std::uint32_t");
                                     return new_type_refs;
                                 }();
                                 return new_union_alternatives;
                             }(),
                             [&] {
                                 auto const new_union_alternatives{
                                     context.storage.declarations.union_alternatives.append()};
                                 context.storage.declarations.union_alternatives
                                     .name[context.storage.declarations.union_alternatives.index(
                                         new_union_alternatives)] =
                                     context.storage.strings.intern("bytes");
                                 context.storage.declarations.union_alternatives
                                     .type[context.storage.declarations.union_alternatives.index(
                                         new_union_alternatives)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("std::uint8_t");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.union_alternatives
                                     .count[context.storage.declarations.union_alternatives.index(
                                         new_union_alternatives)] = 12;
                                 return new_union_alternatives;
                             }()});
                    context.storage.declarations.unions
                        .export_specifier[context.storage.declarations.unions.index(new_unions)] =
                        {};
                    return new_unions;
                }()});
            return new_normal_modules;
        }()}};

    auto const graph{resolve_type_graph(manifest)};
    auto const payload{lispb::codegen::find_test_type(graph, "payloads", "Payload")};
    ASSERT_TRUE(payload.has_value());
    auto const& union_type{graph.definition(*payload)};
    ASSERT_EQ(graph.definitions().unions.alternatives_of(union_type).size(), 2U);
    EXPECT_EQ(text(storage,
                   graph.definitions()
                       .union_alternatives.name[graph.definitions().union_alternatives.index(
                           graph.definitions().unions.alternatives_of(union_type)[0])]),
              "identifier");
    EXPECT_EQ(
        text(storage,
             graph.cpp_spelling(graph.occurrences().type[graph.occurrences().index(
                 graph.definitions()
                     .union_alternatives.semantic_type[graph.definitions().union_alternatives.index(
                         graph.definitions().unions.alternatives_of(union_type)[0])])])),
        "std::uint32_t");
    EXPECT_EQ(
        graph.definitions().union_alternatives.count[graph.definitions().union_alternatives.index(
            graph.definitions().unions.alternatives_of(union_type)[1])],
        12);
    EXPECT_EQ(graph.dependencies_of(*payload).size(), 2U);
}

TEST(SemanticTypeGraph, RejectsUnionAndMixedAggregateCycles) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const union_schema{context.storage.declarations.unions.append()};
    context.storage.declarations.unions
        .name[context.storage.declarations.unions.index(union_schema)] =
        context.storage.strings.intern("Payload");
    context.storage.declarations.unions
        .alternatives[context.storage.declarations.unions.index(union_schema)] =
        context.storage.declarations.range<lispb::codegen::UnionAlternativeId>({[&] {
            auto const new_union_alternatives{
                context.storage.declarations.union_alternatives.append()};
            context.storage.declarations.union_alternatives
                .name[context.storage.declarations.union_alternatives.index(
                    new_union_alternatives)] = context.storage.strings.intern("self");
            context.storage.declarations.union_alternatives
                .type[context.storage.declarations.union_alternatives.index(
                    new_union_alternatives)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("Payload");
                return new_type_refs;
            }();
            return new_union_alternatives;
        }()});
    context.storage.declarations.unions
        .export_specifier[context.storage.declarations.unions.index(union_schema)] = {};
    auto const union_module{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(union_module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("payloads");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Payloads.h");
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("payloads");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(union_module)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({union_schema});
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::make_test_registry(context),
                                      context.storage.declarations,
                                      {union_module}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);

    context.storage.declarations.unions
        .alternatives[context.storage.declarations.unions.index(union_schema)] =
        storage.declarations.range({[&] {
            auto const new_union_alternatives{storage.declarations.union_alternatives.append()};
            storage.declarations.union_alternatives
                .name[storage.declarations.union_alternatives.index(new_union_alternatives)] =
                storage.strings.intern("record");
            storage.declarations.union_alternatives
                .type[storage.declarations.union_alternatives.index(new_union_alternatives)] = [&] {
                auto const new_type_refs{storage.declarations.type_refs.append()};
                storage.declarations.type_refs
                    .name[storage.declarations.type_refs.index(new_type_refs)] =
                    storage.strings.intern("records::Record");
                return new_type_refs;
            }();
            return new_union_alternatives;
        }()});
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(union_module)] =
        storage.declarations.range({union_schema});
    auto const record_schema{context.storage.declarations.records.append()};
    context.storage.declarations.records
        .name[context.storage.declarations.records.index(record_schema)] =
        context.storage.strings.intern("Record");
    context.storage.declarations.records
        .members[context.storage.declarations.records.index(record_schema)] =
        context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
            auto const new_record_members{context.storage.declarations.record_members.append()};
            context.storage.declarations.record_members
                .name[context.storage.declarations.record_members.index(new_record_members)] =
                context.storage.strings.intern("payload");
            context.storage.declarations.record_members
                .type[context.storage.declarations.record_members.index(new_record_members)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("payloads::Payload");
                return new_type_refs;
            }();
            return new_record_members;
        }()});
    context.storage.declarations.records
        .export_specifier[context.storage.declarations.records.index(record_schema)] = {};
    auto const record_module{storage.declarations.normal_modules.append()};
    storage.declarations.normal_modules
        .settings[storage.declarations.normal_modules.index(record_module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("records");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Records.h");
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("records");
        return new_module_settings_store;
    }();
    storage.declarations.normal_modules
        .declarations[storage.declarations.normal_modules.index(record_module)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({record_schema});
    manifest.modules = {record_module, union_module};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, ResolvesTaggedUnionDiscriminantMappingsAndDependencies) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const enums{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(enums)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("events");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Events.h");
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("events");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(
            enums)] = context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("EventKind");
        context.storage.declarations.enums
            .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("std::uint8_t");
            return new_type_refs;
        }();
        context.storage.declarations.enums
            .values[context.storage.declarations.enums.index(new_enums)] =
            context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Spawn");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Damage");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Invalid");
                     context.storage.declarations.enumerators
                         .sentinel[context.storage.declarations.enumerators.index(
                             new_enumerators)] = true;
                     return new_enumerators;
                 }()});
        return new_enums;
    }()});
    auto const unions{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(unions)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("payloads");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Payloads.h");
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("payloads");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(
            unions)] = context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
        auto const new_tagged_unions{context.storage.declarations.tagged_unions.append()};
        context.storage.declarations.tagged_unions
            .name[context.storage.declarations.tagged_unions.index(new_tagged_unions)] =
            context.storage.strings.intern("Event");
        context.storage.declarations.tagged_unions
            .discriminant[context.storage.declarations.tagged_unions.index(new_tagged_unions)] =
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("events::EventKind");
                return new_type_refs;
            }();
        context.storage.declarations.tagged_unions
            .alternatives[context.storage.declarations.tagged_unions.index(new_tagged_unions)] =
            context.storage.declarations.range<lispb::codegen::TaggedUnionAlternativeId>(
                {[&] {
                     auto const new_tagged_union_alternatives{
                         context.storage.declarations.tagged_union_alternatives.append()};
                     context.storage.declarations.tagged_union_alternatives
                         .name[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] =
                         context.storage.strings.intern("spawn");
                     context.storage.declarations.tagged_union_alternatives
                         .type[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint32_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.tagged_union_alternatives
                         .tag[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] =
                         context.storage.strings.intern("Spawn");
                     return new_tagged_union_alternatives;
                 }(),
                 [&] {
                     auto const new_tagged_union_alternatives{
                         context.storage.declarations.tagged_union_alternatives.append()};
                     context.storage.declarations.tagged_union_alternatives
                         .name[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] =
                         context.storage.strings.intern("damage");
                     context.storage.declarations.tagged_union_alternatives
                         .type[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint16_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.tagged_union_alternatives
                         .count[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] = 4;
                     context.storage.declarations.tagged_union_alternatives
                         .tag[context.storage.declarations.tagged_union_alternatives.index(
                             new_tagged_union_alternatives)] =
                         context.storage.strings.intern("Damage");
                     return new_tagged_union_alternatives;
                 }()});
        return new_tagged_unions;
    }()});
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::make_test_registry(context),
                                      context.storage.declarations,
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
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"native_handle",
              storage.cpp_types.add(
                  "NativeHandle",
                  {storage.cpp_types.dependency("NativeHandle", "native/handle.h")})}}),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("mode");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Mode.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{context.storage.declarations.enums.append()};
                    context.storage.declarations.enums
                        .name[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.strings.intern("Mode");
                    context.storage.declarations.enums
                        .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("uint8");
                            return new_type_refs;
                        }();
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{
                                context.storage.declarations.enumerators.append()};
                            context.storage.declarations.enumerators
                                .name[context.storage.declarations.enumerators.index(
                                    new_enumerators)] = context.storage.strings.intern("Value");
                            return new_enumerators;
                        }()});
                    return new_enums;
                }()});
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const handle{graph.find_registered(storage.strings.find("native_handle"))};
    ASSERT_TRUE(handle.has_value());
    auto const external{
        ((graph.definition(*handle)).kind() == lispb::schema::TypeDefinitionKind::external
             ? std::optional{graph.definition(*handle)}
             : std::nullopt)};
    ASSERT_TRUE(external.has_value());
    EXPECT_EQ(
        storage.cpp_types.spelling(
            graph.definitions().externals.cpp_type[graph.definitions().externals.index(*external)]),
        "NativeHandle");
    ASSERT_FALSE(
        storage.cpp_types
            .dependencies(graph.definitions()
                              .externals.cpp_type[graph.definitions().externals.index(*external)])
            .empty());
    EXPECT_EQ(*storage.cpp_types.dependency_header(
                  storage.cpp_types
                      .dependencies(
                          graph.definitions()
                              .externals.cpp_type[graph.definitions().externals.index(*external)])
                      .front()),
              "native/handle.h");
    EXPECT_TRUE(graph.dependencies_of(*handle).empty());
}

TEST(SemanticTypeGraph, RejectsAmbiguousRegisteredDeclarationBindings) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto enum_module = [&](std::string module_name, std::string const& header) {
        return [&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern(std::move(module_name));
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add(header);
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("project");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{context.storage.declarations.enums.append()};
                    context.storage.declarations.enums
                        .name[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.strings.intern("Mode");
                    context.storage.declarations.enums
                        .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("uint8");
                            return new_type_refs;
                        }();
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{
                                context.storage.declarations.enumerators.append()};
                            context.storage.declarations.enumerators
                                .name[context.storage.declarations.enumerators.index(
                                    new_enumerators)] = context.storage.strings.intern("Value");
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
        context.storage.declarations,
        {enum_module("first", "First.h"), enum_module("second", "Second.h")}};

    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsInvalidAndDuplicateDeclaredIdentities) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const make_module = [&](std::string name,
                                 std::vector<lispb::codegen::DeclarationHandle> const& enums) {
        std::vector<lispb::codegen::DeclarationHandle> declarations;
        for (auto const& value : enums) {
            declarations.push_back(value);
        }
        return [&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern(std::move(name));
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Types.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] = storage.declarations.range(declarations);
            return new_normal_modules;
        }();
    };
    auto const make_enum = [&](std::string name) {
        return [&] {
            auto const new_enums{context.storage.declarations.enums.append()};
            context.storage.declarations.enums
                .name[context.storage.declarations.enums.index(new_enums)] =
                context.storage.strings.intern(std::move(name));
            context.storage.declarations.enums
                .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("uint8");
                return new_type_refs;
            }();
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(new_enums)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Value");
                    return new_enumerators;
                }()});
            return new_enums;
        }();
    };

    lispb::codegen::Manifest invalid{lispb::codegen::manifest_schema_version,
                                     lispb::codegen::make_test_registry(context),
                                     context.storage.declarations,
                                     {make_module("types", {make_enum("")})}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(invalid)), std::invalid_argument);

    lispb::codegen::Manifest duplicate{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {make_module("types", {make_enum("Mode"), make_enum("Mode")})}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(duplicate)), std::invalid_argument);
}

TEST(SemanticTypeGraph, RejectsUnresolvedRegisteredReferences) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("mode");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Mode.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_enums{context.storage.declarations.enums.append()};
                    context.storage.declarations.enums
                        .name[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.strings.intern("Mode");
                    context.storage.declarations.enums
                        .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("@missing");
                            return new_type_refs;
                        }();
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(new_enums)] =
                        context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                            auto const new_enumerators{
                                context.storage.declarations.enumerators.append()};
                            context.storage.declarations.enumerators
                                .name[context.storage.declarations.enumerators.index(
                                    new_enumerators)] = context.storage.strings.intern("Value");
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
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context, {{"bar", storage.cpp_types.add("game::Bar")}}),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("A");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("A.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("game");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                     {[&] {
                          auto const new_records{context.storage.declarations.records.append()};
                          context.storage.declarations.records
                              .name[context.storage.declarations.records.index(new_records)] =
                              context.storage.strings.intern("Foo");
                          context.storage.declarations.records
                              .members[context.storage.declarations.records.index(new_records)] =
                              context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                  {[&] {
                                      auto const new_record_members{
                                          context.storage.declarations.record_members.append()};
                                      context.storage.declarations.record_members
                                          .name[context.storage.declarations.record_members.index(
                                              new_record_members)] =
                                          context.storage.strings.intern("value");
                                      context.storage.declarations.record_members
                                          .type[context.storage.declarations.record_members.index(
                                              new_record_members)] = [&] {
                                          auto const new_type_refs{
                                              context.storage.declarations.type_refs.append()};
                                          context.storage.declarations.type_refs
                                              .name[context.storage.declarations.type_refs.index(
                                                  new_type_refs)] =
                                              context.storage.strings.intern("int32");
                                          return new_type_refs;
                                      }();
                                      return new_record_members;
                                  }()});
                          return new_records;
                      }(),
                      [&] {
                          auto const new_records{context.storage.declarations.records.append()};
                          context.storage.declarations.records
                              .name[context.storage.declarations.records.index(new_records)] =
                              context.storage.strings.intern("Bar");
                          context.storage.declarations.records
                              .members[context.storage.declarations.records.index(new_records)] =
                              context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                  {[&] {
                                      auto const new_record_members{
                                          context.storage.declarations.record_members.append()};
                                      context.storage.declarations.record_members
                                          .name[context.storage.declarations.record_members.index(
                                              new_record_members)] =
                                          context.storage.strings.intern("value");
                                      context.storage.declarations.record_members
                                          .type[context.storage.declarations.record_members.index(
                                              new_record_members)] = [&] {
                                          auto const new_type_refs{
                                              context.storage.declarations.type_refs.append()};
                                          context.storage.declarations.type_refs
                                              .name[context.storage.declarations.type_refs.index(
                                                  new_type_refs)] =
                                              context.storage.strings.intern("int32");
                                          return new_type_refs;
                                      }();
                                      return new_record_members;
                                  }()});
                          return new_records;
                      }(),
                      [&] {
                          auto const new_records{context.storage.declarations.records.append()};
                          context.storage.declarations.records
                              .name[context.storage.declarations.records.index(new_records)] =
                              context.storage.strings.intern("User");
                          context.storage.declarations.records
                              .members[context.storage.declarations.records.index(new_records)] =
                              context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                  {[&] {
                                       auto const new_record_members{
                                           context.storage.declarations.record_members.append()};
                                       context.storage.declarations.record_members
                                           .name[context.storage.declarations.record_members.index(
                                               new_record_members)] =
                                           context.storage.strings.intern("local");
                                       context.storage.declarations.record_members
                                           .type[context.storage.declarations.record_members.index(
                                               new_record_members)] = [&] {
                                           auto const new_type_refs{
                                               context.storage.declarations.type_refs.append()};
                                           context.storage.declarations.type_refs
                                               .name[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.intern("Foo");
                                           return new_type_refs;
                                       }();
                                       return new_record_members;
                                   }(),
                                   [&] {
                                       auto const new_record_members{
                                           context.storage.declarations.record_members.append()};
                                       context.storage.declarations.record_members
                                           .name[context.storage.declarations.record_members.index(
                                               new_record_members)] =
                                           context.storage.strings.intern("qualified");
                                       context.storage.declarations.record_members
                                           .type[context.storage.declarations.record_members.index(
                                               new_record_members)] = [&] {
                                           auto const new_type_refs{
                                               context.storage.declarations.type_refs.append()};
                                           context.storage.declarations.type_refs
                                               .name[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.intern("game::Bar");
                                           return new_type_refs;
                                       }();
                                       return new_record_members;
                                   }(),
                                   [&] {
                                       auto const new_record_members{
                                           context.storage.declarations.record_members.append()};
                                       context.storage.declarations.record_members
                                           .name[context.storage.declarations.record_members.index(
                                               new_record_members)] =
                                           context.storage.strings.intern("registered");
                                       context.storage.declarations.record_members
                                           .type[context.storage.declarations.record_members.index(
                                               new_record_members)] = [&] {
                                           auto const new_type_refs{
                                               context.storage.declarations.type_refs.append()};
                                           context.storage.declarations.type_refs
                                               .name[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.intern("@bar");
                                           return new_type_refs;
                                       }();
                                       return new_record_members;
                                   }(),
                                   [&] {
                                       auto const new_record_members{
                                           context.storage.declarations.record_members.append()};
                                       context.storage.declarations.record_members
                                           .name[context.storage.declarations.record_members.index(
                                               new_record_members)] =
                                           context.storage.strings.intern("suffixed");
                                       context.storage.declarations.record_members
                                           .type[context.storage.declarations.record_members.index(
                                               new_record_members)] = [&] {
                                           auto const new_type_refs{
                                               context.storage.declarations.type_refs.append()};
                                           context.storage.declarations.type_refs
                                               .name[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.intern("Foo");
                                           context.storage.declarations.type_refs
                                               .suffix[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.add("*");
                                           return new_type_refs;
                                       }();
                                       return new_record_members;
                                   }(),
                                   [&] {
                                       auto const new_record_members{
                                           context.storage.declarations.record_members.append()};
                                       context.storage.declarations.record_members
                                           .name[context.storage.declarations.record_members.index(
                                               new_record_members)] =
                                           context.storage.strings.intern("ambiguous");
                                       context.storage.declarations.record_members
                                           .type[context.storage.declarations.record_members.index(
                                               new_record_members)] = [&] {
                                           auto const new_type_refs{
                                               context.storage.declarations.type_refs.append()};
                                           context.storage.declarations.type_refs
                                               .name[context.storage.declarations.type_refs.index(
                                                   new_type_refs)] =
                                               context.storage.strings.intern("game::Foo");
                                           return new_type_refs;
                                       }();
                                       return new_record_members;
                                   }()});
                          return new_records;
                      }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("B");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("B.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("game");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                     auto const new_records{context.storage.declarations.records.append()};
                     context.storage.declarations.records
                         .name[context.storage.declarations.records.index(new_records)] =
                         context.storage.strings.intern("Foo");
                     context.storage.declarations.records
                         .members[context.storage.declarations.records.index(new_records)] =
                         context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{
                                 context.storage.declarations.record_members.append()};
                             context.storage.declarations.record_members
                                 .name[context.storage.declarations.record_members.index(
                                     new_record_members)] = context.storage.strings.intern("value");
                             context.storage.declarations.record_members
                                 .type[context.storage.declarations.record_members.index(
                                     new_record_members)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] = context.storage.strings.intern("int32");
                                 return new_type_refs;
                             }();
                             return new_record_members;
                         }()});
                     return new_records;
                 }()});
             return new_normal_modules;
         }()}};
    auto const graph{resolve_type_graph(manifest)};
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
            graph.occurrences().type[graph.occurrences().index(
                graph.definitions()
                    .record_members.semantic_type[graph.definitions().record_members.index(
                        graph.definitions().records.members_of(user)[index])])]);
    }
    EXPECT_FALSE(graph
                     .find_reference(
                         [&] {
                             auto const new_type_refs{storage.declarations.type_refs.append()};
                             storage.declarations.type_refs
                                 .name[storage.declarations.type_refs.index(new_type_refs)] =
                                 storage.strings.intern("game::Foo");
                             return new_type_refs;
                         }(),
                         module)
                     .has_value());
    EXPECT_EQ(graph.origin(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(user).back())])]),
              TypeOrigin::cpp_spelling);
    EXPECT_FALSE(graph
                     .find_reference(
                         [&] {
                             auto const new_type_refs{storage.declarations.type_refs.append()};
                             storage.declarations.type_refs
                                 .name[storage.declarations.type_refs.index(new_type_refs)] =
                                 storage.strings.intern("Foo");
                             return new_type_refs;
                         }(),
                         lispb::codegen::ModuleHandle{})
                     .has_value());
    EXPECT_FALSE(graph
                     .find_reference(
                         [&] {
                             auto const new_type_refs{storage.declarations.type_refs.append()};
                             storage.declarations.type_refs
                                 .name[storage.declarations.type_refs.index(new_type_refs)] =
                                 storage.strings.intern("@missing");
                             return new_type_refs;
                         }(),
                         module)
                     .has_value());
}

TEST(SemanticTypeGraph, TreatsRawCppSpellingsAsExplicitExternalLeaves) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(context),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("values");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Values.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                    auto const new_soas{context.storage.declarations.soas.append()};
                    context.storage.declarations.soas
                        .name[context.storage.declarations.soas.index(new_soas)] =
                        context.storage.strings.intern("Values");
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(new_soas)] =
                        context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                            auto const new_soa_members{
                                context.storage.declarations.soa_members.append()};
                            context.storage.declarations.soa_members
                                .name[context.storage.declarations.soa_members.index(
                                    new_soa_members)] = context.storage.strings.intern("items");
                            context.storage.declarations.soa_members
                                .kind[context.storage.declarations.soa_members.index(
                                    new_soa_members)] = lispb::codegen::SoaMemberKind::array;
                            context.storage.declarations.soa_members
                                .type[context.storage.declarations.soa_members.index(
                                    new_soa_members)] = [&] {
                                auto const new_type_refs{
                                    context.storage.declarations.type_refs.append()};
                                context.storage.declarations.type_refs
                                    .name[context.storage.declarations.type_refs.index(
                                        new_type_refs)] =
                                    context.storage.strings.intern("ExternalValue");
                                return new_type_refs;
                            }();
                            return new_soa_members;
                        }()});
                    return new_soas;
                }()});
            context.storage.declarations.normal_modules
                .soa_backend[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] = lispb::codegen::SoaBackend::standard_library;
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const values{lispb::codegen::find_test_type(graph, "values", "Values")};
    ASSERT_TRUE(values.has_value());
    auto const& soa{graph.definition(*values)};
    ASSERT_EQ(graph.definitions().soas.columns_of(soa).size(), 1U);
    EXPECT_EQ(
        graph.origin(graph.occurrences().type[graph.occurrences().index(
            graph.definitions().soa_columns.semantic_type[graph.definitions().soa_columns.index(
                graph.definitions().soas.columns_of(soa).front())])]),
        TypeOrigin::cpp_spelling);
}

TEST(SemanticTypeGraph, SingleOnlyPhysicalOwnerDoesNotRenameLogicalIdentity) {
    lispb::codegen::CompilerTestContext context;
    [[maybe_unused]] auto& storage{context.storage};
    auto const rows{context.storage.declarations.soas.append()};
    context.storage.declarations.soas.name[context.storage.declarations.soas.index(rows)] =
        context.storage.strings.intern("LogicalRows");
    context.storage.declarations.soas.members[context.storage.declarations.soas.index(rows)] =
        context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
            auto const new_soa_members{context.storage.declarations.soa_members.append()};
            context.storage.declarations.soa_members
                .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                context.storage.strings.intern("values");
            context.storage.declarations.soa_members
                .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                lispb::codegen::SoaMemberKind::array;
            context.storage.declarations.soa_members
                .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("float");
                return new_type_refs;
            }();
            return new_soa_members;
        }()});
    context.storage.declarations.soas
        .equivalent_type[context.storage.declarations.soas.index(rows)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("Row");
        return new_type_refs;
    }();
    context.storage.declarations.soas
        .single_allocation[context.storage.declarations.soas.index(rows)] =
        context.storage.strings.intern("CompactOwner");
    lispb::codegen::Manifest manifest{
        lispb::codegen::manifest_schema_version,
        lispb::codegen::make_test_registry(
            context,
            {{"logical", storage.cpp_types.add("model::LogicalRows")},
             {"physical", storage.cpp_types.add("model::CompactOwner")}}),
        context.storage.declarations,
        {[&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("model");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Model.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("model");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                    {rows,
                     [&] {
                         auto const new_soas{context.storage.declarations.soas.append()};
                         context.storage.declarations.soas
                             .name[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.strings.intern("Layout");
                         context.storage.declarations.soas
                             .members[context.storage.declarations.soas.index(new_soas)] =
                             context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                                 auto const new_soa_members{
                                     context.storage.declarations.soa_members.append()};
                                 context.storage.declarations.soa_members
                                     .name[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = context.storage.strings.intern("xs");
                                 context.storage.declarations.soa_members
                                     .kind[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = lispb::codegen::SoaMemberKind::array;
                                 context.storage.declarations.soa_members
                                     .type[context.storage.declarations.soa_members.index(
                                         new_soa_members)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("float");
                                     return new_type_refs;
                                 }();
                                 return new_soa_members;
                             }()});
                         context.storage.declarations.soas
                             .layout_only[context.storage.declarations.soas.index(new_soas)] = true;
                         return new_soas;
                     }(),
                     [&] {
                         auto const new_records{context.storage.declarations.records.append()};
                         context.storage.declarations.records
                             .name[context.storage.declarations.records.index(new_records)] =
                             context.storage.strings.intern("Reference");
                         context.storage.declarations.records
                             .members[context.storage.declarations.records.index(new_records)] =
                             context.storage.declarations.range<lispb::codegen::RecordMemberId>(
                                 {[&] {
                                     auto const new_record_members{
                                         context.storage.declarations.record_members.append()};
                                     context.storage.declarations.record_members
                                         .name[context.storage.declarations.record_members.index(
                                             new_record_members)] =
                                         context.storage.strings.intern("owner");
                                     context.storage.declarations.record_members
                                         .type[context.storage.declarations.record_members.index(
                                             new_record_members)] = [&] {
                                         auto const new_type_refs{
                                             context.storage.declarations.type_refs.append()};
                                         context.storage.declarations.type_refs
                                             .name[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] =
                                             context.storage.strings.intern("LogicalRows");
                                         context.storage.declarations.type_refs
                                             .suffix[context.storage.declarations.type_refs.index(
                                                 new_type_refs)] = context.storage.strings.add("*");
                                         return new_type_refs;
                                     }();
                                     return new_record_members;
                                 }()});
                         return new_records;
                     }()});
            context.storage.declarations.normal_modules
                .soa_backend[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] = lispb::codegen::SoaBackend::standard_library;
            return new_normal_modules;
        }()}};
    auto const graph{resolve_type_graph(manifest)};
    auto const id{lispb::codegen::find_test_type(graph, "model", "LogicalRows")};
    ASSERT_TRUE(id);
    EXPECT_EQ(text(storage, graph.name(*id)), "LogicalRows");
    EXPECT_EQ(text(storage, graph.cpp_spelling(*id)), "model::CompactOwner");
    EXPECT_EQ(graph.find_registered(storage.strings.find("logical")), id);
    EXPECT_EQ(graph.find_registered(storage.strings.find("physical")), id);
    EXPECT_EQ(graph.find_reference(
                  [&] {
                      auto const new_type_refs{storage.declarations.type_refs.append()};
                      storage.declarations.type_refs
                          .name[storage.declarations.type_refs.index(new_type_refs)] =
                          storage.strings.intern("CompactOwner");
                      return new_type_refs;
                  }(),
                  manifest.modules.front()),
              id);
    auto const& soa{graph.definition(*id)};
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions().soas.equivalent_type[graph.definitions().soas.index(soa)]));
    EXPECT_EQ(
        text(storage,
             graph.name(graph.occurrences().type[graph.occurrences().index(
                 graph.definitions().soas.equivalent_type[graph.definitions().soas.index(soa)])])),
        "Row");
    EXPECT_TRUE(
        text(storage, graph.cpp_spelling(*lispb::codegen::find_test_type(graph, "model", "Layout")))
            .empty());
    auto const& reference{
        graph.definition(*lispb::codegen::find_test_type(graph, "model", "Reference"))};
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(reference).front())])],
              *id);
    EXPECT_EQ(storage.cpp_types.spelling(graph.occurrences().cpp_type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(reference).front())])]),
              "model::CompactOwner*");
}

} // namespace
} // namespace lispb::schema
