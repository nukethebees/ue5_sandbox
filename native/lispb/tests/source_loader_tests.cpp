#include <codegen/source_loader.h>

#include <codegen/manifest_error.h>
#include <codegen/validation.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace codegen {
namespace {

class TemporaryManifest {
  public:
    TemporaryManifest() {
        static int sequence{};
        directory_ = std::filesystem::temp_directory_path() /
                     ("lispb-manifest-test-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory_);
    }

    ~TemporaryManifest() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    void write(std::string const& name, std::string const& content) const {
        std::ofstream output{directory_ / name};
        output << content;
    }

    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }

    void write_root(std::string const& modules) const {
        write("types.lispb", "");
        write("modules.lispb", modules);
    }

    auto load() const -> Manifest {
        std::filesystem::path const modules[]{path("modules.lispb")};
        return load_sources(path("types.lispb"), modules);
    }
  private:
    std::filesystem::path directory_;
};

TEST(SourceLoader, ReadsCommentsAndTypedSoa) {
    TemporaryManifest files;
    files.write("types.lispb", R"(
; Shared type definition.
(type handle
  :spelling "FHandle"
  :header "Handle.h"
  :pass-by value
  (operation add-element add :pass-by value)
  (operation remove-at-swap remove_at_swap)
  (operation set-element set :pass-by value))
)");
    files.write("modules.lispb", R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    :operations (all)
    (member handles array @handle
      (relation offset_into FData :unit elements))))
)");
    auto const manifest{files.load()};

    ASSERT_EQ(manifest.types.size(), 1);
    EXPECT_EQ(manifest.types.at("handle").spelling, "FHandle");
    EXPECT_EQ(manifest.types.at("handle").operation(TypeOperation::add_element), "add");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.structs.front().operations, all_storage_operations());
    auto const& member{module.structs.front().members.front()};
    EXPECT_EQ(resolve_type(member.type, manifest.types).spelling, "FHandle");
    ASSERT_TRUE(member.relationship.has_value());
    EXPECT_EQ(member.relationship->kind, SemanticRelationKind::offset_into);
    EXPECT_EQ(member.relationship->target.name, "FData");
    EXPECT_EQ(member.relationship->unit, SemanticRelationUnit::elements);
}

TEST(SourceLoader, ReadsStandardLibrarySoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module native_data
  :header "NativeData.h"
  :backend standard-library
  (struct Data
    :operations (all)
    (member values array int32)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.backend, SoaBackend::standard_library);
    EXPECT_FALSE(module.settings.source.has_value());
}

TEST(SourceLoader, RejectsUnknownSoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module native_data
  :header "NativeData.h"
  :backend portable
  (struct Data
    (member values array int32)))
)");

    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, ReadsSoaFieldMaskMetadata) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    :field-mask-name FFieldMask
    :field-enum-name EField
    (member masks array FFieldMask)
    (member values array int32
      :mask-field true
      :mask-dimensions ((row_index "3") (column_index "4")))))
)");

    auto const manifest{files.load()};
    auto const& schema{std::get<SoaModuleSchema>(manifest.modules.front()).structs.front()};
    ASSERT_TRUE(schema.field_mask_name.has_value());
    EXPECT_EQ(*schema.field_mask_name, "FFieldMask");
    ASSERT_TRUE(schema.field_enum_name.has_value());
    EXPECT_EQ(*schema.field_enum_name, "EField");
    auto const& member{schema.members[1]};
    EXPECT_TRUE(member.mask_field);
    ASSERT_EQ(member.mask_dimensions.size(), 2);
    EXPECT_EQ(member.mask_dimensions[0].index_name, "row_index");
    EXPECT_EQ(member.mask_dimensions[0].extent, "3");
    EXPECT_EQ(member.mask_dimensions[1].index_name, "column_index");
    EXPECT_EQ(member.mask_dimensions[1].extent, "4");
}

