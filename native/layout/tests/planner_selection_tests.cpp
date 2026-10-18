#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/planner_selection.hpp>
#include <ioj/layout/planner_session.hpp>

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/editable_document.h>
#include <lispb/schema/homogeneous_storage_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

auto selection_graph(lispb::CompilerStorage& storage,
                     bool const insert_before,
                     std::string member_name = "value",
                     bool const record_as_packed = false) -> lispb::schema::TypeGraph {
    std::string source;
    if (insert_before) {
        source += R"((module enums :header "Enums.h"
            (enum Inserted std::uint8_t (value Value))))";
    }
    source += R"((module packed :header "Packed.h"
        (packed-value Packet :storage std::uint8_t (field bits std::uint8_t :bits 8)))
        (module records :header "Records.h" )";
    if (record_as_packed) {
        source += "(packed-value Record :storage std::uint32_t (field " + member_name +
                  " std::uint32_t :bits 32))";
    } else {
        source += "(record Record (member " + member_name + " std::uint32_t))";
    }
    source += R"() (module soa :header "Soa.h" :backend standard-library
        (struct Columns (member values array std::uint32_t))))";
    return fixture_graph(storage, source);
}

TEST(PlannerSelection, TypeSwitchClearsAllLocalWorkloads) {
    lispb::CompilerStorage storage;
    auto const types{selection_graph(storage, false)};
    auto const record{*fixture_find(types, "records", "Record")};
    auto const packed{*fixture_find(types, "packed", "Packet")};
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
    lispb::CompilerStorage storage;
    auto original{selection_graph(storage, false)};
    auto const record{*fixture_find(original, "records", "Record")};
    auto const identity{original.identity(record)};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_members["value"] = AccessOperation::read;
    selection.record_access_set_explicit = true;

    auto reordered{selection_graph(storage, true)};
    selection.reconcile(reordered, identity);
    ASSERT_TRUE(selection.type.has_value());
    EXPECT_NE(*selection.type, record);
    EXPECT_EQ(selection.field, "value");
    EXPECT_EQ(selection.record_access_members.size(), 1);

    auto renamed{selection_graph(storage, true, "renamed")};
    selection.reconcile(renamed, identity);
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_FALSE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, ExplicitEmptyWorkloadSurvivesUnrelatedGraphEdit) {
    lispb::CompilerStorage storage;
    auto const original{selection_graph(storage, false)};
    auto const record{*fixture_find(original, "records", "Record")};
    auto const identity{original.identity(record)};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_set_explicit = true;
    auto const reordered{selection_graph(storage, true)};
    selection.reconcile(reordered, identity);
    EXPECT_EQ(selection.field, "value");
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_TRUE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, OwnerKindChangeClearsLocalWorkload) {
    lispb::CompilerStorage storage;
    auto const original{selection_graph(storage, false)};
    auto const record{*fixture_find(original, "records", "Record")};
    auto const identity{original.identity(record)};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original, record));
    selection.field = "value";
    selection.record_access_members["value"] = AccessOperation::read;
    selection.record_access_set_explicit = true;
    auto const replaced{selection_graph(storage, false, "value", true)};
    selection.reconcile(replaced, identity);
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_FALSE(selection.record_access_set_explicit);
}

auto declaration_selection_manifest(lispb::CompilerStorage& storage) -> lispb::codegen::Manifest {
    return fixture_manifest(storage, R"(
        (module declarations :header "Declarations.h" :source "Declarations.cpp"
            (record RecordA)
            (layout Layout :components (xs ys) (value-type float f))
            (table Table (row first) (column ids std::int32_t))
            (facade Facade Target target (method reset void))))");
}

template <typename Edit>
void edit_selection_declarations(lispb::codegen::Manifest& manifest, Edit edit) {
    auto& declarations{manifest.declarations};
    auto const module{declarations.normal_modules.clone(manifest.modules.front())};
    auto const row{declarations.normal_modules.index(module)};
    auto const original{declarations.items(declarations.normal_modules.declarations[row])};
    std::vector<lispb::codegen::DeclarationHandle> items{original.begin(), original.end()};
    edit(items);
    declarations.normal_modules.declarations[row] = declarations.range(items);
    manifest.modules.front() = module;
}

