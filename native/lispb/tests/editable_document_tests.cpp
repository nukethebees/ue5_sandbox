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
    ; Preserve the zero documentation during ordinary cell edits.
    (value Zero :value "0") ; zero trailing note
    ; Preserve the one documentation too.
    (value One :value   "1")))

(packed-value-module authored_packed
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
      (relation index_into authored::ExistingScalar))
    ; Keep the future segment note.
    (reserved future :bits   16)))

(scalar-module authored_scalars
  :header "AuthoredScalars.h"
  :namespace authored
  (integer-scalar ExistingScalar
    ; Keep the scalar domain note during ordinary edits.
    :signed false
    :minimum   0
    :maximum 1
    :bit-width auto
    (code Pending :value   2 :sentinel true) ; pending code note
    (code Invalid :value   3 :sentinel true))
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

(representation-module authored_representations
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

(record-module authored_records
  :header "AuthoredRecords.h"
  :namespace authored
  (record ExistingRecord
    ; Keep the record note.
    ; Keep the value member note.
    (member value   std::uint32_t) ; value member trailing note
    ; Keep the flag member note.
    (member flag std::uint8_t)))

(union-module authored_unions
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

(soa-module authored_soa
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
      :noexcept true))
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
    auto const pending_begin{updated.find("(code Pending")};
    ASSERT_NE(pending_begin, std::string::npos);
    EXPECT_NE(updated.find("(code Pending :value   4 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("(code Invalid :value   3 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("; pending code note"), std::string::npos);
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
    EXPECT_TRUE(reloaded.integer_scalar_schema(reloaded_scalar)->named_codes[1].sentinel);
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

TEST(EditableSchemaDocument, FallsBackToCanonicalScalarRenderingForStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const scalar{declaration_id(document, "authored_scalars", "ExistingScalar", "authored")};
    auto replacement{*document.integer_scalar_schema(scalar)};
    std::ranges::swap(replacement.named_codes[0], replacement.named_codes[1]);

    auto applied{document.apply(
        ReplaceIntegerScalar{.declaration = scalar, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    auto const& updated{preview->front().updated};
    auto const invalid{updated.find("(code Invalid")};
    auto const pending{updated.find("(code Pending")};
    ASSERT_NE(invalid, std::string::npos);
    ASSERT_NE(pending, std::string::npos);
    EXPECT_LT(invalid, pending);
    EXPECT_EQ(updated.find("; Keep the scalar domain note"), std::string::npos);
    EXPECT_EQ(updated.find("; pending code note"), std::string::npos);
}

TEST(EditableSchemaDocument, PreservesPackedFormattingForNonStructuralEdits) {
    TemporarySchema files;
    auto document{files.load()};
    auto const packed{declaration_id(document, "authored_packed", "ExistingPacked", "authored")};
    auto replacement{*document.packed_value_schema(packed)};
    replacement.invalid_value = 4'294'967'294U;
    replacement.export_specifier = "PACKED_API";
    replacement.byte_order = codegen::PackedByteOrder::big_endian;
    replacement.bit_order = codegen::PackedBitOrder::most_significant_first;
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments[1])};
    field.type.name = "std::uint32_t";
    field.bits = 7;
    field.range_helper = true;
    field.maximum_value = codegen::PackedIntegerValue{120};
    field.named_codes[0].value = codegen::PackedIntegerValue{127};
    field.relationship->kind = codegen::PackedFieldRelationKind::count_of;
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
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.packed_value_schema(packed)->byte_order,
              codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(document.packed_value_schema(packed)->bit_order,
              codegen::PackedBitOrder::most_significant_first);

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

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{files.load()};
    auto const reloaded_packed{
        declaration_id(reloaded, "authored_packed", "ExistingPacked", "authored")};
    auto const* schema{reloaded.packed_value_schema(reloaded_packed)};
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->invalid_value, 4'294'967'294U);
    EXPECT_EQ(schema->export_specifier, "PACKED_API");
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
    EXPECT_EQ(reloaded_field.relationship->kind, codegen::PackedFieldRelationKind::count_of);
    EXPECT_EQ(reloaded_field.relationship->target.name, "authored::OtherScalar");
    EXPECT_EQ(std::get<codegen::PackedReservedBitsSchema>(schema->segments[2]).bits, 17);
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
    std::get<codegen::PackedFieldSchema>(counter).named_codes[0].value = 254;
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
    EXPECT_NE(updated.find("(code Invalid :value   254 :sentinel true"), std::string::npos);
    EXPECT_NE(updated.find("; field code note"), std::string::npos);
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
    applied =
        document.apply(ReplacePackedValue{.declaration = packed, .schema = std::move(deletion)});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().updated.find("(field value"), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; Keep the value segment note."), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("; value segment trailing note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the counter segment note."), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Keep the future segment note."), std::string::npos);

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
    EXPECT_EQ(std::get<codegen::PackedFieldSchema>(schema->segments[1]).named_codes[0].value,
              codegen::PackedIntegerValue{254});

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
    enum_schema.underlying_type.name = "std::uint8_t";
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
    EXPECT_NE(preview->front().updated.find("(enum DesignedState std::uint8_t"), std::string::npos);
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
                                     codegen::PackedFieldRelationSchema{
                                         .kind = codegen::PackedFieldRelationKind::index_into,
                                         .target = codegen::TypeRef{"ExistingPacked"}}},
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
    EXPECT_EQ(created_index.relationship->kind, codegen::PackedFieldRelationKind::index_into);
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
    std::get<codegen::PackedFieldSchema>(relationship_edit.segments.front()).relationship->kind =
        codegen::PackedFieldRelationKind::count_of;
    auto relationship_replaced{document.apply(
        ReplacePackedValue{.declaration = created, .schema = std::move(relationship_edit)})};
    ASSERT_TRUE(relationship_replaced.has_value()) << relationship_replaced.error().message;
    ASSERT_TRUE(*relationship_replaced);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::PackedFieldRelationKind::count_of);
    auto undo_relationship{document.undo()};
    ASSERT_TRUE(undo_relationship.has_value());
    ASSERT_TRUE(*undo_relationship);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::PackedFieldRelationKind::index_into);
    auto redo_relationship{document.redo()};
    ASSERT_TRUE(redo_relationship.has_value());
    ASSERT_TRUE(*redo_relationship);
    EXPECT_EQ(std::get<PackedField>(packed_type(document, created).segments[0]).relationship->kind,
              codegen::PackedFieldRelationKind::count_of);
    undo_relationship = document.undo();
    ASSERT_TRUE(undo_relationship.has_value());
    ASSERT_TRUE(*undo_relationship);

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
    EXPECT_NE(preview->front().updated.find("(relation index_into ExistingPacked)"),
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
    EXPECT_EQ(reloaded_index.relationship->kind, codegen::PackedFieldRelationKind::index_into);
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
            codegen::IntegerScalarSchema{
                .name = "DamageReason",
                .signedness = false,
                .minimum_value = 0,
                .maximum_value = 10,
                .bit_width = std::nullopt,
                .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false},
                                {.name = "Invalid", .value = 15, .sentinel = true}}},
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
    EXPECT_NE(preview->front().updated.find("(code Invalid :value 15 :sentinel true)"),
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
    ASSERT_EQ(schema->named_codes.size(), 2U);
    EXPECT_EQ(schema->named_codes[0].name, "Invalid");
    auto const& reloaded_type{integer_scalar_type(reloaded, *reloaded_declaration)};
    EXPECT_EQ(reloaded_type.bit_width, 5U);
    EXPECT_FALSE(reloaded_type.bit_width_auto);
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
    EXPECT_NE(duplicate.error().message.find("duplicate scalar"), std::string::npos);
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
    nested_user.equivalent_type = codegen::TypeRef{"authored::ExistingSoa"};
    ASSERT_TRUE(document.apply(ReplaceSoa{.declaration = nested, .schema = std::move(nested_user)})
                    .has_value());

    auto record_user{*document.record_schema(record)};
    record_user.members[1].type.name = "authored::ExistingSoa";
    ASSERT_TRUE(
        document.apply(ReplaceRecord{.declaration = record, .schema = std::move(record_user)})
            .has_value());

    auto const revision_before_collision{document.revision()};
    auto collision{document.apply(
        RenameDeclaration{.declaration = declaration, .new_name = "NestedFlagsView"})};
    ASSERT_FALSE(collision.has_value());
    EXPECT_NE(collision.error().message.find("Duplicate generated SOA type name"),
              std::string::npos);
    EXPECT_EQ(document.revision(), revision_before_collision);
    EXPECT_EQ(document.declaration(declaration)->identity.name, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].type.name, "authored::ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].fixed_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->members[0].nested_schema, "ExistingSoa");
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::ExistingSoa");

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
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::RenamedSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::RenamedSoa");

    auto const renamed_type{document.types().find(document.declaration(declaration)->identity)};
    ASSERT_TRUE(renamed_type.has_value());
    auto const& nested_column{soa_type(document, nested).columns[0]};
    EXPECT_EQ(nested_column.semantic_type.type, *renamed_type);
    EXPECT_EQ(nested_column.nested_type, *renamed_type);
    EXPECT_EQ(soa_type(document, nested).equivalent_type->type, *renamed_type);
    EXPECT_EQ(record_type(document, record).members[1].semantic_type.type, *renamed_type);

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
    EXPECT_EQ(document.soa_schema(nested)->equivalent_type->name, "authored::ExistingSoa");
    EXPECT_EQ(document.record_schema(record)->members[1].type.name, "authored::ExistingSoa");
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
    EXPECT_EQ(soa_type(reloaded, reloaded_nested).columns[0].nested_type, *reloaded_type);
    EXPECT_EQ(reloaded.soa_schema(reloaded_nested)->equivalent_type->name, "authored::RenamedSoa");
    EXPECT_EQ(soa_type(reloaded, reloaded_nested).equivalent_type->type, *reloaded_type);
    EXPECT_EQ(reloaded.record_schema(reloaded_record)->members[1].type.name,
              "authored::RenamedSoa");
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
    EXPECT_NE(rejected.error().message.find("integer-scalar"), std::string::npos);
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
        .mask_dimensions = {}});

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

} // namespace
} // namespace lispb::schema