TEST(SourceLoader, ReadsPackedValueModule) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  :namespace project
  (packed-value FighterState
    :storage std::uint32_t
    :byte-order big
    :bit-order msb-first
    :invalid-value 0x7fffffff
    :mutable true
    (field entity_index std::uint32_t :bits auto :range-helper true :minimum 0 :maximum 1000000
      (code Player :value 42)
      (code Invalid :value 1048575 :sentinel true)
      (relation index_into FighterState))
    (reserved future :bits 4)
    (field state State :bits 8 :kind enum)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<PackedValueModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.values.size(), 1);
    auto const& value{module.values.front()};
    EXPECT_EQ(value.name, "FighterState");
    EXPECT_EQ(value.storage_type.name, "std::uint32_t");
    EXPECT_EQ(value.byte_order, PackedByteOrder::big_endian);
    EXPECT_EQ(value.bit_order, PackedBitOrder::most_significant_first);
    EXPECT_EQ(value.invalid_value, std::uint64_t{0x7fffffff});
    EXPECT_TRUE(value.mutable_value);
    ASSERT_EQ(value.segments.size(), 3);
    auto const& index{std::get<PackedFieldSchema>(value.segments[0])};
    EXPECT_FALSE(index.bits.has_value());
    EXPECT_EQ(index.kind, PackedFieldKind::unsigned_integer);
    EXPECT_TRUE(index.range_helper);
    EXPECT_EQ(index.minimum_value, 0U);
    EXPECT_EQ(index.maximum_value, 1'000'000U);
    ASSERT_EQ(index.named_codes.size(), 2U);
    EXPECT_EQ(index.named_codes[0].name, "Player");
    EXPECT_EQ(index.named_codes[0].value, 42U);
    EXPECT_FALSE(index.named_codes[0].sentinel);
    EXPECT_EQ(index.named_codes[1].name, "Invalid");
    EXPECT_EQ(index.named_codes[1].value, 1'048'575U);
    EXPECT_TRUE(index.named_codes[1].sentinel);
    ASSERT_TRUE(index.relationship.has_value());
    EXPECT_EQ(index.relationship->kind, SemanticRelationKind::index_into);
    EXPECT_EQ(index.relationship->target.name, "FighterState");
    auto const& reserved{std::get<PackedReservedBitsSchema>(value.segments[1])};
    EXPECT_EQ(reserved.name, "future");
    EXPECT_EQ(reserved.bits, 4);
    auto const& state{std::get<PackedFieldSchema>(value.segments[2])};
    EXPECT_EQ(state.bits, 8);
    EXPECT_EQ(state.kind, PackedFieldKind::enumeration);
}

TEST(SourceLoader, ReadsSignedArbitraryWidthPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value SignedDelta
    :storage std::uint32_t
    (field delta std::int32_t :bits auto :kind signed :minimum -100 :maximum 100
      (code Origin :value 0)
      (code Unknown :value -128 :sentinel true))
    (reserved future :bits 24)))
)");

    auto const manifest{files.load()};
    auto const& value{std::get<PackedValueModuleSchema>(manifest.modules.front()).values.front()};
    EXPECT_FALSE(value.byte_order.has_value());
    EXPECT_FALSE(value.bit_order.has_value());
    auto const& delta{std::get<PackedFieldSchema>(value.segments.front())};
    EXPECT_EQ(delta.type.name, "std::int32_t");
    EXPECT_FALSE(delta.bits.has_value());
    EXPECT_EQ(delta.kind, PackedFieldKind::signed_integer);
    EXPECT_EQ(delta.minimum_value, PackedIntegerValue{-100});
    EXPECT_EQ(delta.maximum_value, PackedIntegerValue{100});
    ASSERT_EQ(delta.named_codes.size(), 2U);
    EXPECT_EQ(delta.named_codes[0].value, PackedIntegerValue{0});
    EXPECT_EQ(delta.named_codes[1].value, PackedIntegerValue{-128});
    EXPECT_TRUE(delta.named_codes[1].sentinel);
}

TEST(SourceLoader, ReadsLinearQuantizedPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(scalar-module scalars
  :header "Scalars.h"
  :namespace project
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 1000
    :bit-width auto))
(representation-module representations
  :header "Representations.h"
  :namespace project
  (linear-quantized HealthQ8
    :source project::Health
    :bits 8
    :reserved-codes 2
    :clipping clamp))
