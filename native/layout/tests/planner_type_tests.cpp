#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace ioj::layout {
namespace {

TEST(PlannerType, ClassifiesVisibleAndPhysicalDeclarations) {
    auto const project{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
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
    EXPECT_TRUE(enum_capabilities.visible);
    EXPECT_TRUE(enum_capabilities.has_physical_layout);
    EXPECT_FALSE(enum_capabilities.supports_variants);
    auto const packed_capabilities{declaration_capabilities(types.type(*packed))};
    EXPECT_EQ(packed_capabilities.kind, DeclarationKind::packed);
    EXPECT_TRUE(packed_capabilities.editable);
    EXPECT_TRUE(packed_capabilities.supports_variants);
    EXPECT_TRUE(packed_capabilities.supports_access);
    auto const soa_capabilities{declaration_capabilities(types.type(*soa))};
    EXPECT_EQ(soa_capabilities.kind, DeclarationKind::soa);
    EXPECT_TRUE(soa_capabilities.visible);
    EXPECT_TRUE(soa_capabilities.supports_access);
}

TEST(PlannerType, DistinguishesUnknownTargetFactsFromErrors) {
    auto const project{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto loaded{load_lispb_schema(project, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};
    auto const packed{types.find_declared("native_entity_unique_id", "EntityUniqueId")};
    ASSERT_TRUE(packed.has_value());

    EXPECT_EQ(declaration_status(types, *packed, Variant{}, AbiProfile{"unknown"}, 64),
              LayoutStatus::unknown);
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
    codegen::EnumModuleSchema enums{};
    enums.settings.name = "enums";
    enums.settings.header = "Enums.h";
    codegen::EnumSchema kind{};
    kind.name = "Kind";
    kind.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    codegen::EnumeratorSchema value{};
    value.name = "Value";
    kind.values.push_back(std::move(value));
    enums.enums.push_back(std::move(kind));
    manifest.modules.emplace_back(std::move(enums));

    codegen::ScalarModuleSchema scalars{};
    scalars.settings.name = "scalars";
    scalars.settings.header = "Scalars.h";
    codegen::IntegerScalarSchema semantic{};
    semantic.name = "Semantic";
    semantic.minimum_value = 0;
    semantic.maximum_value = 10;
    scalars.scalars.push_back(std::move(semantic));
    manifest.modules.emplace_back(std::move(scalars));

    codegen::UnionModuleSchema unions{};
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
    unions.tagged_unions.push_back(std::move(tagged));
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

} // namespace
} // namespace ioj::layout
