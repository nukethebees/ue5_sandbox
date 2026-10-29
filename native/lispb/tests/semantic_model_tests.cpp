#include "test_utilities.h"

#include <codegen/ast/indices.h>
#include <codegen/schema.h>
#include <codegen/schema/child_occurrences.h>
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
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>
#include <lispb/schema/child_occurrences.h>
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
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_origin.h>
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
    auto const& record_members{graph.definitions().children};
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
                    enums_schemas.cpp_projection[new_enums_row] = lispb::codegen::CppProjection{};
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

TEST(SemanticTypeGraph, FindsContiguousEnumValidationRangesWithoutChangingCodes) {
    auto const check = [](std::string_view body,
                          std::optional<ioj::MinMax<std::uint64_t>> expected) {
        SCOPED_TRACE(body);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{"(module values :header \"Values.h\" (enum State "} +
                          std::string{body} + "))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        auto const& declarations{context.storage.declarations};
        auto const enumeration{
            declarations.items(declarations.normal_modules.declarations[manifest.modules.front()])
                .front()};
        auto const domains{lispb::schema::analyze_enum_domains(manifest)};
        EXPECT_EQ(domains.contiguous_value_range(domains.find(enumeration)), expected);
    };
    check("(value First) (value Second) (value Third)", ioj::MinMax<std::uint64_t>{0, 2});
    check("(value Last :value 9) (value First :value 7) (value Middle :value 8) "
          "(value Alias :value 8)",
          ioj::MinMax<std::uint64_t>{7, 9});
    check(":count COUNT (value First) (value Second) (value COUNT)",
          ioj::MinMax<std::uint64_t>{0, 1});
    check("(value First :value 4) (value Reserved :value 5 :sentinel true) (value Last :value 6)",
          ioj::MinMax<std::uint64_t>{4, 6});
    check(":count COUNT (value First :value 7) (value COUNT :value 8) (value Last :value 8)",
          ioj::MinMax<std::uint64_t>{7, 8});
    check("(value First :value 18446744073709551614) (value Last)",
          ioj::MinMax<std::uint64_t>{18446744073709551614ULL, 18446744073709551615ULL});
    check("(value Last :value 18446744073709551615)",
          ioj::MinMax<std::uint64_t>{18446744073709551615ULL, 18446744073709551615ULL});
    check(":count COUNT (value First) (value COUNT :value EXTERNAL)",
          ioj::MinMax<std::uint64_t>{0, 0});
    check("(value First :value 0) (value Last :value 2)", std::nullopt);
    check("(value First :value -1) (value Last)", std::nullopt);
    check("(value First :value EXTERNAL)", std::nullopt);
    check(":count COUNT (value COUNT)", std::nullopt);
    check("", std::nullopt);
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
                    enums_schemas.cpp_projection[new_enums_row] = lispb::codegen::CppProjection{};
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
    auto const graph{production_graph(context.storage)};
    auto const entity_type{
        *lispb::codegen::find_test_type(graph, "native_entity_type", "EntityType")};
    auto const entity_id{*lispb::codegen::find_test_type(graph, "native_entity_id", "EntityId")};
    auto const definition{graph.definition(entity_id)};
    auto const& records{graph.definitions().records};
    EXPECT_EQ(definition.kind(), TypeDefinitionKind::record);
    EXPECT_EQ(records.layout[records.index(definition)], lispb::codegen::LayoutKind::packed);
    EXPECT_EQ(records.invalid_raw_value[records.index(definition)], 0xffffffffU);
    auto const& layout{graph.bit_layout(entity_id)};
    EXPECT_EQ(layout.encoded_bits, 32U);
    EXPECT_EQ(layout.storage_bits, 32U);
    ASSERT_EQ(layout.placements.size(), 2U);
    EXPECT_EQ(layout.placements[0].path, "index");
    EXPECT_EQ(layout.placements[0].offset, 0U);
    EXPECT_EQ(layout.placements[0].bits, 24U);
    EXPECT_EQ(layout.placements[1].path, "entity_type");
    EXPECT_EQ(layout.placements[1].offset, 24U);
    EXPECT_EQ(layout.placements[1].bits, 8U);
    EXPECT_EQ(layout.placements[1].type, entity_type);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(entity_id), entity_type),
              graph.dependencies_of(entity_id).end());
    EXPECT_NE(std::ranges::find(graph.users_of(entity_type), entity_id),
              graph.users_of(entity_type).end());
}