(packed-value-module packed
  :header "Packed.h"
  :namespace project
  (packed-value Vitals
    :storage std::uint16_t
    (field health project::HealthQ8 :bits auto :kind linear-quantized)
    (field state std::uint8_t :bits 8)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<PackedValueModuleSchema>(manifest.modules.back())};
    auto const& health{std::get<PackedFieldSchema>(module.values.front().segments.front())};
    EXPECT_EQ(health.type.name, "project::HealthQ8");
    EXPECT_FALSE(health.bits.has_value());
    EXPECT_EQ(health.kind, PackedFieldKind::linear_quantized);
}

TEST(SourceLoader, RejectsUnknownPackedPhysicalOrdering) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage std::uint32_t
    :byte-order middle
    (field value std::uint32_t :bits 32)))
)");
    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);

    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage std::uint32_t
    :bit-order byte-first
    (field value std::uint32_t :bits 32)))
)");
    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, ReadsStandaloneIntegerScalarDomain) {
    TemporaryManifest files;
    files.write_root(R"(
(scalar-module semantic_values
  :header "SemanticValues.h"
  :namespace project
  (integer-scalar DamageReason
    :signed false
    :minimum 0
    :maximum 10
    :bit-width auto
    :cpp-emission constants-with-names
    :cpp-type std::uint8_t
    (code Unknown :value 0)
    (code Invalid :value 15 :sentinel true)
    (relation index_into EntityTable))
  (integer-scalar EntityTable
    :signed false
    :minimum 0
    :maximum 1023
    :bit-width 10)
  (integer-scalar PayloadOffset
    :signed false
    :minimum 0
    :maximum 65535
    :bit-width 16
    (relation offset_into EntityTable :unit bytes)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<ScalarModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.scalars.size(), 3U);
    auto const& scalar{module.scalars.front()};
    EXPECT_EQ(scalar.name, "DamageReason");
    EXPECT_FALSE(scalar.signedness);
    EXPECT_EQ(scalar.minimum_value, PackedIntegerValue{0});
    EXPECT_EQ(scalar.maximum_value, PackedIntegerValue{10});
    EXPECT_FALSE(scalar.bit_width.has_value());
    EXPECT_EQ(scalar.cpp_emission, IntegerScalarCppEmission::constants_with_names);
    ASSERT_TRUE(scalar.cpp_type.has_value());
    EXPECT_EQ(scalar.cpp_type->name, "std::uint8_t");
    ASSERT_EQ(scalar.named_codes.size(), 2U);
    EXPECT_EQ(scalar.named_codes[1].value, PackedIntegerValue{15});
    EXPECT_TRUE(scalar.named_codes[1].sentinel);
    ASSERT_TRUE(scalar.relationship.has_value());
    EXPECT_EQ(scalar.relationship->kind, SemanticRelationKind::index_into);
    EXPECT_EQ(scalar.relationship->target.name, "EntityTable");
    EXPECT_FALSE(scalar.relationship->unit.has_value());
    ASSERT_TRUE(module.scalars[2].relationship.has_value());
    EXPECT_EQ(module.scalars[2].relationship->kind, SemanticRelationKind::offset_into);
    EXPECT_EQ(module.scalars[2].relationship->unit, SemanticRelationUnit::bytes);
}

TEST(SourceLoader, ReadsPhysicalRepresentations) {
    TemporaryManifest files;
    files.write_root(R"(
(scalar-module semantic_values
  :header "SemanticValues.h"
  :namespace project
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 1000
    :bit-width auto
    (code Invalid :value 1023 :sentinel true)))
(representation-module representations
  :header "Representations.h"
  :namespace project
  (linear-quantized HealthQ8
    :source project::Health
    :bits 8
    :reserved-codes 1
    :clipping clamp)
  (integer-varint HealthVarint
    :source project::Health
    :encoding unsigned)
  (fixed-point VelocityQ12_4
    :signed true
    :total-bits 16
    :fractional-bits 4
    :rounding toward-zero)
  (mini-float CompactFloat
    :sign-bits 1
    :exponent-bits 5
    :significand-bits 10
    :bias 15)
  (optional-sentinel OptionalHealth
    :source project::Health
    :sentinel Invalid)
  (optional-presence-bit PresentHealth
    :source project::Health))
)");

    auto const manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2U);
    auto const& module{std::get<RepresentationModuleSchema>(manifest.modules[1])};
    ASSERT_EQ(module.linear_quantized.size(), 1U);
    auto const& representation{module.linear_quantized.front()};
    EXPECT_EQ(representation.name, "HealthQ8");
    EXPECT_EQ(representation.source.name, "project::Health");
    EXPECT_EQ(representation.bit_width, 8U);
    EXPECT_EQ(representation.reserved_codes, 1U);
    EXPECT_EQ(representation.clipping, QuantizationClipping::clamp);
    ASSERT_EQ(module.integer_varints.size(), 1U);
    auto const& varint{module.integer_varints.front()};
    EXPECT_EQ(varint.name, "HealthVarint");
    EXPECT_EQ(varint.source.name, "project::Health");
    EXPECT_EQ(varint.encoding, IntegerVarintEncoding::unsigned_varint);
    ASSERT_EQ(module.fixed_points.size(), 1U);
    auto const& fixed_point{module.fixed_points.front()};
    EXPECT_EQ(fixed_point.name, "VelocityQ12_4");
    EXPECT_TRUE(fixed_point.signedness);
    EXPECT_EQ(fixed_point.total_bits, 16U);
    EXPECT_EQ(fixed_point.fractional_bits, 4U);
    EXPECT_EQ(fixed_point.rounding, FixedPointRounding::toward_zero);
    ASSERT_EQ(module.mini_floats.size(), 1U);
    auto const& mini_float{module.mini_floats.front()};
    EXPECT_EQ(mini_float.name, "CompactFloat");
    EXPECT_EQ(mini_float.sign_bits, 1U);
    EXPECT_EQ(mini_float.exponent_bits, 5U);
    EXPECT_EQ(mini_float.significand_bits, 10U);
    EXPECT_EQ(mini_float.exponent_bias, 15);
    ASSERT_EQ(module.optional_sentinels.size(), 1U);
    auto const& optional{module.optional_sentinels.front()};
    EXPECT_EQ(optional.name, "OptionalHealth");
    EXPECT_EQ(optional.source.name, "project::Health");
    EXPECT_EQ(optional.sentinel, "Invalid");
    ASSERT_EQ(module.optional_presence_bits.size(), 1U);
    auto const& presence{module.optional_presence_bits.front()};
    EXPECT_EQ(presence.name, "PresentHealth");
    EXPECT_EQ(presence.source.name, "project::Health");
}

