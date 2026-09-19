#include <lispb/schema/editable_document.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace lispb::schema {
namespace {

class TemporarySchema {
  public:
    TemporarySchema() {
        static int sequence{};
        directory_ = std::filesystem::temp_directory_path() /
                     ("editable-lispb-schema-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory_);
        write("types.lispb", "");
        write("modules.lispb", R"((enum-module authored_enums
  :header "AuthoredEnums.h"
  :namespace authored
  (enum Existing std::uint8_t
    (value Zero :value "0")))
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

} // namespace
} // namespace lispb::schema
