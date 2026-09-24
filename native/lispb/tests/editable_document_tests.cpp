#include <lispb/schema/editable_document.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
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
  :spelling "authored::Existing"
  :header "AuthoredEnums.h")

(type helper
  :spelling "authored::Helper"
  :header "AuthoredHelper.h")
)");
        write("modules.lispb", R"((module authored_enums
  :header "AuthoredEnums.h"
  :namespace authored
  (enum Existing std::uint8_t
    ; Preserve the zero documentation during ordinary cell edits.
    (value Zero :value "0") ; zero trailing note
    ; Preserve the one documentation too.
    (value One :value   "1")))

(module authored_packed
  :header "AuthoredPacked.h"
  :namespace authored
  (packed-value ExistingPacked
    ; Keep the packed declaration note.
    :storage   std::uint32_t
    :byte-order   little ; packed byte-order note
    :invalid-value 4294967295
    ; Keep the value segment note.
    (field value std::uint8_t :bits 8) ; value segment trailing note
    ; Keep the counter segment note.
    (field counter std::uint16_t
      ; Keep the packed field note.
      :bits   8
      :minimum 0
      :maximum 100
      (code Invalid :value   255 :sentinel true) ; field code note
      ; pending field code note
      (code Pending :value 126 :sentinel true) ; pending field trailing note
      ; field relationship note
      (relation index_into authored::ExistingScalar))
    ; Keep the future segment note.
    (reserved future :bits   16)))

(module authored_scalars
  :header "AuthoredScalars.h"
  :namespace authored
  (integer-scalar ExistingScalar
    ; Keep the scalar domain note during ordinary edits.
    :signed false
    :minimum   0
    :maximum 1
    :bit-width auto
    (code Pending :value   2 :sentinel true) ; pending code note
    (code Invalid :value   3 :sentinel true)
    ; Keep the scalar relationship note.
    (relation index_into   authored::ExistingPacked)) ; scalar relationship trailing note
  (integer-scalar OtherScalar
    :signed false
    :minimum 0
    :maximum 3
    :bit-width auto)
  (integer-scalar SignedScalar
    :signed true
    :minimum -100
    :maximum 100
    :bit-width auto))

(module authored_representations
  :header "AuthoredRepresentations.h"
  :namespace authored
  (linear-quantized ExistingQ1
    ; Keep the quantization note.
    :source   authored::ExistingScalar
    :bits   1
    :reserved-codes 0
    :clipping reject)
  (integer-varint ExistingVarint
    ; Keep the varint note.
    :source   authored::ExistingScalar
    :encoding unsigned)
  (optional-sentinel ExistingOptional
    ; Keep the sentinel optional note.
    :source authored::ExistingScalar
    :sentinel Invalid)
  (optional-presence-bit ExistingPresence
    ; Keep the presence optional note.
    :source   authored::ExistingScalar)
  (fixed-point ExistingFixed
    ; Keep the fixed-point note.
    :signed false
    :total-bits   8
    :fractional-bits 4
    :rounding nearest-even)
  (mini-float ExistingMiniFloat
    ; Keep the mini-float note.
    :sign-bits 1
    :exponent-bits   5
    :significand-bits 10
    :bias 15))

(module authored_records
  :header "AuthoredRecords.h"
  :namespace authored
  (record ExistingRecord
    ; Keep the record note.
    ; Keep the value member note.
    (member value   std::uint32_t) ; value member trailing note
    ; Keep the flag member note.
    (member flag std::uint8_t)))

(module authored_unions
  :header "AuthoredUnions.h"
  :namespace authored
  (union ExistingUnion
    ; Keep the raw union note.
    ; Keep the value alternative note.
    (alternative value   std::uint32_t) ; value alternative trailing note
    ; Keep the small alternative note.
    (alternative small std::uint8_t))
  (tagged-union ExistingTagged
    ; Keep the tagged union note.
    :discriminant   authored::Existing
    ; Keep the tagged value alternative note.
    (alternative value   std::uint32_t :tag Zero) ; tagged value trailing note
  ))

(module authored_soa
  :header "AuthoredSoa.h"
  :namespace authored
  :backend standard-library
  (struct ExistingSoa
    ; Keep the SoA declaration note.
    :view-name   ExistingSoaView
    :const-view-name ExistingSoaConstView
    :operations (reserve set-num)
    :using-declarations ("Base::reset")
    ; Keep the values SoA member note.
    (member values array   std::uint32_t) ; SoA values trailing note
    ; Keep the flags SoA member note.
    (member flags array std::uint8_t)
    (function clear void
      ; Keep the custom function note.
      :body ("values.clear();")
      :noexcept true
      ; Keep the count parameter note.
      (parameter count std::uint32_t :default "0")))
  (struct NestedFlags
    (member bits array std::uint8_t)))
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
    auto load_with_module_source(std::string const& name) const -> EditableSchemaDocument {
        auto const modules{std::array{path("modules.lispb"), path(name)}};
        return load_editable_schema_document(path("types.lispb"), modules);
    }
    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }
    void write_source(std::string const& name, std::string_view const text) const {
        write(name, text);
    }
    auto read_source(std::string const& name) const -> std::string {
        std::ifstream input{path(name), std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void replace_module_text(std::string_view const old_text,
                             std::string_view const new_text) const {
        std::ifstream input{path("modules.lispb"), std::ios::binary};
        auto source{
            std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}}};
        auto const position{source.find(old_text)};
        if (position == std::string::npos) {
            throw std::runtime_error{"Temporary schema source fragment was not found"};
        }
        source.replace(position, old_text.size(), new_text);
        write("modules.lispb", source);
    }
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

auto integer_scalar_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> IntegerScalarType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<IntegerScalarType>(document.types().type(*type).definition);
}

auto linear_quantized_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> LinearQuantizedType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<LinearQuantizedType>(document.types().type(*type).definition);
}

auto integer_varint_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> IntegerVarintType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<IntegerVarintType>(document.types().type(*type).definition);
}

auto record_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> RecordType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<RecordType>(document.types().type(*type).definition);
}

auto union_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> UnionType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<UnionType>(document.types().type(*type).definition);
}

auto tagged_union_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> TaggedUnionType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<TaggedUnionType>(document.types().type(*type).definition);
}

auto soa_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> SoaType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<SoaType>(document.types().type(*type).definition);
}

auto optional_sentinel_type(EditableSchemaDocument const& document, DeclarationId const declaration)
    -> OptionalSentinelType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<OptionalSentinelType>(document.types().type(*type).definition);
}

