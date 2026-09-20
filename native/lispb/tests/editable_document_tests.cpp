#include <lispb/schema/editable_document.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>

namespace lispb::schema {
namespace {

class TemporarySchema {
  public:
    TemporarySchema() {
        static int sequence{};
        directory_ = std::filesystem::temp_directory_path() /
                     ("editable-lispb-schema-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory_);
        write("types.lispb", R"((type existing
  :spelling "authored::Existing")
)");
        write("modules.lispb", R"((enum-module authored_enums
  :header "AuthoredEnums.h"
  :namespace authored
  (enum Existing std::uint8_t
    (value Zero :value "0")))

(packed-value-module authored_packed
  :header "AuthoredPacked.h"
  :namespace authored
  (packed-value ExistingPacked
    :storage std::uint8_t
    (field value std::uint8_t :bits 8)))

(soa-module authored_soa
  :header "AuthoredSoa.h"
  :namespace authored
  :backend standard-library
  (struct ExistingSoa
    :view-name ExistingSoaView
    :const-view-name ExistingSoaConstView
    :operations (reserve set-num)
    :using-declarations ("Base::reset")
    (member values array std::uint32_t)
    (function clear void
      :body ("values.clear();")
      :noexcept true)))
)");
    }
    ~TemporarySchema() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    auto load() const -> EditableSchemaDocument {
        auto const modules{std::array{path("modules.lispb")}};
        return load_editable_schema_document(path("types.lispb"), modules);
    }
    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }
  private:
    void write(std::string const& name, std::string_view const text) const {
        std::ofstream output{path(name), std::ios::binary};
        output << text;
    }

    std::filesystem::path directory_;
};

auto fixture_document() -> EditableSchemaDocument {
    auto const root{std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR} / "tests" /
                    "compile_fixture"};
    auto const modules{std::array{root / "modules.lispb"}};
    return load_editable_schema_document(root / "types.lispb", modules);
}

auto declaration_id(EditableSchemaDocument const& document,
                    std::string const& module,
                    std::string const& name,
                    std::string const& namespace_name = "codegen_compile_fixture")
    -> DeclarationId {
    auto const found{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                            .module_name = module,
                                                            .namespace_name = namespace_name,
                                                            .name = name})};
    EXPECT_TRUE(found.has_value());
    return found.value_or(DeclarationId{});
}

auto enum_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> EnumType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<EnumType>(document.types().type(*type).definition);
}

auto packed_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> PackedType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<PackedType>(document.types().type(*type).definition);
}

auto soa_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> SoaType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<SoaType>(document.types().type(*type).definition);
}

TEST(EditableSchemaDocument, RetainsDeclarationSourceOwnershipAndRanges) {
    auto const document{fixture_document()};
    auto const id{declaration_id(document, "plain_enum_fixture", "EPlainFixture")};
    auto const* declaration{document.declaration(id)};

    ASSERT_NE(declaration, nullptr);
    ASSERT_TRUE(declaration->source.has_value());
    auto const& source{document.source_files()[declaration->source->source_file_index]};
    EXPECT_EQ(source.path.filename(), "modules.lispb");
    ASSERT_LE(declaration->source->end_offset, source.text.size());
    auto const text{std::string_view{source.text}.substr(declaration->source->begin_offset,
                                                         declaration->source->end_offset -
                                                             declaration->source->begin_offset)};
    EXPECT_TRUE(text.starts_with("(enum EPlainFixture uint8"));
}

TEST(EditableSchemaDocument, AppliesResolvesAndInvertsTypedCommands) {
    auto document{fixture_document()};
    auto const id{declaration_id(document, "plain_enum_fixture", "EPlainFixture")};
    auto const original_info{*document.declaration(id)};

    auto applied{document.apply(SetEnumeratorDisplayName{
        .enum_declaration = id, .enumerator_name = "First", .display_name = "First Value"})};

    ASSERT_TRUE(applied.has_value());
    EXPECT_TRUE(*applied);
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    EXPECT_FALSE(document.can_redo());
    EXPECT_EQ(document.revision(), 1);
    EXPECT_EQ(document.declaration(id)->identity, original_info.identity);
    auto const& edited{enum_type(document, id)};
    ASSERT_EQ(edited.enumerators.size(), 2);
    EXPECT_EQ(edited.enumerators[0].display_name, "First Value");

    document.mark_saved();
    EXPECT_FALSE(document.dirty());
    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    EXPECT_TRUE(*undo);
    EXPECT_TRUE(document.dirty());
    EXPECT_FALSE(enum_type(document, id).enumerators[0].display_name.has_value());
    auto redo{document.redo()};
    ASSERT_TRUE(redo.has_value());
    EXPECT_TRUE(*redo);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(enum_type(document, id).enumerators[0].display_name, "First Value");
    EXPECT_EQ(document.revision(), 3);
}