TEST(SourceLoader, RejectsInvalidMiniFloatWidthsAndBias) {
    auto load_mini_float = [](std::string const& properties) {
        TemporaryManifest files;
        files.write_root("(representation-module representations\n"
                         "  :header \"Representations.h\"\n"
                         "  (mini-float Invalid " +
                         properties + "))\n");
        return files.load();
    };

    EXPECT_THROW(load_mini_float(":sign-bits 2 :exponent-bits 5 :significand-bits 10 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 1 :significand-bits 10 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 63 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 15 :significand-bits 49 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 32768"),
                 ManifestError);
}

TEST(SourceLoader, ReadsExplicitEnumBitWidth) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module states
  :header "States.h"
  (enum State std::uint8_t
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")))
)");

    auto const manifest{files.load()};
    auto const& schema{std::get<EnumModuleSchema>(manifest.modules.front()).enums.front()};
    EXPECT_EQ(schema.bit_width, 3);
    EXPECT_EQ(schema.signedness, false);
}

TEST(SourceLoader, ReadsEnumWithoutCppBackingType) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module states
  :header "States.h"
  (enum State
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")))
)");

    auto const manifest{files.load()};
    auto const& schema{std::get<EnumModuleSchema>(manifest.modules.front()).enums.front()};
    EXPECT_FALSE(schema.underlying_type.has_value());
    EXPECT_EQ(schema.bit_width, 3);
    EXPECT_EQ(schema.signedness, false);
}

