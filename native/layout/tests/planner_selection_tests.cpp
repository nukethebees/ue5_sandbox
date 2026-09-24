#include <ioj/layout/planner_selection.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace ioj::layout {
namespace {

auto selection_graph(bool const insert_before,
                     std::string member_name = "value",
                     bool const record_as_packed = false) -> lispb::schema::TypeGraph {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    if (insert_before) {
        codegen::NormalModuleSchema enums{};
        enums.settings.name = "enums";
        enums.settings.header = "Enums.h";
        codegen::EnumSchema enumeration{};
        enumeration.name = "Inserted";
        enumeration.underlying_type =
            codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
        enumeration.values.push_back(codegen::EnumeratorSchema{.name = "Value",
                                                               .initializer = std::nullopt,
                                                               .display_name = std::nullopt,
                                                               .hidden = false,
                                                               .serialized_name = std::nullopt});
        enums.declarations.push_back(std::move(enumeration));
        manifest.modules.emplace_back(std::move(enums));
    }
    codegen::NormalModuleSchema packed_module{};
    packed_module.settings.name = "packed";
    packed_module.settings.header = "Packed.h";
    codegen::PackedValueSchema packed{};
    packed.name = "Packet";
    packed.storage_type.name = "std::uint8_t";
    codegen::PackedFieldSchema field{};
    field.name = "bits";
    field.type.name = "std::uint8_t";
    field.bits = 8;
    packed.segments.emplace_back(std::move(field));
    packed_module.declarations.push_back(std::move(packed));
    manifest.modules.emplace_back(std::move(packed_module));

    if (record_as_packed) {
        codegen::NormalModuleSchema replacement{};
        replacement.settings.name = "records";
        replacement.settings.header = "Records.h";
        codegen::PackedValueSchema value{};
        value.name = "Record";
        value.storage_type.name = "std::uint32_t";
        codegen::PackedFieldSchema field{};
        field.name = std::move(member_name);
        field.type.name = "std::uint32_t";
        field.bits = 32;
        value.segments.emplace_back(std::move(field));
        replacement.declarations.push_back(std::move(value));
        manifest.modules.emplace_back(std::move(replacement));
    } else {
        codegen::NormalModuleSchema record_module{};
        record_module.settings.name = "records";
        record_module.settings.header = "Records.h";
        codegen::RecordSchema record{};
        record.name = "Record";
        record.members.push_back(codegen::RecordMemberSchema{
            .name = std::move(member_name),
            .type = {.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .count = std::nullopt,
            .relationship = std::nullopt});
        record_module.declarations.push_back(std::move(record));
        manifest.modules.emplace_back(std::move(record_module));
    }

    codegen::NormalModuleSchema soa_module{};
    soa_module.settings.name = "soa";
    soa_module.settings.header = "Soa.h";
    soa_module.soa_backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema soa{};
    soa.name = "Columns";
    soa.members.push_back(codegen::SoaMemberSchema{
        .name = "values",
        .kind = codegen::SoaMemberKind::array,
        .type = {.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
        .fixed_schema = std::nullopt,
        .nested_schema = std::nullopt,
        .mask_field = false,
        .mask_dimensions = {},
        .relationship = std::nullopt});
    soa_module.declarations.push_back(std::move(soa));
    manifest.modules.emplace_back(std::move(soa_module));
    return lispb::schema::resolve_type_graph(manifest);
}

TEST(PlannerSelection, TypeSwitchClearsAllLocalWorkloads) {
    auto const types{selection_graph(false)};
    auto const record{*types.find_declared("records", "Record")};
    auto const packed{*types.find_declared("packed", "Packet")};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(types, record));
    selection.field = "value";
    selection.packed_access_fields["bits"] = AccessOperation::read;
    selection.record_access_members["value"] = AccessOperation::write;
    selection.soa_access_columns["values"] = AccessOperation::read_write;
    selection.packed_access_set_explicit = true;
    selection.record_access_set_explicit = true;
    selection.soa_access_set_explicit = true;

    EXPECT_FALSE(selection.select_type(types, record));
    EXPECT_EQ(selection.field, "value");
    ASSERT_TRUE(selection.select_type(types, packed));
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.packed_access_fields.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_TRUE(selection.soa_access_columns.empty());
    EXPECT_FALSE(selection.packed_access_set_explicit);
    EXPECT_FALSE(selection.record_access_set_explicit);
    EXPECT_FALSE(selection.soa_access_set_explicit);
}

TEST(PlannerSelection, GraphReplacementPreservesOnlyStillValidMembers) {
    auto original{selection_graph(false)};
    auto const record{*original.find_declared("records", "Record")};
    auto const identity{original.type(record).identity};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_members["value"] = AccessOperation::read;
    selection.record_access_set_explicit = true;

    auto reordered{selection_graph(true)};
    selection.reconcile(reordered, identity);
    ASSERT_TRUE(selection.type.has_value());
    EXPECT_NE(*selection.type, record);
    EXPECT_EQ(selection.field, "value");
    EXPECT_EQ(selection.record_access_members.size(), 1);

    auto renamed{selection_graph(true, "renamed")};
    selection.reconcile(renamed, identity);
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_FALSE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, ExplicitEmptyWorkloadSurvivesUnrelatedGraphEdit) {
    auto const original{selection_graph(false)};
    auto const record{*original.find_declared("records", "Record")};
    auto const identity{original.type(record).identity};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_set_explicit = true;
    auto const reordered{selection_graph(true)};
    selection.reconcile(reordered, identity);
    EXPECT_EQ(selection.field, "value");
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_TRUE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, OwnerKindChangeClearsLocalWorkload) {
    auto const original{selection_graph(false)};
    auto const record{*original.find_declared("records", "Record")};
    auto const identity{original.type(record).identity};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_members["value"] = AccessOperation::read;
    selection.record_access_set_explicit = true;
    auto const replaced{selection_graph(false, "value", true)};
    selection.reconcile(replaced, identity);
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_FALSE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, DeclarationOnlySelectionKeepsIdentityWithoutInventingType) {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema module{};
    module.settings.name = "declarations";
    module.settings.header = "Declarations.h";
    module.settings.source = "Declarations.cpp";
    auto layout{codegen::HomogeneousLayoutSchema{}};
    layout.name = "Layout";
    layout.components = {"xs", "ys"};
    auto value{codegen::HomogeneousValueSchema{}};
    value.type.name = "float";
    value.suffix = "f";
    layout.value_types.push_back(std::move(value));
    module.declarations.emplace_back(std::move(layout));
    auto table{codegen::StaticTableSchema{}};
    table.name = "Table";
    table.rows.push_back(codegen::StaticTableRowSchema{"first"});
    auto column{codegen::StaticTableColumnSchema{}};
    column.name = "ids";
    column.type.name = "std::int32_t";
    table.columns.push_back(std::move(column));
    module.declarations.emplace_back(std::move(table));
    auto facade{codegen::FacadeSchema{}};
    facade.name = "Facade";
    facade.target_type.name = "Target";
    facade.target_member_name = "target";
    auto method{codegen::FacadeMethodSchema{}};
    method.name = "reset";
    method.return_type.name = "void";
    facade.methods.push_back(std::move(method));
    module.declarations.emplace_back(std::move(facade));
    auto record{codegen::RecordSchema{}};
    record.name = "Record";
    module.declarations.emplace_back(std::move(record));
    manifest.modules.emplace_back(std::move(module));
    auto const document{lispb::schema::EditableSchemaDocument::from_manifest(std::move(manifest))};
    PlannerSelection selection;

    for (auto const& info : document.declarations()) {
        if (info.identity.name == "Record") {
            continue;
        }
        auto const* normal{std::get_if<codegen::NormalModuleSchema>(
            &document.manifest().modules[info.module_index])};
        ASSERT_NE(normal, nullptr);
        auto const& schema{normal->declarations[info.declaration_index]};
        EXPECT_TRUE(declaration_capabilities(schema).inspectable);
        EXPECT_FALSE(declaration_capabilities(schema).physical_analysis_available);
        EXPECT_FALSE(document.types().find(info.identity).has_value());
        EXPECT_TRUE(selection.select_declaration(document, info.id));
        EXPECT_EQ(selection.declaration, info.id);
        EXPECT_FALSE(selection.type.has_value());
        selection.reconcile(document.types(), std::nullopt, &document);
        EXPECT_EQ(selection.declaration, info.id);
    }

    auto const record_type{*document.types().find_declared("declarations", "Record")};
    EXPECT_TRUE(selection.select_type(document.types(), record_type));
    EXPECT_FALSE(selection.declaration.has_value());
    EXPECT_EQ(selection.type, record_type);

    auto promoted_manifest{document.manifest()};
    auto promoted{codegen::RecordSchema{}};
    promoted.name = "Layout";
    std::get<codegen::NormalModuleSchema>(promoted_manifest.modules.front()).declarations.front() =
        std::move(promoted);
    auto const promoted_document{
        lispb::schema::EditableSchemaDocument::from_manifest(std::move(promoted_manifest))};
    EXPECT_TRUE(selection.select_declaration(document, document.declarations().front().id));
    selection.reconcile(promoted_document.types(), std::nullopt, &promoted_document);
    EXPECT_FALSE(selection.declaration.has_value());
    ASSERT_TRUE(selection.type.has_value());
    EXPECT_EQ(promoted_document.types().type(*selection.type).identity.name, "Layout");
}

} // namespace
} // namespace ioj::layout