TEST(EditableSchemaDocument, RejectsInvalidEditsWithoutChangingTheDraft) {
    auto document{fixture_document()};
    auto const id{declaration_id(document, "plain_enum_fixture", "EPlainFixture")};

    auto result{document.apply(SetEnumeratorName{
        .enum_declaration = id, .current_name = "ReadableName", .new_name = "First"})};

    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().message.empty());
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.can_undo());
    EXPECT_EQ(document.revision(), 0);
    auto const& type{enum_type(document, id)};
    ASSERT_EQ(type.enumerators.size(), 2);
    EXPECT_EQ(type.enumerators[0].name, "First");
    EXPECT_EQ(type.enumerators[1].name, "ReadableName");
}

TEST(EditableSchemaDocument, EnumeratorRenamePreservesCountSentinelSemantics) {
    auto document{fixture_document()};
    auto const id{declaration_id(document, "reflected_enum_fixture", "EReflectedFixture", "")};

    auto result{document.apply(
        SetEnumeratorName{.enum_declaration = id, .current_name = "COUNT", .new_name = "Count"})};

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
    auto const& renamed{enum_type(document, id)};
    ASSERT_EQ(renamed.enumerators.size(), 2);
    EXPECT_EQ(renamed.count, "Count");
    EXPECT_EQ(renamed.enumerators[1].name, "Count");
    EXPECT_TRUE(renamed.enumerators[1].count_sentinel);

    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    EXPECT_TRUE(*undo);
    auto const& restored{enum_type(document, id)};
    EXPECT_EQ(restored.count, "COUNT");
    EXPECT_EQ(restored.enumerators[1].name, "COUNT");
    EXPECT_TRUE(restored.enumerators[1].count_sentinel);
}