TEST(SourceLoader, ReadsExplicitSignedEnumDomain) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module states
  :header "States.h"
  (enum Delta std::int8_t
    :signed true
    (value Below :value "-1")
    (value Above :value "1")))
)");

    auto const manifest{files.load()};
    auto const& schema{std::get<EnumModuleSchema>(manifest.modules.front()).enums.front()};
    EXPECT_EQ(schema.signedness, true);
    EXPECT_FALSE(schema.bit_width.has_value());
}

TEST(SourceLoader, ReadsNamedEnumSentinels) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module states
  :header "States.h"
  (enum State std::uint8_t
    (value Ready :value "0")
    (value Invalid :value "0xff" :sentinel true)
    (value Pending :value "0xfe" :sentinel true)))
)");

    auto const manifest{files.load()};
    auto const& values{std::get<EnumModuleSchema>(manifest.modules.front()).enums.front().values};
    ASSERT_EQ(values.size(), 3U);
    EXPECT_FALSE(values[0].sentinel);
    EXPECT_TRUE(values[1].sentinel);
    EXPECT_TRUE(values[2].sentinel);
}

TEST(SourceLoader, RejectsEnumBitWidthOutsideNativeAnalysisRange) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module states
  :header "States.h"
  (enum State std::uint8_t
    :bit-width 65
    (value Idle)))
)");

    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, PreservesNumericAndOpaqueEnumInitializers) {
    TemporaryManifest files;
    files.write_root(R"schema(
(enum-module enums
  :header "Enums.h"
  (enum State uint8
    (value Zero :value 0)
    (value High :value "0x7f")))
)schema");

    auto const manifest{files.load()};
    auto const& values{std::get<EnumModuleSchema>(manifest.modules.front()).enums.front().values};
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].initializer, "0");
    EXPECT_EQ(values[1].initializer, "0x7f");

    files.write_root(R"schema(
(enum-module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value -1)))
)schema");
    auto const negative_manifest{files.load()};
    auto const& negative_values{
        std::get<EnumModuleSchema>(negative_manifest.modules.front()).enums.front().values};
    EXPECT_EQ(negative_values.front().initializer, "-1");

    files.write_root(R"schema(
(enum-module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value "static_cast<uint8>(1)")))
)schema");
    auto const opaque_manifest{files.load()};
    auto const& opaque_values{
        std::get<EnumModuleSchema>(opaque_manifest.modules.front()).enums.front().values};
    EXPECT_EQ(opaque_values.front().initializer, "static_cast<uint8>(1)");
}

TEST(SourceLoader, ReadsRecordModuleAndFixedArrays) {
    TemporaryManifest files;
    files.write_root(R"(
(record-module data
  :header "Data.h"
  :namespace project
  (record Position
    (member x float)
    (member y float))
  (record Trail
    :export-specifier PROJECT_API
    (member points Position :count 4
      (relation contains Position))))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<RecordModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.records.size(), 2U);
    EXPECT_EQ(module.records[0].name, "Position");
    ASSERT_EQ(module.records[0].members.size(), 2U);
    EXPECT_EQ(module.records[0].members[0].type.name, "float");
    EXPECT_FALSE(module.records[0].members[0].count.has_value());
    EXPECT_EQ(module.records[1].export_specifier, "PROJECT_API");
    EXPECT_EQ(module.records[1].members[0].count, 4);
    ASSERT_TRUE(module.records[1].members[0].relationship.has_value());
    EXPECT_EQ(module.records[1].members[0].relationship->kind, SemanticRelationKind::contains);
    EXPECT_EQ(module.records[1].members[0].relationship->target.name, "Position");
}