auto optional_presence_bit_type(EditableSchemaDocument const& document,
                                DeclarationId const declaration) -> OptionalPresenceBitType const& {
    auto const* info{document.declaration(declaration)};
    EXPECT_NE(info, nullptr);
    auto const type{document.types().find(info->identity)};
    EXPECT_TRUE(type.has_value());
    return std::get<OptionalPresenceBitType>(document.types().type(*type).definition);
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

TEST(EditableSchemaDocument, RetainsMixedRawAndTaggedUnionSourceOwnership) {
    auto const document{TemporarySchema{}.load()};
    auto const raw{declaration_id(document, "authored_unions", "ExistingUnion", "authored")};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto declaration_text = [&](DeclarationId const id) {
        auto const* declaration{document.declaration(id)};
        EXPECT_NE(declaration, nullptr);
        EXPECT_TRUE(declaration != nullptr && declaration->source.has_value());
        if (declaration == nullptr || !declaration->source.has_value()) {
            return std::string_view{};
        }
        auto const& range{*declaration->source};
        auto const& source{document.source_files()[range.source_file_index].text};
        return std::string_view{source}.substr(range.begin_offset,
                                               range.end_offset - range.begin_offset);
    };

    EXPECT_TRUE(declaration_text(raw).starts_with("(union ExistingUnion"));
    EXPECT_TRUE(declaration_text(tagged).starts_with("(tagged-union ExistingTagged"));
    auto const type{document.types().find_declared("authored_unions", "ExistingTagged")};
    ASSERT_TRUE(type.has_value());
    EXPECT_TRUE(std::holds_alternative<TaggedUnionType>(document.types().type(*type).definition));
}

TEST(EditableSchemaDocument, RetainsOptionalSentinelSourceOwnership) {
    auto const document{TemporarySchema{}.load()};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    auto const* declaration{document.declaration(optional)};
    ASSERT_NE(declaration, nullptr);
    ASSERT_TRUE(declaration->source.has_value());
    auto const& range{*declaration->source};
    auto const& source{document.source_files()[range.source_file_index].text};
    auto const declaration_text{
        std::string_view{source}.substr(range.begin_offset, range.end_offset - range.begin_offset)};
    EXPECT_TRUE(declaration_text.starts_with("(optional-sentinel ExistingOptional"));
    EXPECT_EQ(optional_sentinel_type(document, optional).sentinel_value,
              codegen::PackedIntegerValue{3});
}

TEST(EditableSchemaDocument, RetainsOptionalPresenceBitSourceOwnership) {
    auto const document{TemporarySchema{}.load()};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingPresence", "authored")};
    auto const* declaration{document.declaration(optional)};
    ASSERT_NE(declaration, nullptr);
    ASSERT_TRUE(declaration->source.has_value());
    auto const& range{*declaration->source};
    auto const& source{document.source_files()[range.source_file_index].text};
    auto const declaration_text{
        std::string_view{source}.substr(range.begin_offset, range.end_offset - range.begin_offset)};
    EXPECT_TRUE(declaration_text.starts_with("(optional-presence-bit ExistingPresence"));
    EXPECT_EQ(optional_presence_bit_type(document, optional).payload_bits, 2U);
    EXPECT_EQ(optional_presence_bit_type(document, optional).encoded_bits, 3U);
}

TEST(EditableSchemaDocument, RetainsMiniFloatIdentityAndSourceOwnership) {
    auto const document{TemporarySchema{}.load()};
    auto const mini_float{
        declaration_id(document, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const* declaration{document.declaration(mini_float)};
    ASSERT_NE(declaration, nullptr);
    ASSERT_TRUE(declaration->source.has_value());
    auto const& range{*declaration->source};
    auto const& source{document.source_files()[range.source_file_index].text};
    auto const declaration_text{
        std::string_view{source}.substr(range.begin_offset, range.end_offset - range.begin_offset)};
    EXPECT_TRUE(declaration_text.starts_with("(mini-float ExistingMiniFloat"));

    auto const type{document.types().find(declaration->identity)};
    ASSERT_TRUE(type.has_value());
    auto const& resolved{std::get<MiniFloatType>(document.types().type(*type).definition)};
    EXPECT_EQ(resolved.sign_bits, 1U);
    EXPECT_EQ(resolved.exponent_bits, 5U);
    EXPECT_EQ(resolved.significand_bits, 10U);
    EXPECT_EQ(resolved.exponent_bias, 15);
    EXPECT_TRUE(document.types().dependencies_of(*type).empty());
}

TEST(EditableSchemaDocument, PreservesEnumCommentsAndFormattingForNonStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto replacement{*document.enum_schema(enumeration)};
    replacement.bit_width = 2;
    replacement.signedness = false;
    replacement.values[0].initializer.reset();
    replacement.values[0].display_name = "Zero \"display\"";
    replacement.values[1].serialized_name = "one-wire";
    replacement.values[1].sentinel = true;

    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("; Preserve the zero documentation during ordinary cell edits."),
              std::string::npos);
    EXPECT_NE(updated.find("; zero trailing note"), std::string::npos);
    EXPECT_NE(updated.find("; Preserve the one documentation too."), std::string::npos);
    EXPECT_NE(updated.find("(value Zero"), std::string::npos);
    EXPECT_NE(updated.find("(value One :value   \"1\""), std::string::npos);
    EXPECT_EQ(updated.find(":value \"0\""), std::string::npos);
    EXPECT_NE(updated.find(":display-name \"Zero \\\"display\\\"\""), std::string::npos);
    EXPECT_NE(updated.find(":sentinel true"), std::string::npos);
    EXPECT_NE(updated.find(":serialized-name \"one-wire\""), std::string::npos);
    EXPECT_NE(updated.find(":bit-width 2"), std::string::npos);
    EXPECT_NE(updated.find(":signed false"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_enum{declaration_id(reloaded, "authored_enums", "Existing", "authored")};
    auto const* schema{reloaded.enum_schema(reloaded_enum)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_width, 2U);
    EXPECT_EQ(schema->signedness, false);
    EXPECT_FALSE(schema->values[0].initializer.has_value());
    EXPECT_EQ(schema->values[0].display_name, "Zero \"display\"");
    EXPECT_TRUE(schema->values[1].sentinel);
    EXPECT_EQ(schema->values[1].serialized_name, "one-wire");
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Preserve the zero documentation"), std::string::npos);
    EXPECT_NE(module_source->text.find("; zero trailing note"), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesEnumConversionRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(R"(  :header "AuthoredEnums.h"
  :namespace authored
  (enum Existing std::uint8_t)",
                              R"(  :header "AuthoredEnums.h"
  :source "AuthoredEnums.cpp"
  :namespace authored
  (enum Existing std::uint8_t
    :conversions (
      ; Keep the lexical conversion note.
      lex-to-string ; lexical conversion trailing note
      ; Keep the view conversion note.
      string-view ; view conversion trailing note
    ))");
    auto document{files.load()};
    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};

    auto replacement{*document.enum_schema(enumeration)};
    replacement.conversions = {
        codegen::EnumConversion::string_view,
        codegen::EnumConversion::string,
        codegen::EnumConversion::lex_to_string,
    };
    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& reordered{preview->front().updated};
    auto const view_note{reordered.find("; Keep the view conversion note.")};
    auto const view_row{reordered.find("string-view ; view conversion trailing note")};
    auto const string_row{reordered.find("\n      string\n")};
    auto const lexical_note{reordered.find("; Keep the lexical conversion note.")};
    auto const lexical_row{reordered.find("lex-to-string ; lexical conversion trailing note")};
    ASSERT_NE(view_note, std::string::npos);
    ASSERT_NE(view_row, std::string::npos);
    ASSERT_NE(string_row, std::string::npos);
    ASSERT_NE(lexical_note, std::string::npos);
    ASSERT_NE(lexical_row, std::string::npos);
    EXPECT_LT(view_note, view_row);
    EXPECT_LT(view_row, string_row);
    EXPECT_LT(string_row, lexical_note);
    EXPECT_LT(lexical_note, lexical_row);
    EXPECT_NE(reordered.find("; Preserve the zero documentation"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto edited_document{files.load()};
    auto const edited_enumeration{
        declaration_id(edited_document, "authored_enums", "Existing", "authored")};
    replacement = *edited_document.enum_schema(edited_enumeration);
    replacement.conversions.front() = codegen::EnumConversion::display_string_view;
    applied = edited_document.apply(
        ReplaceEnum{.declaration = edited_enumeration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = edited_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& edited{preview->front().updated};
    EXPECT_NE(edited.find("; Keep the view conversion note."), std::string::npos);
    EXPECT_NE(edited.find("display-string-view ; view conversion trailing note"),
              std::string::npos);
    saved = edited_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_enumeration{
        declaration_id(deleting_document, "authored_enums", "Existing", "authored")};
    replacement = *deleting_document.enum_schema(deleting_enumeration);
    replacement.conversions.pop_back();
    applied = deleting_document.apply(
        ReplaceEnum{.declaration = deleting_enumeration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("; Keep the lexical conversion note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the view conversion note."), std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_enumeration{
        declaration_id(reloaded, "authored_enums", "Existing", "authored")};
    auto const* schema{reloaded.enum_schema(reloaded_enumeration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->conversions,
              (std::vector{codegen::EnumConversion::display_string_view,
                           codegen::EnumConversion::string}));
}

TEST(EditableSchemaDocument, PreservesEnumGenerationPolicyPropertiesDuringEdits) {
    TemporarySchema files;
    files.write_source("enum_policy.lispb", R"((module enum_policy
  :header "EnumPolicy.h"
  (enum PolicyMode std::uint8_t
    ; Keep the declaration policy note.
    :reflection uenum
    :enum-array true
    :count Count
    :export-specifier POLICY_API
    ; Keep the idle value note.
    (value Idle)
    (value Active)
    (value Count :hidden true)))
)");
    auto document{files.load_with_module_source("enum_policy.lispb")};
    auto const enumeration{declaration_id(document, "enum_policy", "PolicyMode", "")};

    auto replacement{*document.enum_schema(enumeration)};
    replacement.reflection = codegen::EnumReflection::blueprint;
    replacement.export_specifier = "POLICY_V2_API";
    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(":reflection blueprint"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":export-specifier POLICY_V2_API"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the declaration policy note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the idle value note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto native_document{files.load_with_module_source("enum_policy.lispb")};
    auto const native_enumeration{declaration_id(native_document, "enum_policy", "PolicyMode", "")};
    replacement = *native_document.enum_schema(native_enumeration);
    replacement.reflection = codegen::EnumReflection::none;
    replacement.enum_array = false;
    replacement.export_specifier.reset();
    replacement.native_api = true;
    applied = native_document.apply(
        ReplaceEnum{.declaration = native_enumeration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = native_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":reflection"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":enum-array"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":export-specifier"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":native-api true"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the declaration policy note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the idle value note."), std::string::npos);

    saved = native_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load_with_module_source("enum_policy.lispb")};
    auto const reloaded_enumeration{declaration_id(reloaded, "enum_policy", "PolicyMode", "")};
    auto const* schema{reloaded.enum_schema(reloaded_enumeration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->reflection, codegen::EnumReflection::none);
    EXPECT_FALSE(schema->enum_array);
    EXPECT_FALSE(schema->export_specifier.has_value());
    EXPECT_TRUE(schema->native_api);

    auto invalid{*schema};
    invalid.export_specifier = "INVALID_API";
    auto const revision_before{reloaded.revision()};
    applied = reloaded.apply(
        ReplaceEnum{.declaration = reloaded_enumeration, .schema = std::move(invalid)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("cannot use Unreal enum generation options"),
              std::string::npos);
    EXPECT_EQ(reloaded.revision(), revision_before);
    EXPECT_FALSE(reloaded.dirty());
    EXPECT_TRUE(reloaded.enum_schema(reloaded_enumeration)->native_api);
    EXPECT_FALSE(reloaded.enum_schema(reloaded_enumeration)->export_specifier.has_value());
}

TEST(EditableSchemaDocument, PreservesEnumUnrealProjectionDuringEdits) {
    TemporarySchema files;
    files.write_source("enum_projection.lispb", R"((module enum_projection
  :header "NativeMode.h"
  (enum NativeMode std::uint8_t
    ; Keep the native declaration note.
    :native-api true
    ; Keep the idle value note.
    (value Idle)
    (value Active)
    ; Keep the projection note.
    (unreal-projection ENativeMode
      :header   "Generated/NativeMode.h"
      :header-include "Generated/NativeMode.h"
      ; Keep the conversion-header note.
      :conversion-header "Generated/NativeModeConversion.h"
      :native-header-include "NativeMode.h"
      :reflection blueprint)))
)");
    auto document{files.load_with_module_source("enum_projection.lispb")};
    auto const enumeration{declaration_id(document, "enum_projection", "NativeMode", "")};

    auto replacement{*document.enum_schema(enumeration)};
    ASSERT_TRUE(replacement.unreal_projection.has_value());
    replacement.unreal_projection->name = "EProjectedNativeMode";
    replacement.unreal_projection->header_include = "Public/ProjectedNativeMode.h";
    replacement.unreal_projection->reflection = codegen::EnumReflection::uenum;
    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& edited{preview->front().updated};
    EXPECT_NE(edited.find("(unreal-projection EProjectedNativeMode"), std::string::npos);
    EXPECT_NE(edited.find(":header   \"Generated/NativeMode.h\""), std::string::npos);
    EXPECT_NE(edited.find(":header-include \"Public/ProjectedNativeMode.h\""), std::string::npos);
    EXPECT_EQ(edited.find(":reflection blueprint"), std::string::npos);
    EXPECT_NE(edited.find("; Keep the native declaration note."), std::string::npos);
    EXPECT_NE(edited.find("; Keep the idle value note."), std::string::npos);
    EXPECT_NE(edited.find("; Keep the projection note."), std::string::npos);
    EXPECT_NE(edited.find("; Keep the conversion-header note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto removing_document{files.load_with_module_source("enum_projection.lispb")};
    auto const removing_enumeration{
        declaration_id(removing_document, "enum_projection", "NativeMode", "")};
    replacement = *removing_document.enum_schema(removing_enumeration);
    replacement.unreal_projection.reset();
    applied = removing_document.apply(
        ReplaceEnum{.declaration = removing_enumeration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = removing_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(unreal-projection"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the native declaration note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the idle value note."), std::string::npos);
    saved = removing_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto adding_document{files.load_with_module_source("enum_projection.lispb")};
    auto const adding_enumeration{
        declaration_id(adding_document, "enum_projection", "NativeMode", "")};
    replacement = *adding_document.enum_schema(adding_enumeration);
    replacement.unreal_projection =
        codegen::EnumUnrealProjection{.name = "ENativeMode",
                                      .header = "Generated/NativeMode.h",
                                      .header_include = "Generated/NativeMode.h",
                                      .conversion_header = "Generated/NativeModeConversion.h",
                                      .native_header_include = "NativeMode.h",
                                      .reflection = codegen::EnumReflection::blueprint};
    applied = adding_document.apply(
        ReplaceEnum{.declaration = adding_enumeration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = adding_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(unreal-projection ENativeMode"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":reflection blueprint"), std::string::npos);
    saved = adding_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load_with_module_source("enum_projection.lispb")};
    auto const reloaded_enumeration{declaration_id(reloaded, "enum_projection", "NativeMode", "")};
    auto const* schema{reloaded.enum_schema(reloaded_enumeration)};
    ASSERT_NE(schema, nullptr);
    ASSERT_TRUE(schema->unreal_projection.has_value());
    EXPECT_EQ(schema->unreal_projection->name, "ENativeMode");
    EXPECT_EQ(schema->unreal_projection->header, "Generated/NativeMode.h");
    EXPECT_EQ(schema->unreal_projection->header_include, "Generated/NativeMode.h");
    EXPECT_EQ(schema->unreal_projection->conversion_header, "Generated/NativeModeConversion.h");
    EXPECT_EQ(schema->unreal_projection->native_header_include, "NativeMode.h");
    EXPECT_EQ(schema->unreal_projection->reflection, codegen::EnumReflection::blueprint);

    auto invalid{*schema};
    invalid.unreal_projection->reflection = codegen::EnumReflection::none;
    auto const revision_before{reloaded.revision()};
    applied = reloaded.apply(
        ReplaceEnum{.declaration = reloaded_enumeration, .schema = std::move(invalid)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("must be reflected"), std::string::npos);
    EXPECT_EQ(reloaded.revision(), revision_before);
    EXPECT_FALSE(reloaded.dirty());
}

TEST(EditableSchemaDocument, PreservesSourceForEnumWithDerivedBacking) {
    TemporarySchema files;
    files.replace_module_text("(enum Existing std::uint8_t", "(enum Existing");
    auto document{files.load()};
    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};
    ASSERT_FALSE(document.enum_schema(enumeration)->underlying_type.has_value());

    auto replacement{*document.enum_schema(enumeration)};
    replacement.values[1].display_name = "One State";
    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(enum Existing\n"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Preserve the one documentation too."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":display-name \"One State\""), std::string::npos);

    auto explicit_backing{*document.enum_schema(enumeration)};
    explicit_backing.underlying_type =
        codegen::TypeRef{.name = "std::uint16_t", .suffix = {}, .nested = std::nullopt};
    auto set_explicit{document.apply(
        ReplaceEnum{.declaration = enumeration, .schema = std::move(explicit_backing)})};
    ASSERT_TRUE(set_explicit.has_value()) << set_explicit.error().message;
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(enum Existing std::uint16_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Preserve the one documentation too."),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_FALSE(document.enum_schema(enumeration)->underlying_type.has_value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_enum{declaration_id(reloaded, "authored_enums", "Existing", "authored")};
    ASSERT_FALSE(reloaded.enum_schema(reloaded_enum)->underlying_type.has_value());
    EXPECT_EQ(reloaded.enum_schema(reloaded_enum)->values[1].display_name, "One State");
}

TEST(EditableSchemaDocument, PreservesEnumRowsAndCommentsForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto replacement{*document.enum_schema(enumeration)};
    auto zero{replacement.values[0]};
    auto one{replacement.values[1]};
    replacement.bit_width = 2;
    one.sentinel = true;
    auto duplicate{one};
    duplicate.name = "OneCopy";
    duplicate.initializer = "2";
    duplicate.sentinel = false;
    replacement.values = {one, duplicate, zero};

    auto applied{
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const one_comment{updated.find("; Preserve the one documentation too.")};
    auto const one_position{updated.find("(value One")};
    auto const copy{updated.find("(value OneCopy")};
    auto const zero_comment{
        updated.find("; Preserve the zero documentation during ordinary cell edits.")};
    auto const zero_position{updated.find("(value Zero")};
    ASSERT_NE(one_comment, std::string::npos);
    ASSERT_NE(one_position, std::string::npos);
    ASSERT_NE(copy, std::string::npos);
    ASSERT_NE(zero_comment, std::string::npos);
    ASSERT_NE(zero_position, std::string::npos);
    EXPECT_LT(one_comment, one_position);
    EXPECT_LT(one_position, copy);
    EXPECT_LT(copy, zero_comment);
    EXPECT_LT(zero_comment, zero_position);
    EXPECT_NE(updated.find("(value One :value   \"1\""), std::string::npos);
    EXPECT_NE(updated.find("; zero trailing note"), std::string::npos);
    EXPECT_NE(updated.find(":sentinel true"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.enum_schema(enumeration)};
    std::erase_if(deletion.values, [](auto const& value) { return value.name == "One"; });
    applied =
        document.apply(ReplaceEnum{.declaration = enumeration, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(value One :"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Preserve the one documentation too."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(value Zero"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Preserve the zero documentation"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; zero trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(value One :"), std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_enum{declaration_id(reloaded, "authored_enums", "Existing", "authored")};
    auto const* schema{reloaded.enum_schema(reloaded_enum)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_width, 2U);
    ASSERT_EQ(schema->values.size(), 2U);
    EXPECT_EQ(schema->values[0].name, "OneCopy");
    EXPECT_EQ(schema->values[0].initializer, "2");
    EXPECT_EQ(schema->values[1].name, "Zero");

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_EQ(module_source->text.find("; Preserve the one documentation too."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Preserve the zero documentation"), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesEnumeratorRowDuringDirectRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};

    auto renamed{document.apply(SetEnumeratorName{
        .enum_declaration = enumeration, .current_name = "One", .new_name = "Uno"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(*renamed);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const comment{updated.find("; Preserve the one documentation too.")};
    auto const renamed_value{updated.find("(value Uno :value   \"1\")")};
    ASSERT_NE(comment, std::string::npos) << updated;
    ASSERT_NE(renamed_value, std::string::npos);
    EXPECT_LT(comment, renamed_value);
    EXPECT_EQ(updated.find("(value One "), std::string::npos);
    EXPECT_NE(updated.find("; zero trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_enum{declaration_id(reloaded, "authored_enums", "Existing", "authored")};
    ASSERT_EQ(reloaded.enum_schema(reloaded_enum)->values.size(), 2U);
    EXPECT_EQ(reloaded.enum_schema(reloaded_enum)->values[1].name, "Uno");
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Preserve the one documentation too."), std::string::npos);
    EXPECT_NE(module_source->text.find("(value Uno :value   \"1\")"), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesScalarAndRepresentationFormattingForNonStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const varint{
        declaration_id(document, "authored_representations", "ExistingVarint", "authored")};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    auto const presence{
        declaration_id(document, "authored_representations", "ExistingPresence", "authored")};
    auto const fixed{
        declaration_id(document, "authored_representations", "ExistingFixed", "authored")};

    auto scalar_replacement{*document.integer_scalar_schema(scalar)};
    scalar_replacement.bit_width = 3;
    scalar_replacement.named_codes[0].value = codegen::PackedIntegerValue{4};
    scalar_replacement.relationship->kind = codegen::SemanticRelationKind::count_of;
    scalar_replacement.relationship->target.name = "authored::OtherScalar";
    scalar_replacement.cpp_emission = codegen::IntegerScalarCppEmission::constants;
    scalar_replacement.cpp_type = codegen::TypeRef{"std::uint8_t"};
    auto scalar_applied{document.apply(
        ReplaceIntegerScalar{.declaration = scalar, .schema = std::move(scalar_replacement)})};
    ASSERT_TRUE(scalar_applied.has_value()) << scalar_applied.error().message;

    auto quantized_replacement{*document.linear_quantized_schema(quantized)};
    quantized_replacement.bit_width = 3;
    quantized_replacement.reserved_codes = 1;
    quantized_replacement.clipping = codegen::QuantizationClipping::clamp;
    auto quantized_applied{document.apply(
        ReplaceLinearQuantized{.declaration = quantized, .schema = quantized_replacement})};
    ASSERT_TRUE(quantized_applied.has_value()) << quantized_applied.error().message;

    auto varint_replacement{*document.integer_varint_schema(varint)};
    varint_replacement.source.name = "authored::OtherScalar";
    auto varint_applied{document.apply(
        ReplaceIntegerVarint{.declaration = varint, .schema = std::move(varint_replacement)})};
    ASSERT_TRUE(varint_applied.has_value()) << varint_applied.error().message;

    auto optional_replacement{*document.optional_sentinel_schema(optional)};
    optional_replacement.sentinel = "Pending";
    auto optional_applied{document.apply(
        ReplaceOptionalSentinel{.declaration = optional, .schema = optional_replacement})};
    ASSERT_TRUE(optional_applied.has_value()) << optional_applied.error().message;

    auto fixed_replacement{*document.fixed_point_schema(fixed)};
    fixed_replacement.signedness = true;
    fixed_replacement.total_bits = 12;
    fixed_replacement.fractional_bits = 5;
    fixed_replacement.rounding = codegen::FixedPointRounding::toward_zero;
    auto fixed_applied{
        document.apply(ReplaceFixedPoint{.declaration = fixed, .schema = fixed_replacement})};
    ASSERT_TRUE(fixed_applied.has_value()) << fixed_applied.error().message;

    auto presence_replacement{*document.optional_presence_bit_schema(presence)};
    presence_replacement.source.name = "authored::OtherScalar";
    auto presence_applied{document.apply(ReplaceOptionalPresenceBit{
        .declaration = presence, .schema = std::move(presence_replacement)})};
    ASSERT_TRUE(presence_applied.has_value()) << presence_applied.error().message;
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("; Keep the scalar domain note"), std::string::npos);
    EXPECT_NE(updated.find(":minimum   0"), std::string::npos);
    EXPECT_NE(updated.find(":cpp-emission constants"), std::string::npos);
    EXPECT_NE(updated.find(":cpp-type std::uint8_t"), std::string::npos);
    auto const pending_begin{updated.find("(code Pending")};
    ASSERT_NE(pending_begin, std::string::npos);
    EXPECT_NE(updated.find("(code Pending :value   4 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("(code Invalid :value   3 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("; pending code note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the scalar relationship note."), std::string::npos);
    EXPECT_NE(updated.find("(relation count_of   authored::OtherScalar)"), std::string::npos);
    EXPECT_NE(updated.find("; scalar relationship trailing note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the quantization note"), std::string::npos);
    EXPECT_NE(updated.find(":source   authored::ExistingScalar"), std::string::npos);
    EXPECT_NE(updated.find(":bits   3"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the varint note"), std::string::npos);
    EXPECT_NE(updated.find(":source   authored::OtherScalar"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the sentinel optional note"), std::string::npos);
    EXPECT_NE(updated.find(":sentinel Pending"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the presence optional note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the fixed-point note"), std::string::npos);
    EXPECT_NE(updated.find(":total-bits   12"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    auto const reloaded_quantized{
        declaration_id(reloaded, "authored_representations", "ExistingQ1", "authored")};
    auto const reloaded_varint{
        declaration_id(reloaded, "authored_representations", "ExistingVarint", "authored")};
    auto const reloaded_optional{
        declaration_id(reloaded, "authored_representations", "ExistingOptional", "authored")};
    auto const reloaded_presence{
        declaration_id(reloaded, "authored_representations", "ExistingPresence", "authored")};
    auto const reloaded_fixed{
        declaration_id(reloaded, "authored_representations", "ExistingFixed", "authored")};
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->bit_width, 3U);
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->maximum_value,
              codegen::PackedIntegerValue{1});
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->named_codes[0].value,
              codegen::PackedIntegerValue{4});
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->cpp_emission,
              codegen::IntegerScalarCppEmission::constants);
    ASSERT_TRUE(reloaded.integer_scalar_schema(reloaded_scalar)->cpp_type.has_value());
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->cpp_type->name, "std::uint8_t");
    EXPECT_TRUE(reloaded.integer_scalar_schema(reloaded_scalar)->named_codes[1].sentinel);
    ASSERT_TRUE(reloaded.integer_scalar_schema(reloaded_scalar)->relationship.has_value());
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->relationship->kind,
              codegen::SemanticRelationKind::count_of);
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->relationship->target.name,
              "authored::OtherScalar");
    EXPECT_EQ(reloaded.linear_quantized_schema(reloaded_quantized)->bit_width, 3U);
    EXPECT_EQ(reloaded.linear_quantized_schema(reloaded_quantized)->clipping,
              codegen::QuantizationClipping::clamp);
    EXPECT_EQ(reloaded.integer_varint_schema(reloaded_varint)->source.name,
              "authored::OtherScalar");
    EXPECT_EQ(reloaded.optional_sentinel_schema(reloaded_optional)->sentinel, "Pending");
    EXPECT_EQ(reloaded.optional_presence_bit_schema(reloaded_presence)->source.name,
              "authored::OtherScalar");
    EXPECT_TRUE(reloaded.fixed_point_schema(reloaded_fixed)->signedness);
    EXPECT_EQ(reloaded.fixed_point_schema(reloaded_fixed)->total_bits, 12U);
    EXPECT_EQ(reloaded.fixed_point_schema(reloaded_fixed)->fractional_bits, 5U);
    EXPECT_EQ(reloaded.fixed_point_schema(reloaded_fixed)->rounding,
              codegen::FixedPointRounding::toward_zero);
}

TEST(EditableSchemaDocument, PreservesIntegerScalarCodesForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto replacement{*document.integer_scalar_schema(scalar)};
    replacement.bit_width = 3;
    replacement.relationship->kind = codegen::SemanticRelationKind::count_of;
    replacement.relationship->target.name = "authored::OtherScalar";
    auto pending{replacement.named_codes[0]};
    auto invalid{replacement.named_codes[1]};
    invalid.value = codegen::PackedIntegerValue{6};
    auto pending_copy{pending};
    pending_copy.name = "PendingCopy";
    pending_copy.value = codegen::PackedIntegerValue{4};
    replacement.named_codes = {invalid, pending_copy, pending};

    auto applied{document.apply(
        ReplaceIntegerScalar{.declaration = scalar, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const scalar_begin{updated.find("(integer-scalar ExistingScalar")};
    auto const scalar_end{updated.find("(integer-scalar OtherScalar", scalar_begin)};
    ASSERT_NE(scalar_begin, std::string::npos);
    ASSERT_NE(scalar_end, std::string::npos);
    auto const scalar_source{updated.substr(scalar_begin, scalar_end - scalar_begin)};
    auto const invalid_position{scalar_source.find("(code Invalid")};
    auto const pending_copy_position{scalar_source.find("(code PendingCopy")};
    auto const pending_position{scalar_source.find("(code Pending :")};
    ASSERT_NE(invalid_position, std::string::npos);
    ASSERT_NE(pending_copy_position, std::string::npos);
    ASSERT_NE(pending_position, std::string::npos);
    EXPECT_LT(invalid_position, pending_copy_position);
    EXPECT_LT(pending_copy_position, pending_position);
    EXPECT_NE(updated.find("(code Invalid :value   6 :sentinel true)"), std::string::npos);
    EXPECT_NE(updated.find("(code PendingCopy :value 4 :sentinel true)"), std::string::npos);
    EXPECT_NE(updated.find("(code Pending :value   2 :sentinel true) ; pending code note"),
              std::string::npos);
    EXPECT_NE(updated.find("; Keep the scalar domain note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the scalar relationship note"), std::string::npos);
    EXPECT_NE(updated.find("(relation count_of   authored::OtherScalar)"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.integer_scalar_schema(scalar)->named_codes.size(), 2U);
    EXPECT_EQ(document.integer_scalar_schema(scalar)->named_codes[0].name, "Pending");
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.integer_scalar_schema(scalar)};
    ASSERT_EQ(deletion.named_codes.size(), 3U);
    deletion.named_codes.erase(deletion.named_codes.begin() + 2);
    auto deleted{
        document.apply(ReplaceIntegerScalar{.declaration = scalar, .schema = std::move(deletion)})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& deleted_source{preview->front().updated};
    auto const deleted_scalar_begin{deleted_source.find("(integer-scalar ExistingScalar")};
    auto const deleted_scalar_end{
        deleted_source.find("(integer-scalar OtherScalar", deleted_scalar_begin)};
    ASSERT_NE(deleted_scalar_begin, std::string::npos);
    ASSERT_NE(deleted_scalar_end, std::string::npos);
    auto const deleted_scalar_source{
        deleted_source.substr(deleted_scalar_begin, deleted_scalar_end - deleted_scalar_begin)};
    EXPECT_EQ(deleted_scalar_source.find("(code Pending :value"), std::string::npos);
    EXPECT_EQ(deleted_scalar_source.find("; pending code note"), std::string::npos);
    EXPECT_NE(deleted_scalar_source.find("(code Invalid :value   6 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(deleted_scalar_source.find("(code PendingCopy :value 4 :sentinel true)\n"
                                         "    ; Keep the scalar relationship note."),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    auto const* schema{reloaded.integer_scalar_schema(reloaded_scalar)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_width, 3U);
    ASSERT_EQ(schema->named_codes.size(), 2U);
    EXPECT_EQ(schema->named_codes[0].name, "Invalid");
    EXPECT_EQ(schema->named_codes[0].value, codegen::PackedIntegerValue{6});
    EXPECT_EQ(schema->named_codes[1].name, "PendingCopy");
    EXPECT_EQ(schema->named_codes[1].value, codegen::PackedIntegerValue{4});
    ASSERT_TRUE(schema->relationship.has_value());
    EXPECT_EQ(schema->relationship->kind, codegen::SemanticRelationKind::count_of);
    EXPECT_EQ(schema->relationship->target.name, "authored::OtherScalar");
}

TEST(EditableSchemaDocument, PreservesNamedCodeRowsDuringDirectRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto scalar_replacement{*document.integer_scalar_schema(scalar)};
    scalar_replacement.named_codes[0].name = "Waiting";
    auto applied{document.apply(
        ReplaceIntegerScalar{.declaration = scalar, .schema = std::move(scalar_replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto packed_replacement{*document.packed_value_schema(packed)};
    auto& counter{std::get<codegen::PackedFieldSchema>(packed_replacement.segments[1])};
    counter.named_codes[1].name = "Queued";
    applied = document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(packed_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("(code Waiting :value   2 :sentinel true) ; pending code note"),
              std::string::npos);
    auto const pending_field_comment{updated.find("; pending field code note")};
    auto const renamed_field_code{
        updated.find("(code Queued :value 126 :sentinel true) ; pending field trailing note")};
    ASSERT_NE(pending_field_comment, std::string::npos);
    ASSERT_NE(renamed_field_code, std::string::npos);
    EXPECT_LT(pending_field_comment, renamed_field_code);
    EXPECT_EQ(updated.find("(code Pending "), std::string::npos);
    EXPECT_NE(updated.find("; Keep the scalar relationship note."), std::string::npos);
    EXPECT_NE(updated.find("; field relationship note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(code Waiting :value   2 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(code Pending :value 126 :sentinel true)"),
              std::string::npos);
    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->named_codes[0].name, "Waiting");
    auto const& reloaded_counter{std::get<codegen::PackedFieldSchema>(
        reloaded.packed_value_schema(reloaded_packed)->segments[1])};
    EXPECT_EQ(reloaded_counter.named_codes[1].name, "Queued");

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(
        module_source->text.find("(code Waiting :value   2 :sentinel true) ; pending code note"),
        std::string::npos);
    EXPECT_NE(module_source->text.find("; pending field code note\n"
                                       "      (code Queued :value 126 :sentinel true) ; pending "
                                       "field trailing note"),
              std::string::npos);
}

TEST(EditableSchemaDocument, PreservesSourceWhenAddingAndRemovingSemanticRelationships) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_scalar{
        declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const other_scalar{
        declaration_id(document, "authored_scalars", "OtherScalar", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto existing_replacement{*document.integer_scalar_schema(existing_scalar)};
    existing_replacement.relationship.reset();
    auto applied{document.apply(ReplaceIntegerScalar{.declaration = existing_scalar,
                                                     .schema = std::move(existing_replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto other_replacement{*document.integer_scalar_schema(other_scalar)};
    other_replacement.relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::Existing"},
                                        .unit = std::nullopt};
    applied = document.apply(
        ReplaceIntegerScalar{.declaration = other_scalar, .schema = std::move(other_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto packed_replacement{*document.packed_value_schema(packed)};
    auto& value_field{std::get<codegen::PackedFieldSchema>(packed_replacement.segments[0])};
    value_field.relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::OtherScalar"},
                                        .unit = std::nullopt};
    auto& counter_field{std::get<codegen::PackedFieldSchema>(packed_replacement.segments[1])};
    counter_field.relationship.reset();
    applied = document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(packed_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("; Keep the scalar domain note"), std::string::npos);
    EXPECT_NE(updated.find("; pending code note"), std::string::npos);
    EXPECT_EQ(updated.find("; Keep the scalar relationship note"), std::string::npos);
    EXPECT_NE(updated.find("; scalar relationship trailing note"), std::string::npos);
    EXPECT_EQ(updated.find("(relation index_into   authored::ExistingPacked)"), std::string::npos);
    EXPECT_NE(updated.find("(relation references authored::Existing)"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the value segment note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the packed field note"), std::string::npos);
    EXPECT_NE(updated.find("; field code note"), std::string::npos);
    EXPECT_EQ(updated.find("; field relationship note"), std::string::npos);
    EXPECT_EQ(updated.find("(relation index_into authored::ExistingScalar)"), std::string::npos);
    EXPECT_NE(updated.find("(relation references authored::OtherScalar)"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("; field relationship note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(relation index_into authored::ExistingScalar)"),
              std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(relation references authored::OtherScalar)"),
              std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_existing{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    auto const reloaded_other{
        declaration_id(reloaded, "authored_scalars", "OtherScalar", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    EXPECT_FALSE(reloaded.integer_scalar_schema(reloaded_existing)->relationship.has_value());
    ASSERT_TRUE(reloaded.integer_scalar_schema(reloaded_other)->relationship.has_value());
    auto const* reloaded_packed_schema{reloaded.packed_value_schema(reloaded_packed)};
    ASSERT_NE(reloaded_packed_schema, nullptr);
    EXPECT_TRUE(std::get<codegen::PackedFieldSchema>(reloaded_packed_schema->segments[0])
                    .relationship.has_value());
    EXPECT_FALSE(std::get<codegen::PackedFieldSchema>(reloaded_packed_schema->segments[1])
                     .relationship.has_value());

    auto remove_other{*reloaded.integer_scalar_schema(reloaded_other)};
    remove_other.relationship.reset();
    applied = reloaded.apply(
        ReplaceIntegerScalar{.declaration = reloaded_other, .schema = std::move(remove_other)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    auto remove_packed{*reloaded.packed_value_schema(reloaded_packed)};
    std::get<codegen::PackedFieldSchema>(remove_packed.segments[0]).relationship.reset();
    applied = reloaded.apply(
        ReplacePackedValue{.declaration = reloaded_packed, .schema = std::move(remove_packed)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(relation references authored::Existing)"),
              std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(relation references authored::OtherScalar)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the scalar domain note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note"), std::string::npos);

    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_other{
        declaration_id(final_document, "authored_scalars", "OtherScalar", "authored")};
    auto const final_packed{
        declaration_id(final_document, "authored_packed", "ExistingPacked", "authored")};
    EXPECT_FALSE(final_document.integer_scalar_schema(final_other)->relationship.has_value());
    EXPECT_FALSE(std::get<codegen::PackedFieldSchema>(
                     final_document.packed_value_schema(final_packed)->segments[0])
                     .relationship.has_value());
}

TEST(EditableSchemaDocument, PreservesSourceWhenAddingAndRemovingTheFirstNamedCode) {
    TemporarySchema files;
    auto document{files.load()};
    auto other_scalar{declaration_id(document, "authored_scalars", "OtherScalar", "authored")};
    auto packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto other_with_relation{*document.integer_scalar_schema(other_scalar)};
    other_with_relation.relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::Existing"},
                                        .unit = std::nullopt};
    auto applied{document.apply(ReplaceIntegerScalar{.declaration = other_scalar,
                                                     .schema = std::move(other_with_relation)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto packed_with_relation{*document.packed_value_schema(packed)};
    std::get<codegen::PackedFieldSchema>(packed_with_relation.segments[0]).relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::OtherScalar"},
                                        .unit = std::nullopt};
    applied = document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(packed_with_relation)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto authored{files.load()};
    other_scalar = declaration_id(authored, "authored_scalars", "OtherScalar", "authored");
    auto const signed_scalar{
        declaration_id(authored, "authored_scalars", "SignedScalar", "authored")};
    packed = declaration_id(authored, "authored_packed", "ExistingPacked", "authored");

    auto other_with_code{*authored.integer_scalar_schema(other_scalar)};
    other_with_code.named_codes.push_back({.name = "OtherZero", .value = 0, .sentinel = false});
    applied = authored.apply(
        ReplaceIntegerScalar{.declaration = other_scalar, .schema = std::move(other_with_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto signed_with_code{*authored.integer_scalar_schema(signed_scalar)};
    signed_with_code.named_codes.push_back({.name = "SignedZero", .value = 0, .sentinel = false});
    applied = authored.apply(
        ReplaceIntegerScalar{.declaration = signed_scalar, .schema = std::move(signed_with_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto packed_with_code{*authored.packed_value_schema(packed)};
    std::get<codegen::PackedFieldSchema>(packed_with_code.segments[0])
        .named_codes.push_back({.name = "ValueOne", .value = 1, .sentinel = false});
    applied = authored.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(packed_with_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{authored.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const other_begin{updated.find("(integer-scalar OtherScalar")};
    auto const other_end{updated.find("(integer-scalar SignedScalar", other_begin)};
    auto const signed_end{updated.find("(module authored_representations", other_end)};
    ASSERT_NE(other_begin, std::string::npos);
    ASSERT_NE(other_end, std::string::npos);
    ASSERT_NE(signed_end, std::string::npos);
    auto const other_source{updated.substr(other_begin, other_end - other_begin)};
    auto const signed_source{updated.substr(other_end, signed_end - other_end)};
    auto const other_code{other_source.find("(code OtherZero :value 0)")};
    auto const other_relation{other_source.find("(relation references authored::Existing)")};
    ASSERT_NE(other_code, std::string::npos);
    ASSERT_NE(other_relation, std::string::npos);
    EXPECT_LT(other_code, other_relation);
    EXPECT_NE(other_source.find(":minimum 0"), std::string::npos);
    EXPECT_NE(signed_source.find("(code SignedZero :value 0)"), std::string::npos);
    EXPECT_NE(signed_source.find(":minimum -100"), std::string::npos);

    auto const packed_begin{updated.find("(packed-value ExistingPacked")};
    auto const packed_end{updated.find("(module authored_scalars", packed_begin)};
    ASSERT_NE(packed_begin, std::string::npos);
    ASSERT_NE(packed_end, std::string::npos);
    auto const packed_source{updated.substr(packed_begin, packed_end - packed_begin)};
    auto const value_begin{packed_source.find("(field value")};
    auto const value_end{packed_source.find("(field counter", value_begin)};
    ASSERT_NE(value_begin, std::string::npos);
    ASSERT_NE(value_end, std::string::npos);
    auto const value_source{packed_source.substr(value_begin, value_end - value_begin)};
    auto const value_code{value_source.find("(code ValueOne :value 1)")};
    auto const value_relation{value_source.find("(relation references authored::OtherScalar)")};
    ASSERT_NE(value_code, std::string::npos);
    ASSERT_NE(value_relation, std::string::npos);
    EXPECT_LT(value_code, value_relation);
    EXPECT_NE(packed_source.find("; Keep the value segment note"), std::string::npos);
    EXPECT_NE(packed_source.find("; Keep the counter segment note"), std::string::npos);

    ASSERT_TRUE(authored.undo().value());
    EXPECT_TRUE(
        std::get<codegen::PackedFieldSchema>(authored.packed_value_schema(packed)->segments[0])
            .named_codes.empty());
    ASSERT_TRUE(authored.redo().value());

    saved = authored.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto with_codes{files.load()};
    auto const coded_other{
        declaration_id(with_codes, "authored_scalars", "OtherScalar", "authored")};
    auto const coded_signed{
        declaration_id(with_codes, "authored_scalars", "SignedScalar", "authored")};
    auto const coded_packed{
        declaration_id(with_codes, "authored_packed", "ExistingPacked", "authored")};
    ASSERT_EQ(with_codes.integer_scalar_schema(coded_other)->named_codes.size(), 1U);
    ASSERT_EQ(with_codes.integer_scalar_schema(coded_signed)->named_codes.size(), 1U);
    ASSERT_EQ(std::get<codegen::PackedFieldSchema>(
                  with_codes.packed_value_schema(coded_packed)->segments[0])
                  .named_codes.size(),
              1U);

    auto other_without_code{*with_codes.integer_scalar_schema(coded_other)};
    other_without_code.named_codes.clear();
    applied = with_codes.apply(
        ReplaceIntegerScalar{.declaration = coded_other, .schema = std::move(other_without_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    auto signed_without_code{*with_codes.integer_scalar_schema(coded_signed)};
    signed_without_code.named_codes.clear();
    applied = with_codes.apply(ReplaceIntegerScalar{.declaration = coded_signed,
                                                    .schema = std::move(signed_without_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    auto packed_without_code{*with_codes.packed_value_schema(coded_packed)};
    std::get<codegen::PackedFieldSchema>(packed_without_code.segments[0]).named_codes.clear();
    applied = with_codes.apply(
        ReplacePackedValue{.declaration = coded_packed, .schema = std::move(packed_without_code)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = with_codes.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(code OtherZero"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(code SignedZero"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(code ValueOne"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(relation references authored::Existing)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(relation references authored::OtherScalar)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note"), std::string::npos);

    saved = with_codes.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_other{
        declaration_id(final_document, "authored_scalars", "OtherScalar", "authored")};
    auto const final_signed{
        declaration_id(final_document, "authored_scalars", "SignedScalar", "authored")};
    auto const final_packed{
        declaration_id(final_document, "authored_packed", "ExistingPacked", "authored")};
    EXPECT_TRUE(final_document.integer_scalar_schema(final_other)->named_codes.empty());
    EXPECT_TRUE(final_document.integer_scalar_schema(final_signed)->named_codes.empty());
    EXPECT_TRUE(std::get<codegen::PackedFieldSchema>(
                    final_document.packed_value_schema(final_packed)->segments[0])
                    .named_codes.empty());
}

TEST(EditableSchemaDocument, PreservesPackedFormattingForNonStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto replacement{*document.packed_value_schema(packed)};
    replacement.invalid_value = 4'294'967'294U;
    replacement.export_specifier = "PACKED_API";
    replacement.mutable_value = true;
    replacement.byte_order = codegen::PackedByteOrder::big_endian;
    replacement.bit_order = codegen::PackedBitOrder::most_significant_first;
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments[1])};
    field.type.name = "std::uint32_t";
    field.bits = 7;
    field.range_helper = true;
    field.maximum_value = codegen::PackedIntegerValue{120};
    field.named_codes[0].value = codegen::PackedIntegerValue{127};
    field.relationship->kind = codegen::SemanticRelationKind::count_of;
    field.relationship->target.name = "authored::OtherScalar";
    std::get<codegen::PackedReservedBitsSchema>(replacement.segments[2]).bits = 17;

    auto applied{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.packed_value_schema(packed)->byte_order,
              codegen::PackedByteOrder::little_endian);
    EXPECT_FALSE(document.packed_value_schema(packed)->bit_order.has_value());
    EXPECT_FALSE(document.packed_value_schema(packed)->mutable_value);
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.packed_value_schema(packed)->byte_order,
              codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(document.packed_value_schema(packed)->bit_order,
              codegen::PackedBitOrder::most_significant_first);
    EXPECT_TRUE(document.packed_value_schema(packed)->mutable_value);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("; Keep the packed declaration note"), std::string::npos);
    EXPECT_NE(updated.find(":storage   std::uint32_t"), std::string::npos);
    EXPECT_NE(updated.find(":byte-order   big ; packed byte-order note"), std::string::npos);
    EXPECT_NE(updated.find(":bit-order msb-first"), std::string::npos);
    EXPECT_NE(updated.find(":invalid-value 4294967294"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the packed field note"), std::string::npos);
    EXPECT_NE(updated.find("(field counter std::uint32_t"), std::string::npos);
    EXPECT_NE(updated.find(":bits   7"), std::string::npos);
    EXPECT_NE(updated.find(":range-helper true"), std::string::npos);
    EXPECT_NE(updated.find("(code Invalid :value   127 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("; field code note"), std::string::npos);
    EXPECT_NE(updated.find("(relation count_of authored::OtherScalar)"), std::string::npos);
    EXPECT_NE(updated.find("(reserved future :bits   17)"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier PACKED_API"), std::string::npos);
    EXPECT_NE(updated.find(":mutable true"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const* schema{reloaded.packed_value_schema(reloaded_packed)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->invalid_value, 4'294'967'294U);
    EXPECT_EQ(schema->export_specifier, "PACKED_API");
    EXPECT_TRUE(schema->mutable_value);
    EXPECT_EQ(schema->byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(schema->bit_order, codegen::PackedBitOrder::most_significant_first);
    auto const& reloaded_type{packed_type(reloaded, reloaded_packed)};
    EXPECT_EQ(reloaded_type.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(reloaded_type.bit_order, codegen::PackedBitOrder::most_significant_first);
    auto const& reloaded_field{std::get<codegen::PackedFieldSchema>(schema->segments[1])};
    EXPECT_EQ(reloaded_field.type.name, "std::uint32_t");
    EXPECT_EQ(reloaded_field.bits, 7);
    EXPECT_TRUE(reloaded_field.range_helper);
    EXPECT_EQ(reloaded_field.maximum_value, codegen::PackedIntegerValue{120});
    EXPECT_EQ(reloaded_field.named_codes[0].value, codegen::PackedIntegerValue{127});
    ASSERT_TRUE(reloaded_field.relationship.has_value());
    EXPECT_EQ(reloaded_field.relationship->kind, codegen::SemanticRelationKind::count_of);
    EXPECT_EQ(reloaded_field.relationship->target.name, "authored::OtherScalar");
    EXPECT_EQ(std::get<codegen::PackedReservedBitsSchema>(schema->segments[2]).bits, 17);
}

TEST(EditableSchemaDocument, PreservesPackedSegmentRowsDuringDirectRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto field_replacement{*document.packed_value_schema(packed)};
    std::get<codegen::PackedFieldSchema>(field_replacement.segments[1]).name = "metadata";
    auto applied{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(field_replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_EQ(updated.find("(field counter "), std::string::npos);
    EXPECT_NE(updated.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(updated.find("(field metadata std::uint16_t"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the packed field note."), std::string::npos);
    EXPECT_NE(updated.find(":bits   8"), std::string::npos);
    EXPECT_NE(updated.find("(code Invalid :value   255 :sentinel true) ; field code note"),
              std::string::npos);
    EXPECT_NE(updated.find("; pending field code note"), std::string::npos);
    EXPECT_NE(updated.find("(code Pending :value 126 :sentinel true)"), std::string::npos);
    EXPECT_NE(updated.find("; field relationship note"), std::string::npos);
    EXPECT_NE(updated.find("(relation index_into authored::ExistingScalar)"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the future segment note."), std::string::npos);
    EXPECT_NE(updated.find("(reserved future :bits   16)"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reserved_document{files.load()};
    auto const saved_packed{
        declaration_id(reserved_document, "authored_packed", "ExistingPacked", "authored")};
    auto reserved_replacement{*reserved_document.packed_value_schema(saved_packed)};
    std::get<codegen::PackedReservedBitsSchema>(reserved_replacement.segments[2]).name = "spare";
    applied = reserved_document.apply(
        ReplacePackedValue{.declaration = saved_packed, .schema = std::move(reserved_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = reserved_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& reserved_updated{preview->front().updated};
    EXPECT_NE(reserved_updated.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(reserved_updated.find("(field metadata std::uint16_t"), std::string::npos);
    EXPECT_NE(reserved_updated.find("; field relationship note"), std::string::npos);
    EXPECT_EQ(reserved_updated.find("(reserved future "), std::string::npos);
    EXPECT_NE(reserved_updated.find("; Keep the future segment note."), std::string::npos);
    EXPECT_NE(reserved_updated.find("(reserved spare :bits   16)"), std::string::npos);

    ASSERT_TRUE(reserved_document.undo().value());
    preview = reserved_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(reserved_document.redo().value());

    saved = reserved_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const* schema{reloaded.packed_value_schema(reloaded_packed)};
    ASSERT_NE(schema, nullptr);
    ASSERT_EQ(schema->segments.size(), 3U);
    auto const& field{std::get<codegen::PackedFieldSchema>(schema->segments[1])};
    EXPECT_EQ(field.name, "metadata");
    ASSERT_EQ(field.named_codes.size(), 2U);
    EXPECT_EQ(field.named_codes[0].name, "Invalid");
    EXPECT_EQ(field.named_codes[0].value, codegen::PackedIntegerValue{255});
    EXPECT_EQ(field.named_codes[1].name, "Pending");
    EXPECT_EQ(field.named_codes[1].value, codegen::PackedIntegerValue{126});
    ASSERT_TRUE(field.relationship.has_value());
    EXPECT_EQ(field.relationship->kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(field.relationship->target.name, "authored::ExistingScalar");
    EXPECT_EQ(std::get<codegen::PackedReservedBitsSchema>(schema->segments[2]).name, "spare");

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(module_source->text.find("(field metadata std::uint16_t"), std::string::npos);
    EXPECT_NE(module_source->text.find("; field relationship note"), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the future segment note."), std::string::npos);
    EXPECT_NE(module_source->text.find("(reserved spare :bits   16)"), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesPackedSegmentsAndCommentsForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto replacement{*document.packed_value_schema(packed)};
    replacement.bit_order = codegen::PackedBitOrder::most_significant_first;
    auto value{replacement.segments[0]};
    auto counter{replacement.segments[1]};
    auto future{replacement.segments[2]};
    auto& counter_field{std::get<codegen::PackedFieldSchema>(counter)};
    auto invalid{counter_field.named_codes[0]};
    auto pending{counter_field.named_codes[1]};
    invalid.value = 253;
    auto pending_copy{pending};
    pending_copy.name = "PendingCopy";
    pending_copy.value = 252;
    counter_field.named_codes = {pending, pending_copy, invalid};
    counter_field.relationship->kind = codegen::SemanticRelationKind::count_of;
    counter_field.relationship->target.name = "authored::OtherScalar";
    std::get<codegen::PackedReservedBitsSchema>(future).bits = 8;
    auto duplicate{future};
    std::get<codegen::PackedReservedBitsSchema>(duplicate).name = "future_copy";
    replacement.segments = {future, counter, duplicate, value};

    auto applied{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const future_comment{updated.find("; Keep the future segment note.")};
    auto const future_position{updated.find("(reserved future")};
    auto const counter_comment{updated.find("; Keep the counter segment note.")};
    auto const counter_position{updated.find("(field counter")};
    auto const copy_position{updated.find("(reserved future_copy")};
    auto const value_comment{updated.find("; Keep the value segment note.")};
    auto const value_position{updated.find("(field value")};
    ASSERT_NE(future_comment, std::string::npos);
    ASSERT_NE(future_position, std::string::npos);
    ASSERT_NE(counter_comment, std::string::npos);
    ASSERT_NE(counter_position, std::string::npos);
    ASSERT_NE(copy_position, std::string::npos);
    ASSERT_NE(value_comment, std::string::npos);
    ASSERT_NE(value_position, std::string::npos);
    EXPECT_LT(future_comment, future_position);
    EXPECT_LT(future_position, counter_comment);
    EXPECT_LT(counter_comment, counter_position);
    EXPECT_LT(counter_position, copy_position);
    EXPECT_LT(copy_position, value_comment);
    EXPECT_LT(value_comment, value_position);
    EXPECT_NE(updated.find("; Keep the packed declaration note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the packed field note"), std::string::npos);
    auto const pending_code_comment{updated.find("; pending field code note")};
    auto const pending_code{updated.find("(code Pending :")};
    auto const pending_copy_code{updated.find("(code PendingCopy")};
    auto const invalid_code{updated.find("(code Invalid")};
    ASSERT_NE(pending_code_comment, std::string::npos);
    ASSERT_NE(pending_code, std::string::npos);
    ASSERT_NE(pending_copy_code, std::string::npos);
    ASSERT_NE(invalid_code, std::string::npos);
    EXPECT_LT(pending_code_comment, pending_code);
    EXPECT_LT(pending_code, pending_copy_code);
    EXPECT_LT(pending_copy_code, invalid_code);
    EXPECT_NE(updated.find("(code Invalid :value   253 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("(code PendingCopy :value 252 :sentinel true)"), std::string::npos);
    EXPECT_NE(updated.find("; field code note"), std::string::npos);
    EXPECT_NE(updated.find("; pending field trailing note"), std::string::npos);
    EXPECT_NE(updated.find("; field relationship note"), std::string::npos);
    EXPECT_NE(updated.find("(relation count_of authored::OtherScalar)"), std::string::npos);
    EXPECT_NE(updated.find("; value segment trailing note"), std::string::npos);
    EXPECT_NE(updated.find(":bit-order msb-first"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.packed_value_schema(packed)};
    std::erase_if(deletion.segments, [](auto const& segment) {
        return codegen::packed_segment_name(segment) == "value";
    });
    auto& deleting_counter{std::get<codegen::PackedFieldSchema>(deletion.segments[1])};
    std::erase_if(deleting_counter.named_codes,
                  [](auto const& code) { return code.name == "Pending"; });
    applied =
        document.apply(ReplacePackedValue{.declaration = packed, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& deletion_source{preview->front().updated};
    auto const packed_begin{deletion_source.find("(packed-value ExistingPacked")};
    auto const packed_end{deletion_source.find("(module authored_scalars", packed_begin)};
    ASSERT_NE(packed_begin, std::string::npos);
    ASSERT_NE(packed_end, std::string::npos);
    auto const packed_source{deletion_source.substr(packed_begin, packed_end - packed_begin)};
    EXPECT_EQ(packed_source.find("(field value"), std::string::npos);
    EXPECT_EQ(packed_source.find("; Keep the value segment note."), std::string::npos);
    EXPECT_EQ(packed_source.find("; value segment trailing note"), std::string::npos);
    EXPECT_EQ(packed_source.find("(code Pending :"), std::string::npos);
    EXPECT_EQ(packed_source.find("; pending field code note"), std::string::npos);
    EXPECT_EQ(packed_source.find("; pending field trailing note"), std::string::npos);
    EXPECT_NE(packed_source.find("(code PendingCopy :value 252 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(packed_source.find("; field relationship note"), std::string::npos);
    EXPECT_NE(packed_source.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(packed_source.find("; Keep the future segment note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(field value"), std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const* schema{reloaded.packed_value_schema(reloaded_packed)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_order, codegen::PackedBitOrder::most_significant_first);
    ASSERT_EQ(schema->segments.size(), 3U);
    EXPECT_EQ(codegen::packed_segment_name(schema->segments[0]), "future");
    EXPECT_EQ(codegen::packed_segment_name(schema->segments[1]), "counter");
    EXPECT_EQ(codegen::packed_segment_name(schema->segments[2]), "future_copy");
    EXPECT_EQ(std::get<codegen::PackedReservedBitsSchema>(schema->segments[0]).bits, 8);
    auto const& reloaded_counter{std::get<codegen::PackedFieldSchema>(schema->segments[1])};
    ASSERT_EQ(reloaded_counter.named_codes.size(), 2U);
    EXPECT_EQ(reloaded_counter.named_codes[0].name, "PendingCopy");
    EXPECT_EQ(reloaded_counter.named_codes[0].value, codegen::PackedIntegerValue{252});
    EXPECT_EQ(reloaded_counter.named_codes[1].name, "Invalid");
    EXPECT_EQ(reloaded_counter.named_codes[1].value, codegen::PackedIntegerValue{253});
    ASSERT_TRUE(reloaded_counter.relationship.has_value());
    EXPECT_EQ(reloaded_counter.relationship->kind, codegen::SemanticRelationKind::count_of);
    EXPECT_EQ(reloaded_counter.relationship->target.name, "authored::OtherScalar");

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_EQ(module_source->text.find("; Keep the value segment note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the future segment note."), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesAggregateFormattingForNonStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const raw{declaration_id(document, "authored_unions", "ExistingUnion", "authored")};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};

    auto record_replacement{*document.record_schema(record)};
    record_replacement.export_specifier = "RECORD_API";
    record_replacement.members[0].type.name = "std::uint64_t";
    record_replacement.members[0].count = 2;
    auto record_applied{document.apply(
        ReplaceRecord{.declaration = record, .schema = std::move(record_replacement)})};
    ASSERT_TRUE(record_applied.has_value()) << record_applied.error().message;

    auto raw_replacement{*document.union_schema(raw)};
    raw_replacement.export_specifier = "UNION_API";
    raw_replacement.alternatives[0].type.name = "std::uint64_t";
    raw_replacement.alternatives[0].count = 3;
    auto raw_applied{
        document.apply(ReplaceUnion{.declaration = raw, .schema = std::move(raw_replacement)})};
    ASSERT_TRUE(raw_applied.has_value()) << raw_applied.error().message;

    auto tagged_replacement{*document.tagged_union_schema(tagged)};
    tagged_replacement.export_specifier = "TAGGED_API";
    tagged_replacement.alternatives[0].type.name = "std::uint16_t";
    tagged_replacement.alternatives[0].count = 4;
    tagged_replacement.alternatives[0].tag = "One";
    auto tagged_applied{document.apply(
        ReplaceTaggedUnion{.declaration = tagged, .schema = std::move(tagged_replacement)})};
    ASSERT_TRUE(tagged_applied.has_value()) << tagged_applied.error().message;
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("; Keep the record note"), std::string::npos);
    EXPECT_NE(updated.find("(member value   std::uint64_t"), std::string::npos);
    EXPECT_NE(updated.find(":count 2"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier RECORD_API"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the raw union note"), std::string::npos);
    EXPECT_NE(updated.find("(alternative value   std::uint64_t"), std::string::npos);
    EXPECT_NE(updated.find(":count 3"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier UNION_API"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the tagged union note"), std::string::npos);
    EXPECT_NE(updated.find(":discriminant   authored::Existing"), std::string::npos);
    EXPECT_NE(updated.find("(alternative value   std::uint16_t"), std::string::npos);
    EXPECT_NE(updated.find(":tag One"), std::string::npos);
    EXPECT_NE(updated.find(":count 4"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier TAGGED_API"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ExistingRecord", "authored")};
    auto const reloaded_raw{
        declaration_id(reloaded, "authored_unions", "ExistingUnion", "authored")};
    auto const reloaded_tagged{
        declaration_id(reloaded, "authored_unions", "ExistingTagged", "authored")};
    auto const* record_schema{reloaded.record_schema(reloaded_record)};
    auto const* raw_schema{reloaded.union_schema(reloaded_raw)};
    auto const* tagged_schema{reloaded.tagged_union_schema(reloaded_tagged)};
    ASSERT_NE(record_schema, nullptr);
    ASSERT_NE(raw_schema, nullptr);
    ASSERT_NE(tagged_schema, nullptr);
    EXPECT_EQ(record_schema->export_specifier, "RECORD_API");
    EXPECT_EQ(record_schema->members[0].type.name, "std::uint64_t");
    EXPECT_EQ(record_schema->members[0].count, 2U);
    EXPECT_EQ(raw_schema->export_specifier, "UNION_API");
    EXPECT_EQ(raw_schema->alternatives[0].type.name, "std::uint64_t");
    EXPECT_EQ(raw_schema->alternatives[0].count, 3U);
    EXPECT_EQ(tagged_schema->export_specifier, "TAGGED_API");
    EXPECT_EQ(tagged_schema->alternatives[0].type.name, "std::uint16_t");
    EXPECT_EQ(tagged_schema->alternatives[0].count, 4U);
    EXPECT_EQ(tagged_schema->alternatives[0].tag, "One");
}

TEST(EditableSchemaDocument, RejectsMalformedAggregateExportSpecifiersWithoutMutation) {
    TemporarySchema files;
    auto document{files.load()};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const raw{declaration_id(document, "authored_unions", "ExistingUnion", "authored")};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto const revision_before{document.revision()};

    auto packed_replacement{*document.packed_value_schema(packed)};
    packed_replacement.export_specifier = "bad-specifier";
    auto applied{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(packed_replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("export specifier"), std::string::npos);

    auto record_replacement{*document.record_schema(record)};
    record_replacement.export_specifier = "two words";
    applied = document.apply(
        ReplaceRecord{.declaration = record, .schema = std::move(record_replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("export specifier"), std::string::npos);

    auto raw_replacement{*document.union_schema(raw)};
    raw_replacement.export_specifier = "class";
    applied =
        document.apply(ReplaceUnion{.declaration = raw, .schema = std::move(raw_replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("export specifier"), std::string::npos);

    auto tagged_replacement{*document.tagged_union_schema(tagged)};
    tagged_replacement.export_specifier = "bad-specifier";
    applied = document.apply(
        ReplaceTaggedUnion{.declaration = tagged, .schema = std::move(tagged_replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("export specifier"), std::string::npos);

    EXPECT_EQ(document.revision(), revision_before);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.packed_value_schema(packed)->export_specifier.has_value());
    EXPECT_FALSE(document.record_schema(record)->export_specifier.has_value());
    EXPECT_FALSE(document.union_schema(raw)->export_specifier.has_value());
    EXPECT_FALSE(document.tagged_union_schema(tagged)->export_specifier.has_value());
}

TEST(EditableSchemaDocument, AuthorsRecordMemberRelationshipsWithReferenceSafety) {
    TemporarySchema files;
    auto document{files.load()};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const other_scalar{
        declaration_id(document, "authored_scalars", "OtherScalar", "authored")};

    auto replacement{*document.record_schema(record)};
    replacement.members[0].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::ExistingScalar"},
                                        .unit = std::nullopt};
    auto applied{
        document.apply(ReplaceRecord{.declaration = record, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(relation references authored::ExistingScalar)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value member note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; value member trailing note"), std::string::npos);

    replacement = *document.record_schema(record);
    replacement.members[0].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::offset_into,
                                        .target = codegen::TypeRef{"authored::OtherScalar"},
                                        .unit = codegen::SemanticRelationUnit::bytes};
    applied =
        document.apply(ReplaceRecord{.declaration = record, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto blocked_delete{document.apply(DeleteIntegerScalar{.declaration = other_scalar})};
    ASSERT_FALSE(blocked_delete.has_value());
    EXPECT_NE(blocked_delete.error().message.find("ExistingRecord"), std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.record_schema(record)->members[0].relationship->kind,
              codegen::SemanticRelationKind::references);
    ASSERT_TRUE(document.redo().value());

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(
        preview->front().updated.find("(relation offset_into authored::OtherScalar :unit bytes)"),
        std::string::npos);
    auto const record_type_id{document.types().find(document.declaration(record)->identity)};
    auto const other_type_id{document.types().find(document.declaration(other_scalar)->identity)};
    ASSERT_TRUE(record_type_id.has_value());
    ASSERT_TRUE(other_type_id.has_value());
    auto const& resolved_record{
        std::get<RecordType>(document.types().type(*record_type_id).definition)};
    ASSERT_TRUE(resolved_record.members[0].relationship.has_value());
    EXPECT_EQ(resolved_record.members[0].relationship->target.type, *other_type_id);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ExistingRecord", "authored")};
    auto const* reloaded_schema{reloaded.record_schema(reloaded_record)};
    ASSERT_NE(reloaded_schema, nullptr);
    ASSERT_TRUE(reloaded_schema->members[0].relationship.has_value());
    EXPECT_EQ(reloaded_schema->members[0].relationship->kind,
              codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(reloaded_schema->members[0].relationship->target.name, "authored::OtherScalar");
    EXPECT_EQ(reloaded_schema->members[0].relationship->unit, codegen::SemanticRelationUnit::bytes);

    auto cleared{*reloaded_schema};
    cleared.members[0].relationship.reset();
    applied =
        reloaded.apply(ReplaceRecord{.declaration = reloaded_record, .schema = std::move(cleared)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(relation offset_into"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value member note."), std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_record{
        declaration_id(final_document, "authored_records", "ExistingRecord", "authored")};
    EXPECT_FALSE(final_document.record_schema(final_record)->members[0].relationship.has_value());
}

TEST(EditableSchemaDocument, AuthorsSoaColumnRelationshipsWithReferenceSafety) {
    TemporarySchema files;
    auto document{files.load()};
    auto const soa{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const other_scalar{
        declaration_id(document, "authored_scalars", "OtherScalar", "authored")};

    auto replacement{*document.soa_schema(soa)};
    replacement.members[0].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::ExistingScalar"},
                                        .unit = std::nullopt};
    auto applied{document.apply(ReplaceSoa{.declaration = soa, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(relation references authored::ExistingScalar)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the values SoA member note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; SoA values trailing note"), std::string::npos);

    replacement = *document.soa_schema(soa);
    replacement.members[0].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::offset_into,
                                        .target = codegen::TypeRef{"authored::OtherScalar"},
                                        .unit = codegen::SemanticRelationUnit::elements};
    applied = document.apply(ReplaceSoa{.declaration = soa, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto blocked_delete{document.apply(DeleteIntegerScalar{.declaration = other_scalar})};
    ASSERT_FALSE(blocked_delete.has_value());
    EXPECT_NE(blocked_delete.error().message.find("ExistingSoa"), std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.soa_schema(soa)->members[0].relationship->kind,
              codegen::SemanticRelationKind::references);
    ASSERT_TRUE(document.redo().value());

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find(
                  "(relation offset_into authored::OtherScalar :unit elements)"),
              std::string::npos);
    auto const soa_type_id{document.types().find(document.declaration(soa)->identity)};
    auto const other_type_id{document.types().find(document.declaration(other_scalar)->identity)};
    ASSERT_TRUE(soa_type_id.has_value());
    ASSERT_TRUE(other_type_id.has_value());
    auto const& resolved{std::get<SoaType>(document.types().type(*soa_type_id).definition)};
    ASSERT_TRUE(resolved.columns[0].relationship.has_value());
    EXPECT_EQ(resolved.columns[0].relationship->target.type, *other_type_id);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_soa{declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* reloaded_schema{reloaded.soa_schema(reloaded_soa)};
    ASSERT_NE(reloaded_schema, nullptr);
    ASSERT_TRUE(reloaded_schema->members[0].relationship.has_value());
    EXPECT_EQ(reloaded_schema->members[0].relationship->kind,
              codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(reloaded_schema->members[0].relationship->target.name, "authored::OtherScalar");
    EXPECT_EQ(reloaded_schema->members[0].relationship->unit,
              codegen::SemanticRelationUnit::elements);

    auto cleared{*reloaded_schema};
    cleared.members[0].relationship.reset();
    applied = reloaded.apply(ReplaceSoa{.declaration = reloaded_soa, .schema = std::move(cleared)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(relation offset_into"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the values SoA member note."),
              std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_soa{declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_FALSE(final_document.soa_schema(final_soa)->members[0].relationship.has_value());
}

TEST(EditableSchemaDocument, PreservesRecordMembersAndCommentsForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto replacement{*document.record_schema(record)};
    replacement.export_specifier = "RECORD_API";
    auto value{replacement.members[0]};
    auto flag{replacement.members[1]};
    value.type.name = "std::uint64_t";
    value.count = 2;
    auto duplicate{flag};
    duplicate.name = "flag_copy";
    duplicate.count = 3;
    replacement.members = {flag, duplicate, value};

    auto applied{
        document.apply(ReplaceRecord{.declaration = record, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const flag_comment{updated.find("; Keep the flag member note.")};
    auto const flag_position{updated.find("(member flag ")};
    auto const copy_position{updated.find("(member flag_copy ")};
    auto const record_comment{updated.find("; Keep the record note.")};
    auto const value_comment{updated.find("; Keep the value member note.")};
    auto const value_position{updated.find("(member value ")};
    ASSERT_NE(flag_comment, std::string::npos);
    ASSERT_NE(flag_position, std::string::npos);
    ASSERT_NE(copy_position, std::string::npos);
    ASSERT_NE(record_comment, std::string::npos);
    ASSERT_NE(value_comment, std::string::npos);
    ASSERT_NE(value_position, std::string::npos);
    EXPECT_LT(flag_comment, flag_position);
    EXPECT_LT(flag_position, copy_position);
    EXPECT_LT(copy_position, record_comment);
    EXPECT_LT(record_comment, value_comment);
    EXPECT_LT(value_comment, value_position);
    EXPECT_NE(updated.find("(member value   std::uint64_t"), std::string::npos);
    EXPECT_NE(updated.find(":count 2"), std::string::npos);
    EXPECT_NE(updated.find("(member flag_copy std::uint8_t :count 3)"), std::string::npos);
    EXPECT_NE(updated.find("; value member trailing note"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier RECORD_API"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.record_schema(record)};
    std::erase_if(deletion.members, [](auto const& member) { return member.name == "value"; });
    applied = document.apply(ReplaceRecord{.declaration = record, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(member value "), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Keep the record note."), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Keep the value member note."), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; value member trailing note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the flag member note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(member value "), std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ExistingRecord", "authored")};
    auto const* schema{reloaded.record_schema(reloaded_record)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "RECORD_API");
    ASSERT_EQ(schema->members.size(), 2U);
    EXPECT_EQ(schema->members[0].name, "flag");
    EXPECT_EQ(schema->members[1].name, "flag_copy");
    EXPECT_EQ(schema->members[1].count, 3U);

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_EQ(module_source->text.find("; Keep the record note."), std::string::npos);
    EXPECT_EQ(module_source->text.find("; Keep the value member note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the flag member note."), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesRawUnionAlternativesForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const raw{declaration_id(document, "authored_unions", "ExistingUnion", "authored")};
    auto replacement{*document.union_schema(raw)};
    auto value{replacement.alternatives[0]};
    auto small{replacement.alternatives[1]};
    value.type.name = "std::uint64_t";
    value.count = 2;
    auto duplicate{small};
    duplicate.name = "small_copy";
    duplicate.count = 3;
    replacement.alternatives = {small, duplicate, value};

    auto applied{
        document.apply(ReplaceUnion{.declaration = raw, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const small_comment{updated.find("; Keep the small alternative note.")};
    auto const small_position{updated.find("(alternative small ")};
    auto const copy_position{updated.find("(alternative small_copy ")};
    auto const value_comment{updated.find("; Keep the value alternative note.")};
    auto const value_position{updated.find("(alternative value ")};
    ASSERT_NE(small_comment, std::string::npos);
    ASSERT_NE(small_position, std::string::npos);
    ASSERT_NE(copy_position, std::string::npos);
    ASSERT_NE(value_comment, std::string::npos);
    ASSERT_NE(value_position, std::string::npos);
    EXPECT_LT(small_comment, small_position);
    EXPECT_LT(small_position, copy_position);
    EXPECT_LT(copy_position, value_comment);
    EXPECT_LT(value_comment, value_position);
    EXPECT_NE(updated.find("(alternative value   std::uint64_t"), std::string::npos);
    EXPECT_NE(updated.find(":count 2"), std::string::npos);
    EXPECT_NE(updated.find("(alternative small_copy std::uint8_t :count 3)"), std::string::npos);
    EXPECT_NE(updated.find("; value alternative trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_raw{
        declaration_id(reloaded, "authored_unions", "ExistingUnion", "authored")};
    auto const* schema{reloaded.union_schema(reloaded_raw)};
    ASSERT_NE(schema, nullptr);
    ASSERT_EQ(schema->alternatives.size(), 3U);
    EXPECT_EQ(schema->alternatives[0].name, "small");
    EXPECT_EQ(schema->alternatives[1].name, "small_copy");
    EXPECT_EQ(schema->alternatives[2].name, "value");
    EXPECT_EQ(schema->alternatives[1].count, 3U);
    EXPECT_EQ(schema->alternatives[2].type.name, "std::uint64_t");
    EXPECT_EQ(schema->alternatives[2].count, 2U);
}

TEST(EditableSchemaDocument, PreservesTaggedUnionAlternativesForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto replacement{*document.tagged_union_schema(tagged)};
    replacement.export_specifier = "TAGGED_API";
    auto value{replacement.alternatives[0]};
    value.type.name = "std::uint64_t";
    value.count = 2;
    codegen::TaggedUnionAlternativeSchema small{};
    small.name = "small";
    small.type.name = "std::uint8_t";
    small.tag = "One";
    small.count = 3;
    replacement.alternatives = {small, value};

    auto applied{document.apply(
        ReplaceTaggedUnion{.declaration = tagged, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const tagged_position{updated.find("(tagged-union ExistingTagged")};
    ASSERT_NE(tagged_position, std::string::npos);
    auto const small_position{updated.find("(alternative small ", tagged_position)};
    auto const declaration_comment{updated.find("; Keep the tagged union note.", tagged_position)};
    auto const value_comment{
        updated.find("; Keep the tagged value alternative note.", tagged_position)};
    auto const value_position{updated.find("(alternative value ", tagged_position)};
    ASSERT_NE(small_position, std::string::npos);
    ASSERT_NE(declaration_comment, std::string::npos);
    ASSERT_NE(value_comment, std::string::npos);
    ASSERT_NE(value_position, std::string::npos);
    EXPECT_LT(declaration_comment, small_position);
    EXPECT_LT(small_position, value_comment);
    EXPECT_LT(value_comment, value_position);
    EXPECT_NE(updated.find("(alternative small std::uint8_t :tag One :count 3)"),
              std::string::npos);
    EXPECT_NE(updated.find("(alternative value   std::uint64_t :tag Zero", tagged_position),
              std::string::npos);
    EXPECT_NE(updated.find(":count 2", value_position), std::string::npos);
    EXPECT_NE(updated.find("; tagged value trailing note"), std::string::npos);
    EXPECT_NE(updated.find(":discriminant   authored::Existing"), std::string::npos);
    EXPECT_NE(updated.find(":export-specifier TAGGED_API"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.tagged_union_schema(tagged)};
    std::erase_if(deletion.alternatives,
                  [](auto const& alternative) { return alternative.name == "value"; });
    applied =
        document.apply(ReplaceTaggedUnion{.declaration = tagged, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const deleted_tagged_position{
        preview->front().updated.find("(tagged-union ExistingTagged")};
    ASSERT_NE(deleted_tagged_position, std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(alternative value ", deleted_tagged_position),
              std::string::npos);
    EXPECT_NE(
        preview->front().updated.find("; Keep the tagged union note.", deleted_tagged_position),
        std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Keep the tagged value alternative note.",
                                            deleted_tagged_position),
              std::string::npos);
    EXPECT_EQ(
        preview->front().updated.find("; tagged value trailing note", deleted_tagged_position),
        std::string::npos);
    EXPECT_NE(preview->front().updated.find("(alternative small ", deleted_tagged_position),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const restored_tagged_position{
        preview->front().updated.find("(tagged-union ExistingTagged")};
    EXPECT_NE(preview->front().updated.find("(alternative value ", restored_tagged_position),
              std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_tagged{
        declaration_id(reloaded, "authored_unions", "ExistingTagged", "authored")};
    auto const* schema{reloaded.tagged_union_schema(reloaded_tagged)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "TAGGED_API");
    ASSERT_EQ(schema->alternatives.size(), 1U);
    EXPECT_EQ(schema->alternatives[0].name, "small");
    EXPECT_EQ(schema->alternatives[0].tag, "One");
    EXPECT_EQ(schema->alternatives[0].count, 3U);
}

TEST(EditableSchemaDocument, PreservesAggregateChildRowsDuringDirectRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const raw{declaration_id(document, "authored_unions", "ExistingUnion", "authored")};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};

    auto record_replacement{*document.record_schema(record)};
    record_replacement.members[0].name = "payload";
    auto applied{document.apply(
        ReplaceRecord{.declaration = record, .schema = std::move(record_replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto raw_replacement{*document.union_schema(raw)};
    raw_replacement.alternatives[0].name = "payload";
    applied =
        document.apply(ReplaceUnion{.declaration = raw, .schema = std::move(raw_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto tagged_replacement{*document.tagged_union_schema(tagged)};
    tagged_replacement.alternatives[0].name = "payload";
    applied = document.apply(
        ReplaceTaggedUnion{.declaration = tagged, .schema = std::move(tagged_replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};

    auto const record_begin{updated.find("(record ExistingRecord")};
    auto const record_end{updated.find("(module authored_unions", record_begin)};
    ASSERT_NE(record_begin, std::string::npos);
    ASSERT_NE(record_end, std::string::npos);
    auto const record_source{updated.substr(record_begin, record_end - record_begin)};
    auto const record_comment{record_source.find("; Keep the value member note.")};
    auto const record_row{record_source.find("(member payload   std::uint32_t)")};
    ASSERT_NE(record_comment, std::string::npos);
    ASSERT_NE(record_row, std::string::npos) << record_source;
    EXPECT_LT(record_comment, record_row);
    EXPECT_NE(record_source.find("; value member trailing note"), std::string::npos);

    auto const raw_begin{updated.find("(union ExistingUnion")};
    auto const raw_end{updated.find("(tagged-union ExistingTagged", raw_begin)};
    ASSERT_NE(raw_begin, std::string::npos);
    ASSERT_NE(raw_end, std::string::npos);
    auto const raw_source{updated.substr(raw_begin, raw_end - raw_begin)};
    auto const raw_comment{raw_source.find("; Keep the value alternative note.")};
    auto const raw_row{raw_source.find("(alternative payload   std::uint32_t)")};
    ASSERT_NE(raw_comment, std::string::npos);
    ASSERT_NE(raw_row, std::string::npos) << raw_source;
    EXPECT_LT(raw_comment, raw_row);
    EXPECT_NE(raw_source.find("; value alternative trailing note"), std::string::npos);

    auto const tagged_begin{updated.find("(tagged-union ExistingTagged")};
    auto const tagged_end{updated.find("(module authored_soa", tagged_begin)};
    ASSERT_NE(tagged_begin, std::string::npos);
    ASSERT_NE(tagged_end, std::string::npos);
    auto const tagged_source{updated.substr(tagged_begin, tagged_end - tagged_begin)};
    auto const tagged_comment{tagged_source.find("; Keep the tagged value alternative note.")};
    auto const tagged_row{tagged_source.find("(alternative payload   std::uint32_t :tag Zero)")};
    ASSERT_NE(tagged_comment, std::string::npos);
    ASSERT_NE(tagged_row, std::string::npos) << tagged_source;
    EXPECT_LT(tagged_comment, tagged_row);
    EXPECT_NE(tagged_source.find("; tagged value trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ExistingRecord", "authored")};
    auto const reloaded_raw{
        declaration_id(reloaded, "authored_unions", "ExistingUnion", "authored")};
    auto const reloaded_tagged{
        declaration_id(reloaded, "authored_unions", "ExistingTagged", "authored")};
    auto const& member{reloaded.record_schema(reloaded_record)->members[0]};
    EXPECT_EQ(member.name, "payload");
    EXPECT_EQ(member.type.name, "std::uint32_t");
    EXPECT_FALSE(member.count.has_value());
    auto const& raw_alternative{reloaded.union_schema(reloaded_raw)->alternatives[0]};
    EXPECT_EQ(raw_alternative.name, "payload");
    EXPECT_EQ(raw_alternative.type.name, "std::uint32_t");
    EXPECT_FALSE(raw_alternative.count.has_value());
    auto const& tagged_alternative{reloaded.tagged_union_schema(reloaded_tagged)->alternatives[0]};
    EXPECT_EQ(tagged_alternative.name, "payload");
    EXPECT_EQ(tagged_alternative.type.name, "std::uint32_t");
    EXPECT_EQ(tagged_alternative.tag, "Zero");
    EXPECT_FALSE(tagged_alternative.count.has_value());
}

TEST(EditableSchemaDocument, DeletesTaggedUnionThroughItsExactSourceRange) {
    TemporarySchema files;
    auto document{files.load()};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto const original_info{*document.declaration(tagged)};
    ASSERT_TRUE(original_info.source.has_value());
    auto const source{*original_info.source};
    auto const original_text{document.source_files()[source.source_file_index].text};
    auto expected_text{original_text};
    expected_text.erase(source.begin_offset, source.end_offset - source.begin_offset);

    ASSERT_TRUE(document.apply(DeleteTaggedUnion{.declaration = tagged}).value());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, expected_text);
    EXPECT_NE(preview->front().updated.find("(union ExistingUnion"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(tagged-union ExistingTagged"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(tagged)->source, original_info.source);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, RenamesEnumAndRepairsTaggedUnionDiscriminant) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_enum{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto const enum_module{document.declaration(existing_enum)->module_index};
    auto const existing_tagged{
        declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto const union_module{document.declaration(existing_tagged)->module_index};

    codegen::EnumSchema enum_schema{};
    enum_schema.name = "LocalKind";
    enum_schema.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    codegen::EnumeratorSchema value{};
    value.name = "Value";
    enum_schema.values.push_back(std::move(value));
    auto const enumeration{document.allocate_declaration_id()};
    ASSERT_TRUE(document
                    .apply(CreateEnum{.declaration = enumeration,
                                      .module_index = enum_module,
                                      .schema = std::move(enum_schema),
                                      .insertion_index = std::nullopt})
                    .has_value());

    codegen::TaggedUnionSchema tagged_schema{};
    tagged_schema.name = "LocalEvent";
    tagged_schema.discriminant.name = "authored::LocalKind";
    codegen::TaggedUnionAlternativeSchema alternative{};
    alternative.name = "value";
    alternative.type.name = "std::uint32_t";
    alternative.tag = "Value";
    tagged_schema.alternatives.push_back(std::move(alternative));
    auto const tagged{document.allocate_declaration_id()};
    ASSERT_TRUE(document
                    .apply(CreateTaggedUnion{.declaration = tagged,
                                             .module_index = union_module,
                                             .schema = std::move(tagged_schema),
                                             .insertion_index = std::nullopt})
                    .has_value());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = enumeration, .new_name = "LocalKindRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.tagged_union_schema(tagged)->discriminant.name,
              "authored::LocalKindRenamed");
    auto const& resolved{tagged_union_type(document, tagged)};
    EXPECT_EQ(document.types().type(resolved.discriminant.type).identity.name, "LocalKindRenamed");
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(":discriminant authored::LocalKindRenamed"),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.tagged_union_schema(tagged)->discriminant.name, "authored::LocalKind");
}

TEST(EditableSchemaDocument, PreservesOwnedSourceWhenRenamingExistingDeclaration) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto renamed{document.apply(
        RenameDeclaration{.declaration = scalar, .new_name = "ExistingScalarRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(*renamed);
    EXPECT_EQ(document.linear_quantized_schema(quantized)->source.name,
              "authored::ExistingScalarRenamed");
    ASSERT_TRUE(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])
            .relationship.has_value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])
            .relationship->target.name,
        "authored::ExistingScalarRenamed");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("(integer-scalar ExistingScalarRenamed"), std::string::npos);
    EXPECT_EQ(updated.find("(integer-scalar ExistingScalar\n"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the scalar domain note during ordinary edits."),
              std::string::npos);
    EXPECT_NE(updated.find(":minimum   0"), std::string::npos);
    EXPECT_NE(updated.find("(code Pending :value   2 :sentinel true)"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the quantization note."), std::string::npos);
    EXPECT_NE(updated.find(":source   authored::ExistingScalarRenamed"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the packed field note."), std::string::npos);
    EXPECT_NE(updated.find("(relation index_into authored::ExistingScalarRenamed)"),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalarRenamed", "authored")};
    auto const reloaded_quantized{
        declaration_id(reloaded, "authored_representations", "ExistingQ1", "authored")};
    EXPECT_NE(reloaded.declaration(reloaded_scalar), nullptr);
    EXPECT_EQ(reloaded.linear_quantized_schema(reloaded_quantized)->source.name,
              "authored::ExistingScalarRenamed");
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the scalar domain note during ordinary edits."),
              std::string::npos);
    EXPECT_NE(module_source->text.find("(code Pending :value   2 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the quantization note."), std::string::npos);
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
    auto create{document.apply(
        CreateEnum{.declaration = created,
                   .module_index = module_index,
                   .schema = codegen::EnumSchema{.name = "DesignedState",
                                                 .underlying_type = std::nullopt,
                                                 .bit_width = 3,
                                                 .signedness = false,
                                                 .values = {{.name = "Idle",
                                                             .initializer = "0",
                                                             .display_name = "Idle State",
                                                             .serialized_name = "idle"},
                                                            {.name = "Active",
                                                             .initializer = "7",
                                                             .display_name = "Active State",
                                                             .serialized_name = "active",
                                                             .sentinel = true}}}})};
    ASSERT_TRUE(create.has_value());
    EXPECT_TRUE(*create);

    auto const semantic{document.types().find_declared("authored_enums", "DesignedState")};
    ASSERT_TRUE(semantic.has_value());
    auto const& type{std::get<EnumType>(document.types().type(*semantic).definition)};
    ASSERT_EQ(type.enumerators.size(), 2);
    EXPECT_EQ(type.bit_width, 3);
    EXPECT_EQ(type.signedness, false);
    EXPECT_FALSE(type.underlying_type.has_value());
    EXPECT_EQ(type.enumerators[1].serialized_name, "active");
    EXPECT_TRUE(type.enumerators[1].sentinel);

    auto invalid_width{*document.enum_schema(created)};
    invalid_width.bit_width = 2;
    auto const rejected{
        document.apply(ReplaceEnum{.declaration = created, .schema = std::move(invalid_width)})};
    EXPECT_FALSE(rejected.has_value());
    EXPECT_EQ(document.enum_schema(created)->bit_width, 3);

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
    EXPECT_NE(preview->front().updated.find("(enum DesignedState\n"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bit-width 3"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":signed false"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":display-name \"Idle State\""), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":sentinel true"), std::string::npos);
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
    EXPECT_EQ(reloaded_enum.bit_width, 3);
    EXPECT_EQ(reloaded_enum.signedness, false);
    EXPECT_FALSE(reloaded_enum.underlying_type.has_value());
    EXPECT_EQ(reloaded_enum.enumerators[0].display_name, "Idle State");
    EXPECT_EQ(reloaded_enum.enumerators[1].display_name, "Enabled");
    EXPECT_EQ(reloaded_enum.enumerators[1].serialized_name, "active");
    EXPECT_TRUE(reloaded_enum.enumerators[1].sentinel);
    EXPECT_FALSE(document.dirty());
    auto const saved_declaration{
        document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_enums",
                                               .namespace_name = "authored",
                                               .name = "DesignedState"})};
    ASSERT_TRUE(saved_declaration.has_value());
    EXPECT_TRUE(document.declaration(*saved_declaration)->source.has_value());
}

TEST(EditableSchemaDocument, CreatesEnumAndBindsPackedFieldAcrossModules) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_enum{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const enumeration{document.allocate_declaration_id()};

    auto created{document.apply(CreateEnum{
        .declaration = enumeration,
        .module_index = document.declaration(existing_enum)->module_index,
        .schema = codegen::EnumSchema{.name = "PackedState",
                                      .underlying_type = codegen::TypeRef{"std::uint8_t"},
                                      .bit_width = 3,
                                      .signedness = false,
                                      .values = {{.name = "Idle", .initializer = "0"},
                                                 {.name = "Active", .initializer = "1"}}}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::PackedState"};
    field.bits = 3;
    field.kind = codegen::PackedFieldKind::enumeration;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const enum_type{document.types().find(document.declaration(enumeration)->identity)};
    ASSERT_TRUE(enum_type.has_value());
    auto const& bound_field{std::get<PackedField>(packed_type(document, packed).segments.front())};
    EXPECT_EQ(bound_field.semantic_type.type, *enum_type);
    EXPECT_EQ(bound_field.bit_width, 3U);
    EXPECT_EQ(bound_field.kind, codegen::PackedFieldKind::enumeration);

    ASSERT_TRUE(document.undo().value());
    EXPECT_NE(document.declaration(enumeration), nullptr);
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        "std::uint8_t");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(enumeration), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());
    auto const rebound_enum_type{
        document.types().find(document.declaration(enumeration)->identity)};
    ASSERT_TRUE(rebound_enum_type.has_value());
    EXPECT_EQ(
        std::get<PackedField>(packed_type(document, packed).segments.front()).semantic_type.type,
        *rebound_enum_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(enum PackedState std::uint8_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("authored::PackedState"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bits 3"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":kind enum"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_enum{declaration_id(reloaded, "authored_enums", "PackedState", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_enum_type{
        reloaded.types().find(reloaded.declaration(reloaded_enum)->identity)};
    ASSERT_TRUE(resolved_enum_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments.front())};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_enum_type);
    EXPECT_EQ(reloaded_field.bit_width, 3U);
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::enumeration);
}

TEST(EditableSchemaDocument, CreatesIntegerScalarAndBindsPackedFieldAcrossModules) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_scalar{
        declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const scalar{document.allocate_declaration_id()};

    auto const* original_packed{document.packed_value_schema(packed)};
    ASSERT_NE(original_packed, nullptr);
    auto const original_field{std::get<codegen::PackedFieldSchema>(original_packed->segments[1])};
    ASSERT_EQ(original_field.name, "counter");
    ASSERT_TRUE(original_field.relationship.has_value());
    auto expect_original_relationship =
        [&](std::optional<codegen::SemanticRelationSchema> const& relationship) {
            ASSERT_TRUE(relationship.has_value());
            EXPECT_EQ(relationship->kind, original_field.relationship->kind);
            EXPECT_EQ(relationship->target.name, original_field.relationship->target.name);
            EXPECT_EQ(relationship->target.suffix, original_field.relationship->target.suffix);
            EXPECT_EQ(relationship->target.nested, original_field.relationship->target.nested);
            EXPECT_EQ(relationship->unit, original_field.relationship->unit);
        };

    auto created{document.apply(CreateIntegerScalar{
        .declaration = scalar,
        .module_index = document.declaration(existing_scalar)->module_index,
        .schema =
            codegen::IntegerScalarSchema{.name = "CounterValue",
                                         .signedness = false,
                                         .minimum_value = *original_field.minimum_value,
                                         .maximum_value = *original_field.maximum_value,
                                         .bit_width = 8,
                                         .named_codes = original_field.named_codes,
                                         .relationship = std::nullopt,
                                         .cpp_emission = codegen::IntegerScalarCppEmission::none,
                                         .cpp_type = std::nullopt},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments[1])};
    field.type = codegen::TypeRef{"authored::CounterValue"};
    field.kind = codegen::PackedFieldKind::unsigned_integer;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const scalar_type{document.types().find(document.declaration(scalar)->identity)};
    ASSERT_TRUE(scalar_type.has_value());
    auto const& bound_field{std::get<PackedField>(packed_type(document, packed).segments[1])};
    EXPECT_EQ(bound_field.semantic_type.type, *scalar_type);
    EXPECT_EQ(bound_field.bit_width, 8U);
    EXPECT_FALSE(bound_field.bit_width_auto);
    EXPECT_EQ(bound_field.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(bound_field.maximum_value, codegen::PackedIntegerValue{100});
    ASSERT_EQ(bound_field.named_codes.size(), 2U);
    EXPECT_EQ(bound_field.named_codes[0].name, "Invalid");
    EXPECT_TRUE(bound_field.named_codes[0].sentinel);
    auto const& bound_schema_field{
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])};
    EXPECT_EQ(bound_schema_field.bits, original_field.bits);
    expect_original_relationship(bound_schema_field.relationship);
    EXPECT_FALSE(bound_schema_field.minimum_value.has_value());
    EXPECT_TRUE(bound_schema_field.named_codes.empty());

    ASSERT_TRUE(document.undo().value());
    EXPECT_NE(document.declaration(scalar), nullptr);
    auto const& restored_field{
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])};
    EXPECT_EQ(restored_field.type.name, original_field.type.name);
    EXPECT_EQ(restored_field.type.suffix, original_field.type.suffix);
    EXPECT_EQ(restored_field.type.nested, original_field.type.nested);
    EXPECT_EQ(restored_field.minimum_value, original_field.minimum_value);
    ASSERT_EQ(restored_field.named_codes.size(), original_field.named_codes.size());
    for (std::size_t index{}; index < original_field.named_codes.size(); ++index) {
        EXPECT_EQ(restored_field.named_codes[index].name, original_field.named_codes[index].name);
        EXPECT_EQ(restored_field.named_codes[index].value, original_field.named_codes[index].value);
        EXPECT_EQ(restored_field.named_codes[index].sentinel,
                  original_field.named_codes[index].sentinel);
    }
    expect_original_relationship(restored_field.relationship);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(scalar), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());
    auto const rebound_scalar_type{document.types().find(document.declaration(scalar)->identity)};
    ASSERT_TRUE(rebound_scalar_type.has_value());
    EXPECT_EQ(std::get<PackedField>(packed_type(document, packed).segments[1]).semantic_type.type,
              *rebound_scalar_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(integer-scalar CounterValue"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bit-width 8"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("authored::CounterValue"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the packed field note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the future segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "CounterValue", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_scalar_type{
        reloaded.types().find(reloaded.declaration(reloaded_scalar)->identity)};
    ASSERT_TRUE(resolved_scalar_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments[1])};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_scalar_type);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_EQ(reloaded_field.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(reloaded_field.maximum_value, codegen::PackedIntegerValue{100});
    ASSERT_EQ(reloaded_field.named_codes.size(), 2U);
    auto const& reloaded_schema_field{std::get<codegen::PackedFieldSchema>(
        reloaded.packed_value_schema(reloaded_packed)->segments[1])};
    expect_original_relationship(reloaded_schema_field.relationship);
    EXPECT_FALSE(reloaded_schema_field.minimum_value.has_value());
    EXPECT_TRUE(reloaded_schema_field.named_codes.empty());
}

TEST(EditableSchemaDocument, BindsPackedFieldToSharedIntegerScalarDomain) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments[1])};
    field.type = codegen::TypeRef{"authored::ExistingScalar"};
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::unsigned_integer;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    field.relationship.reset();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const scalar_type{document.types().find(document.declaration(scalar)->identity)};
    ASSERT_TRUE(scalar_type.has_value());
    auto const& bound_field{std::get<PackedField>(packed_type(document, packed).segments[1])};
    EXPECT_EQ(bound_field.semantic_type.type, *scalar_type);
    EXPECT_EQ(bound_field.bit_width, 2U);
    EXPECT_TRUE(bound_field.bit_width_auto);
    EXPECT_EQ(bound_field.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(bound_field.maximum_value, codegen::PackedIntegerValue{1});
    ASSERT_EQ(bound_field.named_codes.size(), 2U);
    EXPECT_EQ(bound_field.named_codes[0].name, "Pending");
    EXPECT_TRUE(bound_field.named_codes[0].sentinel);

    ASSERT_TRUE(document.undo().value());
    auto const& restored{
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])};
    EXPECT_EQ(restored.type.name, "std::uint16_t");
    EXPECT_EQ(restored.minimum_value, codegen::PackedIntegerValue{0});
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(std::get<PackedField>(packed_type(document, packed).segments[1]).semantic_type.type,
              *scalar_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("authored::ExistingScalar"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the future segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_scalar_type{
        reloaded.types().find(reloaded.declaration(reloaded_scalar)->identity)};
    ASSERT_TRUE(resolved_scalar_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments[1])};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_scalar_type);
    EXPECT_EQ(reloaded_field.bit_width, 2U);
    EXPECT_EQ(reloaded_field.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(reloaded_field.maximum_value, codegen::PackedIntegerValue{1});
    ASSERT_EQ(reloaded_field.named_codes.size(), 2U);
}

TEST(EditableSchemaDocument, BindsPackedFieldToLinearQuantizedRepresentation) {
    TemporarySchema files;
    auto document{files.load()};
    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::ExistingQ1"};
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::linear_quantized;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    field.relationship.reset();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const quantized_type{document.types().find(document.declaration(quantized)->identity)};
    auto const packed_type_id{document.types().find(document.declaration(packed)->identity)};
    ASSERT_TRUE(quantized_type.has_value());
    ASSERT_TRUE(packed_type_id.has_value());
    auto const& bound_field{std::get<PackedField>(packed_type(document, packed).segments.front())};
    EXPECT_EQ(bound_field.semantic_type.type, *quantized_type);
    EXPECT_EQ(bound_field.kind, codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(bound_field.bit_width, 1U);
    EXPECT_TRUE(bound_field.bit_width_auto);
    EXPECT_NE(std::ranges::find(document.types().dependencies_of(*packed_type_id), *quantized_type),
              document.types().dependencies_of(*packed_type_id).end());

    auto invalid{*document.packed_value_schema(packed)};
    std::get<codegen::PackedFieldSchema>(invalid.segments.front()).bits = 2;
    auto rejected{
        document.apply(ReplacePackedValue{.declaration = packed, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("must equal"), std::string::npos);
    EXPECT_TRUE(
        std::get<PackedField>(packed_type(document, packed).segments.front()).bit_width_auto);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        "std::uint8_t");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(
        std::get<PackedField>(packed_type(document, packed).segments.front()).semantic_type.type,
        *document.types().find(document.declaration(quantized)->identity));

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("authored::ExistingQ1"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bits auto"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":kind linear-quantized"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the counter segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_quantized{
        declaration_id(reloaded, "authored_representations", "ExistingQ1", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_quantized_type{
        reloaded.types().find(reloaded.declaration(reloaded_quantized)->identity)};
    ASSERT_TRUE(resolved_quantized_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments.front())};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_quantized_type);
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(reloaded_field.bit_width, 1U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
}

TEST(EditableSchemaDocument, CreatesFixedPointAndBindsPackedFieldAcrossModules) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_fixed{
        declaration_id(document, "authored_representations", "ExistingFixed", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const created{document.allocate_declaration_id()};
    auto const original_field{std::get<codegen::PackedFieldSchema>(
        document.packed_value_schema(packed)->segments.front())};
    ASSERT_EQ(original_field.kind, codegen::PackedFieldKind::unsigned_integer);

    auto added{document.apply(CreateFixedPoint{
        .declaration = created,
        .module_index = document.declaration(existing_fixed)->module_index,
        .schema = codegen::FixedPointSchema{.name = "ValueFixed",
                                            .signedness = false,
                                            .total_bits = 8,
                                            .fractional_bits = 4,
                                            .rounding = codegen::FixedPointRounding::nearest_even},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(added.has_value()) << added.error().message;
    ASSERT_TRUE(*added);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::ValueFixed"};
    field.kind = codegen::PackedFieldKind::fixed_point;
    field.bits.reset();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const fixed_type{document.types().find(document.declaration(created)->identity)};
    ASSERT_TRUE(fixed_type.has_value());
    auto const& placed_field{std::get<PackedField>(packed_type(document, packed).segments.front())};
    EXPECT_EQ(placed_field.semantic_type.type, *fixed_type);
    EXPECT_EQ(placed_field.bit_width, 8U);
    EXPECT_TRUE(placed_field.bit_width_auto);
    auto const& representation{
        std::get<FixedPointType>(document.types().type(*fixed_type).definition)};
    EXPECT_EQ(representation.fractional_bits, 4U);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        original_field.type.name);
    EXPECT_NE(document.declaration(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_TRUE(std::ranges::any_of(*preview, [](auto const& update) {
        return update.updated.find("ValueFixed") != std::string::npos &&
               update.updated.find(":fractional-bits 4") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(*preview, [](auto const& update) {
        return update.updated.find("authored::ValueFixed") != std::string::npos &&
               update.updated.find(":kind fixed-point") != std::string::npos &&
               update.updated.find("; Keep the value segment note.") != std::string::npos;
    }));

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_fixed{
        declaration_id(reloaded, "authored_representations", "ValueFixed", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_fixed_type{
        reloaded.types().find(reloaded.declaration(reloaded_fixed)->identity)};
    ASSERT_TRUE(resolved_fixed_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments.front())};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_fixed_type);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded.fixed_point_schema(reloaded_fixed)->fractional_bits, 4U);
}

TEST(EditableSchemaDocument, RollsBackFixedPointCreationWhenPackedBindingFails) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_fixed{
        declaration_id(document, "authored_representations", "ExistingFixed", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const created{document.allocate_declaration_id()};
    auto const identity{TypeIdentity{.origin = TypeOrigin::declaration,
                                     .module_name = "authored_representations",
                                     .namespace_name = "authored",
                                     .name = "TooNarrowFixed"}};
    auto added{document.apply(CreateFixedPoint{
        .declaration = created,
        .module_index = document.declaration(existing_fixed)->module_index,
        .schema = codegen::FixedPointSchema{.name = identity.name,
                                            .signedness = false,
                                            .total_bits = 7,
                                            .fractional_bits = 3,
                                            .rounding = codegen::FixedPointRounding::nearest_even},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(added.has_value()) << added.error().message;
    ASSERT_TRUE(*added);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::TooNarrowFixed"};
    field.kind = codegen::PackedFieldKind::fixed_point;
    field.bits = 8;
    auto rejected{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("must equal"), std::string::npos);
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        "std::uint8_t");

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    EXPECT_FALSE(document.types().find(identity).has_value());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, CreatesMiniFloatAndBindsPackedFieldAcrossModules) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_mini{
        declaration_id(document, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const created{document.allocate_declaration_id()};
    auto const original_field{std::get<codegen::PackedFieldSchema>(
        document.packed_value_schema(packed)->segments.front())};
    ASSERT_EQ(original_field.kind, codegen::PackedFieldKind::unsigned_integer);

    auto added{document.apply(
        CreateMiniFloat{.declaration = created,
                        .module_index = document.declaration(existing_mini)->module_index,
                        .schema = codegen::MiniFloatSchema{.name = "ValueFloat",
                                                           .sign_bits = 0,
                                                           .exponent_bits = 5,
                                                           .significand_bits = 3,
                                                           .exponent_bias = 15},
                        .insertion_index = std::nullopt})};
    ASSERT_TRUE(added.has_value()) << added.error().message;
    ASSERT_TRUE(*added);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::ValueFloat"};
    field.kind = codegen::PackedFieldKind::mini_float;
    field.bits.reset();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const representation_id{document.types().find(document.declaration(created)->identity)};
    ASSERT_TRUE(representation_id.has_value());
    auto const& placed_field{std::get<PackedField>(packed_type(document, packed).segments.front())};
    EXPECT_EQ(placed_field.semantic_type.type, *representation_id);
    EXPECT_EQ(placed_field.bit_width, 8U);
    EXPECT_TRUE(placed_field.bit_width_auto);
    auto const& representation{
        std::get<MiniFloatType>(document.types().type(*representation_id).definition)};
    EXPECT_EQ(representation.exponent_bits, 5U);
    EXPECT_EQ(representation.significand_bits, 3U);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        original_field.type.name);
    EXPECT_NE(document.declaration(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(mini-float ValueFloat"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("authored::ValueFloat"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":kind mini-float"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_mini{
        declaration_id(reloaded, "authored_representations", "ValueFloat", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_mini{reloaded.types().find(reloaded.declaration(reloaded_mini)->identity)};
    ASSERT_TRUE(resolved_mini.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments.front())};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_mini);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded.mini_float_schema(reloaded_mini)->exponent_bias, 15);
}

TEST(EditableSchemaDocument, RollsBackMiniFloatCreationWhenPackedBindingFails) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_mini{
        declaration_id(document, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const created{document.allocate_declaration_id()};
    auto const identity{TypeIdentity{.origin = TypeOrigin::declaration,
                                     .module_name = "authored_representations",
                                     .namespace_name = "authored",
                                     .name = "TooWideFloat"}};
    auto added{document.apply(
        CreateMiniFloat{.declaration = created,
                        .module_index = document.declaration(existing_mini)->module_index,
                        .schema = codegen::MiniFloatSchema{.name = identity.name,
                                                           .sign_bits = 1,
                                                           .exponent_bits = 5,
                                                           .significand_bits = 3,
                                                           .exponent_bias = 15},
                        .insertion_index = std::nullopt})};
    ASSERT_TRUE(added.has_value()) << added.error().message;
    ASSERT_TRUE(*added);

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::TooWideFloat"};
    field.kind = codegen::PackedFieldKind::mini_float;
    auto rejected{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("must equal"), std::string::npos);
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        "std::uint8_t");

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    EXPECT_FALSE(document.types().find(identity).has_value());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, BindsPackedFieldToFixedPointRepresentation) {
    TemporarySchema files;
    auto document{files.load()};
    auto const fixed{
        declaration_id(document, "authored_representations", "ExistingFixed", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};

    auto replacement{*document.packed_value_schema(packed)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type = codegen::TypeRef{"authored::ExistingFixed"};
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::fixed_point;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    field.relationship.reset();
    auto bound{document.apply(
        ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const fixed_type{document.types().find(document.declaration(fixed)->identity)};
    auto const packed_type_id{document.types().find(document.declaration(packed)->identity)};
    ASSERT_TRUE(fixed_type.has_value());
    ASSERT_TRUE(packed_type_id.has_value());
    auto const& bound_field{std::get<PackedField>(packed_type(document, packed).segments.front())};
    EXPECT_EQ(bound_field.semantic_type.type, *fixed_type);
    EXPECT_EQ(bound_field.kind, codegen::PackedFieldKind::fixed_point);
    EXPECT_EQ(bound_field.bit_width, 8U);
    EXPECT_TRUE(bound_field.bit_width_auto);
    EXPECT_NE(std::ranges::find(document.types().dependencies_of(*packed_type_id), *fixed_type),
              document.types().dependencies_of(*packed_type_id).end());

    auto invalid{*document.packed_value_schema(packed)};
    std::get<codegen::PackedFieldSchema>(invalid.segments.front()).bits = 7;
    auto rejected{
        document.apply(ReplacePackedValue{.declaration = packed, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("must equal"), std::string::npos);
    EXPECT_TRUE(
        std::get<PackedField>(packed_type(document, packed).segments.front()).bit_width_auto);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments.front())
            .type.name,
        "std::uint8_t");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(
        std::get<PackedField>(packed_type(document, packed).segments.front()).semantic_type.type,
        *document.types().find(document.declaration(fixed)->identity));

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("authored::ExistingFixed"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bits auto"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":kind fixed-point"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the value segment note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the counter segment note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_fixed{
        declaration_id(reloaded, "authored_representations", "ExistingFixed", "authored")};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const resolved_fixed_type{
        reloaded.types().find(reloaded.declaration(reloaded_fixed)->identity)};
    ASSERT_TRUE(resolved_fixed_type.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, reloaded_packed).segments.front())};
    EXPECT_EQ(reloaded_field.semantic_type.type, *resolved_fixed_type);
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::fixed_point);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
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

    auto create{
        document.apply(CreatePackedValue{
            .declaration = created,
            .module_index = module_index,
            .schema = codegen::PackedValueSchema{
                .name = "DesignedId",
                .storage_type = codegen::TypeRef{"std::uint32_t"},
                .segments = {codegen::PackedFieldSchema{
                                 .name = "entity_index",
                                 .type = codegen::TypeRef{"std::uint32_t"},
                                 .bits = std::nullopt,
                                 .minimum_value = 0,
                                 .maximum_value = 1'000'000,
                                 .named_codes =
                                     {{.name = "Player", .value = 42, .sentinel = false},
                                      {.name = "Invalid", .value = 1'048'575, .sentinel = true},
                                      {.name = "Pending", .value = 1'048'574, .sentinel = true}},
                                 .relationship =
                                     codegen::SemanticRelationSchema{
                                         .kind = codegen::SemanticRelationKind::index_into,
                                         .target = codegen::TypeRef{"ExistingPacked"},
                                         .unit = std::nullopt}},
                             codegen::PackedReservedBitsSchema{.name = "future", .bits = 4},
                             codegen::PackedFieldSchema{"state",
                                                        codegen::TypeRef{"@existing"},
                                                        8,
                                                        codegen::PackedFieldKind::enumeration}},
                .invalid_value = 0xffffffffU,
                .export_specifier = std::nullopt,
                .byte_order = codegen::PackedByteOrder::big_endian,
                .bit_order = codegen::PackedBitOrder::most_significant_first}})};
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
    ASSERT_EQ(created_type.segments.size(), 3U);
    EXPECT_EQ(created_type.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(created_type.bit_order, codegen::PackedBitOrder::most_significant_first);
    auto const& created_index{std::get<PackedField>(created_type.segments[0])};
    auto const& created_reserved{std::get<PackedReservedBits>(created_type.segments[1])};
    auto const& created_state{std::get<PackedField>(created_type.segments[2])};
    EXPECT_EQ(created_index.name, "entity_index");
    EXPECT_EQ(created_index.bit_width, 20U);
    EXPECT_TRUE(created_index.bit_width_auto);
    EXPECT_EQ(created_index.minimum_value, 0U);
    EXPECT_EQ(created_index.maximum_value, 1'000'000U);
    ASSERT_EQ(created_index.named_codes.size(), 3U);
    EXPECT_EQ(created_index.named_codes[1].name, "Invalid");
    EXPECT_TRUE(created_index.named_codes[1].sentinel);
    ASSERT_TRUE(created_index.relationship.has_value());
    EXPECT_EQ(created_index.relationship->kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(document.types().type(created_index.relationship->target.type).identity.name,
              "ExistingPacked");
    EXPECT_EQ(created_reserved.name, "future");
    EXPECT_EQ(created_reserved.bit_width, 4U);
    EXPECT_EQ(created_state.name, "state");
    EXPECT_EQ(created_state.bit_width, 8U);
    EXPECT_EQ(document.types().type(created_state.semantic_type.type).identity.name, "Existing");

    auto invalid{*document.packed_value_schema(created)};
    std::get<codegen::PackedFieldSchema>(invalid.segments.front()).bits = 25;
    auto rejected{
        document.apply(ReplacePackedValue{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("does not fit"), std::string::npos);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 20U);

    auto explicit_width{*document.packed_value_schema(created)};
    std::get<codegen::PackedFieldSchema>(explicit_width.segments.front()).bits = 20;
    auto width_replaced{document.apply(
        ReplacePackedValue{.declaration = created, .schema = std::move(explicit_width)})};
    ASSERT_TRUE(width_replaced.has_value()) << width_replaced.error().message;
    ASSERT_TRUE(*width_replaced);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 20U);
    EXPECT_FALSE(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width_auto);
    auto undo_width{document.undo()};
    ASSERT_TRUE(undo_width.has_value());
    ASSERT_TRUE(*undo_width);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 20U);
    EXPECT_TRUE(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width_auto);

    auto relationship_edit{*document.packed_value_schema(created)};
    auto& edited_relationship{
        *std::get<codegen::PackedFieldSchema>(relationship_edit.segments.front()).relationship};
    edited_relationship.kind = codegen::SemanticRelationKind::offset_into;
    edited_relationship.unit = codegen::SemanticRelationUnit::bytes;
    auto relationship_replaced{document.apply(
        ReplacePackedValue{.declaration = created, .schema = std::move(relationship_edit)})};
    ASSERT_TRUE(relationship_replaced.has_value()) << relationship_replaced.error().message;
    ASSERT_TRUE(*relationship_replaced);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->unit,
              codegen::SemanticRelationUnit::bytes);
    auto undo_relationship{document.undo()};
    ASSERT_TRUE(undo_relationship.has_value());
    ASSERT_TRUE(*undo_relationship);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::SemanticRelationKind::index_into);
    auto redo_relationship{document.redo()};
    ASSERT_TRUE(redo_relationship.has_value());
    ASSERT_TRUE(*redo_relationship);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->unit,
              codegen::SemanticRelationUnit::bytes);

    auto reordered{*document.packed_value_schema(created)};
    std::swap(reordered.segments[0], reordered.segments[2]);
    auto replace{
        document.apply(ReplacePackedValue{.declaration = created, .schema = std::move(reordered)})};
    ASSERT_TRUE(replace.has_value()) << replace.error().message;
    ASSERT_TRUE(*replace);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).name, "state");

    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    ASSERT_TRUE(*undo);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).name,
              "entity_index");
    auto redo{document.redo()};
    ASSERT_TRUE(redo.has_value());
    ASSERT_TRUE(*redo);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).name, "state");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(packed-value DesignedId"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":storage std::uint32_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":byte-order big"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bit-order msb-first"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(field state @existing :bits 8 :kind enum)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(reserved future :bits 4)"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(
                  "(field entity_index std::uint32_t :bits auto :minimum 0 :maximum 1000000"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(code Player :value 42)"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(code Invalid :value 1048575 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(relation offset_into ExistingPacked :unit bytes)"),
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
    ASSERT_EQ(reloaded_type.segments.size(), 3U);
    EXPECT_EQ(reloaded_type.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(reloaded_type.bit_order, codegen::PackedBitOrder::most_significant_first);
    auto const& reloaded_state{std::get<PackedField>(reloaded_type.segments[0])};
    auto const& reloaded_reserved{std::get<PackedReservedBits>(reloaded_type.segments[1])};
    auto const& reloaded_index{std::get<PackedField>(reloaded_type.segments[2])};
    EXPECT_EQ(reloaded_state.name, "state");
    EXPECT_EQ(reloaded_state.bit_width, 8U);
    EXPECT_EQ(reloaded_reserved.name, "future");
    EXPECT_EQ(reloaded_reserved.bit_width, 4U);
    EXPECT_EQ(reloaded_index.name, "entity_index");
    EXPECT_EQ(reloaded_index.bit_width, 20U);
    EXPECT_TRUE(reloaded_index.bit_width_auto);
    EXPECT_EQ(reloaded_index.minimum_value, 0U);
    EXPECT_EQ(reloaded_index.maximum_value, 1'000'000U);
    ASSERT_EQ(reloaded_index.named_codes.size(), 3U);
    EXPECT_EQ(reloaded_index.named_codes[0].name, "Player");
    EXPECT_EQ(reloaded_index.named_codes[0].value, 42U);
    EXPECT_FALSE(reloaded_index.named_codes[0].sentinel);
    EXPECT_EQ(reloaded_index.named_codes[1].name, "Invalid");
    EXPECT_EQ(reloaded_index.named_codes[1].value, 1'048'575U);
    EXPECT_TRUE(reloaded_index.named_codes[1].sentinel);
    ASSERT_TRUE(reloaded_index.relationship.has_value());
    EXPECT_EQ(reloaded_index.relationship->kind, codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(reloaded_index.relationship->unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(reloaded.types().type(reloaded_index.relationship->target.type).identity.name,
              "ExistingPacked");
    EXPECT_EQ(reloaded_type.invalid_raw_value, 0xffffffffU);
    EXPECT_EQ(reloaded.types().type(reloaded_state.semantic_type.type).identity.name, "Existing");
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(*reloaded.types().find_declared(
                                    "authored_packed", "DesignedId")),
                                reloaded_state.semantic_type.type),
              reloaded.types()
                  .dependencies_of(*reloaded.types().find_declared("authored_packed", "DesignedId"))
                  .end());
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(*reloaded.types().find_declared(
                                    "authored_packed", "DesignedId")),
                                reloaded_index.relationship->target.type),
              reloaded.types()
                  .dependencies_of(*reloaded.types().find_declared("authored_packed", "DesignedId"))
                  .end());
}

TEST(EditableSchemaDocument, RoundTripsSignedArbitraryWidthPackedField) {
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
            .name = "SignedDelta",
            .storage_type = codegen::TypeRef{"std::uint32_t"},
            .segments = {codegen::PackedFieldSchema{
                             .name = "delta",
                             .type = codegen::TypeRef{"std::int32_t"},
                             .bits = std::nullopt,
                             .kind = codegen::PackedFieldKind::signed_integer,
                             .range_helper = false,
                             .minimum_value = -100,
                             .maximum_value = 100,
                             .named_codes = {{.name = "Unknown", .value = -128, .sentinel = true}}},
                         codegen::PackedReservedBitsSchema{.name = "future", .bits = 24}},
            .invalid_value = std::nullopt,
            .export_specifier = std::nullopt,
            .byte_order = std::nullopt,
            .bit_order = std::nullopt}})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).kind,
              codegen::PackedFieldKind::signed_integer);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 8U);
    EXPECT_TRUE(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width_auto);

    auto changed_width{*document.packed_value_schema(created)};
    std::get<codegen::PackedFieldSchema>(changed_width.segments[0]).bits = 9;
    std::get<codegen::PackedReservedBitsSchema>(changed_width.segments[1]).bits = 23;
    auto replaced{document.apply(
        ReplacePackedValue{.declaration = created, .schema = std::move(changed_width)})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 9U);
    auto undo{document.undo()};
    ASSERT_TRUE(undo.has_value());
    ASSERT_TRUE(*undo);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 8U);
    auto redo{document.redo()};
    ASSERT_TRUE(redo.has_value());
    ASSERT_TRUE(*redo);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).bit_width, 9U);
    undo = document.undo();
    ASSERT_TRUE(undo.has_value());
    ASSERT_TRUE(*undo);

    auto invalid{*document.packed_value_schema(created)};
    std::get<codegen::PackedFieldSchema>(invalid.segments[0]).kind =
        codegen::PackedFieldKind::unsigned_integer;
    auto rejected{
        document.apply(ReplacePackedValue{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).kind,
              codegen::PackedFieldKind::signed_integer);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(
                  "(field delta std::int32_t :bits auto :kind signed :minimum -100 :maximum 100"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(code Unknown :value -128 :sentinel true)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_packed",
                                               .namespace_name = "authored",
                                               .name = "SignedDelta"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const& reloaded_field{
        std::get<PackedField>(packed_type(reloaded, *reloaded_declaration).segments[0])};
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::signed_integer);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded_field.minimum_value, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(reloaded_field.maximum_value, codegen::PackedIntegerValue{100});
    ASSERT_EQ(reloaded_field.named_codes.size(), 1U);
    EXPECT_EQ(reloaded_field.named_codes[0].value, codegen::PackedIntegerValue{-128});
    EXPECT_EQ(reloaded.types().type(reloaded_field.semantic_type.type).cpp_spelling,
              "std::int32_t");
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsIntegerScalars) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_scalars",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingScalar"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateIntegerScalar{
        .declaration = created,
        .module_index = module_index,
        .schema =
            codegen::IntegerScalarSchema{.name = "DamageReason",
                                         .signedness = false,
                                         .minimum_value = 0,
                                         .maximum_value = 10,
                                         .bit_width = std::nullopt,
                                         .named_codes =
                                             {{.name = "Unknown", .value = 0, .sentinel = false},
                                              {.name = "Invalid", .value = 15, .sentinel = true}},
                                         .relationship =
                                             codegen::SemanticRelationSchema{
                                                 .kind = codegen::SemanticRelationKind::offset_into,
                                                 .target =
                                                     codegen::TypeRef{"authored::ExistingPacked"},
                                                 .unit = codegen::SemanticRelationUnit::bytes},
                                         .cpp_emission = codegen::IntegerScalarCppEmission::
                                             constants_with_names,
                                         .cpp_type = codegen::TypeRef{"std::uint8_t"}},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    EXPECT_TRUE(document.dirty());
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto const& created_type{integer_scalar_type(document, created)};
    EXPECT_FALSE(created_type.signedness);
    EXPECT_EQ(created_type.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(created_type.maximum_value, codegen::PackedIntegerValue{10});
    EXPECT_TRUE(created_type.bit_width_auto);
    EXPECT_EQ(created_type.bit_width, 4U);
    ASSERT_EQ(created_type.named_codes.size(), 2U);
    EXPECT_TRUE(created_type.named_codes[1].sentinel);
    ASSERT_TRUE(created_type.relationship.has_value());
    EXPECT_EQ(created_type.relationship->kind, codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(created_type.relationship->unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(document.types().type(created_type.relationship->target.type).identity.name,
              "ExistingPacked");

    auto invalid{*document.integer_scalar_schema(created)};
    invalid.bit_width = 3;
    auto rejected{
        document.apply(ReplaceIntegerScalar{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("required 4 bits"), std::string::npos);
    EXPECT_TRUE(integer_scalar_type(document, created).bit_width_auto);

    auto reordered{*document.integer_scalar_schema(created)};
    std::swap(reordered.named_codes[0], reordered.named_codes[1]);
    reordered.bit_width = 5;
    auto replaced{document.apply(
        ReplaceIntegerScalar{.declaration = created, .schema = std::move(reordered)})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_FALSE(integer_scalar_type(document, created).bit_width_auto);
    EXPECT_EQ(integer_scalar_type(document, created).bit_width, 5U);
    EXPECT_EQ(integer_scalar_type(document, created).named_codes[0].name, "Invalid");
    ASSERT_TRUE(document.undo().value());
    EXPECT_TRUE(integer_scalar_type(document, created).bit_width_auto);
    EXPECT_EQ(integer_scalar_type(document, created).named_codes[0].name, "Unknown");
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(integer-scalar DamageReason"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":signed false"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":minimum 0"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":maximum 10"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bit-width 5"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":cpp-emission constants-with-names"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":cpp-type std::uint8_t"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(code Invalid :value 15 :sentinel true)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(
                  "(relation offset_into authored::ExistingPacked :unit bytes)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_scalars",
                                               .namespace_name = "authored",
                                               .name = "DamageReason"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.integer_scalar_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_width, 5U);
    EXPECT_EQ(schema->cpp_emission, codegen::IntegerScalarCppEmission::constants_with_names);
    ASSERT_TRUE(schema->cpp_type.has_value());
    EXPECT_EQ(schema->cpp_type->name, "std::uint8_t");
    ASSERT_EQ(schema->named_codes.size(), 2U);
    EXPECT_EQ(schema->named_codes[0].name, "Invalid");
    ASSERT_TRUE(schema->relationship.has_value());
    EXPECT_EQ(schema->relationship->kind, codegen::SemanticRelationKind::offset_into);
    EXPECT_EQ(schema->relationship->unit, codegen::SemanticRelationUnit::bytes);
    auto const& reloaded_type{integer_scalar_type(reloaded, *reloaded_declaration)};
    EXPECT_EQ(reloaded_type.bit_width, 5U);
    EXPECT_FALSE(reloaded_type.bit_width_auto);
    ASSERT_TRUE(reloaded_type.relationship.has_value());
    EXPECT_EQ(reloaded_type.relationship->unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(reloaded.types().type(reloaded_type.relationship->target.type).identity.name,
              "ExistingPacked");
}

TEST(EditableSchemaDocument, CreatesModuleThenDeclarationAndReloadsWithoutSourceDamage) {
    TemporarySchema files;
    auto document{files.load()};
    ASSERT_GE(document.source_files().size(), 2U);
    auto const original_source{document.source_files()[1].text};
    auto const original_module_count{document.manifest().modules.size()};
    auto const original_revision{document.revision()};

    auto duplicate{document.apply(
        CreateModule{.source_file_index = 1,
                     .schema = codegen::NormalModuleSchema{
                         .settings = codegen::ModuleSettings{.name = "authored_scalars",
                                                             .header = "DuplicateScalars.h",
                                                             .namespace_name = "authored"}}})};
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(document.manifest().modules.size(), original_module_count);
    EXPECT_EQ(document.revision(), original_revision);

    auto created_module{document.apply(
        CreateModule{.source_file_index = 1,
                     .schema = codegen::NormalModuleSchema{
                         .settings = codegen::ModuleSettings{.name = "planner_scalars",
                                                             .header = "PlannerScalars.h",
                                                             .namespace_name = "planner"}}})};
    ASSERT_TRUE(created_module.has_value()) << created_module.error().message;
    ASSERT_TRUE(*created_module);
    ASSERT_EQ(document.manifest().modules.size(), original_module_count + 1);
    auto const module_index{document.manifest().modules.size() - 1};

    auto const declaration{document.allocate_declaration_id()};
    auto created_scalar{document.apply(CreateIntegerScalar{
        .declaration = declaration,
        .module_index = module_index,
        .schema =
            codegen::IntegerScalarSchema{
                .name = "StatusCode",
                .signedness = false,
                .minimum_value = 0,
                .maximum_value = 3,
                .bit_width = 2,
                .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false}},
                .relationship = std::nullopt,
                .cpp_emission = codegen::IntegerScalarCppEmission::none,
                .cpp_type = std::nullopt},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(created_scalar.has_value()) << created_scalar.error().message;
    ASSERT_TRUE(*created_scalar);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_TRUE(preview->front().updated.starts_with(original_source));
    EXPECT_NE(preview->front().updated.find("(module planner_scalars"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":header \"PlannerScalars.h\""), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":namespace planner"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(integer-scalar StatusCode"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.manifest().modules.size(), original_module_count);
    EXPECT_FALSE(document.dirty());
    auto reverted_preview{document.preview_source_updates()};
    ASSERT_TRUE(reverted_preview.has_value());
    EXPECT_TRUE(reverted_preview->empty());
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);
    auto reloaded{files.load()};
    ASSERT_EQ(reloaded.manifest().modules.size(), original_module_count + 1);
    auto const* module{
        std::get_if<codegen::NormalModuleSchema>(&reloaded.manifest().modules.back())};
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->settings.name, "planner_scalars");
    EXPECT_EQ(module->settings.header, "PlannerScalars.h");
    EXPECT_EQ(module->settings.namespace_name, "planner");
    ASSERT_EQ(module->declarations.size(), 1U);
    EXPECT_EQ(std::get<codegen::IntegerScalarSchema>(module->declarations.front()).name,
              "StatusCode");
    EXPECT_TRUE(reloaded.types().find_declared("planner_scalars", "StatusCode").has_value());
}

TEST(EditableSchemaDocument, NormalizesLegacyEmptyModuleKindsOnCreation) {
    TemporarySchema files;
    auto document{files.load()};
    auto const original_module_count{document.manifest().modules.size()};
    auto settings = [](std::string name) {
        return codegen::ModuleSettings{.name = name,
                                       .header = name + ".h",
                                       .source = std::nullopt,
                                       .header_include = std::nullopt,
                                       .namespace_name = "planner",
                                       .include_order = {},
                                       .prelude_lines = {}};
    };
    std::vector<codegen::ModuleSchema> modules;
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_enums"),
                                                  .enum_helper_namespace = std::nullopt});
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_packed")});
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_scalars")});
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_representations")});
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_records")});
    modules.push_back(codegen::NormalModuleSchema{.settings = settings("new_unions")});
    modules.push_back(
        codegen::NormalModuleSchema{.settings = settings("new_soas"),
                                    .soa_backend = codegen::SoaBackend::standard_library,
                                    .soa_array_allocators = {}});

    for (auto& module : modules) {
        auto created{
            document.apply(CreateModule{.source_file_index = 1, .schema = std::move(module)})};
        ASSERT_TRUE(created.has_value()) << created.error().message;
        ASSERT_TRUE(*created);
    }
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    for (auto const head : {"module new_enums",
                            "module new_packed",
                            "module new_scalars",
                            "module new_representations",
                            "module new_records",
                            "module new_unions",
                            "module new_soas"}) {
        EXPECT_NE(preview->front().updated.find(head), std::string::npos) << head;
    }
    EXPECT_NE(preview->front().updated.find(":backend standard-library"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    ASSERT_EQ(reloaded.manifest().modules.size(), original_module_count + 7);
    for (std::size_t index{}; index < 7; ++index) {
        EXPECT_TRUE(std::holds_alternative<codegen::NormalModuleSchema>(
            reloaded.manifest().modules[original_module_count + index]));
    }
}

TEST(EditableSchemaDocument, DeletesSourceBackedMiddleModuleAndRestoresExactIdentity) {
    TemporarySchema files;
    files.write_source("types.lispb", "; No registered aliases.\n");
    auto const deleted_form{std::string{R"((module middle
  :header "Middle.h"
  :namespace demo
  (integer-scalar First :signed false :minimum 0 :maximum 3 :bit-width auto)
  (integer-scalar Second :signed false :minimum 0 :maximum 7 :bit-width auto)))"}};
    auto const original{std::string{R"(; Preserve this file-level introduction.
(module first
  :header "First.h"
  :namespace demo
  (integer-scalar Before :signed false :minimum 0 :maximum 1 :bit-width auto))

; Keep this unusual inter-module comment and spacing.

)"} + deleted_form + R"(

(module last
  :header "Last.h"
  :namespace demo
  (integer-scalar After :signed false :minimum 0 :maximum 15 :bit-width auto))
; Preserve the trailing comment.
)"};
    files.write_source("modules.lispb", original);
    auto expected{original};
    expected.erase(expected.find(deleted_form), deleted_form.size());

    auto document{files.load()};
    ASSERT_EQ(document.manifest().modules.size(), 3U);
    ASSERT_EQ(document.source_files().size(), 2U);
    auto const source_path{document.source_files()[1].path};
    auto const before{declaration_id(document, "first", "Before", "demo")};
    auto const first{declaration_id(document, "middle", "First", "demo")};
    auto const second{declaration_id(document, "middle", "Second", "demo")};
    auto const after{declaration_id(document, "last", "After", "demo")};
    ASSERT_NE(document.declaration(before), nullptr);
    ASSERT_NE(document.declaration(first), nullptr);
    ASSERT_NE(document.declaration(second), nullptr);
    ASSERT_NE(document.declaration(after), nullptr);
    auto const first_info{*document.declaration(first)};
    auto const second_info{*document.declaration(second)};
    auto const after_info{*document.declaration(after)};
    auto expect_declaration = [](DeclarationInfo const* actual, DeclarationInfo const& expected) {
        ASSERT_NE(actual, nullptr);
        EXPECT_EQ(actual->id, expected.id);
        EXPECT_EQ(actual->identity, expected.identity);
        EXPECT_EQ(actual->module_index, expected.module_index);
        EXPECT_EQ(actual->declaration_index, expected.declaration_index);
        EXPECT_EQ(actual->source, expected.source);
    };
    ASSERT_TRUE(first_info.source.has_value());
    ASSERT_TRUE(second_info.source.has_value());
    EXPECT_EQ(first_info.module_index, 1U);
    EXPECT_EQ(first_info.declaration_index, 0U);
    EXPECT_EQ(second_info.declaration_index, 1U);
    auto const original_revision{document.revision()};

    auto deleted{document.apply(DeleteModule{.module_index = 1})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    EXPECT_GT(document.revision(), original_revision);
    ASSERT_EQ(document.manifest().modules.size(), 2U);
    EXPECT_EQ(document.declarations().size(), 2U);
    EXPECT_EQ(std::get<codegen::NormalModuleSchema>(document.manifest().modules[0]).settings.name,
              "first");
    EXPECT_EQ(std::get<codegen::NormalModuleSchema>(document.manifest().modules[1]).settings.name,
              "last");
    EXPECT_EQ(document.declaration(first), nullptr);
    EXPECT_EQ(document.declaration(second), nullptr);
    EXPECT_FALSE(document.types().find_declared("middle", "First").has_value());
    EXPECT_FALSE(document.types().find_declared("middle", "Second").has_value());
    ASSERT_NE(document.declaration(before), nullptr);
    ASSERT_NE(document.declaration(after), nullptr);
    EXPECT_EQ(document.declaration(before)->module_index, 0U);
    EXPECT_EQ(document.declaration(after)->module_index, 1U);
    EXPECT_EQ(document.declaration(after)->id, after);
    EXPECT_TRUE(document.types().find_declared("first", "Before").has_value());
    EXPECT_TRUE(document.types().find_declared("last", "After").has_value());
    EXPECT_EQ(document.source_files().size(), 2U);
    EXPECT_EQ(document.source_files()[1].path, source_path);
    EXPECT_TRUE(std::filesystem::exists(source_path));
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().path, source_path);
    EXPECT_EQ(preview->front().original, original);
    EXPECT_EQ(preview->front().updated, expected);

    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.manifest().modules.size(), 3U);
    EXPECT_EQ(std::get<codegen::NormalModuleSchema>(document.manifest().modules[1]).settings.name,
              "middle");
    expect_declaration(document.declaration(first), first_info);
    expect_declaration(document.declaration(second), second_info);
    expect_declaration(document.declaration(after), after_info);
    EXPECT_TRUE(document.types().find_declared("middle", "First").has_value());
    EXPECT_TRUE(document.types().find_declared("middle", "Second").has_value());
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(document.can_redo());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());

    ASSERT_TRUE(document.redo().value());
    EXPECT_TRUE(document.dirty());
    ASSERT_EQ(document.manifest().modules.size(), 2U);
    EXPECT_EQ(document.declaration(first), nullptr);
    EXPECT_EQ(document.declaration(second), nullptr);
    ASSERT_NE(document.declaration(after), nullptr);
    EXPECT_EQ(document.declaration(after)->module_index, 1U);
    EXPECT_EQ(document.declaration(after)->id, after);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, expected);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);
    EXPECT_EQ(saved->front(), source_path);
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(std::filesystem::exists(source_path));
    EXPECT_EQ(files.read_source("modules.lispb"), expected);
    EXPECT_EQ(files.read_source("types.lispb"), "; No registered aliases.\n");
    auto reloaded{files.load()};
    ASSERT_EQ(reloaded.manifest().modules.size(), 2U);
    EXPECT_FALSE(reloaded.types().find_declared("middle", "First").has_value());
    EXPECT_FALSE(reloaded.types().find_declared("middle", "Second").has_value());
    EXPECT_TRUE(reloaded.types().find_declared("first", "Before").has_value());
    EXPECT_TRUE(reloaded.types().find_declared("last", "After").has_value());
    EXPECT_TRUE(reloaded.find_declaration(after_info.identity).has_value());
    EXPECT_EQ(reloaded.source_files()[1].path, source_path);
}

TEST(EditableSchemaDocument, RejectsDeletionOfTheLastModuleWithoutChangingDraft) {
    TemporarySchema files;
    files.write_source("types.lispb", "");
    files.write_source("modules.lispb", R"((module only
  :header "Only.h"
  :namespace demo
  (integer-scalar Value :signed false :minimum 0 :maximum 3 :bit-width auto))
)");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "only", "Value", "demo")};
    auto const revision{document.revision()};
    auto const type_count{document.types().types().size()};

    auto deleted{document.apply(DeleteModule{.module_index = 0})};
    ASSERT_FALSE(deleted.has_value());
    EXPECT_NE(deleted.error().message.find("retain at least one module"), std::string::npos);
    ASSERT_EQ(document.manifest().modules.size(), 1U);
    ASSERT_EQ(document.declarations().size(), 1U);
    EXPECT_EQ(document.declarations().front().id, declaration);
    EXPECT_TRUE(document.types().find_declared("only", "Value").has_value());
    EXPECT_EQ(document.types().types().size(), type_count);
    EXPECT_EQ(document.revision(), revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.can_undo());
    EXPECT_FALSE(document.can_redo());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, RejectsDeletionWhenAnotherModuleUsesADeclaration) {
    TemporarySchema files;
    files.write_source("types.lispb", "");
    files.write_source("modules.lispb", R"((module targets
  :header "Targets.h"
  :namespace demo
  (integer-scalar Target :signed false :minimum 0 :maximum 3 :bit-width auto))

(module consumers
  :header "Consumers.h"
  :namespace demo
  (record Consumer
    (member value demo::Target)))
)");
    auto document{files.load()};
    auto const target{declaration_id(document, "targets", "Target", "demo")};
    auto const consumer{declaration_id(document, "consumers", "Consumer", "demo")};
    auto const revision{document.revision()};
    auto const type_count{document.types().types().size()};

    auto deleted{document.apply(DeleteModule{.module_index = 0})};
    ASSERT_FALSE(deleted.has_value());
    EXPECT_NE(deleted.error().message.find("Consumer"), std::string::npos);
    ASSERT_EQ(document.manifest().modules.size(), 2U);
    ASSERT_EQ(document.declarations().size(), 2U);
    ASSERT_NE(document.declaration(target), nullptr);
    ASSERT_NE(document.declaration(consumer), nullptr);
    EXPECT_EQ(document.declaration(target)->module_index, 0U);
    EXPECT_EQ(document.declaration(consumer)->module_index, 1U);
    EXPECT_TRUE(document.types().find_declared("targets", "Target").has_value());
    EXPECT_TRUE(document.types().find_declared("consumers", "Consumer").has_value());
    EXPECT_EQ(document.types().types().size(), type_count);
    EXPECT_EQ(document.revision(), revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.can_undo());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, AllowsDeletionOfEntireModuleWithInternalDependencies) {
    TemporarySchema files;
    files.write_source("types.lispb", "");
    files.write_source("modules.lispb", R"((module related
  :header "Related.h"
  :namespace demo
  (record Foo (member value std::uint8_t))
  (record Bar (member foo demo::Foo)))

(module survivor
  :header "Survivor.h"
  :namespace demo
  (integer-scalar Keep :signed false :minimum 0 :maximum 1 :bit-width auto))
)");
    auto document{files.load()};
    auto const foo{declaration_id(document, "related", "Foo", "demo")};
    auto const bar{declaration_id(document, "related", "Bar", "demo")};
    auto const keep{declaration_id(document, "survivor", "Keep", "demo")};

    auto deleted{document.apply(DeleteModule{.module_index = 0})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);
    EXPECT_EQ(document.declaration(foo), nullptr);
    EXPECT_EQ(document.declaration(bar), nullptr);
    ASSERT_NE(document.declaration(keep), nullptr);
    EXPECT_EQ(document.declaration(keep)->module_index, 0U);
    EXPECT_FALSE(document.types().find_declared("related", "Foo").has_value());
    EXPECT_FALSE(document.types().find_declared("related", "Bar").has_value());
    EXPECT_TRUE(document.types().find_declared("survivor", "Keep").has_value());
}

TEST(EditableSchemaDocument, RejectsDeletionOfRegisteredDeclarationWithoutChangingDraft) {
    TemporarySchema files;
    files.write_source("types.lispb", R"((type target
  :spelling "demo::Target"
  :header "Targets.h")
)");
    files.write_source("modules.lispb", R"((module targets
  :header "Targets.h"
  :namespace demo
  (integer-scalar Target :signed false :minimum 0 :maximum 3 :bit-width auto))

(module survivor
  :header "Survivor.h"
  :namespace demo
  (integer-scalar Keep :signed false :minimum 0 :maximum 1 :bit-width auto))
)");
    auto document{files.load()};
    auto const target{declaration_id(document, "targets", "Target", "demo")};
    auto const revision{document.revision()};
    auto const type_count{document.types().types().size()};

    auto deleted{document.apply(DeleteModule{.module_index = 0})};
    ASSERT_FALSE(deleted.has_value());
    EXPECT_NE(deleted.error().message.find("registered as '@target'"), std::string::npos);
    ASSERT_EQ(document.manifest().modules.size(), 2U);
    ASSERT_EQ(document.declarations().size(), 2U);
    EXPECT_EQ(document.declarations().front().id, target);
    EXPECT_TRUE(document.types().find_declared("targets", "Target").has_value());
    EXPECT_TRUE(document.types().find_registered("target").has_value());
    EXPECT_EQ(document.types().types().size(), type_count);
    EXPECT_EQ(document.revision(), revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.can_undo());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, RestoresPendingModuleOwnershipAcrossIndexShifts) {
    TemporarySchema files;
    files.write_source("types.lispb", "");
    files.write_source("modules.lispb", R"((module first
  :header "First.h"
  :namespace demo
  (integer-scalar First :signed false :minimum 0 :maximum 1 :bit-width auto))

(module middle
  :header "Middle.h"
  :namespace demo
  (integer-scalar Middle :signed false :minimum 0 :maximum 3 :bit-width auto))
)");
    files.write_source("other.lispb", R"((module other
  :header "Other.h"
  :namespace demo
  (integer-scalar Other :signed false :minimum 0 :maximum 7 :bit-width auto))
)");
    auto document{files.load_with_module_source("other.lispb")};
    ASSERT_EQ(document.manifest().modules.size(), 3U);
    ASSERT_EQ(document.source_files().size(), 3U);
    auto const middle{declaration_id(document, "middle", "Middle", "demo")};
    auto const other{declaration_id(document, "other", "Other", "demo")};
    auto created_module{document.apply(CreateModule{
        .source_file_index = 2,
        .schema = codegen::NormalModuleSchema{
            .settings = codegen::ModuleSettings{
                .name = "pending", .header = "Pending.h", .namespace_name = "demo"}}})};
    ASSERT_TRUE(created_module.has_value()) << created_module.error().message;
    ASSERT_TRUE(*created_module);

    auto const pending{document.allocate_declaration_id()};
    auto created_declaration{document.apply(CreateIntegerScalar{
        .declaration = pending,
        .module_index = 3,
        .schema =
            codegen::IntegerScalarSchema{.name = "Pending",
                                         .signedness = false,
                                         .minimum_value = 0,
                                         .maximum_value = 3,
                                         .bit_width = 2,
                                         .named_codes = {},
                                         .relationship = std::nullopt,
                                         .cpp_emission = codegen::IntegerScalarCppEmission::none,
                                         .cpp_type = std::nullopt},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(created_declaration.has_value()) << created_declaration.error().message;
    ASSERT_TRUE(*created_declaration);
    ASSERT_NE(document.declaration(pending), nullptr);
    EXPECT_EQ(document.declaration(pending)->module_index, 3U);
    EXPECT_FALSE(document.declaration(pending)->source.has_value());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().path, files.path("other.lispb"));
    EXPECT_NE(preview->front().updated.find("(module pending"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(integer-scalar Pending"), std::string::npos);

    auto deleted_middle{document.apply(DeleteModule{.module_index = 1})};
    ASSERT_TRUE(deleted_middle.has_value()) << deleted_middle.error().message;
    ASSERT_TRUE(*deleted_middle);
    ASSERT_EQ(document.manifest().modules.size(), 3U);
    EXPECT_EQ(document.declaration(middle), nullptr);
    ASSERT_NE(document.declaration(other), nullptr);
    ASSERT_NE(document.declaration(pending), nullptr);
    EXPECT_EQ(document.declaration(other)->module_index, 1U);
    EXPECT_EQ(document.declaration(pending)->module_index, 2U);
    EXPECT_FALSE(document.declaration(pending)->source.has_value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    EXPECT_EQ((*preview)[0].path, files.path("modules.lispb"));
    EXPECT_EQ((*preview)[1].path, files.path("other.lispb"));
    EXPECT_NE((*preview)[1].updated.find("(integer-scalar Pending"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(middle), nullptr);
    ASSERT_NE(document.declaration(other), nullptr);
    ASSERT_NE(document.declaration(pending), nullptr);
    EXPECT_EQ(document.declaration(middle)->module_index, 1U);
    EXPECT_EQ(document.declaration(other)->module_index, 2U);
    EXPECT_EQ(document.declaration(pending)->module_index, 3U);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().path, files.path("other.lispb"));

    auto deleted_pending{document.apply(DeleteModule{.module_index = 3})};
    ASSERT_TRUE(deleted_pending.has_value()) << deleted_pending.error().message;
    ASSERT_TRUE(*deleted_pending);
    EXPECT_EQ(document.declaration(pending), nullptr);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());

    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(pending), nullptr);
    EXPECT_EQ(document.declaration(pending)->id, pending);
    EXPECT_EQ(document.declaration(pending)->module_index, 3U);
    EXPECT_FALSE(document.declaration(pending)->source.has_value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().path, files.path("other.lispb"));
    EXPECT_NE(preview->front().updated.find("(integer-scalar Pending"), std::string::npos);

    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(pending), nullptr);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
}

TEST(EditableSchemaDocument, PreservesDeclarationTombstonesAcrossModuleDeletionHistory) {
    TemporarySchema files;
    files.write_source("types.lispb", "");
    auto const doomed_form{std::string{
        "(module doomed\n"
        "  :header \"Doomed.h\"\n"
        "  :namespace demo\n"
        "  (integer-scalar Alpha :signed false :minimum 0 :maximum 1 :bit-width auto)\n"
        "  (integer-scalar Beta :signed false :minimum 0 :maximum 3 :bit-width auto))"}};
    auto const beta_form{
        std::string{"(integer-scalar Beta :signed false :minimum 0 :maximum 3 :bit-width auto)"}};
    auto const original{doomed_form + R"(

(module survivor
  :header "Survivor.h"
  :namespace demo
  (integer-scalar Keep :signed false :minimum 0 :maximum 7 :bit-width auto))
)"};
    files.write_source("modules.lispb", original);
    auto after_declaration_delete{original};
    after_declaration_delete.erase(after_declaration_delete.find(beta_form), beta_form.size());
    auto after_module_delete{original};
    after_module_delete.erase(after_module_delete.find(doomed_form), doomed_form.size());

    auto document{files.load()};
    auto const alpha{declaration_id(document, "doomed", "Alpha", "demo")};
    auto const beta{declaration_id(document, "doomed", "Beta", "demo")};
    auto const keep{declaration_id(document, "survivor", "Keep", "demo")};
    ASSERT_NE(document.declaration(beta), nullptr);
    auto const beta_info{*document.declaration(beta)};

    auto deleted_declaration{document.apply(DeleteIntegerScalar{.declaration = beta})};
    ASSERT_TRUE(deleted_declaration.has_value()) << deleted_declaration.error().message;
    ASSERT_TRUE(*deleted_declaration);
    EXPECT_EQ(document.declaration(beta), nullptr);
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, after_declaration_delete);

    auto deleted_module{document.apply(DeleteModule{.module_index = 0})};
    ASSERT_TRUE(deleted_module.has_value()) << deleted_module.error().message;
    ASSERT_TRUE(*deleted_module);
    EXPECT_EQ(document.declaration(alpha), nullptr);
    EXPECT_EQ(document.declaration(beta), nullptr);
    ASSERT_NE(document.declaration(keep), nullptr);
    EXPECT_EQ(document.declaration(keep)->module_index, 0U);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, after_module_delete);

    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(alpha), nullptr);
    EXPECT_EQ(document.declaration(alpha)->id, alpha);
    EXPECT_EQ(document.declaration(beta), nullptr);
    ASSERT_NE(document.declaration(keep), nullptr);
    EXPECT_EQ(document.declaration(keep)->module_index, 1U);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, after_declaration_delete);

    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(beta), nullptr);
    EXPECT_EQ(document.declaration(beta)->id, beta_info.id);
    EXPECT_EQ(document.declaration(beta)->module_index, beta_info.module_index);
    EXPECT_EQ(document.declaration(beta)->declaration_index, beta_info.declaration_index);
    EXPECT_EQ(document.declaration(beta)->source, beta_info.source);
    EXPECT_FALSE(document.dirty());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());

    ASSERT_TRUE(document.redo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, after_declaration_delete);
    ASSERT_TRUE(document.redo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, after_module_delete);
}

TEST(EditableSchemaDocument, MovesDeclarationsAcrossCompatibleModuleSources) {
    TemporarySchema files;
    files.write_source("destinations.lispb", R"((module destination_scalars
  :header "DestinationScalars.h"
  :namespace authored
  (integer-scalar DestinationScalar
    :signed false
    :minimum 0
    :maximum 7
    :bit-width 3)
  (integer-scalar SignedScalar
    :signed true
    :minimum -8
    :maximum 7
    :bit-width 4))

(module foreign_scalars
  :header "ForeignScalars.h"
  :namespace foreign)

(module destination_representations
  :header "DestinationRepresentations.h"
  :namespace authored
  (linear-quantized DestinationQ
    :source authored::ExistingScalar
    :bits 2
    :reserved-codes 0
    :clipping reject))

(module destination_unions
  :header "DestinationUnions.h"
  :namespace authored
  (union DestinationUnion
    (alternative value std::uint16_t)))
)");
    auto document{files.load_with_module_source("destinations.lispb")};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const varint{
        declaration_id(document, "authored_representations", "ExistingVarint", "authored")};
    auto const tagged{declaration_id(document, "authored_unions", "ExistingTagged", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const signed_scalar{
        declaration_id(document, "authored_scalars", "SignedScalar", "authored")};
    auto find_module = [&](std::string_view const name) {
        auto const modules{document.manifest().modules};
        auto const found{std::ranges::find_if(modules, [&](auto const& module) {
            return std::visit([&](auto const& value) { return value.settings.name == name; },
                              module);
        })};
        EXPECT_NE(found, modules.end());
        return static_cast<std::size_t>(std::distance(modules.begin(), found));
    };
    auto declaration_text = [&](DeclarationId const declaration) {
        auto const* info{document.declaration(declaration)};
        EXPECT_NE(info, nullptr);
        EXPECT_TRUE(info != nullptr && info->source.has_value());
        if (info == nullptr || !info->source.has_value()) {
            return std::string{};
        }
        auto const& range{*info->source};
        auto const& source{document.source_files()[range.source_file_index].text};
        return source.substr(range.begin_offset, range.end_offset - range.begin_offset);
    };
    auto const scalar_text{declaration_text(scalar)};
    auto const varint_text{declaration_text(varint)};
    auto const tagged_text{declaration_text(tagged)};
    auto const destination_scalars{find_module("destination_scalars")};
    auto const destination_representations{find_module("destination_representations")};
    auto const destination_unions{find_module("destination_unions")};

    auto const original_revision{document.revision()};
    auto cross_kind{document.apply(MoveDeclaration{.declaration = scalar,
                                                   .module_index = destination_unions,
                                                   .insertion_index = std::nullopt})};
    ASSERT_TRUE(cross_kind.has_value()) << cross_kind.error().message;
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "destination_unions");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.revision(), original_revision + 2);
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "authored_scalars");

    auto const before_rejection{document.revision()};
    auto rejected = document.apply(MoveDeclaration{.declaration = signed_scalar,
                                                   .module_index = destination_scalars,
                                                   .insertion_index = std::nullopt});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("Duplicate declaration name"), std::string::npos);
    EXPECT_EQ(document.revision(), before_rejection);
    EXPECT_EQ(document.declaration(signed_scalar)->identity.module_name, "authored_scalars");
    auto const* unchanged_destination{std::get_if<codegen::NormalModuleSchema>(
        &document.manifest().modules[destination_scalars])};
    ASSERT_NE(unchanged_destination, nullptr);
    EXPECT_EQ(unchanged_destination->declarations.size(), 2U);

    auto moved_scalar{document.apply(MoveDeclaration{
        .declaration = scalar, .module_index = destination_scalars, .insertion_index = 0})};
    ASSERT_TRUE(moved_scalar.has_value()) << moved_scalar.error().message;
    ASSERT_TRUE(*moved_scalar);
    EXPECT_EQ(document.declaration(scalar)->id, scalar);
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "destination_scalars");
    EXPECT_EQ(document.declaration(scalar)->declaration_index, 0U);
    EXPECT_FALSE(document.declaration(scalar)->source.has_value());
    auto const& packed_field{std::get<PackedField>(packed_type(document, packed).segments[1])};
    ASSERT_TRUE(packed_field.relationship.has_value());
    EXPECT_EQ(document.types().type(packed_field.relationship->target.type).identity.module_name,
              "destination_scalars");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    auto preview_text = [&](std::string_view const filename) -> std::string const& {
        auto const found{std::ranges::find_if(
            *preview, [&](auto const& update) { return update.path.filename() == filename; })};
        EXPECT_NE(found, preview->end());
        return found->updated;
    };
    EXPECT_EQ(preview_text("modules.lispb").find(scalar_text), std::string::npos);
    EXPECT_NE(preview_text("destinations.lispb").find(scalar_text), std::string::npos);
    EXPECT_LT(preview_text("destinations.lispb").find(scalar_text),
              preview_text("destinations.lispb").find("(integer-scalar DestinationScalar"));

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "authored_scalars");
    ASSERT_TRUE(document.declaration(scalar)->source.has_value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value());
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto moved_varint{document.apply(MoveDeclaration{
        .declaration = varint, .module_index = destination_representations, .insertion_index = 0})};
    ASSERT_TRUE(moved_varint.has_value()) << moved_varint.error().message;
    ASSERT_TRUE(*moved_varint);
    auto moved_tagged{document.apply(MoveDeclaration{
        .declaration = tagged, .module_index = destination_unions, .insertion_index = 0})};
    ASSERT_TRUE(moved_tagged.has_value()) << moved_tagged.error().message;
    ASSERT_TRUE(*moved_tagged);
    EXPECT_EQ(document.declaration(varint)->identity.module_name, "destination_representations");
    EXPECT_EQ(document.declaration(varint)->declaration_index, 0U);
    EXPECT_EQ(document.declaration(tagged)->identity.module_name, "destination_unions");
    EXPECT_EQ(document.declaration(tagged)->declaration_index, 0U);
    EXPECT_EQ(integer_varint_type(document, varint).encoding,
              codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_FALSE(tagged_union_type(document, tagged).alternatives.empty());
    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    EXPECT_EQ(linear_quantized_type(document, quantized).bit_width, 1U);
    EXPECT_EQ(optional_sentinel_type(document, optional).sentinel_name, "Invalid");

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    EXPECT_EQ(preview_text("modules.lispb").find(varint_text), std::string::npos);
    EXPECT_EQ(preview_text("modules.lispb").find(tagged_text), std::string::npos);
    EXPECT_NE(preview_text("destinations.lispb").find(varint_text), std::string::npos);
    EXPECT_NE(preview_text("destinations.lispb").find(tagged_text), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 2U);
    auto reloaded{files.load_with_module_source("destinations.lispb")};
    EXPECT_TRUE(
        reloaded.types().find_declared("destination_scalars", "ExistingScalar").has_value());
    EXPECT_TRUE(reloaded.types()
                    .find_declared("destination_representations", "ExistingVarint")
                    .has_value());
    EXPECT_TRUE(reloaded.types().find_declared("destination_unions", "ExistingTagged").has_value());
    EXPECT_FALSE(reloaded.types().find_declared("authored_scalars", "ExistingScalar").has_value());
    EXPECT_FALSE(
        reloaded.types().find_declared("authored_representations", "ExistingVarint").has_value());
    EXPECT_FALSE(reloaded.types().find_declared("authored_unions", "ExistingTagged").has_value());
    auto const destination_source{
        std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
            return source.path.filename() == "destinations.lispb";
        })};
    ASSERT_NE(destination_source, reloaded.source_files().end());
    EXPECT_NE(destination_source->text.find(scalar_text), std::string::npos);
    EXPECT_NE(destination_source->text.find(varint_text), std::string::npos);
    EXPECT_NE(destination_source->text.find(tagged_text), std::string::npos);
}

TEST(EditableSchemaDocument, RepairsReferencesWhenMovingAcrossNamespaces) {
    TemporarySchema files;
    files.write_source("migrated.lispb", R"((module migrated_scalars
  :header "MigratedScalars.h"
  :namespace migrated)

(module migrated_enums
  :header "MigratedEnums.h"
  :namespace migrated)

(module migrated_soa
  :header "MigratedSoa.h"
  :namespace migrated
  :backend standard-library)
)");
    auto document{files.load_with_module_source("migrated.lispb")};
    auto find_module = [&](std::string_view const name) {
        auto const modules{document.manifest().modules};
        auto const found{std::ranges::find_if(modules, [&](auto const& module) {
            return std::visit([&](auto const& value) { return value.settings.name == name; },
                              module);
        })};
        EXPECT_NE(found, modules.end());
        return static_cast<std::size_t>(std::distance(modules.begin(), found));
    };
    auto declaration_text = [&](DeclarationId const declaration) {
        auto const* info{document.declaration(declaration)};
        EXPECT_NE(info, nullptr);
        EXPECT_TRUE(info != nullptr && info->source.has_value());
        if (info == nullptr || !info->source.has_value()) {
            return std::string{};
        }
        auto const& range{*info->source};
        auto const& source{document.source_files()[range.source_file_index].text};
        return source.substr(range.begin_offset, range.end_offset - range.begin_offset);
    };

    auto const enumeration{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto const migrated_enums{find_module("migrated_enums")};
    auto const revision_before_alias{document.revision()};
    auto rejected{document.apply(MoveDeclaration{.declaration = enumeration,
                                                 .module_index = migrated_enums,
                                                 .insertion_index = std::nullopt})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("types-registry editing"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_alias);
    EXPECT_EQ(document.declaration(enumeration)->identity.module_name, "authored_enums");

    auto const existing_soa{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const nested_soa{declaration_id(document, "authored_soa", "NestedFlags", "authored")};
    auto nested_user{*document.soa_schema(nested_soa)};
    nested_user.members[0].kind = codegen::SoaMemberKind::nested;
    nested_user.members[0].type.name = "authored::ExistingSoa";
    nested_user.members[0].fixed_schema = "ExistingSoa";
    nested_user.members[0].nested_schema = "ExistingSoa";
    auto added_local_user{
        document.apply(ReplaceSoa{.declaration = nested_soa, .schema = std::move(nested_user)})};
    ASSERT_TRUE(added_local_user.has_value()) << added_local_user.error().message;
    ASSERT_TRUE(*added_local_user);
    auto const revision_before_local_rejection{document.revision()};
    rejected = document.apply(MoveDeclaration{.declaration = existing_soa,
                                              .module_index = find_module("migrated_soa"),
                                              .insertion_index = std::nullopt});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("module-local nested/fixed"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_local_rejection);
    EXPECT_EQ(document.declaration(existing_soa)->identity.module_name, "authored_soa");
    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.dirty());

    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const varint{
        declaration_id(document, "authored_representations", "ExistingVarint", "authored")};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    auto const presence{
        declaration_id(document, "authored_representations", "ExistingPresence", "authored")};
    auto const scalar_text{declaration_text(scalar)};
    auto const original_type{document.types().find(document.declaration(scalar)->identity)};
    ASSERT_TRUE(original_type.has_value());
    auto const original_user_count{document.types().users_of(*original_type).size()};
    auto const migrated_scalars{find_module("migrated_scalars")};

    auto moved{document.apply(MoveDeclaration{
        .declaration = scalar, .module_index = migrated_scalars, .insertion_index = std::nullopt})};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "migrated_scalars");
    EXPECT_EQ(document.declaration(scalar)->identity.namespace_name, "migrated");
    auto const& packed_field{std::get<PackedField>(packed_type(document, packed).segments[1])};
    ASSERT_TRUE(packed_field.relationship.has_value());
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[1])
            .relationship->target.name,
        "migrated::ExistingScalar");
    EXPECT_EQ(document.linear_quantized_schema(quantized)->source.name, "migrated::ExistingScalar");
    EXPECT_EQ(document.integer_varint_schema(varint)->source.name, "migrated::ExistingScalar");
    EXPECT_EQ(document.optional_sentinel_schema(optional)->source.name, "migrated::ExistingScalar");
    EXPECT_EQ(document.optional_presence_bit_schema(presence)->source.name,
              "migrated::ExistingScalar");
    auto const moved_type{document.types().find(document.declaration(scalar)->identity)};
    ASSERT_TRUE(moved_type.has_value());
    EXPECT_EQ(document.types().users_of(*moved_type).size(), original_user_count);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    auto preview_text = [&](std::string_view const filename) -> std::string const& {
        auto const found{std::ranges::find_if(
            *preview, [&](auto const& update) { return update.path.filename() == filename; })};
        EXPECT_NE(found, preview->end());
        return found->updated;
    };
    EXPECT_EQ(preview_text("modules.lispb").find(scalar_text), std::string::npos);
    EXPECT_NE(preview_text("modules.lispb").find("migrated::ExistingScalar"), std::string::npos);
    EXPECT_NE(preview_text("migrated.lispb").find(scalar_text), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(scalar)->identity.module_name, "authored_scalars");
    EXPECT_EQ(document.integer_varint_schema(varint)->source.name, "authored::ExistingScalar");
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value());
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.integer_varint_schema(varint)->source.name, "migrated::ExistingScalar");

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 2U);
    auto reloaded{files.load_with_module_source("migrated.lispb")};
    auto const reloaded_scalar{
        declaration_id(reloaded, "migrated_scalars", "ExistingScalar", "migrated")};
    auto const reloaded_optional{
        declaration_id(reloaded, "authored_representations", "ExistingOptional", "authored")};
    EXPECT_EQ(optional_sentinel_type(reloaded, reloaded_optional).source.type,
              *reloaded.types().find(reloaded.declaration(reloaded_scalar)->identity));
    auto const migrated_source{
        std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
            return source.path.filename() == "migrated.lispb";
        })};
    ASSERT_NE(migrated_source, reloaded.source_files().end());
    EXPECT_NE(migrated_source->text.find(scalar_text), std::string::npos);
}

TEST(EditableSchemaDocument, RenamesIntegerScalarAndRepairsResolvedUsers) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                                  .module_name = "authored_scalars",
                                                                  .namespace_name = "authored",
                                                                  .name = "ExistingScalar"})};
    ASSERT_TRUE(declaration.has_value());

    auto duplicate{
        document.apply(RenameDeclaration{.declaration = *declaration, .new_name = "OtherScalar"})};
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_NE(duplicate.error().message.find("Duplicate declaration name"), std::string::npos);
    EXPECT_EQ(document.declaration(*declaration)->identity.name, "ExistingScalar");
    EXPECT_FALSE(document.dirty());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = *declaration, .new_name = "RenamedScalar"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(*renamed);
    EXPECT_EQ(document.declaration(*declaration)->identity.name, "RenamedScalar");
    auto const renamed_type{document.types().find(document.declaration(*declaration)->identity)};
    ASSERT_TRUE(renamed_type.has_value());

    auto const quantized{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const varint{
        declaration_id(document, "authored_representations", "ExistingVarint", "authored")};
    auto const optional{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    auto const presence{
        declaration_id(document, "authored_representations", "ExistingPresence", "authored")};
    EXPECT_EQ(linear_quantized_type(document, quantized).source.type, *renamed_type);
    EXPECT_EQ(integer_varint_type(document, varint).source.type, *renamed_type);
    EXPECT_EQ(optional_sentinel_type(document, optional).source.type, *renamed_type);
    EXPECT_EQ(optional_presence_bit_type(document, presence).source.type, *renamed_type);
    EXPECT_EQ(document.linear_quantized_schema(quantized)->source.name, "authored::RenamedScalar");
    EXPECT_EQ(document.integer_varint_schema(varint)->source.name, "authored::RenamedScalar");
    EXPECT_EQ(document.optional_sentinel_schema(optional)->source.name, "authored::RenamedScalar");
    EXPECT_EQ(document.optional_presence_bit_schema(presence)->source.name,
              "authored::RenamedScalar");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(integer-scalar ExistingScalar"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":source authored::ExistingScalar"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(integer-scalar RenamedScalar"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":source authored::RenamedScalar"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(*declaration)->identity.name, "ExistingScalar");
    EXPECT_EQ(document.linear_quantized_schema(quantized)->source.name, "authored::ExistingScalar");
    EXPECT_EQ(document.optional_sentinel_schema(optional)->source.name, "authored::ExistingScalar");
    EXPECT_EQ(document.optional_presence_bit_schema(presence)->source.name,
              "authored::ExistingScalar");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(*declaration)->identity.name, "RenamedScalar");

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_scalars",
                                               .namespace_name = "authored",
                                               .name = "RenamedScalar"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const reloaded_quantized{
        declaration_id(reloaded, "authored_representations", "ExistingQ1", "authored")};
    auto const reloaded_source{
        reloaded.types().find(reloaded.declaration(*reloaded_declaration)->identity)};
    ASSERT_TRUE(reloaded_source.has_value());
    EXPECT_EQ(linear_quantized_type(reloaded, reloaded_quantized).source.type, *reloaded_source);
    auto const reloaded_optional{
        declaration_id(reloaded, "authored_representations", "ExistingOptional", "authored")};
    EXPECT_EQ(optional_sentinel_type(reloaded, reloaded_optional).source.type, *reloaded_source);
    auto const reloaded_presence{
        declaration_id(reloaded, "authored_representations", "ExistingPresence", "authored")};
    EXPECT_EQ(optional_presence_bit_type(reloaded, reloaded_presence).source.type,
              *reloaded_source);
}

TEST(EditableSchemaDocument, RenamesIntegerScalarRelationshipTargetAndPreservesSource) {
    TemporarySchema files;
    auto document{files.load()};
    auto const target{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};

    auto const blocked_delete{document.apply(DeletePackedValue{.declaration = target})};
    ASSERT_FALSE(blocked_delete.has_value());
    EXPECT_NE(blocked_delete.error().message.find("ExistingScalar"), std::string::npos);
    EXPECT_NE(document.declaration(target), nullptr);

    auto renamed{
        document.apply(RenameDeclaration{.declaration = target, .new_name = "RenamedPacked"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(*renamed);
    ASSERT_TRUE(document.integer_scalar_schema(scalar)->relationship.has_value());
    EXPECT_EQ(document.integer_scalar_schema(scalar)->relationship->target.name,
              "authored::RenamedPacked");
    auto const renamed_target{document.types().find(document.declaration(target)->identity)};
    ASSERT_TRUE(renamed_target.has_value());
    EXPECT_EQ(integer_scalar_type(document, scalar).relationship->target.type, *renamed_target);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(packed-value RenamedPacked"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the scalar relationship note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(relation index_into   authored::RenamedPacked)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; scalar relationship trailing note"),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.integer_scalar_schema(scalar)->relationship->target.name,
              "authored::ExistingPacked");
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_scalar{
        declaration_id(reloaded, "authored_scalars", "ExistingScalar", "authored")};
    ASSERT_TRUE(reloaded.integer_scalar_schema(reloaded_scalar)->relationship.has_value());
    EXPECT_EQ(reloaded.integer_scalar_schema(reloaded_scalar)->relationship->target.name,
              "authored::RenamedPacked");
    EXPECT_EQ(reloaded.types()
                  .type(integer_scalar_type(reloaded, reloaded_scalar).relationship->target.type)
                  .identity.name,
              "RenamedPacked");
}

TEST(EditableSchemaDocument, RenamesEnumAndRepairsPackedSemanticReference) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_enum{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto const enumeration{document.allocate_declaration_id()};
    auto created{document.apply(CreateEnum{
        .declaration = enumeration,
        .module_index = document.declaration(existing_enum)->module_index,
        .schema = codegen::EnumSchema{
            .name = "PackedMode",
            .underlying_type = codegen::TypeRef{"std::uint8_t"},
            .bit_width = 1,
            .signedness = false,
            .values = {{.name = "Off", .initializer = "0"}, {.name = "On", .initializer = "1"}}}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto replacement{*document.packed_value_schema(packed)};
    auto const original_packed{replacement};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments[0])};
    field.type.name = "authored::PackedMode";
    field.kind = codegen::PackedFieldKind::enumeration;
    ASSERT_TRUE(
        document.apply(ReplacePackedValue{.declaration = packed, .schema = std::move(replacement)})
            .has_value());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = enumeration, .new_name = "PackedModeRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.declaration(enumeration)->identity.name, "PackedModeRenamed");
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[0])
            .type.name,
        "authored::PackedModeRenamed");
    auto const renamed_type{document.types().find(document.declaration(enumeration)->identity)};
    ASSERT_TRUE(renamed_type.has_value());
    EXPECT_EQ(std::get<PackedField>(packed_type(document, packed).segments[0]).semantic_type.type,
              *renamed_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(enum PackedModeRenamed"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("authored::PackedModeRenamed"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(enumeration)->identity.name, "PackedMode");
    EXPECT_EQ(
        std::get<codegen::PackedFieldSchema>(document.packed_value_schema(packed)->segments[0])
            .type.name,
        "authored::PackedMode");

    auto const revision_before_blocked_delete{document.revision()};
    ASSERT_TRUE(document.can_redo());
    auto blocked_delete{document.apply(DeleteEnum{.declaration = enumeration})};
    ASSERT_FALSE(blocked_delete.has_value());
    EXPECT_NE(blocked_delete.error().message.find("ExistingPacked"), std::string::npos);
    EXPECT_NE(document.declaration(enumeration), nullptr);
    EXPECT_EQ(document.revision(), revision_before_blocked_delete);
    EXPECT_TRUE(document.can_redo());

    ASSERT_TRUE(document.apply(ReplacePackedValue{.declaration = packed, .schema = original_packed})
                    .has_value());
    auto deleted{document.apply(DeleteEnum{.declaration = enumeration})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);
    EXPECT_EQ(document.declaration(enumeration), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_NE(document.declaration(enumeration), nullptr);
    EXPECT_EQ(document.declaration(enumeration)->identity.name, "PackedMode");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(enumeration), nullptr);
}

TEST(EditableSchemaDocument, RenamesSoaAndRepairsDirectAndNestedUsers) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const nested{declaration_id(document, "authored_soa", "NestedFlags", "authored")};
    auto const record{declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const original_id{document.declaration(declaration)->id};

    auto nested_user{*document.soa_schema(nested)};
    nested_user.members[0].kind = codegen::SoaMemberKind::nested;
    nested_user.members[0].type.name = "authored::ExistingSoa";
    nested_user.members[0].fixed_schema = "ExistingSoa";
    nested_user.members[0].nested_schema = "ExistingSoa";
    nested_user.members[0].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::contains,
                                        .target = codegen::TypeRef{"authored::ExistingSoa"},
                                        .unit = std::nullopt};
    nested_user.equivalent_type = codegen::TypeRef{"authored::ExistingSoa"};
    ASSERT_TRUE(document.apply(ReplaceSoa{.declaration = nested, .schema = std::move(nested_user)})
                    .has_value());

    auto record_user{*document.record_schema(record)};
    record_user.members[1].type.name = "authored::ExistingSoa";
    record_user.members[1].relationship =
        codegen::SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                        .target = codegen::TypeRef{"authored::ExistingSoa"},
                                        .unit = std::nullopt};
    ASSERT_TRUE(
        document.apply(ReplaceRecord{.declaration = record, .schema = std::move(record_user)})
            .has_value());

    auto const revision_before_collision{document.revision()};
    auto collision{document.apply(
        RenameDeclaration{.declaration = declaration, .new_name = "NestedFlagsView"})};
    ASSERT_FALSE(collision.has_value());
    EXPECT_NE(collision.error().message.find("Generated C++ name collision"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_collision);
    EXPECT_EQ(document.declaration(declaration)->identity.name, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].type.name, "authored::ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].fixed_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].nested_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].relationship->target.name,
              "authored::ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].relationship->target.name,
              "authored::ExistingSoa");

    auto renamed{
        document.apply(RenameDeclaration{.declaration = declaration, .new_name = "RenamedSoa"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(*renamed);
    ASSERT_NE(document.declaration(declaration), nullptr);
    EXPECT_EQ(document.declaration(declaration)->id, original_id);
    EXPECT_EQ(document.declaration(declaration)->identity.name, "RenamedSoa");
    EXPECT_EQ(document.soa_schema(declaration)->view_name, "ExistingSoaView");
    EXPECT_EQ(document.soa_schema(declaration)->const_view_name, "ExistingSoaConstView");
    EXPECT_EQ(document.soa_schema(nested)->members[0].type.name, "authored::RenamedSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].fixed_schema, "RenamedSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].nested_schema, "RenamedSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].relationship->target.name,
              "authored::RenamedSoa");
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::RenamedSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::RenamedSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].relationship->target.name,
              "authored::RenamedSoa");

    auto const renamed_type{document.types().find(document.declaration(declaration)->identity)};
    ASSERT_TRUE(renamed_type.has_value());
    auto const& nested_column{soa_type(document, nested).columns[0]};
    EXPECT_EQ(nested_column.semantic_type.type, *renamed_type);
    EXPECT_EQ(nested_column.nested_type, *renamed_type);
    EXPECT_EQ(nested_column.relationship->target.type, *renamed_type);
    EXPECT_EQ(soa_type(document, nested).equivalent_type->type, *renamed_type);
    EXPECT_EQ(record_type(document, record).members[1].semantic_type.type, *renamed_type);
    EXPECT_EQ(record_type(document, record).members[1].relationship->target.type, *renamed_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("(struct RenamedSoa"), std::string::npos);
    EXPECT_EQ(updated.find("(struct ExistingSoa"), std::string::npos);
    EXPECT_NE(updated.find(":view-name   ExistingSoaView"), std::string::npos);
    EXPECT_NE(updated.find(":const-view-name ExistingSoaConstView"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the SoA declaration note."), std::string::npos);
    EXPECT_NE(updated.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(updated.find(":body (\"values.clear();\")"), std::string::npos);
    EXPECT_NE(updated.find("authored::RenamedSoa"), std::string::npos);
    EXPECT_NE(updated.find(":fixed-schema RenamedSoa"), std::string::npos);
    EXPECT_NE(updated.find(":nested-schema RenamedSoa"), std::string::npos);
    EXPECT_EQ(updated.find("authored::ExistingSoa"), std::string::npos);
    EXPECT_EQ(updated.find(":fixed-schema ExistingSoa"), std::string::npos);
    EXPECT_EQ(updated.find(":nested-schema ExistingSoa"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(declaration)->identity.name, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].type.name, "authored::ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].fixed_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].nested_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].relationship->target.name,
              "authored::ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].relationship->target.name,
              "authored::ExistingSoa");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(declaration)->identity.name, "RenamedSoa");

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "RenamedSoa", "authored")};
    auto const reloaded_nested{declaration_id(reloaded, "authored_soa", "NestedFlags", "authored")};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ExistingRecord", "authored")};
    auto const reloaded_type{
        reloaded.types().find(reloaded.declaration(reloaded_declaration)->identity)};
    ASSERT_TRUE(reloaded_type.has_value());
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->view_name, "ExistingSoaView");
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->const_view_name, "ExistingSoaConstView");
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->members[0].type.name, "authored::RenamedSoa");
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->members[0].fixed_schema, "RenamedSoa");
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->members[0].nested_schema, "RenamedSoa");
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->members[0].relationship->target.name,
              "authored::RenamedSoa");
    EXPECT_EQ(soa_type(reloaded, reloaded_nested).columns[0].nested_type, *reloaded_type);
    EXPECT_EQ(soa_type(reloaded, reloaded_nested).columns[0].relationship->target.type,
              *reloaded_type);
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->equivalent_type->name, "authored::RenamedSoa");
    EXPECT_EQ(soa_type(reloaded, reloaded_nested).equivalent_type->type, *reloaded_type);
    EXPECT_EQ(reloaded.record_schema(reloaded_record)->members[1].type.name,
              "authored::RenamedSoa");
    EXPECT_EQ(reloaded.record_schema(reloaded_record)->members[1].relationship->target.name,
              "authored::RenamedSoa");
    EXPECT_EQ(record_type(reloaded, reloaded_record).members[1].relationship->target.type,
              *reloaded_type);
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the SoA declaration note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the custom function note."), std::string::npos);
}

TEST(EditableSchemaDocument, RejectsRenamingRegisteredSoaAlias) {
    auto document{fixture_document()};
    auto const declaration{declaration_id(document, "soa_fixture", "FChild")};
    auto const revision{document.revision()};

    auto renamed{
        document.apply(RenameDeclaration{.declaration = declaration, .new_name = "FRenamedChild"})};

    ASSERT_FALSE(renamed.has_value());
    EXPECT_NE(renamed.error().message.find("registered as '@child'"), std::string::npos);
    EXPECT_EQ(document.revision(), revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(document.declaration(declaration)->identity.name, "FChild");
}

TEST(EditableSchemaDocument, DeletesSourceDeclarationThroughExactUndoableTombstone) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_scalars", "OtherScalar", "authored")};
    auto const original_info{*document.declaration(declaration)};
    ASSERT_TRUE(original_info.source.has_value());
    auto const source{*original_info.source};
    auto const original_text{document.source_files()[source.source_file_index].text};
    auto expected_text{original_text};
    expected_text.erase(source.begin_offset, source.end_offset - source.begin_offset);

    auto deleted{document.apply(DeleteIntegerScalar{.declaration = declaration})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);
    EXPECT_EQ(document.declaration(declaration), nullptr);
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, expected_text);
    EXPECT_NE(preview->front().updated.find("(integer-scalar ExistingScalar"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(integer-scalar OtherScalar"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    auto const* restored{document.declaration(declaration)};
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->source, original_info.source);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());

    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(declaration), nullptr);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated, expected_text);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    EXPECT_FALSE(reloaded
                     .find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                    .module_name = "authored_scalars",
                                                    .namespace_name = "authored",
                                                    .name = "OtherScalar"})
                     .has_value());
    EXPECT_TRUE(reloaded
                    .find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                   .module_name = "authored_scalars",
                                                   .namespace_name = "authored",
                                                   .name = "ExistingScalar"})
                    .has_value());
    EXPECT_EQ(reloaded.source_files()[source.source_file_index].text, expected_text);
}

TEST(EditableSchemaDocument, RejectsDeletingDeclarationBoundToRegisteredAlias) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_enums", "Existing", "authored")};
    auto const revision_before{document.revision()};

    auto deleted{document.apply(DeleteEnum{.declaration = declaration})};

    ASSERT_FALSE(deleted.has_value());
    EXPECT_NE(deleted.error().message.find("registered as '@existing'"), std::string::npos);
    EXPECT_NE(document.declaration(declaration), nullptr);
    EXPECT_EQ(document.revision(), revision_before);
}

TEST(EditableSchemaDocument, CreatesEditsAndReloadsLinearQuantizedRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "ExistingQ1"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const source{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                             .module_name = "authored_scalars",
                                                             .namespace_name = "authored",
                                                             .name = "SignedScalar"})};
    ASSERT_TRUE(source.has_value());
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateLinearQuantized{
        .declaration = created,
        .module_index = module_index,
        .schema =
            codegen::LinearQuantizedSchema{.name = "ExistingQ2",
                                           .source = codegen::TypeRef{"authored::SignedScalar"},
                                           .bit_width = 2,
                                           .reserved_codes = 1,
                                           .clipping = codegen::QuantizationClipping::clamp},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    auto const source_type{document.types().find(document.declaration(*source)->identity)};
    ASSERT_TRUE(source_type.has_value());
    EXPECT_EQ(linear_quantized_type(document, created).source.type, *source_type);
    auto const& signed_source{
        std::get<IntegerScalarType>(document.types().type(*source_type).definition)};
    EXPECT_TRUE(signed_source.signedness);
    EXPECT_EQ(signed_source.minimum_value, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(signed_source.maximum_value, codegen::PackedIntegerValue{100});
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto invalid{*document.linear_quantized_schema(created)};
    invalid.reserved_codes = 3;
    auto rejected{document.apply(
        ReplaceLinearQuantized{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("at least two usable codes"), std::string::npos);
    EXPECT_EQ(linear_quantized_type(document, created).reserved_codes, 1U);

    auto replacement{*document.linear_quantized_schema(created)};
    replacement.bit_width = 8;
    replacement.reserved_codes = 2;
    replacement.clipping = codegen::QuantizationClipping::reject;
    auto replaced{document.apply(
        ReplaceLinearQuantized{.declaration = created, .schema = std::move(replacement)})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(linear_quantized_type(document, created).bit_width, 8U);
    EXPECT_EQ(linear_quantized_type(document, created).reserved_codes, 2U);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(linear_quantized_type(document, created).bit_width, 2U);
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(linear-quantized ExistingQ2"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":source authored::SignedScalar"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bits 8"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":reserved-codes 2"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":clipping reject"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "ExistingQ2"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.linear_quantized_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->bit_width, 8U);
    EXPECT_EQ(schema->reserved_codes, 2U);
    EXPECT_EQ(schema->clipping, codegen::QuantizationClipping::reject);
    EXPECT_EQ(linear_quantized_type(reloaded, *reloaded_declaration).source.type,
              *reloaded.types().find_declared("authored_scalars", "SignedScalar"));
}

TEST(EditableSchemaDocument, CreatesEditsAndReloadsIntegerVarintRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "ExistingVarint"})};
    ASSERT_TRUE(existing.has_value());
    ASSERT_TRUE(document.declaration(*existing)->source.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(
        CreateIntegerVarint{.declaration = created,
                            .module_index = module_index,
                            .schema =
                                codegen::IntegerVarintSchema{
                                    .name = "ExistingVarint2",
                                    .source = codegen::TypeRef{"authored::ExistingScalar"},
                                    .encoding = codegen::IntegerVarintEncoding::unsigned_varint},
                            .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    EXPECT_EQ(integer_varint_type(document, created).encoding,
              codegen::IntegerVarintEncoding::unsigned_varint);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto invalid{*document.integer_varint_schema(created)};
    invalid.encoding = codegen::IntegerVarintEncoding::zigzag_varint;
    auto rejected{
        document.apply(ReplaceIntegerVarint{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("require a signed source"), std::string::npos);
    EXPECT_EQ(integer_varint_type(document, created).encoding,
              codegen::IntegerVarintEncoding::unsigned_varint);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(integer-varint ExistingVarint2"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":encoding unsigned"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "ExistingVarint2"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.integer_varint_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->encoding, codegen::IntegerVarintEncoding::unsigned_varint);
}

TEST(EditableSchemaDocument, CreatesEditsAndReloadsFixedPointRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        declaration_id(document, "authored_representations", "ExistingQ1", "authored")};
    auto const module_index{document.declaration(existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateFixedPoint{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::FixedPointSchema{.name = "VelocityQ12_4",
                                            .signedness = true,
                                            .total_bits = 16,
                                            .fractional_bits = 4,
                                            .rounding = codegen::FixedPointRounding::nearest_even},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    auto const type_id{document.types().find(document.declaration(created)->identity)};
    ASSERT_TRUE(type_id.has_value());
    auto const& created_type{std::get<FixedPointType>(document.types().type(*type_id).definition)};
    EXPECT_EQ(created_type.total_bits, 16U);
    EXPECT_EQ(created_type.fractional_bits, 4U);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto invalid{*document.fixed_point_schema(created)};
    invalid.fractional_bits = invalid.total_bits;
    auto const revision_before_invalid{document.revision()};
    auto rejected{
        document.apply(ReplaceFixedPoint{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("one sign bit remains"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_invalid);

    auto replacement{*document.fixed_point_schema(created)};
    replacement.total_bits = 24;
    replacement.fractional_bits = 8;
    replacement.rounding = codegen::FixedPointRounding::toward_zero;
    ASSERT_TRUE(
        document.apply(ReplaceFixedPoint{.declaration = created, .schema = std::move(replacement)})
            .has_value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(fixed-point VelocityQ12_4"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":signed true"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":total-bits 24"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":fractional-bits 8"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":rounding toward-zero"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "VelocityQ12_4"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.fixed_point_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_TRUE(schema->signedness);
    EXPECT_EQ(schema->total_bits, 24U);
    EXPECT_EQ(schema->fractional_bits, 8U);
    EXPECT_EQ(schema->rounding, codegen::FixedPointRounding::toward_zero);
}

TEST(EditableSchemaDocument, EditsDeletesAndPreservesMiniFloatSource) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{
        declaration_id(document, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const original_info{*document.declaration(declaration)};
    ASSERT_TRUE(original_info.source.has_value());

    auto deleted{document.apply(DeleteMiniFloat{.declaration = declaration})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    EXPECT_EQ(document.declaration(declaration), nullptr);
    auto deletion_preview{document.preview_source_updates()};
    ASSERT_TRUE(deletion_preview.has_value()) << deletion_preview.error().message;
    EXPECT_EQ(deletion_preview->front().updated.find("(mini-float ExistingMiniFloat"),
              std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(declaration), nullptr);
    EXPECT_EQ(document.declaration(declaration)->source, original_info.source);

    auto invalid{*document.mini_float_schema(declaration)};
    invalid.exponent_bits = 1;
    auto const revision_before_invalid{document.revision()};
    auto rejected{
        document.apply(ReplaceMiniFloat{.declaration = declaration, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("exponent width"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_invalid);

    auto replacement{*document.mini_float_schema(declaration)};
    replacement.exponent_bits = 8;
    replacement.significand_bits = 7;
    replacement.exponent_bias = 127;
    auto applied{document.apply(
        ReplaceMiniFloat{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.mini_float_schema(declaration)->exponent_bits, 5U);
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.mini_float_schema(declaration)->exponent_bits, 8U);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("; Keep the mini-float note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":exponent-bits   8"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":significand-bits 7"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bias 127"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const* schema{reloaded.mini_float_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->sign_bits, 1U);
    EXPECT_EQ(schema->exponent_bits, 8U);
    EXPECT_EQ(schema->significand_bits, 7U);
    EXPECT_EQ(schema->exponent_bias, 127);
}

TEST(EditableSchemaDocument, CreatesRenamesAndReloadsMiniFloatRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        declaration_id(document, "authored_representations", "ExistingMiniFloat", "authored")};
    auto const module_index{document.declaration(existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{
        document.apply(CreateMiniFloat{.declaration = created,
                                       .module_index = module_index,
                                       .schema = codegen::MiniFloatSchema{.name = "CompactFloat",
                                                                          .sign_bits = 1,
                                                                          .exponent_bits = 4,
                                                                          .significand_bits = 3,
                                                                          .exponent_bias = 7},
                                       .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    auto const type_id{document.types().find(document.declaration(created)->identity)};
    ASSERT_TRUE(type_id.has_value());
    auto const& created_type{std::get<MiniFloatType>(document.types().type(*type_id).definition)};
    EXPECT_EQ(created_type.exponent_bits, 4U);
    EXPECT_TRUE(document.types().dependencies_of(*type_id).empty());
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto duplicate_name{
        document.apply(RenameDeclaration{.declaration = created, .new_name = "ExistingMiniFloat"})};
    ASSERT_FALSE(duplicate_name.has_value());
    EXPECT_EQ(document.declaration(created)->identity.name, "CompactFloat");

    auto renamed{
        document.apply(RenameDeclaration{.declaration = created, .new_name = "CompactFloat8"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.declaration(created)->identity.name, "CompactFloat8");
    EXPECT_EQ(document.mini_float_schema(created)->name, "CompactFloat8");

    auto deleted{document.apply(DeleteMiniFloat{.declaration = created})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(created), nullptr);
    EXPECT_EQ(document.declaration(created)->id, created);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(mini-float CompactFloat8"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":sign-bits 1"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":exponent-bits 4"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":significand-bits 3"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":bias 7"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_representations", "CompactFloat8", "authored")};
    auto const* schema{reloaded.mini_float_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->sign_bits, 1U);
    EXPECT_EQ(schema->exponent_bits, 4U);
    EXPECT_EQ(schema->significand_bits, 3U);
    EXPECT_EQ(schema->exponent_bias, 7);
}

TEST(EditableSchemaDocument, CreatesEditsDeletesAndReloadsOptionalSentinelRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        declaration_id(document, "authored_representations", "ExistingOptional", "authored")};
    auto const original_info{*document.declaration(existing)};
    ASSERT_TRUE(original_info.source.has_value());

    auto deleted{document.apply(DeleteOptionalSentinel{.declaration = existing})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    EXPECT_EQ(document.declaration(existing), nullptr);
    auto deletion_preview{document.preview_source_updates()};
    ASSERT_TRUE(deletion_preview.has_value()) << deletion_preview.error().message;
    EXPECT_EQ(deletion_preview->front().updated.find("(optional-sentinel ExistingOptional"),
              std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(existing), nullptr);
    EXPECT_EQ(document.declaration(existing)->source, original_info.source);

    auto const module_index{document.declaration(existing)->module_index};
    auto const created{document.allocate_declaration_id()};
    auto create{document.apply(CreateOptionalSentinel{
        .declaration = created,
        .module_index = module_index,
        .schema =
            codegen::OptionalSentinelSchema{.name = "OptionalScalar",
                                            .source = codegen::TypeRef{"authored::ExistingScalar"},
                                            .sentinel = "Invalid"},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    EXPECT_EQ(optional_sentinel_type(document, created).sentinel_value,
              codegen::PackedIntegerValue{3});
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto invalid{*document.optional_sentinel_schema(created)};
    invalid.sentinel = "Missing";
    auto const revision_before_invalid{document.revision()};
    auto rejected{document.apply(
        ReplaceOptionalSentinel{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("unknown source code"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_invalid);

    auto replacement{*document.optional_sentinel_schema(created)};
    replacement.sentinel = "Pending";
    ASSERT_TRUE(document
                    .apply(ReplaceOptionalSentinel{.declaration = created,
                                                   .schema = std::move(replacement)})
                    .has_value());
    EXPECT_EQ(optional_sentinel_type(document, created).sentinel_value,
              codegen::PackedIntegerValue{2});
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.optional_sentinel_schema(created)->sentinel, "Invalid");
    ASSERT_TRUE(document.redo().value());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = created, .new_name = "OptionalScalarRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.declaration(created)->identity.name, "OptionalScalarRenamed");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(optional-sentinel OptionalScalarRenamed"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":source authored::ExistingScalar"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":sentinel Pending"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "OptionalScalarRenamed"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.optional_sentinel_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->source.name, "authored::ExistingScalar");
    EXPECT_EQ(schema->sentinel, "Pending");
    EXPECT_EQ(optional_sentinel_type(reloaded, *reloaded_declaration).sentinel_value,
              codegen::PackedIntegerValue{2});
}

TEST(EditableSchemaDocument, CreatesEditsDeletesAndReloadsOptionalPresenceBitRepresentations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{
        declaration_id(document, "authored_representations", "ExistingPresence", "authored")};
    auto const original_info{*document.declaration(existing)};
    ASSERT_TRUE(original_info.source.has_value());

    auto deleted{document.apply(DeleteOptionalPresenceBit{.declaration = existing})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    EXPECT_EQ(document.declaration(existing), nullptr);
    auto deletion_preview{document.preview_source_updates()};
    ASSERT_TRUE(deletion_preview.has_value()) << deletion_preview.error().message;
    EXPECT_EQ(deletion_preview->front().updated.find("(optional-presence-bit ExistingPresence"),
              std::string::npos);
    EXPECT_NE(deletion_preview->front().updated.find("(optional-sentinel ExistingOptional"),
              std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(existing), nullptr);
    EXPECT_EQ(document.declaration(existing)->source, original_info.source);

    auto const module_index{document.declaration(existing)->module_index};
    auto const created{document.allocate_declaration_id()};
    auto create{document.apply(CreateOptionalPresenceBit{
        .declaration = created,
        .module_index = module_index,
        .schema =
            codegen::OptionalPresenceBitSchema{
                .name = "PresentScalar", .source = codegen::TypeRef{"authored::ExistingScalar"}},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    EXPECT_EQ(optional_presence_bit_type(document, created).payload_bits, 2U);
    EXPECT_EQ(optional_presence_bit_type(document, created).encoded_bits, 3U);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto invalid{*document.optional_presence_bit_schema(created)};
    invalid.source = codegen::TypeRef{"std::uint32_t"};
    auto const revision_before_invalid{document.revision()};
    auto rejected{document.apply(
        ReplaceOptionalPresenceBit{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("integer domain"), std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_invalid);

    auto replacement{*document.optional_presence_bit_schema(created)};
    replacement.source = codegen::TypeRef{"authored::OtherScalar"};
    ASSERT_TRUE(document
                    .apply(ReplaceOptionalPresenceBit{.declaration = created,
                                                      .schema = std::move(replacement)})
                    .has_value());
    EXPECT_EQ(optional_presence_bit_type(document, created).payload_bits, 2U);
    auto const other{document.types().find_declared("authored_scalars", "OtherScalar")};
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(optional_presence_bit_type(document, created).source.type, *other);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.optional_presence_bit_schema(created)->source.name,
              "authored::ExistingScalar");
    ASSERT_TRUE(document.redo().value());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = created, .new_name = "PresentScalarRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.declaration(created)->identity.name, "PresentScalarRenamed");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(optional-presence-bit PresentScalarRenamed"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":source authored::OtherScalar"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_representations",
                                               .namespace_name = "authored",
                                               .name = "PresentScalarRenamed"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.optional_presence_bit_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->source.name, "authored::OtherScalar");
    EXPECT_EQ(optional_presence_bit_type(reloaded, *reloaded_declaration).payload_bits, 2U);
    EXPECT_EQ(optional_presence_bit_type(reloaded, *reloaded_declaration).encoded_bits, 3U);
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsRecords) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_records",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingRecord"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateRecord{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::RecordSchema{.name = "DesignedRecord",
                                        .members = {{.name = "items",
                                                     .type = codegen::TypeRef{"ExistingRecord"},
                                                     .count = 2},
                                                    {.name = "tag",
                                                     .type = codegen::TypeRef{"std::uint8_t"}}},
                                        .export_specifier = "PROJECT_API"},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto const& created_type{record_type(document, created)};
    ASSERT_EQ(created_type.members.size(), 2U);
    EXPECT_EQ(created_type.members[0].count, 2);
    EXPECT_EQ(document.types().type(created_type.members[0].semantic_type.type).identity.name,
              "ExistingRecord");

    auto invalid{*document.record_schema(created)};
    invalid.members[1].name = "items";
    auto rejected{
        document.apply(ReplaceRecord{.declaration = created, .schema = std::move(invalid)})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("duplicate"), std::string::npos);

    auto recursive{*document.record_schema(created)};
    recursive.members[1].type = codegen::TypeRef{"DesignedRecord"};
    rejected =
        document.apply(ReplaceRecord{.declaration = created, .schema = std::move(recursive)});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("cycle"), std::string::npos);

    auto reordered{*document.record_schema(created)};
    std::swap(reordered.members[0], reordered.members[1]);
    auto replaced{
        document.apply(ReplaceRecord{.declaration = created, .schema = std::move(reordered)})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(record_type(document, created).members[0].name, "tag");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(record_type(document, created).members[0].name, "items");
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(record DesignedRecord"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":export-specifier PROJECT_API"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member items ExistingRecord :count 2)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_records",
                                               .namespace_name = "authored",
                                               .name = "DesignedRecord"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.record_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "PROJECT_API");
    ASSERT_EQ(schema->members.size(), 2U);
    EXPECT_EQ(schema->members[0].name, "tag");
    EXPECT_EQ(schema->members[1].name, "items");
    EXPECT_EQ(schema->members[1].count, 2);
    auto const& reloaded_type{record_type(reloaded, *reloaded_declaration)};
    auto const dependency{reloaded_type.members[1].semantic_type.type};
    auto const declared{*reloaded.types().find_declared("authored_records", "DesignedRecord")};
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(declared), dependency),
              reloaded.types().dependencies_of(declared).end());
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsRawUnions) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_unions",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingUnion"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    auto create{document.apply(CreateUnion{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::UnionSchema{.name = "DesignedUnion",
                                       .alternatives = {{.name = "nested",
                                                         .type = codegen::TypeRef{"ExistingUnion"}},
                                                        {.name = "bytes",
                                                         .type = codegen::TypeRef{"std::uint8_t"},
                                                         .count = 12}},
                                       .export_specifier = "PROJECT_API"},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());

    auto const& created_type{union_type(document, created)};
    ASSERT_EQ(created_type.alternatives.size(), 2U);
    EXPECT_EQ(created_type.alternatives[1].count, 12);

    auto invalid{*document.union_schema(created)};
    invalid.alternatives[1].name = "nested";
    auto rejected{document.apply(ReplaceUnion{.declaration = created, .schema = invalid})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("duplicate"), std::string::npos);

    invalid = *document.union_schema(created);
    invalid.alternatives[1].type = codegen::TypeRef{"DesignedUnion"};
    rejected = document.apply(ReplaceUnion{.declaration = created, .schema = invalid});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("cycle"), std::string::npos);

    auto reordered{*document.union_schema(created)};
    std::swap(reordered.alternatives[0], reordered.alternatives[1]);
    auto replaced{document.apply(ReplaceUnion{.declaration = created, .schema = reordered})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(union_type(document, created).alternatives[0].name, "bytes");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(union_type(document, created).alternatives[0].name, "nested");
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(union DesignedUnion"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":export-specifier PROJECT_API"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(alternative bytes std::uint8_t :count 12)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_unions",
                                               .namespace_name = "authored",
                                               .name = "DesignedUnion"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* schema{reloaded.union_schema(*reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "PROJECT_API");
    ASSERT_EQ(schema->alternatives.size(), 2U);
    EXPECT_EQ(schema->alternatives[0].name, "bytes");
    EXPECT_EQ(schema->alternatives[0].count, 12);
    auto const& reloaded_type{union_type(reloaded, *reloaded_declaration)};
    auto const dependency{reloaded_type.alternatives[1].semantic_type.type};
    auto const declared{*reloaded.types().find_declared("authored_unions", "DesignedUnion")};
    EXPECT_NE(std::ranges::find(reloaded.types().dependencies_of(declared), dependency),
              reloaded.types().dependencies_of(declared).end());
}

TEST(EditableSchemaDocument, CreatesEditsReordersAndReloadsTaggedUnions) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing{document.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                                               .module_name = "authored_unions",
                                                               .namespace_name = "authored",
                                                               .name = "ExistingTagged"})};
    ASSERT_TRUE(existing.has_value());
    auto const module_index{document.declaration(*existing)->module_index};
    auto const created{document.allocate_declaration_id()};

    codegen::TaggedUnionSchema schema{};
    schema.name = "DesignedTagged";
    schema.discriminant.name = "authored::Existing";
    schema.export_specifier = "PROJECT_API";
    codegen::TaggedUnionAlternativeSchema scalar{};
    scalar.name = "scalar";
    scalar.type.name = "std::uint32_t";
    scalar.tag = "Zero";
    codegen::TaggedUnionAlternativeSchema bytes{};
    bytes.name = "bytes";
    bytes.type.name = "std::uint8_t";
    bytes.count = 12;
    bytes.tag = "One";
    schema.alternatives = {std::move(scalar), std::move(bytes)};
    auto create{document.apply(CreateTaggedUnion{.declaration = created,
                                                 .module_index = module_index,
                                                 .schema = std::move(schema),
                                                 .insertion_index = std::nullopt})};
    ASSERT_TRUE(create.has_value()) << create.error().message;
    ASSERT_TRUE(*create);
    auto const original_info{*document.declaration(created)};
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.declaration(created)->id, original_info.id);

    auto const& created_type{tagged_union_type(document, created)};
    ASSERT_EQ(created_type.alternatives.size(), 2U);
    EXPECT_EQ(created_type.alternatives[1].tag, "One");
    EXPECT_EQ(created_type.alternatives[1].count, 12);

    auto invalid{*document.tagged_union_schema(created)};
    invalid.alternatives[1].tag = "Zero";
    auto rejected{document.apply(ReplaceTaggedUnion{.declaration = created, .schema = invalid})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("duplicate"), std::string::npos);
    EXPECT_EQ(document.revision(), 3U);

    invalid = *document.tagged_union_schema(created);
    invalid.alternatives[1].tag = "Missing";
    rejected = document.apply(ReplaceTaggedUnion{.declaration = created, .schema = invalid});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("Missing"), std::string::npos);

    auto reordered{*document.tagged_union_schema(created)};
    std::swap(reordered.alternatives[0], reordered.alternatives[1]);
    auto replaced{document.apply(ReplaceTaggedUnion{.declaration = created, .schema = reordered})};
    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(tagged_union_type(document, created).alternatives[0].name, "bytes");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(tagged_union_type(document, created).alternatives[0].name, "scalar");
    ASSERT_TRUE(document.redo().value());

    auto renamed{document.apply(
        RenameDeclaration{.declaration = created, .new_name = "DesignedTaggedRenamed"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    EXPECT_EQ(document.declaration(created)->identity.name, "DesignedTaggedRenamed");
    EXPECT_NE(document.tagged_union_schema(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created)->identity.name, "DesignedTagged");

    ASSERT_TRUE(document.apply(DeleteTaggedUnion{.declaration = created}).value());
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created)->id, original_info.id);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(tagged-union DesignedTagged"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":discriminant authored::Existing"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(alternative bytes std::uint8_t :tag One :count 12)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_FALSE(document.dirty());

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        reloaded.find_declaration(TypeIdentity{.origin = TypeOrigin::declaration,
                                               .module_name = "authored_unions",
                                               .namespace_name = "authored",
                                               .name = "DesignedTagged"})};
    ASSERT_TRUE(reloaded_declaration.has_value());
    auto const* reloaded_schema{reloaded.tagged_union_schema(*reloaded_declaration)};
    ASSERT_NE(reloaded_schema, nullptr);
    EXPECT_EQ(reloaded_schema->export_specifier, "PROJECT_API");
    ASSERT_EQ(reloaded_schema->alternatives.size(), 2U);
    EXPECT_EQ(reloaded_schema->alternatives[0].name, "bytes");
    EXPECT_EQ(reloaded_schema->alternatives[0].tag, "One");
    EXPECT_EQ(reloaded_schema->alternatives[0].count, 12);
    auto const& reloaded_type{tagged_union_type(reloaded, *reloaded_declaration)};
    auto const declared{*reloaded.types().find_declared("authored_unions", "DesignedTagged")};
    auto const dependencies{reloaded.types().dependencies_of(declared)};
    EXPECT_NE(std::ranges::find(dependencies, reloaded_type.discriminant.type), dependencies.end());
    EXPECT_NE(std::ranges::find(dependencies, reloaded_type.alternatives[1].semantic_type.type),
              dependencies.end());
}

TEST(EditableSchemaDocument, CreatesRecordAndBindsSoaColumnAcrossModules) {
    TemporarySchema files;
    auto document{files.load()};
    auto const existing_record{
        declaration_id(document, "authored_records", "ExistingRecord", "authored")};
    auto const soa{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const record{document.allocate_declaration_id()};

    auto created{document.apply(CreateRecord{
        .declaration = record,
        .module_index = document.declaration(existing_record)->module_index,
        .schema = codegen::RecordSchema{
            .name = "ValuesRecord",
            .members = {{.name = "value", .type = codegen::TypeRef{"std::uint32_t"}}}}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);

    auto replacement{*document.soa_schema(soa)};
    auto const original_kind{replacement.members.front().kind};
    replacement.members.front().type = codegen::TypeRef{"authored::ValuesRecord"};
    auto bound{document.apply(ReplaceSoa{.declaration = soa, .schema = std::move(replacement)})};
    ASSERT_TRUE(bound.has_value()) << bound.error().message;
    ASSERT_TRUE(*bound);

    auto const record_type{document.types().find(document.declaration(record)->identity)};
    ASSERT_TRUE(record_type.has_value());
    auto const& bound_column{soa_type(document, soa).columns.front()};
    EXPECT_EQ(bound_column.semantic_type.type, *record_type);
    EXPECT_EQ(document.soa_schema(soa)->members.front().kind, original_kind);
    ASSERT_EQ(document.soa_schema(soa)->members.size(), 2U);
    EXPECT_EQ(document.soa_schema(soa)->members[1].name, "flags");

    ASSERT_TRUE(document.undo().value());
    EXPECT_NE(document.declaration(record), nullptr);
    EXPECT_EQ(document.soa_schema(soa)->members.front().type.name, "std::uint32_t");
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(record), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.redo().value());
    auto const rebound_record_type{document.types().find(document.declaration(record)->identity)};
    ASSERT_TRUE(rebound_record_type.has_value());
    EXPECT_EQ(soa_type(document, soa).columns.front().semantic_type.type, *rebound_record_type);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(record ValuesRecord"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("authored::ValuesRecord"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the values SoA member note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; SoA values trailing note"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_record{
        declaration_id(reloaded, "authored_records", "ValuesRecord", "authored")};
    auto const reloaded_soa{declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const resolved_record_type{
        reloaded.types().find(reloaded.declaration(reloaded_record)->identity)};
    ASSERT_TRUE(resolved_record_type.has_value());
    auto const& reloaded_column{soa_type(reloaded, reloaded_soa).columns.front()};
    EXPECT_EQ(reloaded_column.semantic_type.type, *resolved_record_type);
    EXPECT_EQ(reloaded.soa_schema(reloaded_soa)->members.front().kind, original_kind);
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

TEST(EditableSchemaDocument, PreparesAndReloadsAdvancedSoaDuplicates) {
    TemporarySchema files;
    auto document{files.load()};
    auto const source{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const module_index{document.declaration(source)->module_index};

    auto const blocker{document.allocate_declaration_id()};
    auto blocked_name{document.apply(
        CreateSoa{.declaration = blocker,
                  .module_index = module_index,
                  .schema = codegen::SoaSchema{.name = "ExistingSoa_copyView",
                                               .members = {codegen::SoaMemberSchema{
                                                   .name = "value",
                                                   .kind = codegen::SoaMemberKind::array,
                                                   .type = codegen::TypeRef{"std::uint8_t"}}}},
                  .insertion_index = std::nullopt})};
    ASSERT_TRUE(blocked_name.has_value()) << blocked_name.error().message;
    ASSERT_TRUE(*blocked_name);

    auto advanced{*document.soa_schema(source)};
    advanced.field_mask_name = "ExistingSoaFieldMask";
    advanced.field_enum_name = "ExistingSoaField";
    advanced.members[0].mask_field = true;
    advanced.members.push_back(
        codegen::SoaMemberSchema{.name = "field_mask",
                                 .kind = codegen::SoaMemberKind::array,
                                 .type = codegen::TypeRef{"ExistingSoaFieldMask"}});
    advanced.fixed =
        codegen::FixedSoaSchema{.storage_name = "ExistingSoaFixedStorage",
                                .containers = {"ExistingSoaFixed", "CustomFixedContainer"}};
    advanced.single_allocation = "ExistingSoaSingle";
    advanced.single_allocation_variants = {codegen::SingleAllocationVariant{
        .name = "ExistingSoaPool", .allocator = codegen::TypeRef{"@existing"}}};
    auto applied{document.apply(ReplaceSoa{.declaration = source, .schema = std::move(advanced)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const revision_before_prepare{document.revision()};
    auto prepared{document.prepare_soa_duplicate(source)};
    ASSERT_TRUE(prepared.has_value()) << prepared.error().message;
    EXPECT_EQ(document.revision(), revision_before_prepare);
    EXPECT_EQ(prepared->name, "ExistingSoa_copy");
    EXPECT_EQ(prepared->view_name, "ExistingSoa_copyView2");
    EXPECT_EQ(prepared->const_view_name, "ExistingSoa_copyConstView");
    EXPECT_EQ(prepared->field_mask_name, "ExistingSoa_copyFieldMask");
    EXPECT_EQ(prepared->field_enum_name, "ExistingSoa_copyField");
    ASSERT_TRUE(prepared->fixed.has_value());
    EXPECT_EQ(prepared->fixed->storage_name, "ExistingSoa_copyFixedStorage");
    EXPECT_EQ(prepared->fixed->containers,
              (std::vector<std::string>{"ExistingSoa_copyFixed", "CustomFixedContainer_copy"}));
    EXPECT_EQ(prepared->single_allocation, "ExistingSoa_copySingle");
    ASSERT_EQ(prepared->single_allocation_variants.size(), 1U);
    EXPECT_EQ(prepared->single_allocation_variants[0].name, "ExistingSoa_copyPool");
    ASSERT_EQ(prepared->members.size(), 3U);
    EXPECT_EQ(prepared->members[2].type.name, "ExistingSoa_copyFieldMask");
    ASSERT_EQ(prepared->functions.size(), 1U);
    EXPECT_EQ(prepared->functions[0].body_lines, std::vector<std::string>{"values.clear();"});

    auto const duplicate{document.allocate_declaration_id()};
    auto created{document.apply(CreateSoa{.declaration = duplicate,
                                          .module_index = module_index,
                                          .schema = std::move(*prepared),
                                          .insertion_index = std::nullopt})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);
    EXPECT_EQ(document.soa_schema(duplicate)->field_mask_name, "ExistingSoa_copyFieldMask");

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(duplicate), nullptr);
    ASSERT_TRUE(document.redo().value());
    ASSERT_NE(document.soa_schema(duplicate), nullptr);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_NE(updated.find("(struct ExistingSoa_copy"), std::string::npos);
    EXPECT_NE(updated.find(":view-name ExistingSoa_copyView2"), std::string::npos);
    EXPECT_NE(updated.find(":field-mask-name ExistingSoa_copyFieldMask"), std::string::npos);
    EXPECT_NE(updated.find("(member field_mask array ExistingSoa_copyFieldMask)"),
              std::string::npos);
    EXPECT_NE(updated.find("(fixed ExistingSoa_copyFixedStorage"), std::string::npos);
    EXPECT_NE(updated.find("(single-allocation ExistingSoa_copySingle"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_duplicate{
        declaration_id(reloaded, "authored_soa", "ExistingSoa_copy", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_duplicate)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->view_name, "ExistingSoa_copyView2");
    EXPECT_EQ(schema->const_view_name, "ExistingSoa_copyConstView");
    EXPECT_EQ(schema->field_mask_name, "ExistingSoa_copyFieldMask");
    EXPECT_EQ(schema->field_enum_name, "ExistingSoa_copyField");
    ASSERT_TRUE(schema->fixed.has_value());
    EXPECT_EQ(schema->fixed->storage_name, "ExistingSoa_copyFixedStorage");
    EXPECT_EQ(schema->single_allocation, "ExistingSoa_copySingle");
    ASSERT_EQ(schema->single_allocation_variants.size(), 1U);
    EXPECT_EQ(schema->single_allocation_variants[0].name, "ExistingSoa_copyPool");
    EXPECT_EQ(schema->members[2].type.name, "ExistingSoa_copyFieldMask");
    EXPECT_TRUE(schema->members[0].mask_field);
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions[0].body_lines, std::vector<std::string>{"values.clear();"});
}

TEST(EditableSchemaDocument, AuthorsAndRemovesFixedSoaLayouts) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const module_index{document.declaration(declaration)->module_index};

    auto const blocker{document.allocate_declaration_id()};
    auto created{document.apply(
        CreateSoa{.declaration = blocker,
                  .module_index = module_index,
                  .schema = codegen::SoaSchema{.name = "ExistingSoaFixedStorage",
                                               .members = {codegen::SoaMemberSchema{
                                                   .name = "value",
                                                   .kind = codegen::SoaMemberKind::array,
                                                   .type = codegen::TypeRef{"std::uint8_t"}}}},
                  .insertion_index = std::nullopt})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);

    auto storage_name{
        document.unique_soa_generated_type_name(declaration, "ExistingSoaFixedStorage")};
    ASSERT_TRUE(storage_name.has_value()) << storage_name.error().message;
    EXPECT_EQ(*storage_name, "ExistingSoaFixedStorage2");
    auto first_container{document.unique_soa_generated_type_name(declaration, "ExistingSoaFixed")};
    ASSERT_TRUE(first_container.has_value()) << first_container.error().message;

    auto enabled{*document.soa_schema(declaration)};
    enabled.fixed = codegen::FixedSoaSchema{.storage_name = *storage_name,
                                            .containers = {*first_container, "CustomFixed"}};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(enabled)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto duplicate_name{
        document.unique_soa_generated_type_name(declaration, *first_container + "_copy")};
    ASSERT_TRUE(duplicate_name.has_value()) << duplicate_name.error().message;
    auto edited{*document.soa_schema(declaration)};
    std::swap(edited.fixed->containers[0], edited.fixed->containers[1]);
    edited.fixed->containers.insert(edited.fixed->containers.begin() + 1, *duplicate_name);
    applied = document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(edited)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->fixed->containers,
              (std::vector<std::string>{"ExistingSoaFixed", "CustomFixed"}));
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(
        document.soa_schema(declaration)->fixed->containers,
        (std::vector<std::string>{"CustomFixed", "ExistingSoaFixed_copy", "ExistingSoaFixed"}));

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(
        preview->front().updated.find("(fixed ExistingSoaFixedStorage2 :containers (CustomFixed "
                                      "ExistingSoaFixed_copy ExistingSoaFixed))"),
        std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* reloaded_schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(reloaded_schema, nullptr);
    ASSERT_TRUE(reloaded_schema->fixed.has_value());
    EXPECT_EQ(reloaded_schema->fixed->storage_name, "ExistingSoaFixedStorage2");
    EXPECT_EQ(
        reloaded_schema->fixed->containers,
        (std::vector<std::string>{"CustomFixed", "ExistingSoaFixed_copy", "ExistingSoaFixed"}));

    auto disabled{*reloaded_schema};
    disabled.fixed.reset();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(disabled)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_TRUE(reloaded.soa_schema(reloaded_declaration)->fixed.has_value());
    ASSERT_TRUE(reloaded.redo().value());
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->fixed.has_value());
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(fixed "), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_FALSE(final_document.soa_schema(final_declaration)->fixed.has_value());
}

TEST(EditableSchemaDocument, PreservesFixedSoaContainerRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(
        R"(      (parameter count std::uint32_t :default "0")))
  (struct NestedFlags)",
        R"(      (parameter count std::uint32_t :default "0"))
    ; Keep the fixed-layout note.
    (fixed ExistingSoaFixedStorage
      :containers (
        ; Keep the first fixed container note.
        ExistingSoaFixedA ; first fixed container trailing note
        ; Keep the second fixed container note.
        ExistingSoaFixedB ; second fixed container trailing note
      ))
  )
  (struct NestedFlags)");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.fixed->storage_name = "ExistingSoaCompactStorage";
    replacement.fixed->containers = {
        "ExistingSoaFixedB", "ExistingSoaFixedCopy", "ExistingSoaFixedA"};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const second_comment{updated.find("; Keep the second fixed container note.")};
    auto const second_row{updated.find("ExistingSoaFixedB ; second fixed container trailing note")};
    auto const copy_row{updated.find("ExistingSoaFixedCopy")};
    auto const first_comment{updated.find("; Keep the first fixed container note.")};
    auto const first_row{updated.find("ExistingSoaFixedA ; first fixed container trailing note")};
    ASSERT_NE(second_comment, std::string::npos);
    ASSERT_NE(second_row, std::string::npos);
    ASSERT_NE(copy_row, std::string::npos);
    ASSERT_NE(first_comment, std::string::npos);
    ASSERT_NE(first_row, std::string::npos);
    EXPECT_LT(second_comment, second_row);
    EXPECT_LT(second_row, copy_row);
    EXPECT_LT(copy_row, first_comment);
    EXPECT_LT(first_comment, first_row);
    EXPECT_NE(updated.find("(fixed ExistingSoaCompactStorage"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the fixed-layout note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto renamed_document{files.load()};
    auto const renamed_declaration{
        declaration_id(renamed_document, "authored_soa", "ExistingSoa", "authored")};
    auto renamed{*renamed_document.soa_schema(renamed_declaration)};
    renamed.fixed->containers[0] = "ExistingSoaFixedScratch";
    applied = renamed_document.apply(
        ReplaceSoa{.declaration = renamed_declaration, .schema = std::move(renamed)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& renamed_source{preview->front().updated};
    EXPECT_NE(renamed_source.find("; Keep the second fixed container note."), std::string::npos);
    EXPECT_NE(renamed_source.find("ExistingSoaFixedScratch ; second fixed container trailing note"),
              std::string::npos);
    ASSERT_TRUE(renamed_document.undo().value());
    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(renamed_document.redo().value());
    saved = renamed_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_declaration{
        declaration_id(deleting_document, "authored_soa", "ExistingSoa", "authored")};
    auto deleting{*deleting_document.soa_schema(deleting_declaration)};
    std::erase(deleting.fixed->containers, "ExistingSoaFixedA");
    applied = deleting_document.apply(
        ReplaceSoa{.declaration = deleting_declaration, .schema = std::move(deleting)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& deleting_source{preview->front().updated};
    EXPECT_EQ(deleting_source.find("; Keep the first fixed container note."), std::string::npos);
    EXPECT_EQ(deleting_source.find("; first fixed container trailing note"), std::string::npos);
    EXPECT_NE(deleting_source.find("; Keep the second fixed container note."), std::string::npos);
    EXPECT_NE(deleting_source.find("ExistingSoaFixedScratch"), std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    ASSERT_TRUE(schema->fixed.has_value());
    EXPECT_EQ(schema->fixed->storage_name, "ExistingSoaCompactStorage");
    EXPECT_EQ(schema->fixed->containers,
              (std::vector<std::string>{"ExistingSoaFixedScratch", "ExistingSoaFixedCopy"}));
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the fixed-layout note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the second fixed container note."),
              std::string::npos);
    EXPECT_EQ(module_source->text.find("; Keep the first fixed container note."),
              std::string::npos);
}

TEST(EditableSchemaDocument, SwitchesSoaViewTypesBetweenExplicitAndDerivedNames) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto derived{*document.soa_schema(declaration)};
    derived.view_name.reset();
    derived.const_view_name.reset();
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(derived)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":view-name"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":const-view-name"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->view_name, "ExistingSoaView");
    EXPECT_EQ(document.soa_schema(declaration)->const_view_name, "ExistingSoaConstView");
    ASSERT_TRUE(document.redo().value());
    EXPECT_FALSE(document.soa_schema(declaration)->view_name.has_value());
    EXPECT_FALSE(document.soa_schema(declaration)->const_view_name.has_value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->view_name.has_value());
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->const_view_name.has_value());

    auto explicit_names{*reloaded.soa_schema(reloaded_declaration)};
    explicit_names.view_name = "ExistingSoaMutableRows";
    explicit_names.const_view_name = "ExistingSoaRows";
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(explicit_names)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find(":view-name ExistingSoaMutableRows"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":const-view-name ExistingSoaRows"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->view_name.has_value());
    ASSERT_TRUE(reloaded.redo().value());
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->view_name, "ExistingSoaMutableRows");
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_EQ(final_document.soa_schema(final_declaration)->view_name, "ExistingSoaMutableRows");
    EXPECT_EQ(final_document.soa_schema(final_declaration)->const_view_name, "ExistingSoaRows");
}

TEST(EditableSchemaDocument, AuthorsAndRemovesSingleAllocationOwners) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const module_index{document.declaration(declaration)->module_index};

    auto const blocker{document.allocate_declaration_id()};
    auto created{document.apply(
        CreateSoa{.declaration = blocker,
                  .module_index = module_index,
                  .schema = codegen::SoaSchema{.name = "ExistingSoaSingleStorage",
                                               .members = {codegen::SoaMemberSchema{
                                                   .name = "value",
                                                   .kind = codegen::SoaMemberKind::array,
                                                   .type = codegen::TypeRef{"std::uint8_t"}}}},
                  .insertion_index = std::nullopt})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);

    auto owner_name{document.unique_soa_storage_owner_name(declaration, "ExistingSoaSingle")};
    ASSERT_TRUE(owner_name.has_value()) << owner_name.error().message;
    EXPECT_EQ(*owner_name, "ExistingSoaSingle2");
    auto enabled{*document.soa_schema(declaration)};
    enabled.fixed =
        codegen::FixedSoaSchema{.storage_name = "ExistingSoaCombinedStorage", .containers = {}};
    enabled.single_allocation = *owner_name;
    enabled.single_allocation_variants = {codegen::SingleAllocationVariant{
        .name = "ExistingSoaPool", .allocator = codegen::TypeRef{"@existing"}}};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(enabled)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(single-allocation ExistingSoaSingle2"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(variant ExistingSoaPool @existing)"),
              std::string::npos);
    auto const fixed_position{preview->front().updated.find("(fixed ExistingSoaCombinedStorage)")};
    auto const single_position{
        preview->front().updated.find("(single-allocation ExistingSoaSingle2")};
    ASSERT_NE(fixed_position, std::string::npos);
    ASSERT_NE(single_position, std::string::npos);
    EXPECT_LT(fixed_position, single_position);
    EXPECT_NE(preview->front().updated.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.soa_schema(declaration)->single_allocation.has_value());
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.soa_schema(declaration)->single_allocation, "ExistingSoaSingle2");

    auto renamed{*document.soa_schema(declaration)};
    renamed.single_allocation = "ExistingSoaCompact";
    applied = document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(renamed)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_EQ(document.soa_schema(declaration)->single_allocation_variants.size(), 1U);
    EXPECT_EQ(document.soa_schema(declaration)->single_allocation_variants[0].name,
              "ExistingSoaPool");

    auto duplicate_name{
        document.unique_soa_storage_owner_name(declaration, "ExistingSoaPool_copy")};
    ASSERT_TRUE(duplicate_name.has_value()) << duplicate_name.error().message;
    auto variants{*document.soa_schema(declaration)};
    auto variant_copy{variants.single_allocation_variants.front()};
    variant_copy.name = *duplicate_name;
    variant_copy.allocator.name = "std::uint32_t";
    variants.single_allocation_variants.push_back(std::move(variant_copy));
    std::swap(variants.single_allocation_variants[0], variants.single_allocation_variants[1]);
    applied = document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(variants)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    EXPECT_EQ(document.soa_schema(declaration)->single_allocation_variants[0].name,
              "ExistingSoaPool_copy");
    EXPECT_EQ(document.soa_schema(declaration)->single_allocation_variants[0].allocator.name,
              "std::uint32_t");
    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.soa_schema(declaration)->single_allocation_variants.size(), 1U);
    ASSERT_TRUE(document.redo().value());
    ASSERT_EQ(document.soa_schema(declaration)->single_allocation_variants.size(), 2U);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(single-allocation ExistingSoaCompact"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* reloaded_schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(reloaded_schema, nullptr);
    EXPECT_EQ(reloaded_schema->single_allocation, "ExistingSoaCompact");
    ASSERT_EQ(reloaded_schema->single_allocation_variants.size(), 2U);
    EXPECT_EQ(reloaded_schema->single_allocation_variants[0].name, "ExistingSoaPool_copy");
    EXPECT_EQ(reloaded_schema->single_allocation_variants[0].allocator.name, "std::uint32_t");
    EXPECT_EQ(reloaded_schema->single_allocation_variants[1].name, "ExistingSoaPool");
    EXPECT_EQ(reloaded_schema->single_allocation_variants[1].allocator.name, "@existing");

    auto disabled{*reloaded_schema};
    disabled.single_allocation.reset();
    disabled.single_allocation_variants.clear();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(disabled)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_TRUE(reloaded.soa_schema(reloaded_declaration)->single_allocation.has_value());
    ASSERT_TRUE(reloaded.redo().value());
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->single_allocation.has_value());
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->front().updated.find("(single-allocation "), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_FALSE(final_document.soa_schema(final_declaration)->single_allocation.has_value());
    EXPECT_TRUE(final_document.soa_schema(final_declaration)->single_allocation_variants.empty());
    EXPECT_TRUE(final_document.soa_schema(final_declaration)->fixed.has_value());
}

TEST(EditableSchemaDocument, PreservesSingleAllocationVariantRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(
        R"(      (parameter count std::uint32_t :default "0")))
  (struct NestedFlags)",
        R"(      (parameter count std::uint32_t :default "0"))
    ; Keep the single-allocation note.
    (single-allocation ExistingSoaSingle
      ; Keep the pool variant note.
      (variant ExistingSoaPool   @existing) ; pool variant trailing note
      ; Keep the arena variant note.
      (variant ExistingSoaArena std::uint32_t) ; arena variant trailing note
    ))
  (struct NestedFlags)");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.single_allocation = "ExistingSoaCompact";
    auto pool{replacement.single_allocation_variants[0]};
    auto arena{replacement.single_allocation_variants[1]};
    arena.allocator.name = "std::uint64_t";
    auto pool_copy{pool};
    pool_copy.name = "ExistingSoaPoolCopy";
    pool_copy.allocator.name = "std::uint16_t";
    replacement.single_allocation_variants = {arena, pool_copy, pool};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const arena_comment{updated.find("; Keep the arena variant note.")};
    auto const arena_row{updated.find("(variant ExistingSoaArena std::uint64_t)")};
    auto const copy_row{updated.find("(variant ExistingSoaPoolCopy std::uint16_t)")};
    auto const pool_comment{updated.find("; Keep the pool variant note.")};
    auto const pool_row{updated.find("(variant ExistingSoaPool   @existing)")};
    ASSERT_NE(arena_comment, std::string::npos);
    ASSERT_NE(arena_row, std::string::npos);
    ASSERT_NE(copy_row, std::string::npos);
    ASSERT_NE(pool_comment, std::string::npos);
    ASSERT_NE(pool_row, std::string::npos);
    EXPECT_LT(arena_comment, arena_row);
    EXPECT_LT(arena_row, copy_row);
    EXPECT_LT(copy_row, pool_comment);
    EXPECT_LT(pool_comment, pool_row);
    EXPECT_NE(updated.find("(single-allocation ExistingSoaCompact"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the single-allocation note."), std::string::npos);
    EXPECT_NE(updated.find("; arena variant trailing note"), std::string::npos);
    EXPECT_NE(updated.find("; pool variant trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto renamed_document{files.load()};
    auto const renamed_declaration{
        declaration_id(renamed_document, "authored_soa", "ExistingSoa", "authored")};
    auto renamed{*renamed_document.soa_schema(renamed_declaration)};
    renamed.single_allocation_variants[0].name = "ExistingSoaScratch";
    applied = renamed_document.apply(
        ReplaceSoa{.declaration = renamed_declaration, .schema = std::move(renamed)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& renamed_source{preview->front().updated};
    EXPECT_NE(renamed_source.find("; Keep the arena variant note."), std::string::npos);
    EXPECT_NE(renamed_source.find("(variant ExistingSoaScratch std::uint64_t)"), std::string::npos);
    EXPECT_NE(renamed_source.find("; arena variant trailing note"), std::string::npos);
    ASSERT_TRUE(renamed_document.undo().value());
    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(renamed_document.redo().value());
    saved = renamed_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_declaration{
        declaration_id(deleting_document, "authored_soa", "ExistingSoa", "authored")};
    auto deleting{*deleting_document.soa_schema(deleting_declaration)};
    std::erase_if(deleting.single_allocation_variants,
                  [](auto const& variant) { return variant.name == "ExistingSoaPool"; });
    applied = deleting_document.apply(
        ReplaceSoa{.declaration = deleting_declaration, .schema = std::move(deleting)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& deleting_source{preview->front().updated};
    EXPECT_EQ(deleting_source.find("; Keep the pool variant note."), std::string::npos);
    EXPECT_EQ(deleting_source.find("; pool variant trailing note"), std::string::npos);
    EXPECT_NE(deleting_source.find("; Keep the arena variant note."), std::string::npos);
    EXPECT_NE(deleting_source.find("(variant ExistingSoaScratch std::uint64_t)"),
              std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->single_allocation, "ExistingSoaCompact");
    ASSERT_EQ(schema->single_allocation_variants.size(), 2U);
    EXPECT_EQ(schema->single_allocation_variants[0].name, "ExistingSoaScratch");
    EXPECT_EQ(schema->single_allocation_variants[0].allocator.name, "std::uint64_t");
    EXPECT_EQ(schema->single_allocation_variants[1].name, "ExistingSoaPoolCopy");
    EXPECT_EQ(schema->single_allocation_variants[1].allocator.name, "std::uint16_t");
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the single-allocation note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the arena variant note."), std::string::npos);
    EXPECT_EQ(module_source->text.find("; Keep the pool variant note."), std::string::npos);
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
    replacement.view_name = "EditedSoaView";
    replacement.const_view_name = "EditedSoaConstView";
    replacement.export_specifier = "SOA_API";
    auto applied{
        document.apply(ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(source.find(":view-name   EditedSoaView"), std::string::npos);
    EXPECT_NE(source.find(":const-view-name EditedSoaConstView"), std::string::npos);
    EXPECT_NE(source.find(":operations (reserve set-num)"), std::string::npos);
    EXPECT_NE(source.find(":export-specifier SOA_API"), std::string::npos);
    EXPECT_NE(source.find(":using-declarations (\"Base::reset\")"), std::string::npos);
    EXPECT_NE(source.find("(function clear void"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note"), std::string::npos);
    EXPECT_NE(source.find(":body (\"values.clear();\")"), std::string::npos);
    EXPECT_NE(source.find(":noexcept true"), std::string::npos);
    EXPECT_NE(source.find("(member values array   std::uint16_t)"), std::string::npos);
    EXPECT_NE(source.find("; SoA values trailing note"), std::string::npos);

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
    EXPECT_EQ(schema->view_name, "EditedSoaView");
    EXPECT_EQ(schema->const_view_name, "EditedSoaConstView");
    EXPECT_EQ(schema->export_specifier, "SOA_API");
    EXPECT_EQ(schema->operations.size(), 2U);
    EXPECT_EQ(schema->using_declarations, std::vector<std::string>{"Base::reset"});
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions.front().body_lines, std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(schema->functions.front().is_noexcept);
    EXPECT_EQ(schema->members.front().type.name, "std::uint16_t");
}

TEST(EditableSchemaDocument, AuthorsSoaStorageOperationsWithoutDisturbingAdvancedMetadata) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const original_operations{document.soa_schema(declaration)->operations};
    ASSERT_EQ(
        original_operations,
        (std::vector{codegen::StorageOperation::reserve, codegen::StorageOperation::set_num}));

    auto replacement{*document.soa_schema(declaration)};
    replacement.operations = {
        codegen::StorageOperation::reset,
        codegen::StorageOperation::add_defaulted,
        codegen::StorageOperation::copy_element,
    };
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    EXPECT_EQ(document.soa_schema(declaration)->operations,
              (std::vector{codegen::StorageOperation::reset,
                           codegen::StorageOperation::add_defaulted,
                           codegen::StorageOperation::copy_element}));

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->operations, original_operations);
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find(":operations (reset add-defaulted copy-element)"), std::string::npos);
    EXPECT_NE(source.find(":using-declarations (\"Base::reset\")"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note"), std::string::npos);
    EXPECT_NE(source.find(":body (\"values.clear();\")"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->operations,
              (std::vector{codegen::StorageOperation::reset,
                           codegen::StorageOperation::add_defaulted,
                           codegen::StorageOperation::copy_element}));
    EXPECT_EQ(schema->using_declarations, std::vector<std::string>{"Base::reset"});
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions.front().body_lines, std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(schema->functions.front().is_noexcept);
}

TEST(EditableSchemaDocument, PreservesSoaStorageOperationRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(R"(    :operations (reserve set-num))",
                              R"(    :operations (
      ; Keep the reserve operation note.
      reserve ; reserve operation trailing note
      ; Keep the set operation note.
      set-num ; set operation trailing note
    ))");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.operations = {
        codegen::StorageOperation::set_num,
        codegen::StorageOperation::reset,
        codegen::StorageOperation::reserve,
    };
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& reordered{preview->front().updated};
    auto const set_note{reordered.find("; Keep the set operation note.")};
    auto const set_row{reordered.find("set-num ; set operation trailing note")};
    auto const reset_row{reordered.find("      reset")};
    auto const reserve_note{reordered.find("; Keep the reserve operation note.")};
    auto const reserve_row{reordered.find("reserve ; reserve operation trailing note")};
    ASSERT_NE(set_note, std::string::npos);
    ASSERT_NE(set_row, std::string::npos);
    ASSERT_NE(reset_row, std::string::npos);
    ASSERT_NE(reserve_note, std::string::npos);
    ASSERT_NE(reserve_row, std::string::npos);
    EXPECT_LT(set_note, set_row);
    EXPECT_LT(set_row, reset_row);
    EXPECT_LT(reset_row, reserve_note);
    EXPECT_LT(reserve_note, reserve_row);
    EXPECT_NE(reordered.find("; Keep the custom function note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto edited_document{files.load()};
    auto const edited_declaration{
        declaration_id(edited_document, "authored_soa", "ExistingSoa", "authored")};
    replacement = *edited_document.soa_schema(edited_declaration);
    replacement.operations.front() = codegen::StorageOperation::copy_element;
    applied = edited_document.apply(
        ReplaceSoa{.declaration = edited_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = edited_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& edited{preview->front().updated};
    EXPECT_NE(edited.find("; Keep the set operation note."), std::string::npos);
    EXPECT_NE(edited.find("copy-element ; set operation trailing note"), std::string::npos);
    EXPECT_NE(edited.find("; Keep the reserve operation note."), std::string::npos);
    saved = edited_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_declaration{
        declaration_id(deleting_document, "authored_soa", "ExistingSoa", "authored")};
    replacement = *deleting_document.soa_schema(deleting_declaration);
    replacement.operations.pop_back();
    applied = deleting_document.apply(
        ReplaceSoa{.declaration = deleting_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& deleting{preview->front().updated};
    EXPECT_EQ(deleting.find("; Keep the reserve operation note."), std::string::npos);
    EXPECT_EQ(deleting.find("; reserve operation trailing note"), std::string::npos);
    EXPECT_NE(deleting.find("; Keep the set operation note."), std::string::npos);
    EXPECT_NE(deleting.find("copy-element ; set operation trailing note"), std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto clearing_document{files.load()};
    auto const clearing_declaration{
        declaration_id(clearing_document, "authored_soa", "ExistingSoa", "authored")};
    replacement = *clearing_document.soa_schema(clearing_declaration);
    replacement.operations.clear();
    applied = clearing_document.apply(
        ReplaceSoa{.declaration = clearing_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = clearing_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":operations"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note."), std::string::npos);
    saved = clearing_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto adding_document{files.load()};
    auto const adding_declaration{
        declaration_id(adding_document, "authored_soa", "ExistingSoa", "authored")};
    replacement = *adding_document.soa_schema(adding_declaration);
    replacement.operations = {codegen::StorageOperation::reserve};
    applied = adding_document.apply(
        ReplaceSoa{.declaration = adding_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = adding_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(":operations (reserve)"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note."), std::string::npos);
    saved = adding_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->operations, (std::vector{codegen::StorageOperation::reserve}));
}

TEST(EditableSchemaDocument, PreservesAllSoaStorageOperationShorthandUntilItChanges) {
    TemporarySchema files;
    files.replace_module_text(R"(    :operations (reserve set-num))",
                              R"(    :operations (all) ; Keep the all-operations shorthand.)");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_EQ(document.soa_schema(declaration)->operations, codegen::all_storage_operations());

    auto replacement{*document.soa_schema(declaration)};
    replacement.members.front().type.name = "std::uint16_t";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(
        preview->front().updated.find(":operations (all) ; Keep the all-operations shorthand."),
        std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member values array   std::uint16_t)"),
              std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    replacement = *reloaded.soa_schema(reloaded_declaration);
    replacement.operations.pop_back();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_EQ(updated.find(":operations (all)"), std::string::npos);
    EXPECT_NE(updated.find(":operations (reset reserve add-uninitialised add-defaulted "
                           "remove-at-swap set-num copy-element)"),
              std::string::npos);
    EXPECT_NE(updated.find("; Keep the all-operations shorthand."), std::string::npos);

    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_EQ(final_document.soa_schema(final_declaration)->operations.size(),
              codegen::all_storage_operations().size() - 1);
}

TEST(EditableSchemaDocument, AuthorsAndRemovesSoaGenerationPolicy) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const equivalent{document.types().find_registered("existing")};
    ASSERT_TRUE(equivalent.has_value());

    auto replacement{*document.soa_schema(declaration)};
    replacement.export_specifier = "SOA_API";
    replacement.equivalent_type = codegen::TypeRef{"@existing"};
    replacement.copy_element_memberwise = true;
    replacement.layout_only = true;
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const* schema{document.soa_schema(declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "SOA_API");
    ASSERT_TRUE(schema->equivalent_type.has_value());
    EXPECT_EQ(schema->equivalent_type->name, "@existing");
    EXPECT_TRUE(schema->copy_element_memberwise);
    EXPECT_TRUE(schema->layout_only);
    ASSERT_TRUE(soa_type(document, declaration).equivalent_type.has_value());
    EXPECT_EQ(soa_type(document, declaration).equivalent_type->type, *equivalent);
    auto const type{document.types().find(document.declaration(declaration)->identity)};
    ASSERT_TRUE(type.has_value());
    EXPECT_NE(std::ranges::find(document.types().dependencies_of(*type), *equivalent),
              document.types().dependencies_of(*type).end());

    ASSERT_TRUE(document.undo().value());
    schema = document.soa_schema(declaration);
    EXPECT_FALSE(schema->export_specifier.has_value());
    EXPECT_FALSE(schema->equivalent_type.has_value());
    EXPECT_FALSE(schema->copy_element_memberwise);
    EXPECT_FALSE(schema->layout_only);
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find(":export-specifier SOA_API"), std::string::npos);
    EXPECT_NE(source.find(":equivalent-type @existing"), std::string::npos);
    EXPECT_NE(source.find(":copy-element-memberwise true"), std::string::npos);
    EXPECT_NE(source.find(":layout-only true"), std::string::npos);
    EXPECT_NE(source.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    schema = reloaded.soa_schema(reloaded_declaration);
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "SOA_API");
    EXPECT_EQ(schema->equivalent_type->name, "@existing");
    EXPECT_TRUE(schema->copy_element_memberwise);
    EXPECT_TRUE(schema->layout_only);

    replacement = *schema;
    replacement.export_specifier.reset();
    replacement.equivalent_type.reset();
    replacement.copy_element_memberwise = false;
    replacement.layout_only = false;
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":export-specifier"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":equivalent-type"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":copy-element-memberwise"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":layout-only"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    schema = final_document.soa_schema(final_declaration);
    ASSERT_NE(schema, nullptr);
    EXPECT_FALSE(schema->export_specifier.has_value());
    EXPECT_FALSE(schema->equivalent_type.has_value());
    EXPECT_FALSE(schema->copy_element_memberwise);
    EXPECT_FALSE(schema->layout_only);
}

TEST(EditableSchemaDocument, AuthorsReordersAndRemovesSoaUsingDeclarations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto const original_revision{document.revision()};
    auto replacement{*document.soa_schema(declaration)};
    replacement.using_declarations.push_back(" \t");
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_NE(applied.error().message.find("using declaration"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_EQ(document.soa_schema(declaration)->using_declarations,
              std::vector<std::string>{"Base::reset"});

    replacement = *document.soa_schema(declaration);
    replacement.using_declarations.push_back("Index = std::uint32_t");
    replacement.using_declarations.push_back("Weight = float");
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    replacement = *document.soa_schema(declaration);
    std::rotate(replacement.using_declarations.begin(),
                replacement.using_declarations.end() - 1,
                replacement.using_declarations.end());
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    auto const reordered{
        std::vector<std::string>{"Weight = float", "Base::reset", "Index = std::uint32_t"}};
    EXPECT_EQ(document.soa_schema(declaration)->using_declarations, reordered);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->using_declarations,
              (std::vector<std::string>{"Base::reset", "Index = std::uint32_t", "Weight = float"}));
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.soa_schema(declaration)->using_declarations, reordered);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find(":using-declarations (\"Weight = float\" \"Base::reset\" "
                          "\"Index = std::uint32_t\")"),
              std::string::npos);
    EXPECT_NE(source.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->using_declarations, reordered);

    replacement = *reloaded.soa_schema(reloaded_declaration);
    replacement.using_declarations.clear();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":using-declarations"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    EXPECT_TRUE(final_document.soa_schema(final_declaration)->using_declarations.empty());
}

TEST(EditableSchemaDocument, AuthorsOrdersAndEditsSoaFunctionSignatures) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto const original_revision{document.revision()};
    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().name = "ExistingSoa";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().name, "clear");

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().is_inline = true;
    replacement.functions.insert(replacement.functions.begin(),
                                 codegen::FunctionSchema{
                                     .name = "inspect",
                                     .return_type = codegen::TypeRef{"std::uint32_t"},
                                     .parameters = {codegen::ParameterSchema{
                                         .type = codegen::TypeRef{"std::uint32_t"},
                                         .name = "index",
                                         .default_value = "0",
                                     }},
                                     .body_lines = {"return values[index];"},
                                     .is_const = true,
                                     .is_noexcept = true,
                                     .definition_in_source = true,
                                 });
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const* schema{document.soa_schema(declaration)};
    ASSERT_EQ(schema->functions.size(), 2U);
    EXPECT_EQ(schema->functions[0].name, "inspect");
    EXPECT_EQ(schema->functions[1].name, "clear");
    EXPECT_EQ(schema->functions[1].body_lines, std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(schema->functions[1].is_noexcept);
    EXPECT_TRUE(schema->functions[1].is_inline);

    ASSERT_TRUE(document.undo().value());
    ASSERT_EQ(document.soa_schema(declaration)->functions.size(), 1U);
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().name, "clear");
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().name, "inspect");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    auto const inspect_position{source.find("(function inspect std::uint32_t")};
    auto const clear_position{source.find("(function clear void")};
    EXPECT_NE(inspect_position, std::string::npos);
    EXPECT_NE(clear_position, std::string::npos);
    EXPECT_LT(inspect_position, clear_position);
    EXPECT_NE(source.find(":definition-in-source true"), std::string::npos);
    EXPECT_NE(source.find("(parameter index std::uint32_t :default \"0\")"), std::string::npos);
    EXPECT_NE(source.find("; Keep the SoA declaration note."), std::string::npos);
    EXPECT_NE(source.find("; Keep the values SoA member note."), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    schema = reloaded.soa_schema(reloaded_declaration);
    ASSERT_EQ(schema->functions.size(), 2U);
    EXPECT_EQ(schema->functions[0].name, "inspect");
    ASSERT_EQ(schema->functions[0].parameters.size(), 1U);
    EXPECT_EQ(schema->functions[0].parameters[0].default_value, "0");
    EXPECT_TRUE(schema->functions[0].is_const);
    EXPECT_TRUE(schema->functions[0].definition_in_source);
    EXPECT_EQ(schema->functions[1].body_lines, std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(schema->functions[1].is_inline);

    replacement = *schema;
    std::swap(replacement.functions[0], replacement.functions[1]);
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->functions[0].name, "clear");
    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->functions[0].name, "inspect");
    ASSERT_TRUE(reloaded.redo().value());
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->functions[0].name, "clear");

    replacement = *reloaded.soa_schema(reloaded_declaration);
    replacement.functions.erase(replacement.functions.begin() + 1);
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    schema = final_document.soa_schema(final_declaration);
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions.front().name, "clear");
    EXPECT_EQ(schema->functions.front().body_lines, std::vector<std::string>{"values.clear();"});
    auto const module_source{
        std::ranges::find_if(final_document.source_files(), [](auto const& file) {
            return file.path.filename() == "modules.lispb";
        })};
    ASSERT_NE(module_source, final_document.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the custom function note."), std::string::npos);
}

TEST(EditableSchemaDocument, AuthorsOrdersAndEditsSoaFunctionParameters) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const original_revision{document.revision()};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().parameters.front().default_value = "  ";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().parameters.front().default_value,
              "0");

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().parameters.push_back(codegen::ParameterSchema{
        .type = codegen::TypeRef{"std::uint16_t"},
        .name = "limit",
        .default_value = std::nullopt,
    });
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);

    replacement = *document.soa_schema(declaration);
    auto& parameters{replacement.functions.front().parameters};
    parameters.front().type = codegen::TypeRef{"std::uint16_t"};
    parameters.front().default_value = "7";
    parameters.push_back(codegen::ParameterSchema{
        .type = codegen::TypeRef{"std::uint32_t"},
        .name = "limit",
        .default_value = "32",
    });
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().parameters.size(), 1U);
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().parameters.size(), 2U);

    replacement = *document.soa_schema(declaration);
    auto copy{replacement.functions.front().parameters.back()};
    copy.name = "limit_copy";
    replacement.functions.front().parameters.push_back(std::move(copy));
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    replacement = *document.soa_schema(declaration);
    auto& reordered_parameters{replacement.functions.front().parameters};
    std::rotate(
        reordered_parameters.begin(), reordered_parameters.begin() + 2, reordered_parameters.end());
    reordered_parameters.erase(reordered_parameters.begin() + 2);
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const* schema{document.soa_schema(declaration)};
    ASSERT_EQ(schema->functions.front().parameters.size(), 2U);
    EXPECT_EQ(schema->functions.front().parameters[0].name, "limit_copy");
    EXPECT_EQ(schema->functions.front().parameters[1].name, "count");
    EXPECT_EQ(schema->functions.front().parameters[1].type.name, "std::uint16_t");
    EXPECT_EQ(schema->functions.front().parameters[1].default_value, "7");

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    auto const copy_position{source.find("(parameter limit_copy std::uint32_t :default \"32\")")};
    auto const comment_position{source.find("; Keep the count parameter note.")};
    auto const count_position{source.find("(parameter count std::uint16_t :default \"7\")")};
    ASSERT_NE(copy_position, std::string::npos);
    ASSERT_NE(comment_position, std::string::npos);
    ASSERT_NE(count_position, std::string::npos);
    EXPECT_LT(copy_position, comment_position);
    EXPECT_LT(comment_position, count_position);
    EXPECT_NE(source.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(source.find(":body (\"values.clear();\")"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    schema = reloaded.soa_schema(reloaded_declaration);
    ASSERT_EQ(schema->functions.front().parameters.size(), 2U);
    EXPECT_EQ(schema->functions.front().parameters[0].name, "limit_copy");
    EXPECT_EQ(schema->functions.front().parameters[0].default_value, "32");
    EXPECT_EQ(schema->functions.front().parameters[1].name, "count");
    EXPECT_EQ(schema->functions.front().parameters[1].type.name, "std::uint16_t");
    EXPECT_EQ(schema->functions.front().parameters[1].default_value, "7");
}

TEST(EditableSchemaDocument, AuthorsOrdersAndEditsSoaFunctionBodiesAndDependencies) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const original_revision{document.revision()};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().dependencies = {"missing"};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_TRUE(document.soa_schema(declaration)->functions.front().dependencies.empty());

    replacement = *document.soa_schema(declaration);
    auto& function{replacement.functions.front()};
    function.body_lines.push_back("values.shrink_to_fit();");
    function.dependencies = {"existing", "helper"};
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().body_lines,
              std::vector<std::string>{"values.clear();"});
    EXPECT_TRUE(document.soa_schema(declaration)->functions.front().dependencies.empty());
    ASSERT_TRUE(document.redo().value());

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().body_lines.push_back(
        replacement.functions.front().body_lines.back());
    replacement.functions.front().dependencies.push_back(
        replacement.functions.front().dependencies.back());
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    replacement = *document.soa_schema(declaration);
    auto& body_lines{replacement.functions.front().body_lines};
    auto& dependencies{replacement.functions.front().dependencies};
    std::rotate(body_lines.begin(), body_lines.begin() + 1, body_lines.end());
    body_lines.erase(body_lines.begin() + 1);
    std::rotate(dependencies.begin(), dependencies.begin() + 1, dependencies.end());
    dependencies.erase(dependencies.begin() + 1);
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().body_lines.erase(
        replacement.functions.front().body_lines.begin() + 1);
    replacement.functions.front().dependencies.erase(
        replacement.functions.front().dependencies.begin() + 1);
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const* schema{document.soa_schema(declaration)};
    EXPECT_EQ(schema->functions.front().body_lines,
              std::vector<std::string>{"values.shrink_to_fit();"});
    EXPECT_EQ(schema->functions.front().dependencies, std::vector<std::string>{"helper"});

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find(":body (\"values.shrink_to_fit();\")"), std::string::npos);
    EXPECT_NE(source.find(":dependencies (\"helper\")"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(source.find("; Keep the count parameter note."), std::string::npos);
    EXPECT_NE(source.find("(parameter count std::uint32_t :default \"0\")"), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    schema = reloaded.soa_schema(reloaded_declaration);
    EXPECT_EQ(schema->functions.front().body_lines,
              std::vector<std::string>{"values.shrink_to_fit();"});
    EXPECT_EQ(schema->functions.front().dependencies, std::vector<std::string>{"helper"});
    EXPECT_EQ(schema->functions.front().parameters.front().name, "count");
}

TEST(EditableSchemaDocument, PreservesSoaFunctionBodyAndDependencyRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(R"(      :body ("values.clear();"))",
                              R"(      :body (
        ; Keep the first body note.
        "values.clear();" ; first body trailing note
        ; Keep the second body note.
        "flags.clear();" ; second body trailing note
      )
      :dependencies (
        ; Keep the existing dependency note.
        "existing" ; existing dependency trailing note
        ; Keep the helper dependency note.
        "helper" ; helper dependency trailing note
      ))");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().body_lines = {
        "flags.clear();", "values.shrink_to_fit();", "values.clear();"};
    replacement.functions.front().dependencies = {"helper", "existing"};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& reordered{preview->front().updated};
    auto const second_body_note{reordered.find("; Keep the second body note.")};
    auto const flags_row{reordered.find("\"flags.clear();\"")};
    auto const inserted_body_row{reordered.find("\"values.shrink_to_fit();\"")};
    auto const first_body_note{reordered.find("; Keep the first body note.")};
    auto const values_row{reordered.find("\"values.clear();\"")};
    auto const helper_note{reordered.find("; Keep the helper dependency note.")};
    auto const helper_row{reordered.find("\"helper\"")};
    auto const existing_note{reordered.find("; Keep the existing dependency note.")};
    auto const existing_row{reordered.find("\"existing\"")};
    ASSERT_NE(second_body_note, std::string::npos);
    ASSERT_NE(flags_row, std::string::npos);
    ASSERT_NE(inserted_body_row, std::string::npos);
    ASSERT_NE(first_body_note, std::string::npos);
    ASSERT_NE(values_row, std::string::npos);
    ASSERT_NE(helper_note, std::string::npos);
    ASSERT_NE(helper_row, std::string::npos);
    ASSERT_NE(existing_note, std::string::npos);
    ASSERT_NE(existing_row, std::string::npos);
    EXPECT_LT(second_body_note, flags_row);
    EXPECT_LT(flags_row, inserted_body_row);
    EXPECT_LT(inserted_body_row, first_body_note);
    EXPECT_LT(first_body_note, values_row);
    EXPECT_LT(helper_note, helper_row);
    EXPECT_LT(helper_row, existing_note);
    EXPECT_LT(existing_note, existing_row);
    EXPECT_NE(reordered.find("; second body trailing note"), std::string::npos);
    EXPECT_NE(reordered.find("; first body trailing note"), std::string::npos);
    EXPECT_NE(reordered.find("; helper dependency trailing note"), std::string::npos);
    EXPECT_NE(reordered.find("; existing dependency trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    EXPECT_EQ(
        schema->functions.front().body_lines,
        (std::vector<std::string>{"flags.clear();", "values.shrink_to_fit();", "values.clear();"}));
    EXPECT_EQ(schema->functions.front().dependencies,
              (std::vector<std::string>{"helper", "existing"}));

    replacement = *schema;
    replacement.functions.front().body_lines.front() = "flags.reset();";
    replacement.functions.front().dependencies.erase(
        replacement.functions.front().dependencies.begin() + 1);
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_EQ(reloaded.soa_schema(reloaded_declaration)->functions.front().body_lines.front(),
              "flags.clear();");
    ASSERT_TRUE(reloaded.redo().value());

    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& edited{preview->front().updated};
    EXPECT_NE(edited.find("; Keep the second body note."), std::string::npos);
    EXPECT_NE(edited.find("\"flags.reset();\" ; second body trailing note"), std::string::npos);
    EXPECT_NE(edited.find("; Keep the helper dependency note."), std::string::npos);
    EXPECT_NE(edited.find("; helper dependency trailing note"), std::string::npos);
    EXPECT_EQ(edited.find("; Keep the existing dependency note."), std::string::npos);
    EXPECT_EQ(edited.find("; existing dependency trailing note"), std::string::npos);

    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    replacement = *final_document.soa_schema(final_declaration);
    replacement.functions.front().body_lines.erase(
        replacement.functions.front().body_lines.begin() + 2);
    replacement.functions.front().dependencies.front() = "existing";
    applied = final_document.apply(
        ReplaceSoa{.declaration = final_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = final_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& final_source{preview->front().updated};
    EXPECT_NE(final_source.find("; Keep the helper dependency note."), std::string::npos);
    EXPECT_NE(final_source.find("\"existing\" ; helper dependency trailing note"),
              std::string::npos);
    EXPECT_EQ(final_source.find("; Keep the first body note."), std::string::npos);
    EXPECT_EQ(final_source.find("; first body trailing note"), std::string::npos);
    EXPECT_NE(final_source.find("; Keep the second body note."), std::string::npos);
    EXPECT_NE(final_source.find("\"values.shrink_to_fit();\""), std::string::npos);

    saved = final_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto persisted{files.load()};
    auto const persisted_declaration{
        declaration_id(persisted, "authored_soa", "ExistingSoa", "authored")};
    schema = persisted.soa_schema(persisted_declaration);
    EXPECT_EQ(schema->functions.front().body_lines,
              (std::vector<std::string>{"flags.reset();", "values.shrink_to_fit();"}));
    EXPECT_EQ(schema->functions.front().dependencies, (std::vector<std::string>{"existing"}));
}

TEST(EditableSchemaDocument, AuthorsSoaFunctionAdvancedSignatureProperties) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto const original_revision{document.revision()};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().template_parameters = " \t";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().requires_clause = "  ";
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);

    replacement = *document.soa_schema(declaration);
    replacement.functions.front().trailing_return_type = codegen::TypeRef{"std::uint16_t"};
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(document.revision(), original_revision);

    replacement = *document.soa_schema(declaration);
    auto& function{replacement.functions.front()};
    function.return_type = codegen::TypeRef{"auto"};
    function.trailing_return_type = codegen::TypeRef{"std::uint16_t"};
    function.template_parameters = "typename T";
    function.requires_clause = "sizeof(T) > 0";
    applied =
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().return_type.name, "void");
    EXPECT_FALSE(
        document.soa_schema(declaration)->functions.front().trailing_return_type.has_value());
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    EXPECT_NE(source.find("(function clear auto"), std::string::npos);
    EXPECT_NE(source.find(":trailing-return-type std::uint16_t"), std::string::npos);
    EXPECT_NE(source.find(":template-parameters \"typename T\""), std::string::npos);
    EXPECT_NE(source.find(":requires \"sizeof(T) > 0\""), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(source.find("; Keep the count parameter note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_TRUE(schema->functions.front().trailing_return_type.has_value());
    EXPECT_EQ(schema->functions.front().return_type.name, "auto");
    EXPECT_EQ(schema->functions.front().trailing_return_type->name, "std::uint16_t");
    EXPECT_EQ(schema->functions.front().template_parameters, "typename T");
    EXPECT_EQ(schema->functions.front().requires_clause, "sizeof(T) > 0");

    replacement = *schema;
    auto& reloaded_function{replacement.functions.front()};
    reloaded_function.return_type = *reloaded_function.trailing_return_type;
    reloaded_function.trailing_return_type.reset();
    reloaded_function.template_parameters.reset();
    reloaded_function.requires_clause.reset();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    schema = final_document.soa_schema(final_declaration);
    EXPECT_EQ(schema->functions.front().return_type.name, "std::uint16_t");
    EXPECT_FALSE(schema->functions.front().trailing_return_type.has_value());
    EXPECT_FALSE(schema->functions.front().template_parameters.has_value());
    EXPECT_FALSE(schema->functions.front().requires_clause.has_value());
}

TEST(EditableSchemaDocument, PreservesSoaFunctionAndParameterRowsDuringRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().name = "clear_items";
    replacement.functions.front().parameters.front().name = "element_count";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().name, "clear");
    EXPECT_EQ(document.soa_schema(declaration)->functions.front().parameters.front().name, "count");
    ASSERT_TRUE(document.redo().value());

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    auto const function_comment{source.find("; Keep the custom function note.")};
    auto const body{source.find(":body (\"values.clear();\")")};
    auto const parameter_comment{source.find("; Keep the count parameter note.")};
    auto const parameter{source.find("(parameter element_count std::uint32_t :default \"0\")")};
    EXPECT_NE(source.find("(function clear_items void"), std::string::npos);
    EXPECT_EQ(source.find("(function clear void"), std::string::npos);
    ASSERT_NE(function_comment, std::string::npos);
    ASSERT_NE(body, std::string::npos);
    ASSERT_NE(parameter_comment, std::string::npos);
    ASSERT_NE(parameter, std::string::npos);
    EXPECT_LT(function_comment, body);
    EXPECT_LT(body, parameter_comment);
    EXPECT_LT(parameter_comment, parameter);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    EXPECT_EQ(schema->functions.front().name, "clear_items");
    EXPECT_EQ(schema->functions.front().parameters.front().name, "element_count");
    EXPECT_EQ(schema->functions.front().body_lines, std::vector<std::string>{"values.clear();"});
    auto const module_source{
        std::ranges::find_if(reloaded.source_files(), [](auto const& source_file) {
            return source_file.path.filename() == "modules.lispb";
        })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the count parameter note."), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesAndLocallyEditsRawSoaFunctionBodies) {
    TemporarySchema files;
    files.replace_module_text(":body (\"values.clear();\")", ":body #cpp{values.clear();}cpp#");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.functions.front().name = "clear_items";
    replacement.functions.front().parameters.front().name = "element_count";
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(":body #cpp{values.clear();}cpp#"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the count parameter note."), std::string::npos);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    replacement = *reloaded.soa_schema(reloaded_declaration);
    replacement.functions.front().body_lines = {"values.reserve(element_count);"};
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(replacement)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    EXPECT_EQ(updated.find("#cpp{"), std::string::npos);
    EXPECT_NE(updated.find(":body (\"values.reserve(element_count);\")"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(updated.find("; Keep the count parameter note."), std::string::npos);
    EXPECT_NE(updated.find("(parameter element_count std::uint32_t :default \"0\")"),
              std::string::npos);

    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{final_document.soa_schema(final_declaration)};
    EXPECT_EQ(schema->functions.front().name, "clear_items");
    EXPECT_EQ(schema->functions.front().parameters.front().name, "element_count");
    EXPECT_EQ(schema->functions.front().body_lines,
              std::vector<std::string>{"values.reserve(element_count);"});
}

TEST(EditableSchemaDocument, PreservesSoaMembersAndAdvancedFormsForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto replacement{*document.soa_schema(declaration)};
    replacement.export_specifier = "SOA_API";
    auto values{replacement.members[0]};
    auto flags{replacement.members[1]};
    values.type.name = "std::uint16_t";
    flags.kind = codegen::SoaMemberKind::nested;
    flags.type.name = "std::uint32_t";
    flags.fixed_schema = "FixedFlags";
    flags.nested_schema = "NestedFlags";
    auto duplicate{flags};
    duplicate.name = "flags_copy";
    duplicate.type.name = "std::uint8_t";
    replacement.members = {flags, duplicate, values};

    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& source{preview->front().updated};
    auto const flags_comment{source.find("; Keep the flags SoA member note.")};
    auto const flags_position{source.find("(member flags ")};
    auto const copy_position{source.find("(member flags_copy ")};
    auto const values_comment{source.find("; Keep the values SoA member note.")};
    auto const values_position{source.find("(member values ")};
    auto const function_position{source.find("(function clear void")};
    ASSERT_NE(flags_comment, std::string::npos);
    ASSERT_NE(flags_position, std::string::npos);
    ASSERT_NE(copy_position, std::string::npos);
    ASSERT_NE(values_comment, std::string::npos);
    ASSERT_NE(values_position, std::string::npos);
    ASSERT_NE(function_position, std::string::npos);
    EXPECT_LT(flags_comment, flags_position);
    EXPECT_LT(flags_position, copy_position);
    EXPECT_LT(copy_position, values_comment);
    EXPECT_LT(values_comment, values_position);
    EXPECT_LT(values_position, function_position);
    EXPECT_NE(source.find("(member flags nested std::uint32_t"), std::string::npos);
    EXPECT_NE(source.find("(member flags_copy nested std::uint8_t"), std::string::npos);
    EXPECT_NE(source.find(":fixed-schema FixedFlags"), std::string::npos);
    EXPECT_NE(source.find(":nested-schema NestedFlags"), std::string::npos);
    EXPECT_NE(source.find("(member values array   std::uint16_t)"), std::string::npos);
    EXPECT_NE(source.find("; SoA values trailing note"), std::string::npos);
    EXPECT_NE(source.find("; Keep the SoA declaration note"), std::string::npos);
    EXPECT_NE(source.find("; Keep the custom function note"), std::string::npos);
    EXPECT_NE(source.find(":body (\"values.clear();\")"), std::string::npos);
    EXPECT_NE(source.find(":noexcept true"), std::string::npos);
    EXPECT_NE(source.find(":export-specifier SOA_API"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto deletion{*document.soa_schema(declaration)};
    std::erase_if(deletion.members, [](auto const& member) { return member.name == "flags"; });
    applied = document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(member flags "), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Keep the flags SoA member note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member flags_copy "), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the values SoA member note."),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(member flags "), std::string::npos);
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->export_specifier, "SOA_API");
    ASSERT_EQ(schema->members.size(), 2U);
    EXPECT_EQ(schema->members[0].name, "flags_copy");
    EXPECT_EQ(schema->members[0].kind, codegen::SoaMemberKind::nested);
    EXPECT_EQ(schema->members[0].type.name, "std::uint8_t");
    EXPECT_EQ(schema->members[0].fixed_schema, "FixedFlags");
    EXPECT_EQ(schema->members[0].nested_schema, "NestedFlags");
    EXPECT_EQ(schema->members[1].name, "values");
    EXPECT_EQ(schema->members[1].type.name, "std::uint16_t");
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions[0].body_lines, std::vector<std::string>{"values.clear();"});
}

TEST(EditableSchemaDocument, PreservesSoaMemberRowDuringDirectRename) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto replacement{*document.soa_schema(declaration)};
    replacement.members[0].name = "payload";

    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const member_comment{updated.find("; Keep the values SoA member note.")};
    auto const member_row{updated.find("(member payload array   std::uint32_t)")};
    auto const function_row{updated.find("(function clear void")};
    ASSERT_NE(member_comment, std::string::npos);
    ASSERT_NE(member_row, std::string::npos) << updated;
    ASSERT_NE(function_row, std::string::npos);
    EXPECT_LT(member_comment, member_row);
    EXPECT_LT(member_row, function_row);
    EXPECT_NE(updated.find("; SoA values trailing note"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the custom function note."), std::string::npos);
    EXPECT_NE(updated.find(":body (\"values.clear();\")"), std::string::npos);
    EXPECT_NE(updated.find("; Keep the count parameter note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    ASSERT_EQ(schema->members.size(), 2U);
    EXPECT_EQ(schema->members[0].name, "payload");
    EXPECT_EQ(schema->members[0].type.name, "std::uint32_t");
    ASSERT_EQ(schema->functions.size(), 1U);
    EXPECT_EQ(schema->functions[0].body_lines, std::vector<std::string>{"values.clear();"});

    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the values SoA member note."), std::string::npos);
    EXPECT_NE(module_source->text.find("(member payload array   std::uint32_t)"),
              std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the custom function note."), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesSoaUsingDeclarationRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(R"(    :using-declarations ("Base::reset"))",
                              R"(    :using-declarations (
      ; Keep the reset using note.
      "Base::reset" ; reset using trailing note
      ; Keep the copy using note.
      "Base::copy" ; copy using trailing note
    ))");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    replacement.using_declarations = {"Base::copy", "Base::reserve", "Base::reset"};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const copy_comment{updated.find("; Keep the copy using note.")};
    auto const copy_row{updated.find("\"Base::copy\" ; copy using trailing note")};
    auto const reserve_row{updated.find("\"Base::reserve\"")};
    auto const reset_comment{updated.find("; Keep the reset using note.")};
    auto const reset_row{updated.find("\"Base::reset\" ; reset using trailing note")};
    ASSERT_NE(copy_comment, std::string::npos);
    ASSERT_NE(copy_row, std::string::npos);
    ASSERT_NE(reserve_row, std::string::npos);
    ASSERT_NE(reset_comment, std::string::npos);
    ASSERT_NE(reset_row, std::string::npos);
    EXPECT_LT(copy_comment, copy_row);
    EXPECT_LT(copy_row, reserve_row);
    EXPECT_LT(reserve_row, reset_comment);
    EXPECT_LT(reset_comment, reset_row);
    EXPECT_NE(updated.find("; Keep the custom function note."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto edited_document{files.load()};
    auto const edited_declaration{
        declaration_id(edited_document, "authored_soa", "ExistingSoa", "authored")};
    auto edited{*edited_document.soa_schema(edited_declaration)};
    edited.using_declarations[0] = "Base::move";
    applied = edited_document.apply(
        ReplaceSoa{.declaration = edited_declaration, .schema = std::move(edited)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = edited_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& edited_source{preview->front().updated};
    EXPECT_NE(edited_source.find("; Keep the copy using note."), std::string::npos);
    EXPECT_NE(edited_source.find("\"Base::move\" ; copy using trailing note"), std::string::npos);
    ASSERT_TRUE(edited_document.undo().value());
    preview = edited_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(edited_document.redo().value());
    saved = edited_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_declaration{
        declaration_id(deleting_document, "authored_soa", "ExistingSoa", "authored")};
    auto deleting{*deleting_document.soa_schema(deleting_declaration)};
    std::erase(deleting.using_declarations, "Base::reset");
    applied = deleting_document.apply(
        ReplaceSoa{.declaration = deleting_declaration, .schema = std::move(deleting)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& deleting_source{preview->front().updated};
    EXPECT_EQ(deleting_source.find("; Keep the reset using note."), std::string::npos);
    EXPECT_EQ(deleting_source.find("; reset using trailing note"), std::string::npos);
    EXPECT_NE(deleting_source.find("; Keep the copy using note."), std::string::npos);
    EXPECT_NE(deleting_source.find("\"Base::move\""), std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->using_declarations,
              (std::vector<std::string>{"Base::move", "Base::reserve"}));
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the copy using note."), std::string::npos);
    EXPECT_EQ(module_source->text.find("; Keep the reset using note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the custom function note."), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesSoaMaskDimensionRowsDuringEdits) {
    TemporarySchema files;
    files.replace_module_text(
        R"(    :using-declarations ("Base::reset")
    ; Keep the values SoA member note.
    (member values array   std::uint32_t) ; SoA values trailing note
    ; Keep the flags SoA member note.)",
        R"(    :using-declarations ("Base::reset")
    :field-mask-name ExistingSoaFieldMask
    :field-enum-name ExistingSoaField
    ; Keep the values SoA member note.
    (member values array   std::uint32_t
      :mask-field true
      :mask-dimensions (
        ; Keep the lane dimension note.
        (lane   "4") ; lane dimension trailing note
        ; Keep the batch dimension note.
        (batch "8") ; batch dimension trailing note
      )) ; SoA values trailing note
    (member field_mask array ExistingSoaFieldMask)
    ; Keep the flags SoA member note.)");
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};

    auto replacement{*document.soa_schema(declaration)};
    auto batch{replacement.members[0].mask_dimensions[1]};
    batch.extent = "16";
    auto lane{replacement.members[0].mask_dimensions[0]};
    auto tile{codegen::SoaMaskDimensionSchema{.index_name = "tile", .extent = "32"}};
    replacement.members[0].mask_dimensions = {batch, tile, lane};
    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const batch_comment{updated.find("; Keep the batch dimension note.")};
    auto const batch_row{updated.find("(batch \"16\") ; batch dimension trailing note")};
    auto const tile_row{updated.find("(tile \"32\")")};
    auto const lane_comment{updated.find("; Keep the lane dimension note.")};
    auto const lane_row{updated.find("(lane   \"4\") ; lane dimension trailing note")};
    ASSERT_NE(batch_comment, std::string::npos);
    ASSERT_NE(batch_row, std::string::npos);
    ASSERT_NE(tile_row, std::string::npos);
    ASSERT_NE(lane_comment, std::string::npos);
    ASSERT_NE(lane_row, std::string::npos);
    EXPECT_LT(batch_comment, batch_row);
    EXPECT_LT(batch_row, tile_row);
    EXPECT_LT(tile_row, lane_comment);
    EXPECT_LT(lane_comment, lane_row);
    EXPECT_NE(updated.find("; Keep the values SoA member note."), std::string::npos);
    EXPECT_NE(updated.find("; SoA values trailing note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto renamed_document{files.load()};
    auto const renamed_declaration{
        declaration_id(renamed_document, "authored_soa", "ExistingSoa", "authored")};
    auto renamed{*renamed_document.soa_schema(renamed_declaration)};
    renamed.members[0].mask_dimensions[0].index_name = "group";
    applied = renamed_document.apply(
        ReplaceSoa{.declaration = renamed_declaration, .schema = std::move(renamed)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& renamed_source{preview->front().updated};
    EXPECT_NE(renamed_source.find("; Keep the batch dimension note."), std::string::npos);
    EXPECT_NE(renamed_source.find("(group \"16\") ; batch dimension trailing note"),
              std::string::npos);
    ASSERT_TRUE(renamed_document.undo().value());
    preview = renamed_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(renamed_document.redo().value());
    saved = renamed_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto deleting_document{files.load()};
    auto const deleting_declaration{
        declaration_id(deleting_document, "authored_soa", "ExistingSoa", "authored")};
    auto deleting{*deleting_document.soa_schema(deleting_declaration)};
    std::erase_if(deleting.members[0].mask_dimensions,
                  [](auto const& dimension) { return dimension.index_name == "lane"; });
    applied = deleting_document.apply(
        ReplaceSoa{.declaration = deleting_declaration, .schema = std::move(deleting)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = deleting_document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    auto const& deleting_source{preview->front().updated};
    EXPECT_EQ(deleting_source.find("; Keep the lane dimension note."), std::string::npos);
    EXPECT_EQ(deleting_source.find("; lane dimension trailing note"), std::string::npos);
    EXPECT_NE(deleting_source.find("; Keep the batch dimension note."), std::string::npos);
    EXPECT_NE(deleting_source.find("(group \"16\")"), std::string::npos);
    ASSERT_TRUE(deleting_document.undo().value());
    ASSERT_TRUE(deleting_document.redo().value());
    saved = deleting_document.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(schema, nullptr);
    ASSERT_EQ(schema->members[0].mask_dimensions.size(), 2U);
    EXPECT_EQ(schema->members[0].mask_dimensions[0].index_name, "group");
    EXPECT_EQ(schema->members[0].mask_dimensions[0].extent, "16");
    EXPECT_EQ(schema->members[0].mask_dimensions[1].index_name, "tile");
    EXPECT_EQ(schema->members[0].mask_dimensions[1].extent, "32");
    auto const module_source{std::ranges::find_if(reloaded.source_files(), [](auto const& source) {
        return source.path.filename() == "modules.lispb";
    })};
    ASSERT_NE(module_source, reloaded.source_files().end());
    EXPECT_NE(module_source->text.find("; Keep the batch dimension note."), std::string::npos);
    EXPECT_EQ(module_source->text.find("; Keep the lane dimension note."), std::string::npos);
    EXPECT_NE(module_source->text.find("; Keep the values SoA member note."), std::string::npos);
}

TEST(EditableSchemaDocument, EnablesAndDisablesSoaFieldMaskAsCoordinatedEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const declaration{declaration_id(document, "authored_soa", "ExistingSoa", "authored")};
    auto enabled{*document.soa_schema(declaration)};
    enabled.field_mask_name = "ExistingSoaFieldMask";
    enabled.field_enum_name = "ExistingSoaField";
    enabled.members[0].mask_field = true;
    enabled.members[0].mask_dimensions.push_back(
        codegen::SoaMaskDimensionSchema{.index_name = "lane", .extent = "4"});
    enabled.members[0].mask_dimensions.push_back(
        codegen::SoaMaskDimensionSchema{.index_name = "batch", .extent = "8"});
    enabled.members.push_back(codegen::SoaMemberSchema{
        .name = "field_mask",
        .kind = codegen::SoaMemberKind::array,
        .type =
            codegen::TypeRef{.name = "ExistingSoaFieldMask", .suffix = {}, .nested = std::nullopt},
        .fixed_schema = std::nullopt,
        .nested_schema = std::nullopt,
        .mask_field = false,
        .mask_dimensions = {},
        .relationship = std::nullopt});

    auto applied{
        document.apply(ReplaceSoa{.declaration = declaration, .schema = std::move(enabled)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find(":field-mask-name ExistingSoaFieldMask"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":field-enum-name ExistingSoaField"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find(":mask-field true"), std::string::npos);
    EXPECT_NE(preview->front().updated.find(":mask-dimensions ((lane \"4\") (batch \"8\"))"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("(member field_mask array ExistingSoaFieldMask)"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.soa_schema(declaration)->field_mask_name.has_value());
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.soa_schema(declaration)->field_mask_name.has_value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_declaration{
        declaration_id(reloaded, "authored_soa", "ExistingSoa", "authored")};
    auto const* reloaded_schema{reloaded.soa_schema(reloaded_declaration)};
    ASSERT_NE(reloaded_schema, nullptr);
    EXPECT_EQ(reloaded_schema->field_mask_name, "ExistingSoaFieldMask");
    EXPECT_EQ(reloaded_schema->field_enum_name, "ExistingSoaField");
    ASSERT_EQ(reloaded_schema->members.size(), 3U);
    EXPECT_TRUE(reloaded_schema->members[0].mask_field);
    ASSERT_EQ(reloaded_schema->members[0].mask_dimensions.size(), 2U);
    EXPECT_EQ(reloaded_schema->members[0].mask_dimensions[0].index_name, "lane");
    EXPECT_EQ(reloaded_schema->members[0].mask_dimensions[0].extent, "4");
    EXPECT_EQ(reloaded_schema->members[0].mask_dimensions[1].index_name, "batch");
    EXPECT_EQ(reloaded_schema->members[0].mask_dimensions[1].extent, "8");

    auto disabled{*reloaded_schema};
    std::erase_if(disabled.members,
                  [](auto const& member) { return member.type.name == "ExistingSoaFieldMask"; });
    for (auto& member : disabled.members) {
        member.mask_field = false;
        member.mask_dimensions.clear();
    }
    disabled.field_mask_name.reset();
    disabled.field_enum_name.reset();
    applied = reloaded.apply(
        ReplaceSoa{.declaration = reloaded_declaration, .schema = std::move(disabled)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = reloaded.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find(":field-mask-name"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":field-enum-name"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":mask-field"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find(":mask-dimensions"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("(member field_mask "), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the custom function note"), std::string::npos);

    ASSERT_TRUE(reloaded.undo().value());
    EXPECT_TRUE(reloaded.soa_schema(reloaded_declaration)->field_mask_name.has_value());
    ASSERT_TRUE(reloaded.redo().value());
    EXPECT_FALSE(reloaded.soa_schema(reloaded_declaration)->field_mask_name.has_value());
    saved = reloaded.save();
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto final_document{files.load()};
    auto const final_declaration{
        declaration_id(final_document, "authored_soa", "ExistingSoa", "authored")};
    auto const* final_schema{final_document.soa_schema(final_declaration)};
    ASSERT_NE(final_schema, nullptr);
    EXPECT_FALSE(final_schema->field_mask_name.has_value());
    EXPECT_FALSE(final_schema->field_enum_name.has_value());
    EXPECT_EQ(final_schema->members.size(), 2U);
    EXPECT_TRUE(std::ranges::none_of(final_schema->members,
                                     [](auto const& member) { return member.mask_field; }));
}

TEST(EditableDocument, SourceBackedMixedModuleMovesAndRestores) {
    TemporarySchema files;
    files.write_source("mixed.lispb", R"(
; Keep the module note.
(module mixed_one
  :header "MixedOne.h"
  :namespace mixed
  (enum State std::uint8_t
    (value Alive)
    (value Dead))
  ; Keep the scalar note.
  (integer-scalar Health :signed false :minimum 0 :maximum 100 :bit-width auto)
  (record Snapshot
    (member state State)
    (member health Health)))

(module mixed_two
  :header "MixedTwo.h"
  :namespace other
  (enum State std::uint8_t
    (value Ready)))
)");
    auto document{files.load_with_module_source("mixed.lispb")};
    auto const first_module{document.manifest().modules.size() - 2};
    auto const second_module{first_module + 1};
    auto const health{declaration_id(document, "mixed_one", "Health", "mixed")};
    auto const state{declaration_id(document, "mixed_one", "State", "mixed")};
    auto const snapshot{declaration_id(document, "mixed_one", "Snapshot", "mixed")};
    auto const revision{document.revision()};

    auto moved{document.apply(MoveDeclaration{
        .declaration = health, .module_index = second_module, .insertion_index = 0})};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);
    EXPECT_EQ(document.declaration(health)->id, health);
    EXPECT_EQ(document.declaration(health)->declaration_index, 0U);
    EXPECT_EQ(document.declaration(health)->identity.module_name, "mixed_two");
    EXPECT_EQ(document.declaration(health)->identity.namespace_name, "other");
    EXPECT_TRUE(document.types().find_declared("mixed_two", "Health").has_value());
    ASSERT_NE(document.record_schema(snapshot), nullptr);
    EXPECT_EQ(document.record_schema(snapshot)->members[1].type.name, "other::Health");
    auto const before_rejection{document.revision()};
    auto rejected{
        document.apply(MoveDeclaration{.declaration = state, .module_index = second_module})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(document.revision(), before_rejection);
    EXPECT_EQ(document.declaration(state)->identity.module_name, "mixed_one");
    EXPECT_TRUE(document.types().find_declared("mixed_one", "State").has_value());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("; Keep the scalar note."), std::string::npos);

    auto deleted{document.apply(DeleteModule{.module_index = first_module})};
    ASSERT_TRUE(deleted.has_value()) << deleted.error().message;
    ASSERT_TRUE(*deleted);
    EXPECT_EQ(document.declaration(state), nullptr);
    EXPECT_EQ(document.declaration(snapshot), nullptr);
    ASSERT_TRUE(document.undo().value());
    ASSERT_NE(document.declaration(state), nullptr);
    ASSERT_NE(document.declaration(snapshot), nullptr);
    EXPECT_EQ(document.declaration(state)->id, state);
    ASSERT_TRUE(document.redo().value());
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.revision(), revision + 6);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(document.declaration(health)->identity.module_name, "mixed_one");
    EXPECT_TRUE(document.declaration(health)->source.has_value());
}

TEST(EditableDocument, SourceBackedMixedModuleSaveAndReloadPreservesUnchangedText) {
    TemporarySchema files;
    files.write_source("mixed_save.lispb", R"(
; Keep this file note.
(module first
  :header "First.h"
  :namespace one
  ; Keep this declaration note.
  (enum State std::uint8_t
    (value Alive))
  (record Snapshot
    (member state State)))

(module second
  :header "Second.h"
  :namespace two
  (integer-scalar Health :signed false :minimum 0 :maximum 100 :bit-width auto))
)");
    auto document{files.load_with_module_source("mixed_save.lispb")};
    auto const state{declaration_id(document, "first", "State", "one")};
    auto const snapshot{declaration_id(document, "first", "Snapshot", "one")};
    auto const second{document.manifest().modules.size() - 1};
    auto moved{document.apply(MoveDeclaration{.declaration = state, .module_index = second})};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load_with_module_source("mixed_save.lispb")};
    EXPECT_TRUE(reloaded.types().find_declared("second", "State").has_value());
    auto const reloaded_snapshot{declaration_id(reloaded, "first", "Snapshot", "one")};
    ASSERT_NE(reloaded.record_schema(reloaded_snapshot), nullptr);
    EXPECT_EQ(reloaded.record_schema(reloaded_snapshot)->members.front().type.name, "two::State");
    auto const source{std::ranges::find_if(reloaded.source_files(), [](auto const& file) {
        return file.path.filename() == "mixed_save.lispb";
    })};
    ASSERT_NE(source, reloaded.source_files().end());
    EXPECT_NE(source->text.find("; Keep this file note."), std::string::npos);
    EXPECT_NE(source->text.find("; Keep this declaration note."), std::string::npos);
    EXPECT_EQ(document.declaration(state)->id, state);
    EXPECT_EQ(document.declaration(snapshot)->id, snapshot);
}

TEST(EditableDocument, NormalModuleTypedEditsShareOneDeclarationSequence) {
    TemporarySchema files;
    files.write_source("typed_mixed.lispb", R"(
(module mixed
  :header "Mixed.h"
  :namespace example
  (enum State std::uint8_t
    (value Alive))
  (record Snapshot
    (member state State)))
)");
    auto document{files.load_with_module_source("typed_mixed.lispb")};
    auto const module_index{document.manifest().modules.size() - 1};
    auto const state{declaration_id(document, "mixed", "State", "example")};
    auto const snapshot{declaration_id(document, "mixed", "Snapshot", "example")};

    auto changed{document.apply(SetEnumeratorDisplayName{
        .enum_declaration = state, .enumerator_name = "Alive", .display_name = "Living"})};
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    auto replacement{*document.record_schema(snapshot)};
    replacement.members.push_back({.name = "previous", .type = codegen::TypeRef{"State"}});
    changed = document.apply(ReplaceRecord{.declaration = snapshot, .schema = replacement});
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    ASSERT_EQ(document.record_schema(snapshot)->members.size(), 2U);

    auto const created{document.allocate_declaration_id()};
    changed = document.apply(CreateRecord{
        .declaration = created,
        .module_index = module_index,
        .schema = codegen::RecordSchema{.name = "History",
                                        .members = {{.name = "state",
                                                     .type = codegen::TypeRef{"State"}}}},
        .insertion_index = 1});
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    ASSERT_EQ(document.declaration(created)->declaration_index, 1U);
    EXPECT_EQ(document.declaration(snapshot)->declaration_index, 2U);
    changed = document.apply(DeleteRecord{created});
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    EXPECT_EQ(document.declaration(created), nullptr);
    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.declaration(created)->id, created);
    EXPECT_EQ(document.declaration(snapshot)->id, snapshot);
}

TEST(EditableDocument, PendingNormalModuleCreatesMixedDeclarationsAndReloads) {
    TemporarySchema files;
    auto document{files.load()};
    auto const module_index{document.manifest().modules.size()};
    auto created{document.apply(
        CreateModule{.source_file_index = 1,
                     .schema = codegen::NormalModuleSchema{
                         .settings = codegen::ModuleSettings{.name = "pending_mixed",
                                                             .header = "PendingMixed.h",
                                                             .namespace_name = "pending"}}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    auto const state{document.allocate_declaration_id()};
    created = document.apply(CreateEnum{
        .declaration = state,
        .module_index = module_index,
        .schema = codegen::EnumSchema{.name = "State",
                                      .underlying_type = codegen::TypeRef{"std::uint8_t"},
                                      .values = {{.name = "Alive"}}}});
    ASSERT_TRUE(created.has_value()) << created.error().message;
    auto const snapshot{document.allocate_declaration_id()};
    created = document.apply(
        CreateRecord{.declaration = snapshot,
                     .module_index = module_index,
                     .schema = codegen::RecordSchema{
                         .name = "Snapshot",
                         .members = {{.name = "state", .type = codegen::TypeRef{"State"}}}}});
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_EQ(document.declaration(snapshot)->declaration_index, 1U);
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_state{declaration_id(reloaded, "pending_mixed", "State", "pending")};
    auto const reloaded_snapshot{declaration_id(reloaded, "pending_mixed", "Snapshot", "pending")};
    EXPECT_NE(reloaded.enum_schema(reloaded_state), nullptr);
    EXPECT_NE(reloaded.record_schema(reloaded_snapshot), nullptr);
}

TEST(EditableDocument, PendingNormalModuleSerializesSpecializedDeclarations) {
    TemporarySchema files;
    auto document{files.load()};
    auto const first_index{document.manifest().modules.size()};
    auto created{document.apply(
        CreateModule{.source_file_index = 1,
                     .schema = codegen::NormalModuleSchema{
                         .settings = codegen::ModuleSettings{.name = "pending_specialized",
                                                             .header = "PendingSpecialized.h"}}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    created = document.apply(
        CreateModule{.source_file_index = 1,
                     .schema = codegen::NormalModuleSchema{
                         .settings = codegen::ModuleSettings{.name = "pending_vectors",
                                                             .header = "PendingVectors.h",
                                                             .source = "PendingVectors.cpp"}}});
    ASSERT_TRUE(created.has_value()) << created.error().message;

    auto add = [&](std::size_t const module_index, codegen::DeclarationSchema schema) {
        auto changed{
            document.apply(CreateDeclaration{.declaration = document.allocate_declaration_id(),
                                             .module_index = module_index,
                                             .schema = std::move(schema)})};
        ASSERT_TRUE(changed.has_value()) << changed.error().message;
    };
    add(first_index,
        codegen::StaticTableSchema{.name = "Lookup",
                                   .rows = {{"first"}},
                                   .columns = {{"value", codegen::TypeRef{"int32"}}}});
    add(first_index,
        codegen::FacadeSchema{
            .name = "Access",
            .target_type = codegen::TypeRef{"@helper"},
            .target_member_name = "target",
            .methods = {{.name = "read", .return_type = codegen::TypeRef{"void"}}}});
    add(first_index + 1,
        codegen::VectorSoaSchema{.name = "Vectors",
                                 .value_type = codegen::TypeRef{"float"},
                                 .components = {"xs", "ys"},
                                 .equivalent_type = codegen::TypeRef{"@helper"}});
    add(first_index + 1,
        codegen::HomogeneousLayoutSchema{
            .name = "Pairs",
            .components = {"xs", "ys"},
            .value_types = {{.type = codegen::TypeRef{"float"}, .suffix = "f"}}});
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto const reloaded{files.load()};
    auto const& table_module{std::get<codegen::NormalModuleSchema>(
        reloaded.manifest().modules.at(reloaded.manifest().modules.size() - 2))};
    auto const& vector_module{
        std::get<codegen::NormalModuleSchema>(reloaded.manifest().modules.back())};
    ASSERT_EQ(table_module.declarations.size(), 2U);
    ASSERT_EQ(vector_module.declarations.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<codegen::StaticTableSchema>(table_module.declarations[0]));
    EXPECT_TRUE(std::holds_alternative<codegen::FacadeSchema>(table_module.declarations[1]));
    EXPECT_TRUE(std::holds_alternative<codegen::VectorSoaSchema>(vector_module.declarations[0]));
    EXPECT_TRUE(
        std::holds_alternative<codegen::HomogeneousLayoutSchema>(vector_module.declarations[1]));

    auto edited{files.load()};
    auto table{std::get<codegen::StaticTableSchema>(table_module.declarations[0])};
    table.rows.push_back({"second"});
    table.groups.push_back(
        {.name = "row", .type = codegen::TypeRef{"@helper"}, .columns = {"value"}});
    auto const table_id{declaration_id(edited, "pending_specialized", "Lookup", "")};
    ASSERT_TRUE(edited.apply(ReplaceDeclaration{table_id, table}).has_value());
    auto facade{std::get<codegen::FacadeSchema>(table_module.declarations[1])};
    facade.reference_target = true;
    facade.methods.front().parameters.push_back(
        {.type = codegen::TypeRef{"@helper"}, .name = "input"});
    auto const facade_id{declaration_id(edited, "pending_specialized", "Access", "")};
    ASSERT_TRUE(edited.apply(ReplaceDeclaration{facade_id, facade}).has_value());
    auto layout{std::get<codegen::HomogeneousLayoutSchema>(vector_module.declarations[1])};
    layout.value_types.push_back({.type = codegen::TypeRef{"double"}, .suffix = "d"});
    auto const layout_id{declaration_id(edited, "pending_vectors", "Pairs", "")};
    ASSERT_TRUE(edited.apply(ReplaceDeclaration{layout_id, layout}).has_value());
    ASSERT_TRUE(edited.undo().value());
    ASSERT_TRUE(edited.redo().value());
    auto const consumer_id{edited.allocate_declaration_id()};
    ASSERT_TRUE(
        edited
            .apply(CreateDeclaration{
                .declaration = consumer_id,
                .module_index = first_index + 1,
                .schema =
                    codegen::RecordSchema{
                        .name = "Consumer",
                        .members = {{.name = "pairs", .type = codegen::TypeRef{"FPairsf"}}}}})
            .has_value());
    ASSERT_TRUE(edited.save().has_value());

    auto const saved_layout_id{declaration_id(edited, "pending_vectors", "Pairs", "")};
    EXPECT_FALSE(edited.apply(DeleteDeclaration{saved_layout_id}).has_value());
    auto renamed{edited.apply(RenameDeclaration{saved_layout_id, "Coordinates"})};
    ASSERT_TRUE(renamed.has_value()) << renamed.error().message;
    ASSERT_TRUE(edited.undo().value());
    ASSERT_TRUE(edited.redo().value());
    ASSERT_TRUE(edited.save().has_value());
    auto const final_document{files.load()};
    auto const& types{final_document.types()};
    auto const resolved_table{types.find_declared("pending_specialized", "Lookup")};
    ASSERT_TRUE(resolved_table.has_value());
    auto const& table_type{std::get<StaticTableType>(types.type(*resolved_table).definition)};
    EXPECT_EQ(table_type.rows.size(), 2U);
    EXPECT_EQ(table_type.columns.front().count, 2U);
    EXPECT_EQ(table_type.groups.front().result_type.type, *types.find_registered("helper"));
    auto const& facade_type{std::get<FacadeType>(
        types.type(*types.find_declared("pending_specialized", "Access")).definition)};
    EXPECT_TRUE(facade_type.reference_target);
    EXPECT_EQ(facade_type.methods.front().parameters.front().type.type,
              *types.find_registered("helper"));
    auto const layout_identity{
        final_document
            .declaration(declaration_id(final_document, "pending_vectors", "Coordinates", ""))
            ->identity};
    EXPECT_FALSE(types.find(layout_identity).has_value());
    EXPECT_EQ(types.types_for_declaration(layout_identity).size(), 2U);
    auto const storage{types.find_declared("pending_vectors", "FCoordinatesf")};
    ASSERT_TRUE(storage.has_value());
    auto const consumer{types.find_declared("pending_vectors", "Consumer")};
    ASSERT_TRUE(consumer.has_value());
    EXPECT_EQ(
        std::get<RecordType>(types.type(*consumer).definition).members.front().semantic_type.type,
        *storage);
}

TEST(EditableDocument, SourceBackedSpecialDeclarationsMoveWithCanonicalFallbacks) {
    TemporarySchema files;
    files.write_source("special_moves.lispb", R"(
(module source
  :header "Source.h"
  :source "Source.cpp"
  (vector-soa Vectors
    :value-type float
    :components (xs ys)
    :equivalent-type int32)
  (layout Pairs
    :components (xs ys)
    (value-type float f))
  (table Lookup
    (row first)
    (column value int32))
  (facade Access int32 target
    (method reset void)))

(module destination
  :header "Destination.h"
  :source "Destination.cpp")
)");
    auto document{files.load_with_module_source("special_moves.lispb")};
    auto const source_index{document.manifest().modules.size() - 2};
    auto const destination_index{source_index + 1};
    std::array declarations{
        declaration_id(document, "source", "Vectors", ""),
        declaration_id(document, "source", "Pairs", ""),
        declaration_id(document, "source", "Lookup", ""),
        declaration_id(document, "source", "Access", ""),
    };

    for (auto const id : declarations) {
        auto moved{
            document.apply(MoveDeclaration{.declaration = id, .module_index = destination_index})};
        ASSERT_TRUE(moved.has_value()) << moved.error().message;
        ASSERT_TRUE(*moved);
        EXPECT_EQ(document.declaration(id)->id, id);
    }
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    for (auto const name : {"Vectors", "Pairs", "Lookup", "Access"}) {
        EXPECT_NE(preview->front().updated.find(name), std::string::npos);
    }

    for (std::size_t index{}; index < declarations.size(); ++index) {
        ASSERT_TRUE(document.undo().value());
    }
    for (std::size_t index{}; index < declarations.size(); ++index) {
        ASSERT_TRUE(document.redo().value());
    }

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load_with_module_source("special_moves.lispb")};
    auto const& destination{
        std::get<codegen::NormalModuleSchema>(reloaded.manifest().modules.at(destination_index))};
    ASSERT_EQ(destination.declarations.size(), declarations.size());
    EXPECT_TRUE(std::holds_alternative<codegen::VectorSoaSchema>(destination.declarations[0]));
    EXPECT_TRUE(
        std::holds_alternative<codegen::HomogeneousLayoutSchema>(destination.declarations[1]));
    EXPECT_TRUE(std::holds_alternative<codegen::StaticTableSchema>(destination.declarations[2]));
    EXPECT_TRUE(std::holds_alternative<codegen::FacadeSchema>(destination.declarations[3]));
}

TEST(EditableDocument, HeterogeneousMovePreservesModuleComments) {
    TemporarySchema files;
    files.write_source("mixed_move.lispb", R"(
; Keep this outer comment.
(module scalars
  :header "Scalars.h"
  :namespace legacy
  ; Keep this declaration comment.
  (integer-scalar Health :signed false :minimum 0 :maximum 100 :bit-width auto))

(module enums
  :header "Enums.h"
  :namespace legacy
  (enum State std::uint8_t
    (value Alive)))
)");
    auto document{files.load_with_module_source("mixed_move.lispb")};
    auto const scalar_index{document.manifest().modules.size() - 2};
    auto const health{declaration_id(document, "scalars", "Health", "legacy")};
    auto const state{declaration_id(document, "enums", "State", "legacy")};

    auto preserved{
        document.apply(RenameDeclaration{.declaration = health, .new_name = "HealthValue"})};
    ASSERT_TRUE(preserved.has_value()) << preserved.error().message;
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(module scalars"), std::string::npos);

    auto moved{document.apply(MoveDeclaration{.declaration = state, .module_index = scalar_index})};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_NE(preview->front().updated.find("(module scalars"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep this declaration comment."), std::string::npos);
    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load_with_module_source("mixed_move.lispb")};
    auto const& module{
        std::get<codegen::NormalModuleSchema>(reloaded.manifest().modules.at(scalar_index))};
    ASSERT_EQ(module.declarations.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<codegen::IntegerScalarSchema>(module.declarations[0]));
    EXPECT_TRUE(std::holds_alternative<codegen::EnumSchema>(module.declarations[1]));
    EXPECT_EQ(document.declaration(state)->id, state);
}

TEST(EditableDocument, VectorDeclarationEditsPreserveModuleSource) {
    TemporarySchema files;
    files.write_source("vector_edit.lispb", R"(
; Keep this file comment.
(module vector_edits
  :header "LegacyVectors.h"
  :source "LegacyVectors.cpp"
  (vector-soa LegacyVectors
    :value-type float
    :components (xs ys)
    :equivalent-type int32))
)");
    auto document{files.load_with_module_source("vector_edit.lispb")};
    auto const declaration{declaration_id(document, "vector_edits", "LegacyVectors", "")};
    auto const& module{std::get<codegen::NormalModuleSchema>(document.manifest().modules.back())};
    auto replacement{std::get<codegen::VectorSoaSchema>(module.declarations.front())};
    replacement.components = {"xs", "zs"};

    auto changed{document.apply(
        ReplaceDeclaration{.declaration = declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    ASSERT_TRUE(*changed);
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_NE(preview->front().updated.find("(module vector_edits"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(vector-soa LegacyVectors"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep this file comment."), std::string::npos);

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.redo().value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load_with_module_source("vector_edit.lispb")};
    auto const& reloaded_module{
        std::get<codegen::NormalModuleSchema>(reloaded.manifest().modules.back())};
    auto const& vector{std::get<codegen::VectorSoaSchema>(reloaded_module.declarations.front())};
    EXPECT_EQ(vector.components, (std::vector<std::string>{"xs", "zs"}));
}

} // namespace
} // namespace lispb::schema