TEST(EditableSchemaDocument, CreatesPreviewsSavesAndReloadsEnumDeclarations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_enums",
                                                               .namespace_name = "authored",
                                                               .name = "Existing"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};
    auto create{document.apply(CreateEnum{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::EnumSchema{.name = "DesignedState",
                                      .underlying_type = codegen::TypeRef{"std::uint8_t"},
                                      .values = {{.name = "Idle",
                                                  .initializer = "0",
                                                  .display_name = "Idle State",
                                                  .serialized_name = "idle"},
                                                 {.name = "Active",
                                                  .initializer = "1",
                                                  .display_name = "Active State",
                                                  .serialized_name = "active"}}}})};
    ASSERT_TRUE(create.has_value());
    EXPECT_TRUE(*create);

    auto const semantic{document.types().find_declared("authored_enums", "DesignedState")};
    ASSERT_TRUE(semantic.has_value());
    auto const& type{std::get<EnumType>(document.types().type(*semantic).definition)};
    ASSERT_EQ(type.enumerators.size(), 2);
    EXPECT_EQ(type.enumerators[1].serialized_name, "active");

    auto replacement{*document.enum_schema(created)};
    replacement.values[1].display_name = "Enabled";
    auto replaced{
        document.apply(ReplaceEnum{.declaration = created, .schema = std::move(replacement)})};
    ASSERT_TRUE(replaced.has_value());
    EXPECT_TRUE(*replaced);
    EXPECT_EQ(
        std::get<EnumType>(document.types().type(*semantic).definition).enumerators[1].display_name,
        "Enabled");
    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    EXPECT_TRUE(*undo);
    EXPECT_EQ(
        std::get<EnumType>(document.types().type(*semantic).definition).enumerators[1].display_name,
        "Active State");
    auto redo{document.redo()};
    ASSERT_TRUE(redo.has_value());
    EXPECT_TRUE(*redo);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value());
    ASSERT_EQ(preview->size(), 1);
    EXPECT_NE(preview->front().updated.find("(enum DesignedState std::uint8_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":display-name \"Idle State\""), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(enum Existing"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1);
    EXPECT_EQ(saved->front(), files.path("modules.lispb"));
    EXPECT_FALSE(std::filesystem::exists(files.path("modules.lispb.layout-planner.tmp")));

    auto reloaded{files.load()};
    auto const reloaded_type{reloaded.types().find_declared("authored_enums", "DesignedState")};
    ASSERT_TRUE(reloaded_type.has_value());
    auto const& reloaded_enum{std::get<EnumType>(reloaded.types().type(*reloaded_type).definition)};
    ASSERT_EQ(reloaded_enum.enumerators.size(), 2);
    EXPECT_EQ(reloaded_enum.enumerators[0].display_name, "Idle State");
    EXPECT_EQ(reloaded_enum.enumerators[1].display_name, "Enabled");
    EXPECT_EQ(reloaded_enum.enumerators[1].serialized_name, "active");
    EXPECT_FALSE(document.dirty());
    auto const saved_declaration{
        document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_enums",
                                               .namespace_name = "authored",
                                               .name = "DesignedState"})};
    ASSERT_TRUE(saved_declaration.has_value());
    EXPECT_TRUE(document.declaration(*saved_declaration)->source.has_value());
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsPackedValues) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_packed",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingPacked"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreatePackedValue{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::PackedValueSchema{
            .name = "DesignedId",
            .storage_type = codegen::TypeRef{"std::uint32_t"},
            .fields = {codegen::PackedFieldSchema{
                           "entity_index", codegen::TypeRef{"std::uint32_t"}, 24},
                       codegen::PackedFieldSchema{"state",
                                                  codegen::TypeRef{"@existing"},
                                                  8,
                                                  codegen::PackedFieldKind::enumeration}},
            .invalid_value = 0xffffffffU}})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);

    auto undo_create{document.undo()};
    ASSERT_TRUE(undo_create.has_value());
    ASSERT_TRUE(*undo_create);
    EXPECT_EQ(document.declaration(created), nullptr);
    EXPECT_FALSE(document.types().find_declared("authored_packed", "DesignedId").has_value());
    auto redo_create{document.redo()};
    ASSERT_TRUE(redo_create.has_value());
    ASSERT_TRUE(*redo_create);
    ASSERT_NE(document.declaration(created), nullptr);

    auto const& created_type{packed_type(document, created)};
    ASSERT_EQ(created_type.fields.size(), 2U);
    EXPECT_EQ(created_type.fields[0].name, "entity_index");
    EXPECT_EQ(created_type.fields[0].bit_width, 24U);
    EXPECT_EQ(created_type.fields[1].name, "state");
    EXPECT_EQ(created_type.fields[1].bit_width, 8U);
    EXPECT_EQ(document.types().type(created_type.fields[1].semantic_type.type).identity.name,
              "Existing");

    auto invalid{*document.packed_value_schema(created)};
    invalid.fields.front().bits = 25;
    auto rejected{
        document.apply(ReplacePackedValue{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("does not fit"), std::string::npos);
    EXPECT_EQ(packed_type(document, created).fields[0].bit_width, 24U);

    auto reordered{*document.packed_value_schema(created)};
    std::swap(reordered.fields[0], reordered.fields[1]);
    auto replace{
        document.apply(ReplacePackedValue{.declaration = created, .schema = std::move(reordered)})};
    ASSERT_TRUE(replace.has_value()) << replace.error().message;
    ASSERT_TRUE(*replace);
    EXPECT_EQ(packed_type(document, created).fields[0].name, "state");

    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    ASSERT_TRUE(*undo);
    EXPECT_EQ(packed_type(document, created).fields[0].name, "entity_index");
    auto redo{document.redo()};
    ASSERT_TRUE(redo.has_value());
    ASSERT_TRUE(*redo);
    EXPECT_EQ(packed_type(document, created).fields[0].name, "state");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(packed-value DesignedId"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":storage std::uint32_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(field state @existing :bits 8 :kind enum)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(field entity_index std::uint32_t :bits 24)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_packed",
                                               .namespace_name = "authored",
                                               .name = "DesignedId"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const& reloaded_type{packed_type(reloaded, *reloaded_declaration)};
    ASSERT_EQ(reloaded_type.fields.size(), 2U);
    EXPECT_EQ(reloaded_type.fields[0].name, "state");
    EXPECT_EQ(reloaded_type.fields[0].bit_width, 8U);
    EXPECT_EQ(reloaded_type.fields[1].name, "entity_index");
    EXPECT_EQ(reloaded_type.fields[1].bit_width, 24U);
    EXPECT_EQ(reloaded_type.invalid_raw_value, 0xffffffffU);
    EXPECT_EQ(reloaded.types().type(reloaded_type.fields[0].semantic_type.type).identity.name,
              "Existing");
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(*reloaded.types().find_declared(
                                    "authored_packed", "DesignedId")),
                                reloaded_type.fields[0].semantic_type.type),
              reloaded.types()
                  .dependencies_of(*reloaded.types().find_declared("authored_packed", "DesignedId"))
                  .end());
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsSoas) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_soa",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingSoa"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateSoa{
        .declaration = created,
        .module_index = module_index,
        .schema =
            codegen::SoaSchema{.name = "DesignedColumns",
                               .members = {codegen::SoaMemberSchema{
                                               .name = "ids",
                                               .kind = codegen::SoaMemberKind::array,
                                               .type = codegen::TypeRef{"std::uint32_t"}},
                                           codegen::SoaMemberSchema{
                                               .name = "states",
                                               .kind = codegen::SoaMemberKind::array,
                                               .type =
                                                   codegen::TypeRef{"@existing"}}},
                               .operations = {codegen::StorageOperation::reserve,
                                              codegen::StorageOperation::set_num}},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);

    auto undo_create{document.undo()};
    ASSERT_TRUE(undo_create.has_value());
    ASSERT_TRUE(*undo_create);
    EXPECT_EQ(document.declaration(created), nullptr);
    auto redo_create{document.redo()};
    ASSERT_TRUE(redo_create.has_value());
    ASSERT_TRUE(*redo_create);

    auto const& created_type{soa_type(document, created)};
    ASSERT_EQ(created_type.columns.size(), 2U);
    EXPECT_EQ(created_type.columns[0].name, "ids");
    EXPECT_EQ(created_type.columns[1].name, "states");
    EXPECT_EQ(document.types().type(created_type.columns[1].semantic_type.type).identity.name,
              "Existing");

    auto invalid{*document.soa_schema(created)};
    invalid.members[1].name = "ids";
    auto rejected{document.apply(ReplaceSoa{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("duplicate"), std::string::npos);
    EXPECT_EQ(soa_type(document, created).columns[1].name, "states");

    auto reordered{*document.soa_schema(created)};
    std::swap(reordered.members[0], reordered.members[1]);
    auto replace{
        document.apply(ReplaceSoa{.declaration = created, .schema = std::move(reordered)})};
    ASSERT_TRUE(replace.has_value()) << replace.error().message;
    ASSERT_TRUE(*replace);
    EXPECT_EQ(soa_type(document, created).columns[0].name, "states");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(soa_type(document, created).columns[0].name, "ids");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(soa_type(document, created).columns[0].name, "states");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(struct DesignedColumns"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":operations (reserve set-num)"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member states array @existing)"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member ids array std::uint32_t)"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_soa",
                                               .namespace_name = "authored",
                                               .name = "DesignedColumns"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const& reloaded_type{soa_type(reloaded, *reloaded_declaration)};
    ASSERT_EQ(reloaded_type.columns.size(), 2U);
    EXPECT_EQ(reloaded_type.columns[0].name, "states");
    EXPECT_EQ(reloaded_type.columns[1].name, "ids");
    auto const state_type{reloaded_type.columns[0].semantic_type.type};
    EXPECT_EQ(reloaded.types().type(state_type).identity.name, "Existing");
    auto const declared{*reloaded.types().find_declared("authored_soa", "DesignedColumns")};
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(declared), state_type),
              reloaded.types().dependencies_of(declared).end());
}