auto selection_identity(lispb::schema::EditableSchemaDocument const& document,
                        std::string_view name) -> lispb::schema::TypeIdentity {
    return {.origin = lispb::schema::TypeOrigin::declaration,
            .module_name = document.storage().strings.find("declarations"),
            .namespace_name = {},
            .name = document.storage().strings.find(name)};
}

auto declaration_selection_document() -> lispb::schema::EditableSchemaDocument {
    auto storage{std::make_unique<lispb::CompilerStorage>()};
    auto manifest{declaration_selection_manifest(*storage)};
    return lispb::schema::EditableSchemaDocument::from_manifest(std::move(storage),
                                                                std::move(manifest));
}

TEST(PlannerSelection, DeclarationOnlySelectionKeepsIdentityWithoutInventingType) {
    auto const document{declaration_selection_document()};
    PlannerSelection selection;

    auto const generated{document.types().types_for_declaration(
        *document.declaration_handle(document.declarations().ordered[1]))};
    ASSERT_EQ(generated.size(), 1U);
    auto const& storage{generated.front()};
    EXPECT_EQ(document.types().source_declaration(storage),
              *document.declaration_handle(document.declarations().ordered[1]));
    EXPECT_TRUE(((document.types().definition(storage)).kind() ==
                 lispb::schema::TypeDefinitionKind::homogeneous_storage));
    EXPECT_TRUE(declaration_capabilities(document.types(), storage).editable);
    EXPECT_FALSE(declaration_capabilities(document.types(), storage).physical_analysis_available);

    auto const inventory{external_dependencies(document.types())};
    for (auto const& [spelling, owner] : {std::pair{"float", "Layout"},
                                          std::pair{"std::int32_t", "Table"},
                                          std::pair{"Target", "Facade"}}) {
        auto const entry{
            std::ranges::find(inventory, std::string{spelling}, &ExternalDependency::cpp_spelling)};
        ASSERT_NE(entry, inventory.end());
        ASSERT_EQ(entry->declarations.size(), 1U);
        EXPECT_EQ(document.storage().strings.view(entry->declarations.front().name), owner);
        EXPECT_EQ(entry->modules, (std::vector<std::string>{"declarations"}));
    }
    EXPECT_EQ(std::ranges::find(inventory, std::string{"void"}, &ExternalDependency::cpp_spelling),
              inventory.end());
    PlannerAnalysisSession session{document.types()};

    for (auto const& info : document.declarations().ordered) {
        if (document.storage().strings.view(
                document.declarations().identity[document.declarations().index(info)].name) ==
            "RecordA") {
            continue;
        }
        auto const schema{*document.declaration_handle(info)};
        ASSERT_TRUE(session.inputs.selection.select_type(
            document.types(), fixture_find(document.types(), "declarations", "RecordA")));
        ASSERT_TRUE(session.refresh(&document));
        ASSERT_TRUE(session.results().record_analysis.has_value());
        ASSERT_TRUE(session.inputs.selection.select_declaration(document, info));
        ASSERT_TRUE(session.refresh(&document));
        EXPECT_FALSE(session.results().record_analysis.has_value());
        EXPECT_TRUE(declaration_capabilities(schema).inspectable);
        EXPECT_FALSE(declaration_capabilities(schema).physical_analysis_available);
        auto const semantic{document.types().find(
            document.declarations().identity[document.declarations().index(info)])};
        EXPECT_TRUE(selection.select_declaration(document, info));
        EXPECT_EQ(selection.declaration, semantic.has_value() ? std::nullopt : std::optional{info});
        EXPECT_EQ(selection.type, semantic);
        selection.reconcile(document.types(),
                            document.declarations().identity[document.declarations().index(info)],
                            &document);
        EXPECT_EQ(selection.declaration, semantic.has_value() ? std::nullopt : std::optional{info});
        EXPECT_EQ(selection.type, semantic);
    }

    auto const record_type{*fixture_find(document.types(), "declarations", "RecordA")};
    EXPECT_TRUE(selection.select_type(document.types(), record_type));
    EXPECT_FALSE(selection.declaration.has_value());
    EXPECT_EQ(selection.type, record_type);

    auto promoted_storage{std::make_unique<lispb::CompilerStorage>()};
    auto promoted_manifest{declaration_selection_manifest(*promoted_storage)};
    auto const promoted{promoted_storage->declarations.records.append()};
    promoted_storage->declarations.records
        .name[promoted_storage->declarations.records.index(promoted)] =
        promoted_storage->strings.intern("Layout");
    edit_selection_declarations(promoted_manifest,
                                [&](auto& declarations) { declarations[1] = promoted; });
    auto const promoted_document{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(promoted_storage), std::move(promoted_manifest))};
    EXPECT_TRUE(selection.select_declaration(document, document.declarations().ordered[1]));
    selection.reconcile(promoted_document.types(), std::nullopt, &promoted_document);
    EXPECT_FALSE(selection.declaration.has_value());
    ASSERT_TRUE(selection.type.has_value());
    EXPECT_EQ(
        promoted_document.storage().strings.view(promoted_document.types().name(*selection.type)),
        "Layout");
}

