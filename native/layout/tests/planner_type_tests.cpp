#include "analyzer_test_fixtures.hpp"

#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <string_view>

namespace ioj::layout {
namespace {

TEST(PlannerType, OpaqueExternalFactsUnblockOnlyValueDependenciesOnTheActiveTarget) {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema module{};
    module.settings.name = "external_users";
    module.settings.header = "external_users.hpp";
    for (auto const pointer : {false, true}) {
        codegen::RecordSchema record{};
        record.name = pointer ? "Pointer" : "Value";
        codegen::RecordMemberSchema member{};
        member.name = "value";
        member.type.name = "OpaqueVector";
        member.type.suffix = pointer ? "*" : "";
        record.members.push_back(member);
        module.declarations.push_back(record);
    }
    manifest.modules.emplace_back(module);
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const dependencies{external_dependencies(types)};
    ASSERT_EQ(dependencies.size(), 1);
    auto const external{dependencies.front().types.front()};
    PlannerAnalysisSession session{types};
    session.inputs.selection.select_type(types, external);
    ASSERT_TRUE(session.refresh(nullptr));
    ASSERT_TRUE(session.results().external_type.has_value());
    auto const& before{*session.results().external_type};
    EXPECT_FALSE(before.semantics_known);
    EXPECT_FALSE(before.physical.facts.has_value());
    ASSERT_EQ(before.blocked_declarations.size(), 1);
    EXPECT_EQ(before.blocked_declarations.front().name, "Value");
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
    auto const inventory{external_dependencies(types, &session.primary_abi())};
    EXPECT_TRUE(inventory.front().physical_facts.has_value());
    EXPECT_FALSE(inventory.front().semantics_known);
    EXPECT_FALSE(session.comparison_abi().find("OpaqueVector").has_value());
    auto const imported{parse_abi_profile(serialize_abi_profile(session.primary_abi()))};
    ASSERT_TRUE(imported);
    EXPECT_EQ(imported->find("OpaqueVector"), after.physical.facts);
    auto const value{*types.find_declared("external_users", "Value")};
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
    auto const enumeration{types.find_declared("native_entity_type", "EntityType")};
    auto const packed{types.find_declared("native_entity_unique_id", "EntityUniqueId")};
    auto const soa{types.find_declared("native_world_aabbs", "WorldAABBsColumns")};
    ASSERT_TRUE(enumeration.has_value());
    ASSERT_TRUE(packed.has_value());
    ASSERT_TRUE(soa.has_value());

    auto const enum_capabilities{declaration_capabilities(types.type(*enumeration))};
    EXPECT_EQ(enum_capabilities.kind, DeclarationKind::enumeration);
    EXPECT_TRUE(enum_capabilities.inspectable);
    EXPECT_TRUE(enum_capabilities.editable);
    EXPECT_TRUE(enum_capabilities.physical_analysis_available);
    EXPECT_FALSE(enum_capabilities.supports_variant_overrides);
    auto const packed_capabilities{declaration_capabilities(types.type(*packed))};
    EXPECT_EQ(packed_capabilities.kind, DeclarationKind::packed);
    EXPECT_TRUE(packed_capabilities.supports_variant_overrides);
    auto const soa_capabilities{declaration_capabilities(types.type(*soa))};
    EXPECT_EQ(soa_capabilities.kind, DeclarationKind::soa);
    EXPECT_TRUE(soa_capabilities.inspectable);
    EXPECT_TRUE(soa_capabilities.editable);
    EXPECT_TRUE(soa_capabilities.physical_analysis_available);
    EXPECT_TRUE(soa_capabilities.supports_variant_overrides);
}

auto backend_soa_graph() -> lispb::schema::TypeGraph {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    for (auto const backend :
         {codegen::SoaBackend::unreal, codegen::SoaBackend::standard_library}) {
        codegen::NormalModuleSchema module{};
        module.settings.name = backend == codegen::SoaBackend::unreal ? "unreal" : "stdlib";
        module.settings.header = module.settings.name + ".h";
        module.settings.source = module.settings.name + ".cpp";
        module.soa_backend = backend;
        codegen::SoaSchema soa{};
        soa.name = "Columns";
        codegen::SoaMemberSchema member{};
        member.name = "values";
        member.kind = codegen::SoaMemberKind::array;
        member.type.name = "std::uint32_t";
        soa.members.push_back(std::move(member));
        module.declarations.push_back(std::move(soa));
        codegen::VectorSoaSchema vector{};
        vector.name = "VectorColumns";
        vector.value_type.name = "std::uint32_t";
        vector.equivalent_type.name = "Vector";
        vector.components = {"x", "y"};
        vector.equivalent_members = {"x", "y"};
        module.declarations.push_back(std::move(vector));
        manifest.modules.emplace_back(std::move(module));
    }
    return lispb::schema::resolve_type_graph(manifest);
}

TEST(PlannerType, SoaBackendAndSourceKindControlAnalysisAndAuthoring) {
    auto const types{backend_soa_graph()};
    for (auto const* module : {"unreal", "stdlib"}) {
        auto const physical{std::string_view{module} == "stdlib"};
        for (auto const* name : {"Columns", "VectorColumns"}) {
            auto const vector{std::string_view{name} == "VectorColumns"};
            auto const type{types.find_declared(module, name)};
            ASSERT_TRUE(type.has_value());
            auto const capabilities{declaration_capabilities(types.type(*type))};
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

TEST(PlannerType, SupportedKindsKeepInspectionAndAnalysisCapabilities) {
    using namespace lispb::schema;
    auto const definitions{std::vector<std::pair<TypeDefinition, DeclarationKind>>{
        {EnumType{}, DeclarationKind::enumeration},
        {IntegerScalarType{}, DeclarationKind::integer_scalar},
        {LinearQuantizedType{}, DeclarationKind::linear_quantized},
        {IntegerVarintType{}, DeclarationKind::integer_varint},
        {FixedPointType{}, DeclarationKind::fixed_point},
        {MiniFloatType{}, DeclarationKind::mini_float},
        {OptionalSentinelType{}, DeclarationKind::optional_sentinel},
        {OptionalPresenceBitType{}, DeclarationKind::optional_presence_bit},
        {PackedType{}, DeclarationKind::packed},
        {RecordType{}, DeclarationKind::record},
        {UnionType{}, DeclarationKind::union_},
        {TaggedUnionType{}, DeclarationKind::tagged_union},
    }};
    for (auto const& [definition, kind] : definitions) {
        auto node{TypeNode{}};
        node.definition = definition;
        auto const capabilities{declaration_capabilities(node)};
        EXPECT_EQ(capabilities.kind, kind);
        EXPECT_TRUE(capabilities.inspectable);
        EXPECT_TRUE(capabilities.editable);
        EXPECT_TRUE(capabilities.physical_analysis_available);
        EXPECT_STRNE(declaration_kind_label(kind), "unknown");
    }
}

TEST(PlannerType, SwitchingToUnrealSoaClearsPhysicalResults) {
    auto const types{backend_soa_graph()};
    auto const supported{*types.find_declared("stdlib", "Columns")};
    auto const unsupported{*types.find_declared("unreal", "Columns")};
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
    auto const packed{types.find_declared("native_entity_unique_id", "EntityUniqueId")};
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
    auto const& packed_type{std::get<lispb::schema::PackedType>(types.type(*packed).definition)};
    auto const* field{std::get_if<lispb::schema::PackedField>(&packed_type.segments.front())};
    ASSERT_NE(field, nullptr);
    invalid.overrides.packed_field_widths.emplace(
        FieldOverrideId{.type = *packed, .field_name = field->name}, 1'024);
    EXPECT_EQ(declaration_status(types, *packed, invalid, AbiProfile::host_common(), 64),
              LayoutStatus::error);
}

TEST(PlannerType, TaggedUnionsNeedRealTargetFactsWhileSemanticScalarsDoNot) {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema enums{};
    enums.settings.name = "enums";
    enums.settings.header = "Enums.h";
    codegen::EnumSchema kind{};
    kind.name = "Kind";
    kind.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    codegen::EnumeratorSchema value{};
    value.name = "Value";
    kind.values.push_back(std::move(value));
    enums.declarations.push_back(std::move(kind));
    manifest.modules.emplace_back(std::move(enums));

    codegen::NormalModuleSchema scalars{};
    scalars.settings.name = "scalars";
    scalars.settings.header = "Scalars.h";
    codegen::IntegerScalarSchema semantic{};
    semantic.name = "Semantic";
    semantic.minimum_value = 0;
    semantic.maximum_value = 10;
    scalars.declarations.push_back(std::move(semantic));
    manifest.modules.emplace_back(std::move(scalars));

    codegen::NormalModuleSchema unions{};
    unions.settings.name = "unions";
    unions.settings.header = "Unions.h";
    codegen::TaggedUnionSchema tagged{};
    tagged.name = "Tagged";
    tagged.discriminant.name = "Kind";
    codegen::TaggedUnionAlternativeSchema alternative{};
    alternative.name = "value";
    alternative.tag = "Value";
    alternative.type.name = "std::uint32_t";
    tagged.alternatives.push_back(std::move(alternative));
    unions.declarations.push_back(std::move(tagged));
    manifest.modules.emplace_back(std::move(unions));

    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const tagged_type{*types.find_declared("unions", "Tagged")};
    auto const semantic_type{*types.find_declared("scalars", "Semantic")};
    EXPECT_EQ(declaration_status(types, tagged_type, Variant{}, AbiProfile{"unknown"}, 1),
              LayoutStatus::unknown);
    EXPECT_EQ(declaration_status(types, tagged_type, Variant{}, AbiProfile::host_common(), 1),
              LayoutStatus::available);
    EXPECT_EQ(declaration_status(types, semantic_type, Variant{}, AbiProfile{"unknown"}, 1),
              LayoutStatus::available);
}

TEST(PlannerType, RelationshipCapacityErrorsMatchSelectedPackedAndScalarAnalysis) {
    for (auto const kind :
         {codegen::SemanticRelationKind::index_into, codegen::SemanticRelationKind::count_of}) {
        auto const fixture{relationship_capacity_type(kind, 8)};
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

    auto const scalar_fixture{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::index_into, 8, 255)};
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
    auto const fixture{soa_type({{"opaque", "UnknownColumnType"}})};
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
    auto const fixture{relationship_capacity_type(codegen::SemanticRelationKind::index_into, 17)};
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
    auto const fixture{
        integer_varint_type(false, 0, 16'384, codegen::IntegerVarintEncoding::unsigned_varint)};
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
