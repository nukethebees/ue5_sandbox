#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <lispb/compiler_storage.h>
#include <lispb/project_loader.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout {
namespace {

TEST(PlannerType, OpaqueExternalFactsUnblockOnlyValueDependenciesOnTheActiveTarget) {
    lispb::CompilerStorage storage;
    auto const types{fixture_graph(storage, R"(
        (module external_users :header "external_users.hpp"
            (record Value (member value OpaqueVector))
            (record Pointer (member value (type-ref OpaqueVector :suffix "*")))))")};
    auto const dependencies{external_dependencies(types)};
    ASSERT_EQ(dependencies.size(), 1);
    auto const external{dependencies.front().types.front()};
    PlannerAnalysisSession session{types};
    auto aliased_profile{session.primary_abi()};
    aliased_profile.set_representation("OpaqueVector", "MissingSdkRepresentation");
    session.set_primary_abi(std::move(aliased_profile));
    session.inputs.selection.select_type(types, external);
    ASSERT_TRUE(session.refresh(nullptr));
    ASSERT_TRUE(session.results().external_type.has_value());
    auto const& before{*session.results().external_type};
    EXPECT_FALSE(before.semantics_known);
    EXPECT_FALSE(before.physical.facts.has_value());
    ASSERT_EQ(before.blocked_declarations.size(), 1);
    EXPECT_EQ(types.strings().view(before.blocked_declarations.front().name), "Value");
    TypeFacts facts{};
    facts.size_bytes = 3;
    facts.alignment_bytes = 4;
    EXPECT_FALSE(session.set_external_type_facts(external, facts));
    facts.size_bytes = 12;
    facts.origin = FactOrigin::compiler_probe;
    facts.provenance = "SDK assumption";
    ASSERT_TRUE(session.set_external_type_facts(external, facts));
    ASSERT_TRUE(session.refresh(nullptr));
    auto const& after{*session.results().external_type};
    EXPECT_FALSE(after.semantics_known);
    ASSERT_TRUE(after.physical.facts.has_value());
    EXPECT_EQ(after.physical.facts->origin, FactOrigin::manual_assumption);
    EXPECT_EQ(after.physical.facts->provenance, "SDK assumption");
    EXPECT_TRUE(after.blocked_declarations.empty());
    EXPECT_EQ(session.status(external), LayoutStatus::available);
    auto const inventory{external_dependencies(types, &session.primary_abi())};
    EXPECT_TRUE(inventory.front().physical_facts.has_value());
    EXPECT_FALSE(inventory.front().semantics_known);
    EXPECT_FALSE(session.comparison_abi().find("OpaqueVector").has_value());
    auto const imported{parse_abi_profile(serialize_abi_profile(session.primary_abi()))};
    ASSERT_TRUE(imported);
    EXPECT_EQ(imported->find("OpaqueVector"), after.physical.facts);
    auto const value{*fixture_find(types, "external_users", "Value")};
    auto const resolved{PhysicalFactsResolver{types, *imported}.resolve(value)};
    ASSERT_TRUE(resolved.facts.has_value());
    EXPECT_EQ(resolved.facts->size_bytes, 12);
    session.set_primary_abi(AbiProfile::host_common());
    ASSERT_TRUE(session.refresh(nullptr));
    EXPECT_FALSE(session.results().external_type->physical.facts.has_value());
}

TEST(PlannerType, ClassifiesInspectableAndPhysicalDeclarations) {
    auto const project{std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb"};
    auto loaded{load_lispb_schema(project, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};
    auto const enumeration{fixture_find(types, "native_entity_type", "EntityType")};
    auto const packed{fixture_find(types, "native_entity_unique_id", "EntityUniqueId")};
    auto const soa{fixture_find(types, "native_world_aabbs", "WorldAABBsColumns")};
    ASSERT_TRUE(enumeration.has_value());
    ASSERT_TRUE(packed.has_value());
    ASSERT_TRUE(soa.has_value());

    auto const enum_capabilities{declaration_capabilities(types, *enumeration)};
    EXPECT_EQ(enum_capabilities.kind, DeclarationKind::enumeration);
    EXPECT_TRUE(enum_capabilities.inspectable);
    EXPECT_TRUE(enum_capabilities.editable);
    EXPECT_TRUE(enum_capabilities.physical_analysis_available);
    EXPECT_FALSE(enum_capabilities.supports_variant_overrides);
    auto const packed_capabilities{declaration_capabilities(types, *packed)};
    EXPECT_EQ(packed_capabilities.kind, DeclarationKind::packed);
    EXPECT_TRUE(packed_capabilities.supports_variant_overrides);
    auto const soa_capabilities{declaration_capabilities(types, *soa)};
    EXPECT_EQ(soa_capabilities.kind, DeclarationKind::soa);
    EXPECT_TRUE(soa_capabilities.inspectable);
    EXPECT_TRUE(soa_capabilities.editable);
    EXPECT_TRUE(soa_capabilities.physical_analysis_available);
    EXPECT_TRUE(soa_capabilities.supports_variant_overrides);
}

auto backend_soa_graph(lispb::CompilerStorage& storage) -> lispb::schema::TypeGraph {
    std::string source;
    for (auto const standard : {false, true}) {
        auto const name{standard ? "stdlib" : "unreal"};
        source += "(module " + std::string{name} + " :header \"" + name + ".h\" :source \"" + name +
                  ".cpp\" :backend " + (standard ? "standard-library" : "unreal") + R"(
                    (struct Columns (member values array std::uint32_t))
                    (vector-soa VectorColumns :value-type std::uint32_t
                        :equivalent-type Vector :components (x y) :equivalent-members (x y))))";
    }
    return fixture_graph(storage, source);
}

TEST(PlannerType, SoaBackendAndSourceKindControlAnalysisAndAuthoring) {
    lispb::CompilerStorage storage;
    auto const types{backend_soa_graph(storage)};
    for (auto const* module : {"unreal", "stdlib"}) {
        auto const physical{std::string_view{module} == "stdlib"};
        for (auto const* name : {"Columns", "VectorColumns"}) {
            auto const vector{std::string_view{name} == "VectorColumns"};
            auto const type{fixture_find(types, module, name)};
            ASSERT_TRUE(type.has_value());
            auto const capabilities{declaration_capabilities(types, *type)};
            EXPECT_TRUE(capabilities.inspectable);
            EXPECT_EQ(capabilities.kind,
                      vector ? DeclarationKind::vector_soa : DeclarationKind::soa);
            EXPECT_EQ(capabilities.editable, physical && !vector);
            EXPECT_EQ(capabilities.physical_analysis_available, physical);
            EXPECT_EQ(capabilities.supports_variant_overrides, physical);

            PlannerAnalysisSession session{types};
            EXPECT_TRUE(
                session.inputs.selection.select_type(session.inputs.workspace.types(), *type));
            EXPECT_TRUE(session.refresh(nullptr));
            EXPECT_EQ(session.results().active_soa.has_value(), physical);
        }
    }
}

TEST(PlannerType, SwitchingToUnrealSoaClearsPhysicalResults) {
    lispb::CompilerStorage storage;
    auto const types{backend_soa_graph(storage)};
    auto const supported{*fixture_find(types, "stdlib", "Columns")};
    auto const unsupported{*fixture_find(types, "unreal", "Columns")};
    PlannerAnalysisSession session{types};
    EXPECT_TRUE(session.inputs.selection.select_type(session.inputs.workspace.types(), supported));
    EXPECT_TRUE(session.refresh(nullptr));
    ASSERT_TRUE(session.results().active_soa.has_value());

    EXPECT_TRUE(
        session.inputs.selection.select_type(session.inputs.workspace.types(), unsupported));
    EXPECT_TRUE(session.refresh(nullptr));
    EXPECT_FALSE(session.results().active_soa.has_value());
    EXPECT_FALSE(session.results().baseline_soa.has_value());
    EXPECT_FALSE(session.results().soa_target_comparison.has_value());

    EXPECT_TRUE(session.inputs.selection.select_type(session.inputs.workspace.types(), supported));
    EXPECT_TRUE(session.refresh(nullptr));
    EXPECT_TRUE(session.results().active_soa.has_value());
}

TEST(PlannerType, DistinguishesUnknownTargetFactsFromErrors) {
    auto const project{std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb"};
    auto loaded{load_lispb_schema(project, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};
    auto const packed{fixture_find(types, "native_entity_unique_id", "EntityUniqueId")};
    ASSERT_TRUE(packed.has_value());

    EXPECT_EQ(declaration_status(types, *packed, Variant{}, AbiProfile{"unknown"}, 64),
              LayoutStatus::unknown);
    auto const unknown_packed{
        Analyzer::analyze_packed(types, *packed, Variant{}, AbiProfile{"unknown"})};
    EXPECT_FALSE(unknown_packed.storage_facts.has_value());
    EXPECT_TRUE(std::ranges::any_of(unknown_packed.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::warning &&
               diagnostic.message.contains("Unknown physical facts for packed storage");
    }));
    EXPECT_EQ(declaration_status(types, *packed, Variant{}, AbiProfile::host_common(), 64),
              LayoutStatus::available);

    auto invalid{Variant{}};
    auto const& packed_type{types.definition(*packed)};
    auto const field{types.definitions().packed.segments_of(packed_type).front()};
    invalid.overrides.packed_field_widths.emplace(
        FieldOverrideId{
            .type = *packed,
            .field_name = std::string{types.strings().view(
                types.definitions()
                    .packed_fields.name[types.definitions().packed_fields.index(field)])}},
        1'024);
    EXPECT_EQ(declaration_status(types, *packed, invalid, AbiProfile::host_common(), 64),
              LayoutStatus::error);
}

TEST(PlannerType, TaggedUnionsNeedRealTargetFactsWhileSemanticScalarsDoNot) {
    lispb::CompilerStorage storage;
    auto const types{fixture_graph(storage, R"(
        (module enums :header "Enums.h" (enum Kind std::uint8_t (value Value)))
        (module scalars :header "Scalars.h"
            (integer-scalar Semantic :signed false :minimum 0 :maximum 10))
        (module unions :header "Unions.h"
            (tagged-union Tagged :discriminant Kind
                (alternative value std::uint32_t :tag Value))))")};
    auto const tagged_type{*fixture_find(types, "unions", "Tagged")};
    auto const semantic_type{*fixture_find(types, "scalars", "Semantic")};
    EXPECT_EQ(declaration_status(types, tagged_type, Variant{}, AbiProfile{"unknown"}, 1),
              LayoutStatus::unknown);
    EXPECT_EQ(declaration_status(types, tagged_type, Variant{}, AbiProfile::host_common(), 1),
              LayoutStatus::available);
    EXPECT_EQ(declaration_status(types, semantic_type, Variant{}, AbiProfile{"unknown"}, 1),
              LayoutStatus::available);
}

TEST(PlannerType, RelationshipCapacityErrorsMatchSelectedPackedAndScalarAnalysis) {
    lispb::CompilerStorage storage;
    for (auto const kind : {lispb::codegen::SemanticRelationKind::index_into,
                            lispb::codegen::SemanticRelationKind::count_of}) {
        auto const fixture{relationship_capacity_type(storage, kind, 8)};
        PlannerAnalysisSession session{fixture.types};
        session.inputs.selection.select_type(session.inputs.workspace.types(), fixture.packed);
        ASSERT_TRUE(session.refresh(nullptr));
        ASSERT_TRUE(session.results().active_packed.has_value());
        EXPECT_TRUE(std::ranges::any_of(session.results().active_packed->diagnostics,
                                        [](Diagnostic const& diagnostic) {
                                            return diagnostic.severity == DiagnosticSeverity::error;
                                        }));
        EXPECT_EQ(session.status(fixture.packed), LayoutStatus::error);
    }

    auto const scalar_fixture{relationship_capacity_scalar_type(
        storage, lispb::codegen::SemanticRelationKind::index_into, 8, 255)};
    PlannerAnalysisSession scalar_session{scalar_fixture.types};
    scalar_session.inputs.selection.select_type(scalar_session.inputs.workspace.types(),
                                                scalar_fixture.packed);
    ASSERT_TRUE(scalar_session.refresh(nullptr));
    ASSERT_TRUE(scalar_session.results().integer_scalar_analysis.has_value());
    EXPECT_TRUE(std::ranges::any_of(scalar_session.results().integer_scalar_analysis->diagnostics,
                                    [](Diagnostic const& diagnostic) {
                                        return diagnostic.severity == DiagnosticSeverity::error;
                                    }));
    EXPECT_EQ(scalar_session.status(scalar_fixture.packed), LayoutStatus::error);
}

TEST(PlannerType, UnknownSoaColumnFactsRemainUnknown) {
    lispb::CompilerStorage storage;
    auto const fixture{soa_type(storage, {{"opaque", "UnknownColumnType"}})};
    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 64)};
    ASSERT_EQ(analysis.columns.size(), 1);
    EXPECT_FALSE(analysis.columns.front().type_facts.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_TRUE(std::ranges::any_of(analysis.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::warning &&
               diagnostic.message.contains("Unknown physical facts for SoA column");
    }));
    EXPECT_EQ(
        declaration_status(fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 64),
        LayoutStatus::unknown);
}

TEST(PlannerType, StatusTracksActiveVariantRelationshipOverrides) {
    lispb::CompilerStorage storage;
    auto const fixture{
        relationship_capacity_type(storage, lispb::codegen::SemanticRelationKind::index_into, 17)};
    PlannerAnalysisSession session{fixture.types};
    EXPECT_EQ(session.status(fixture.packed), LayoutStatus::available);

    session.inputs.workspace.create_variant("narrow");
    ASSERT_TRUE(session.inputs.workspace.set_packed_field_width(fixture.packed, "value", 8));
    session.inputs.selection.select_type(session.inputs.workspace.types(), fixture.packed);
    ASSERT_TRUE(session.refresh(nullptr));
    EXPECT_EQ(session.status(fixture.packed), LayoutStatus::error);
    ASSERT_TRUE(session.results().active_packed.has_value());
    EXPECT_TRUE(std::ranges::any_of(session.results().active_packed->diagnostics,
                                    [](Diagnostic const& diagnostic) {
                                        return diagnostic.severity == DiagnosticSeverity::error;
                                    }));
}

TEST(PlannerType, VarintStatusUsesSelectedElementCount) {
    lispb::CompilerStorage storage;
    auto const fixture{integer_varint_type(
        storage, false, 0, 16'384, lispb::codegen::IntegerVarintEncoding::unsigned_varint)};
    auto const large_count{(std::numeric_limits<std::uint64_t>::max)()};
    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type, large_count)};
    EXPECT_FALSE(analysis.maximum_total_bytes.has_value());
    EXPECT_TRUE(std::ranges::any_of(analysis.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::error;
    }));

    auto const abi{AbiProfile::host_common()};
    EXPECT_EQ(declaration_status(fixture.types,
                                 fixture.type,
                                 Variant{},
                                 abi,
                                 64,
                                 large_count,
                                 SoaAllocationStrategy::separate_columns,
                                 {}),
              LayoutStatus::error);
    EXPECT_EQ(declaration_status(fixture.types,
                                 fixture.type,
                                 Variant{},
                                 abi,
                                 64,
                                 1,
                                 SoaAllocationStrategy::separate_columns,
                                 {}),
              LayoutStatus::available);

    PlannerAnalysisSession session{fixture.types};
    EXPECT_EQ(session.status(fixture.type), LayoutStatus::available);
    ASSERT_TRUE(session.inputs.workspace.set_element_count(large_count));
    EXPECT_EQ(session.status(fixture.type), LayoutStatus::error);
}

} // namespace
} // namespace ioj::layout