TEST(PlannerSelection, DeclarationOnlySelectionSurvivesInsertedEarlierDeclaration) {
    auto const original{declaration_selection_document()};
    auto const old_id{original.declarations().ordered[1]};
    auto reordered_storage{std::make_unique<lispb::CompilerStorage>()};
    auto reordered_manifest{declaration_selection_manifest(*reordered_storage)};
    auto const new_record{reordered_storage->declarations.records.append()};
    reordered_storage->declarations.records
        .name[reordered_storage->declarations.records.index(new_record)] =
        reordered_storage->strings.intern("NewRecord");
    edit_selection_declarations(reordered_manifest, [&](auto& declarations) {
        declarations.insert(declarations.begin(), new_record);
    });
    auto const reordered{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(reordered_storage), std::move(reordered_manifest))};
    auto const new_id{reordered.find_declaration(selection_identity(reordered, "Layout"))};
    ASSERT_TRUE(new_id.has_value());
    EXPECT_NE(*new_id, old_id);
    ASSERT_TRUE(reordered.declarations().contains(old_id));
    EXPECT_EQ(reordered.storage().strings.view(
                  reordered.declarations().identity[reordered.declarations().index(old_id)].name),
              "RecordA");

    PlannerAnalysisSession session{original.types()};
    ASSERT_TRUE(session.inputs.selection.select_declaration(original, old_id));
    session.replace_types(reordered, std::nullopt);
    EXPECT_EQ(session.inputs.selection.declaration, new_id);
    EXPECT_FALSE(session.inputs.selection.type.has_value());
    EXPECT_EQ(session.inputs.selection.identity(), selection_identity(reordered, "Layout"));
}

TEST(PlannerSelection, DeclarationOnlySelectionSurvivesRemovedEarlierDeclaration) {
    auto const original{declaration_selection_document()};
    auto const old_id{original.declarations().ordered[1]};
    auto const identity{
        original.declarations()
            .identity[original.declarations().index(original.declarations().ordered[1])]};
    auto changed_storage{std::make_unique<lispb::CompilerStorage>()};
    auto changed_manifest{declaration_selection_manifest(*changed_storage)};
    edit_selection_declarations(
        changed_manifest, [](auto& declarations) { declarations.erase(declarations.begin()); });
    auto const changed{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(changed_storage), std::move(changed_manifest))};
    auto const new_id{changed.find_declaration(
        selection_identity(changed, original.storage().strings.view(identity.name)))};
    ASSERT_TRUE(new_id.has_value());
    EXPECT_NE(*new_id, old_id);

    PlannerSelection selection;
    ASSERT_TRUE(selection.select_declaration(original, old_id));
    selection.reconcile(changed.types(), std::nullopt, &changed);
    EXPECT_EQ(selection.declaration, new_id);
    EXPECT_FALSE(selection.type.has_value());
}

TEST(PlannerSelection, RemovedDeclarationClearsSelectionDespiteReusedId) {
    auto const original{declaration_selection_document()};
    auto const old_id{original.declarations().ordered[1]};
    auto changed_storage{std::make_unique<lispb::CompilerStorage>()};
    auto changed_manifest{declaration_selection_manifest(*changed_storage)};
    edit_selection_declarations(
        changed_manifest, [](auto& declarations) { declarations.erase(declarations.begin() + 1); });
    auto const changed{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(changed_storage), std::move(changed_manifest))};
    ASSERT_TRUE(changed.declarations().contains(old_id));
    EXPECT_EQ(changed.storage().strings.view(
                  changed.declarations().identity[changed.declarations().index(old_id)].name),
              "Table");

    PlannerSelection selection;
    ASSERT_TRUE(selection.select_declaration(original, old_id));
    selection.reconcile(changed.types(), std::nullopt, &changed);
    EXPECT_FALSE(selection.declaration.has_value());
    EXPECT_FALSE(selection.type.has_value());
    EXPECT_FALSE(selection.identity().has_value());
}