TEST(SourceLoader, ReadsRawUnionModuleAndFixedArrayAlternatives) {
    TemporaryManifest files;
    files.write_root(R"(
(union-module payloads
  :header "Payloads.h"
  :namespace project
  (union Payload
    :export-specifier PROJECT_API
    (alternative identifier std::uint32_t)
    (alternative bytes std::uint8_t :count 12)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<UnionModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.unions.size(), 1U);
    EXPECT_EQ(module.unions[0].name, "Payload");
    EXPECT_EQ(module.unions[0].export_specifier, "PROJECT_API");
    ASSERT_EQ(module.unions[0].alternatives.size(), 2U);
    EXPECT_EQ(module.unions[0].alternatives[0].name, "identifier");
    EXPECT_FALSE(module.unions[0].alternatives[0].count.has_value());
    EXPECT_EQ(module.unions[0].alternatives[1].type.name, "std::uint8_t");
    EXPECT_EQ(module.unions[0].alternatives[1].count, 12);
}

TEST(SourceLoader, RejectsInvalidRawUnionAlternatives) {
    TemporaryManifest files;
    files.write_root(R"(
(union-module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t :count 0)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);

    files.write_root(R"(
(union-module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t)
    (alternative value std::uint16_t)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);
}

TEST(SourceLoader, ReadsTaggedUnionDiscriminantAndSymbolicMappings) {
    TemporaryManifest files;
    files.write_root(R"(
(enum-module events
  :header "Events.h"
  (enum EventKind std::uint8_t
    (value Spawn)
    (value Damage)
    (value Invalid :sentinel true)))
(union-module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant events::EventKind
    :export-specifier PROJECT_API
    (alternative spawn std::uint32_t :tag Spawn)
    (alternative damage std::uint16_t :count 4 :tag Damage)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<UnionModuleSchema>(manifest.modules[1])};
    ASSERT_EQ(module.tagged_unions.size(), 1U);
    auto const& tagged{module.tagged_unions.front()};
    EXPECT_EQ(tagged.name, "Event");
    EXPECT_EQ(tagged.discriminant.name, "events::EventKind");
    EXPECT_EQ(tagged.export_specifier, "PROJECT_API");
    ASSERT_EQ(tagged.alternatives.size(), 2U);
    EXPECT_EQ(tagged.alternatives[0].tag, "Spawn");
    EXPECT_FALSE(tagged.alternatives[0].count.has_value());
    EXPECT_EQ(tagged.alternatives[1].tag, "Damage");
    EXPECT_EQ(tagged.alternatives[1].count, 4);
}

TEST(SourceLoader, RejectsDuplicateTaggedUnionNamesAndTags) {
    TemporaryManifest files;
    files.write_root(R"(
(union-module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant std::uint8_t
    (alternative value std::uint32_t :tag Value)
    (alternative value std::uint16_t :tag Other)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);

    files.write_root(R"(
(union-module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant std::uint8_t
    (alternative first std::uint32_t :tag Value)
    (alternative second std::uint16_t :tag Value)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);
}

TEST(SourceLoader, RejectsNonIntegerPackedFieldWidthWithSourceLocation) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage uint8
    (field value uint8 :bits 1.5)))
)");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:6:30"), std::string::npos);
        EXPECT_NE(message.find("packed field bits must be an integer"), std::string::npos);
    }
}

TEST(SourceLoader, RejectsNegativePackedInvalidValue) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage uint8
    :invalid-value -1
    (field value uint8 :bits 8)))
)");

    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, ReportsSourceLocationForUnknownProperties) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all\n  :header \"All.h\"\n  :headers ()\n  :typo true)\n");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:4:3"), std::string::npos);
        EXPECT_NE(message.find("unknown property ':typo'"), std::string::npos);
    }
}

TEST(SourceLoader, RejectsUnknownModuleDeclarations) {
    TemporaryManifest files;
    files.write_root("(mystery-module bad :header \"Bad.h\")");
    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module bad
  :header "Bad.h"
  (struct FData
    :operations (all reset)
    (member values array int32)))
)");
    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, LoadsStructuredTypeReferencesAndFacadeStorage) {
    TemporaryManifest files;
    files.write_root(R"(
(facade-module facade
  :header "Facade.h"
  :source "Facade.cpp"
  :namespace project
  (facade FFacade @target target
    :target-storage reference
    :definitions-in-source true
    (method get (type-ref @vector :suffix " const&")
      :const true
      :target-name get_value
      (parameter index int32 :default "0"))))
)");

    auto const manifest{files.load()};
    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules.front()).facade};
    EXPECT_TRUE(facade.reference_target);
    EXPECT_TRUE(facade.definitions_in_source);
    EXPECT_EQ(facade.methods.front().return_type.suffix, " const&");
    EXPECT_EQ(facade.methods.front().target_name, "get_value");
    EXPECT_TRUE(facade.methods.front().is_const);
}