TEST(EditableSchemaDocument, SoaColumnEditPreservesOtherDeclarationMetadata) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                                  .module_name = "authored_soa",
                                                                  .namespace_name = "authored",
                                                                  .name = "ExistingSoa"})};
    ASSERT_TRUE(declaration.has_value());

    auto replacement{*document.soa_schema(*declaration)};
    replacement.members.front().type.name = "std::uint16_t";
    auto applied{
        document.apply(ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find(":view-name ExistingSoaView"), std::string::npos);
    EXPECT_NE(source.find(":const-view-name ExistingSoaConstView"), std::string::npos);
    EXPECT_NE(source.find(":operations (reserve set-num)"), std::string::npos);
    EXPECT_NE(source.find(":using-declarations (\"Base::reset\")"), std::string::npos);
    EXPECT_NE(source.find("(function clear void"), std::string::npos);
    EXPECT_NE(source.find(":body (\"values.clear();\")"), std::string::npos);
    EXPECT_NE(source.find(":noexcept true"), std::string::npos);
    EXPECT_NE(source.find("(member values array std::uint16_t)"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_soa",
                                               .namespace_name = "authored",
                                               .name = "ExistingSoa"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.soa_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->view_name, "ExistingSoaView");
    EXPECT_EQ(schema->const_view_name, "ExistingSoaConstView");
    EXPECT_EQ(schema->operations.size(), 2U);
    EXPECT_EQ(schema->using_declarations, std::vector<std::string>{"Base::reset"});
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions.front().body_lines, std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(schema->functions.front().is_noexcept);
    EXPECT_EQ(schema->members.front().type.name, "std::uint16_t");
}

} // namespace
} // namespace lispb::schema