TEST(PlannerSelection, SemanticDeclarationCanBecomeDeclarationOnly) {
    auto const original{declaration_selection_document()};
    auto const identity{
        original.declarations()
            .identity[original.declarations().index(original.declarations().ordered.front())]};
    auto const record{*original.types().find(identity)};
    auto changed_storage{std::make_unique<lispb::CompilerStorage>()};
    auto changed_manifest{declaration_selection_manifest(*changed_storage)};
    edit_selection_declarations(changed_manifest, [&](auto& declarations) {
        auto const demoted{
            changed_storage->declarations.homogeneous_layouts.clone(declarations[1])};
        changed_storage->declarations.homogeneous_layouts
            .name[changed_storage->declarations.homogeneous_layouts.index(demoted)] =
            changed_storage->strings.intern("RecordA");
        declarations.front() = demoted;
    });
    auto const changed{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(changed_storage), std::move(changed_manifest))};

    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original.types(), record));
    selection.field = "member";
    selection.record_access_members["member"] = AccessOperation::read;
    selection.record_access_set_explicit = true;
    selection.reconcile(changed.types(), std::nullopt, &changed);
    EXPECT_FALSE(selection.type.has_value());
    EXPECT_EQ(selection.declaration,
              changed.find_declaration(
                  selection_identity(changed, original.storage().strings.view(identity.name))));
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_FALSE(selection.record_access_set_explicit);
}

TEST(PlannerSelection, ChangingDeclarationOnlyOwnerClearsTypeLocalState) {
    auto const document{declaration_selection_document()};
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_declaration(document, document.declarations().ordered[1]));
    selection.field = "old";
    selection.packed_access_fields["old"] = AccessOperation::read;
    selection.record_access_members["old"] = AccessOperation::write;
    selection.soa_access_columns["old"] = AccessOperation::read_write;
    selection.packed_access_set_explicit = true;
    selection.record_access_set_explicit = true;
    selection.soa_access_set_explicit = true;

    ASSERT_TRUE(selection.select_declaration(document, document.declarations().ordered[2]));
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.packed_access_fields.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_TRUE(selection.soa_access_columns.empty());
    EXPECT_FALSE(selection.packed_access_set_explicit);
    EXPECT_FALSE(selection.record_access_set_explicit);
    EXPECT_FALSE(selection.soa_access_set_explicit);
}

TEST(PlannerSelection, DeclarationOnlyKindChangeClearsTypeLocalState) {
    auto const original{declaration_selection_document()};
    auto const identity{
        original.declarations()
            .identity[original.declarations().index(original.declarations().ordered[1])]};
    auto changed_storage{std::make_unique<lispb::CompilerStorage>()};
    auto changed_manifest{declaration_selection_manifest(*changed_storage)};
    edit_selection_declarations(changed_manifest, [&](auto& declarations) {
        auto const replacement{changed_storage->declarations.facades.clone(declarations[3])};
        changed_storage->declarations.facades
            .name[changed_storage->declarations.facades.index(replacement)] =
            changed_storage->strings.intern("Layout");
        declarations[1] = replacement;
    });
    auto const changed{lispb::schema::EditableSchemaDocument::from_manifest(
        std::move(changed_storage), std::move(changed_manifest))};

    PlannerSelection selection;
    ASSERT_TRUE(selection.select_declaration(original, original.declarations().ordered[1]));
    selection.field = "old";
    selection.soa_access_columns["old"] = AccessOperation::read;
    selection.soa_access_set_explicit = true;
    selection.reconcile(changed.types(), std::nullopt, &changed);
    EXPECT_FALSE(selection.declaration.has_value());
    EXPECT_EQ(selection.type,
              changed.types().find(
                  selection_identity(changed, original.storage().strings.view(identity.name))));
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.soa_access_columns.empty());
    EXPECT_FALSE(selection.soa_access_set_explicit);
}

} // namespace
} // namespace ioj::layout