TEST(SourceLoader, LoadsOpaqueCppBlocksAndKeepsQuotedBodiesCompatible) {
    TemporaryManifest files;
    files.write_root(
        "(soa-module example\n"
        "  :header \"Generated.h\"\n"
        "  :prelude #cpp{class FForward;\n"
        "#define GENERATED_PATH \"C:\\\\generated\"}cpp#\n"
        "  (struct FData\n"
        "    (member values array int32)\n"
        "    (function update void\n"
        "      :body #cpp{if (dt <= 0.0f) {\n"
        "    return;\n"
        "}\n"
        "\n"
        "values[0] += dt;}cpp#\n"
        "      (parameter dt float))\n"
        "    (function reset void\n"
        "      :body (\"values[0] = 0;\"))))\n"
        "(facade-module facade\n"
        "  :header \"Facade.h\"\n"
        "  (facade FFacade Target target\n"
        "    :validation #cpp{checkf(target != nullptr, TEXT(\"missing target\"));}cpp#))");

    auto const manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2);
#ifdef _WIN32
    auto const newline{std::string{"\r\n"}};
#else
    auto const newline{std::string{"\n"}};
#endif

    auto const& soa{std::get<SoaModuleSchema>(manifest.modules[0])};
    ASSERT_EQ(soa.settings.prelude_lines.size(), 1);
    EXPECT_EQ(soa.settings.prelude_lines[0],
              "class FForward;" + newline + "#define GENERATED_PATH \"C:\\\\generated\"");
    ASSERT_EQ(soa.structs[0].functions.size(), 2);
    ASSERT_EQ(soa.structs[0].functions[0].body_lines.size(), 1);
    EXPECT_EQ(soa.structs[0].functions[0].body_lines[0],
              "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
                  "values[0] += dt;");
    EXPECT_EQ(soa.structs[0].functions[1].body_lines, (std::vector<std::string>{"values[0] = 0;"}));

    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules[1]).facade};
    EXPECT_EQ(facade.validation_lines,
              (std::vector<std::string>{"checkf(target != nullptr, TEXT(\"missing target\"));"}));
}

TEST(SourceLoader, AcceptsEmptyCppBodyAndRejectsWrongRawTag) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    (function empty void :body #cpp{}cpp#)))
)");
    auto const manifest{files.load()};
    auto const& function{std::get<SoaModuleSchema>(manifest.modules[0]).structs[0].functions[0]};
    EXPECT_TRUE(function.body_lines.empty());

    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    (function wrong void :body #hlsl{return 0;}hlsl#)))
)");
    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_TRUE(message.contains("body requires a #cpp raw literal; got #hlsl"));
        EXPECT_TRUE(message.contains("modules.lispb:5:32"));
    }
}

TEST(SourceLoader, RejectsRawLiteralForOrdinaryTextField) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all :header #cpp{Generated.h}cpp# :headers ())");

    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, LoadsSettingsControls) {
    TemporaryManifest files;
    files.write_root(R"(
(settings-module settings
  :header "Settings.h"
  :api-name TSettingsAccess
  :state-name FSettingsState
  (category video "Video")
  (setting resolution-scale "Resolution Scale"
    :category video
    :value-type float
    :backend engine
    :apply deferred
    (control float-range :min 50 :max 100 :step 0.5)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SettingsModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.settings_list.size(), 1);
    EXPECT_EQ(module.settings_list.front().apply_mode, SettingApplyMode::deferred);
    EXPECT_EQ(module.settings_list.front().control.kind, SettingControlKind::float_range);
    EXPECT_EQ(module.settings_list.front().control.step, 0.5);
}

} // namespace
} // namespace codegen