TEST(SemanticTypeGraph, ResolvesPackedIntegerScalarDomainAndDependency) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module domains
        (integer-scalar Health :minimum 0 :maximum 1000
          (code Invalid :value 4095 :sentinel true)))
      (module packed :header "Packed.h" :namespace project
        (record Status :layout packed (member health @domains/Health)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const health{*lispb::codegen::find_test_type(graph, "domains", "Health")};
    auto const status{*lispb::codegen::find_test_type(graph, "packed", "Status")};
    auto const& scalars{graph.definitions().integer_scalars};
    auto const scalar{graph.definition(health)};
    auto const row{scalars.index(scalar)};
    EXPECT_EQ(scalars.bit_width[row], 12U);
    EXPECT_EQ(scalars.minimum_value[row], lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(scalars.maximum_value[row], lispb::codegen::PackedIntegerValue{1000});
    EXPECT_EQ(scalars.named_codes[row].size(), 1U);
    EXPECT_FALSE(is_valid(scalars.cpp_representation[row]));
    auto const& layout{graph.bit_layout(status)};
    ASSERT_EQ(layout.placements.size(), 1U);
    EXPECT_EQ(layout.placements[0].type, health);
    EXPECT_EQ(layout.placements[0].bits, 12U);
    EXPECT_EQ(layout.storage_bits, 16U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(status), health),
              graph.dependencies_of(status).end());
    EXPECT_NE(std::ranges::find(graph.users_of(health), status), graph.users_of(health).end());
}

TEST(SemanticTypeGraph, RetainsPackedPhysicalOrdering) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module wire :header "Wire.h"
        (record Header :layout packed :bits 32 :byte-order big :bit-order msb-first
          (member kind std::uint8_t)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const header{*lispb::codegen::find_test_type(graph, "wire", "Header")};
    auto const& records{graph.definitions().records};
    auto const row{records.index(graph.definition(header))};
    EXPECT_EQ(records.byte_order[row], lispb::codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(records.bit_order[row], lispb::codegen::PackedBitOrder::most_significant_first);
    ASSERT_EQ(graph.bit_layout(header).placements.size(), 1U);
    EXPECT_EQ(graph.bit_layout(header).placements[0].offset, 24U);
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

class InvalidNestedSoaType : public testing::TestWithParam<char const*> {};

TEST_P(InvalidNestedSoaType, RejectsNestedMembersWithoutDeclaredSoaTypes) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(
        context,
        std::string{"(module test :header \"Test.h\" "
                    "(record Record (member value float)) "
                    "(struct Child (member values array float)) "
                    "(struct Parent (member child nested \""} +
            GetParam() + "\")))")};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SemanticTypeGraph,
                         InvalidNestedSoaType,
                         testing::Values("Missing", "int", "Record", "Child*", "Child const"));

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
    auto& record_members_schemas{declarations.child_occurrences};

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
                         records_schemas.cpp_projection[new_records_row] =
                             lispb::codegen::CppProjection{};
                         records_schemas.name[new_records_row] = strings.intern("Position");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
                         records_schemas.cpp_projection[new_records_row] =
                             lispb::codegen::CppProjection{};
                         records_schemas.name[new_records_row] = strings.intern("Table");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
                         records_schemas.cpp_projection[new_records_row] =
                             lispb::codegen::CppProjection{};
                         records_schemas.name[new_records_row] = strings.intern("Path");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto const& record_members{graph.definitions().children};
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
    auto& record_members{declarations.child_occurrences};

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
                records.cpp_projection[new_records_row] = lispb::codegen::CppProjection{};
                records.name[new_records_row] = strings.intern("Record");
                records.members[new_records_row] =
                    declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
                 declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
             records.cpp_projection[new_records_row] = lispb::codegen::CppProjection{};
             records.name[new_records_row] = strings.intern("Second");
             records.members[new_records_row] =
                 declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto& record_members{declarations.child_occurrences};
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
                    records.cpp_projection[new_records_row] = lispb::codegen::CppProjection{};
                    records.name[new_records_row] = strings.intern("Node");
                    records.members[new_records_row] =
                        declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto& union_alternatives_schemas{declarations.child_occurrences};

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
                    unions_schemas.cpp_projection[new_unions_row] = lispb::codegen::CppProjection{};
                    unions_schemas.name[new_unions_row] = strings.intern("Payload");
                    unions_schemas.alternatives[new_unions_row] =
                        declarations.range<lispb::codegen::ChildOccurrenceId>(
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
    auto const& union_alternatives{graph.definitions().children};
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
    auto& union_alternatives{declarations.child_occurrences};

    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.child_occurrences};
    auto& strings{context.storage.strings};
    [[maybe_unused]] auto& storage{context.storage};
    auto& semantic_declarations{storage.declarations};
    auto& semantic_strings{storage.strings};
    auto const union_schema{unions.append()};
    auto const union_schema_row{unions.index(union_schema)};
    unions.name[union_schema_row] = strings.intern("Payload");
    unions.alternatives[union_schema_row] =
        declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    records.members[record_schema_row] =
        declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module payloads :header "Payloads.h"
        (enum Kind :bit-width 2 (value Point) (value Samples))
        (integer-scalar Coordinate :bit-width 8)
        (record Point (member x @Coordinate) (member y @Coordinate))
        (record Samples (member values @Coordinate :count 4))
        (union Payload (case Point @Point) (case Samples @Samples))
        (record Event :layout packed :bits 64
          (member tag @Kind)
          (member payload @Payload :bits 62 :tag tag)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const event{*lispb::codegen::find_test_type(graph, "payloads", "Event")};
    auto const payload{*lispb::codegen::find_test_type(graph, "payloads", "Payload")};
    auto const kind{*lispb::codegen::find_test_type(graph, "payloads", "Kind")};
    auto const& children{graph.definitions().children};
    auto const members{graph.definitions().records.members_of(graph.definition(event))};
    ASSERT_EQ(members.size(), 2U);
    EXPECT_EQ(text(context.storage, children.tag[children.index(members[1])]), "tag");
    auto const alternatives{graph.definitions().unions.alternatives_of(graph.definition(payload))};
    ASSERT_EQ(alternatives.size(), 2U);
    EXPECT_EQ(text(context.storage, children.case_label[children.index(alternatives[0])]), "Point");
    auto const& layout{graph.bit_layout(event)};
    EXPECT_EQ(layout.encoded_bits, 64U);
    auto const leaf{std::ranges::find(layout.placements, "payload_point_x", &BitPlacement::path)};
    ASSERT_NE(leaf, layout.placements.end());
    EXPECT_EQ(leaf->offset, 2U);
    EXPECT_EQ(leaf->bits, 8U);
    ASSERT_EQ(leaf->guards.size(), 1U);
    EXPECT_EQ(leaf->guards[0].enumeration, kind);
    EXPECT_EQ(leaf->guards[0].offset, 0U);
    EXPECT_EQ(leaf->guards[0].value, 0U);
    EXPECT_NE(std::ranges::find(graph.dependencies_of(event), payload),
              graph.dependencies_of(event).end());
    EXPECT_NE(std::ranges::find(graph.dependencies_of(event), kind),
              graph.dependencies_of(event).end());
}

TEST(SemanticTypeGraph, RejectsPackedFixedPointHelperNameCollisions) {
    for (auto const name : {"distance_value",
                            "set_distance_value",
                            "try_encode_distance_value",
                            "distance_fractional_bits",
                            "distance_raw_layout"}) {
        SCOPED_TRACE(name);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{R"(
          (module values :header "Values.h"
            (fixed-point Distance :signed true :total-bits 16 :fractional-bits 4
              :rounding toward-zero)
            (record Position :layout packed
              (member distance @Distance)
              (member )"} +
                          name + R"( std::uint8_t)
              (projections (cpp)))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
}

TEST(SemanticTypeGraph, RejectsPackedAccessorMatchingOwningTypeName) {
    for (auto const kind : {"record", "union"}) {
        SCOPED_TRACE(kind);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{"(module values :header \"Values.h\" ("} + kind +
                          " value :layout packed (member value std::uint8_t) "
                          "(projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
}

TEST(SemanticTypeGraph, RejectsPackedRootMetadataNameCollisions) {
    for (auto const name : {"extract", "insert", "storage", "storage_type", "packed_integer_tag"}) {
        SCOPED_TRACE(name);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{R"((module values :header "Values.h"
          (record Value :layout packed (member )"} +
                          name + " std::uint8_t) (projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
    for (auto const kind : {"record", "union"}) {
        for (auto const name : {"Base",
                                "extract",
                                "insert",
                                "storage",
                                "storage_type",
                                "packed_integer_tag",
                                "raw_value",
                                "from_raw",
                                "is_valid",
                                "value_"}) {
            SCOPED_TRACE(kind);
            SCOPED_TRACE(name);
            lispb::codegen::CompilerTestContext context;
            auto const source{std::string{"(module values :header \"Values.h\" ("} + kind + ' ' +
                              name +
                              " :layout packed (member data std::uint8_t) (projections (cpp))))"};
            auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
            EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
        }
        lispb::codegen::CompilerTestContext context;
        auto const source{
            std::string{"(module values :header \"Values.h\" ("} + kind +
            " RawTag :layout packed (member Base std::uint8_t) (projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_NO_THROW(resolve_type_graph(manifest));
    }
}

TEST(SemanticTypeGraph, RejectsScalarNamedCodesCollidingWithDomainMetadata) {
    for (auto const name :
         {"Base", "extract", "insert", "storage", "packed_integer_tag", "value_layout"}) {
        SCOPED_TRACE(name);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{R"((module values :header "Values.h"
          (integer-scalar Value :bit-width 8 (code )"} +
                          name + " :value 1) (projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
    for (auto const name : {"Base",
                            "extract",
                            "insert",
                            "storage",
                            "storage_type",
                            "packed_integer_tag",
                            "value_type",
                            "value_layout",
                            "value",
                            "set_value",
                            "is_valid_value",
                            "raw_value",
                            "from_raw",
                            "is_valid",
                            "value_"}) {
        SCOPED_TRACE(name);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{"(module values :header \"Values.h\" (integer-scalar "} +
                          name + " :bit-width 8 (projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
}

TEST(SemanticTypeGraph, RejectsPackedValidationAndEncodingMetadataCollisions) {
    for (auto const source : {
             R"((module values :header "Values.h"
               (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100)
               (record Value :layout packed (member health @Health)
                 (member is_valid_health std::uint8_t) (projections (cpp)))))",
             R"((module values :header "Values.h"
               (enum Mode :bit-width 8 (value first) (projections (cpp)))
               (record Value :layout packed (member mode @Mode)
                 (member mode_layout std::uint8_t) (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100)
               (optional-presence-bit MaybeHealth :source @Health)
               (record Value :layout packed (member maybe @MaybeHealth)
                 (member maybe_encoded_payload_layout std::uint8_t) (projections (cpp)))))",
             R"((module values :header "Values.h"
               (record invalid_value :layout packed :invalid-value 255
                 (member data std::uint8_t) (projections (cpp)))))",
             R"((module values :header "Values.h"
               (fixed-point value_layout :signed false :total-bits 8 :fractional-bits 0 :minimum 0
                 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (fixed-point packed_integer_tag :signed false :total-bits 8 :fractional-bits 0 :maximum 100
                 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8)
               (linear-quantized value_layout :source @Source :bits 6 :reserved-codes 1
                 :clipping clamp (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8)
               (optional-presence-bit payload_layout :source @Source (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8)
               (optional-presence-bit value_layout :source @Source (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8 :minimum 0 :maximum 100 (code Absent :value 255 :sentinel true))
               (optional-sentinel sentinel :source @Source :sentinel Absent (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8 :minimum 0 :maximum 100 (code Invalid :value 255 :sentinel true))
               (optional-sentinel payload_Invalid :source @Source :sentinel Invalid (projections (cpp)))))"}) {
        SCOPED_TRACE(source);
        lispb::codegen::CompilerTestContext context;
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
    for (auto const source : {
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8)
               (optional-presence-bit presence_mask :source @Source (projections (cpp)))
               (fixed-point Distance :signed true :total-bits 16 :fractional-bits 4)
               (record Position :layout packed
                 (member distance @Distance)
                 (member distance_magnitude_bits std::uint8_t)
                 (member maybe @presence_mask)
                 (member maybe_encoded_presence_mask std::uint8_t)
                 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (record value_type :layout packed (member data std::uint8_t) (projections (cpp)))
               (record invalid_value :layout packed (member data std::uint8_t) (projections (cpp)))
               (integer-scalar value_field :bit-width 8 (projections (cpp)))
               (integer-scalar RawTag :bit-width 8 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Source :bit-width 8)
               (optional-presence-bit payload_type :source @Source (projections (cpp)))
               (optional-presence-bit payload_field :source @Source (projections (cpp)))
               (record Value :layout packed
                 (member maybe @payload_type)
                 (member maybe_encoded_payload_type std::uint8_t)
                 (member maybe_encoded_payload_field std::uint8_t)
                 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (fixed-point minimum_value :signed false :total-bits 8 :fractional-bits 0 :minimum 0 (projections (cpp)))
               (fixed-point maximum_value :signed false :total-bits 8 :fractional-bits 0 :maximum 100 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (enum Mode :bit-width 8 (value first) (projections (cpp)))
               (record Value :layout packed (member mode @Mode)
                 (member mode_first_code std::uint8_t)
                 (member mode_range std::uint8_t)
                 (member mode_range_type std::uint8_t)
                 (member mode_type std::uint8_t)
                 (member mode_field std::uint8_t)
                 (projections (cpp)))))",
             R"((module values :header "Values.h"
               (integer-scalar Value :bit-width 8
                 (code minimum_value :value 1) (code maximum_value :value 2)
                 (projections (cpp)))
               (record Metadata :layout packed
                 (member encoded_bits std::uint8_t)
                 (member storage_bits std::uint8_t)
                 (member default_raw_value std::uint8_t)
                 (projections (cpp)))))"}) {
        SCOPED_TRACE(source);
        lispb::codegen::CompilerTestContext context;
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_NO_THROW(resolve_type_graph(manifest));
    }
}

TEST(SemanticTypeGraph, RejectsInvalidPackedDefaultsAcrossEncodings) {
    for (auto const body : {"(member value std::uint8_t :default -1)",
                            "(member value std::int8_t :default 255)",
                            "(member value @Signed :default 255)",
                            "(member value @Unsigned :default -1)",
                            "(member value @Present :default 456)",
                            "(member value @Sentinel :default 200)",
                            "(member value @SignedPresent :default 384)",
                            "(member value @SignedSentinel :default 128)",
                            "(member value @Child :default 1)",
                            "(member value @Choices :default 1)",
                            ("(member kind @Kind) (member payload @Choices :tag kind) "
                             "(member value @Health :default 999)")}) {
        SCOPED_TRACE(body);
        lispb::codegen::CompilerTestContext context;
        auto const source{std::string{R"((module values :header "Values.h"
          (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100
            (code Absent :value 255 :sentinel true))
          (fixed-point Signed :signed true :total-bits 8 :fractional-bits 0)
          (fixed-point Unsigned :signed false :total-bits 8 :fractional-bits 0)
          (optional-presence-bit Present :source @Health)
          (optional-sentinel Sentinel :source @Health :sentinel Absent)
          (integer-scalar SignedHealth :signed true :bit-width 8 :minimum -100 :maximum 100
            (code Absent :value -127 :sentinel true))
          (optional-presence-bit SignedPresent :source @SignedHealth)
          (optional-sentinel SignedSentinel :source @SignedHealth :sentinel Absent)
          (record Child (member value @Health))
          (enum Kind :bit-width 1 (value Child))
          (union Choices (member child @Child :case Child))
          (record Value :layout packed )"} +
                          body + " (projections (cpp))))"};
        auto const manifest{lispb::codegen::parse_test_manifest(context, source)};
        EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
    }
}

TEST(SemanticTypeGraph, AcceptsValidPackedOptionalAndSignedDefaults) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100
          (code Absent :value 255 :sentinel true))
        (integer-scalar SignedHealth :signed true :bit-width 8 :minimum -100 :maximum 100
          (code Absent :value -128 :sentinel true))
        (fixed-point Signed :signed true :total-bits 8 :fractional-bits 0)
        (optional-presence-bit Present :source @Health)
        (optional-sentinel Sentinel :source @Health :sentinel Absent)
        (optional-presence-bit SignedPresent :source @SignedHealth)
        (optional-sentinel SignedSentinel :source @SignedHealth :sentinel Absent)
        (record Value :layout packed
          (member signed_value @Signed :default -1)
          (member absent @Present :default 200)
          (member present @Present :default 356)
          (member sentinel @Sentinel :default 255)
          (member signed_present @SignedPresent :default 511)
          (member signed_sentinel @SignedSentinel :default 128)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const type{*lispb::codegen::find_test_type(graph, "values", "Value")};
    EXPECT_TRUE(graph.bit_layout(type).default_raw_value.has_value());
}

TEST(SemanticTypeGraph, RejectsSignedEnumProjectionInPackedRecord) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (enum Kind :bit-width 8 (value First) (value Second)
          (projections (cpp :storage std::int16_t)))
        (record Value :layout packed (member kind @Kind) (projections (cpp)))))")};
    EXPECT_THROW(resolve_type_graph(manifest), std::invalid_argument);
}

TEST(SemanticTypeGraph, PackedEnumProjectionOverridesAuthoredUnderlyingStorage) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (enum Kind std::int16_t :bit-width 8 (value First) (value Second)
          (projections (cpp :storage std::uint16_t)))
        (record Value :layout packed (member kind @Kind) (projections (cpp)))))")};
    EXPECT_NO_THROW(resolve_type_graph(manifest));
}

TEST(SemanticTypeGraph, TaggedPackedDefaultsRemainDisabledAfterValidation) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100)
        (enum Kind :bit-width 1 (value Health))
        (union Choices (member health @Health :default 42))
        (record Value :layout packed
          (member kind @Kind :default 0)
          (member payload @Choices :tag kind)
          (member other @Health :default 100)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const type{*lispb::codegen::find_test_type(graph, "values", "Value")};
    EXPECT_FALSE(graph.bit_layout(type).default_raw_value.has_value());
}

TEST(SemanticTypeGraph, UnionDefaultsSelectOneCompleteAlternative) {
    lispb::codegen::CompilerTestContext context;
    auto const manifest{lispb::codegen::parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Code :bit-width 2 :minimum 1 :maximum 2)
        (union Choice :layout packed
          (member first @Code :default 1)
          (member second @Code :default 2)
          (projections (cpp)))
        (union Partial :layout packed
          (member first @Code)
          (member second @Code :default 2)
          (projections (cpp)))
        (record Nested :layout packed
          (member choice @Choice)
          (member suffix @Code :default 2)
          (projections (cpp)))))")};
    auto const graph{resolve_type_graph(manifest)};
    auto const choice{*lispb::codegen::find_test_type(graph, "values", "Choice")};
    auto const partial{*lispb::codegen::find_test_type(graph, "values", "Partial")};
    auto const nested{*lispb::codegen::find_test_type(graph, "values", "Nested")};
    EXPECT_EQ(graph.bit_layout(choice).default_raw_value, 1U);
    EXPECT_EQ(graph.bit_layout(partial).default_raw_value, 2U);
    EXPECT_EQ(graph.bit_layout(nested).default_raw_value, 9U);
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
                    enums.cpp_projection[new_enums_row] = lispb::codegen::CppProjection{};
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
                    enums.cpp_projection[new_enums_row] = lispb::codegen::CppProjection{};
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

class InvalidDeclaredIdentities : public testing::TestWithParam<std::vector<std::string>> {};

TEST_P(InvalidDeclaredIdentities, RejectsInvalidAndDuplicateDeclaredIdentities) {
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

    std::vector<lispb::codegen::DeclarationHandle> enums;
    for (auto const& name : GetParam()) {
        enums.push_back(make_enum(name));
    }
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::make_test_registry(context),
                                      semantic_declarations,
                                      {make_module("types", enums)}};
    EXPECT_THROW(static_cast<void>(resolve_type_graph(manifest)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SemanticTypeGraph,
                         InvalidDeclaredIdentities,
                         testing::Values(std::vector<std::string>{""},
                                         std::vector<std::string>{"Mode", "Mode"}));

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
                    enums.cpp_projection[new_enums_row] = lispb::codegen::CppProjection{};
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
    auto& record_members_schemas{declarations.child_occurrences};
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
                          records_schemas.cpp_projection[new_records_row] =
                              lispb::codegen::CppProjection{};
                          records_schemas.name[new_records_row] = strings.intern("Foo");
                          records_schemas.members[new_records_row] =
                              declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
                          records_schemas.cpp_projection[new_records_row] =
                              lispb::codegen::CppProjection{};
                          records_schemas.name[new_records_row] = strings.intern("Bar");
                          records_schemas.members[new_records_row] =
                              declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
                          records_schemas.cpp_projection[new_records_row] =
                              lispb::codegen::CppProjection{};
                          records_schemas.name[new_records_row] = strings.intern("User");
                          records_schemas.members[new_records_row] = declarations.range<
                              lispb::codegen::ChildOccurrenceId>(
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
                     records_schemas.cpp_projection[new_records_row] =
                         lispb::codegen::CppProjection{};
                     records_schemas.name[new_records_row] = strings.intern("Foo");
                     records_schemas.members[new_records_row] =
                         declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto const& semantic_record_members{graph.definitions().children};
    auto const& semantic_records{graph.definitions().records};
    auto const& occurrences{graph.occurrences()};
    auto const& store{storage.declarations};
    auto const module{manifest.modules.front()};
    auto const schema{
        store.items(store.normal_modules.declarations[store.normal_modules.index(module)]).back()};
    auto const& user{graph.definition(*lispb::codegen::find_test_type(graph, "A", "User"))};
    auto const bound_member_count{store.records.members[store.records.index(schema)].size() - 1};
    for (std::size_t index{}; index < bound_member_count; ++index) {
        EXPECT_EQ(graph.find_reference(
                      store.child_occurrences.type[store.child_occurrences.index(
                          store.items(store.records.members[store.records.index(schema)])[index])],
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
    auto& record_members_schemas{declarations.child_occurrences};
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
                         records_schemas.cpp_projection[new_records_row] =
                             lispb::codegen::CppProjection{};
                         records_schemas.name[new_records_row] = strings.intern("Reference");
                         records_schemas.members[new_records_row] =
                             declarations.range<lispb::codegen::ChildOccurrenceId>({[&] {
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
    auto const& record_members{graph.definitions().children};
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
        "model::LogicalRows*");
}

} // namespace
} // namespace lispb::schema
