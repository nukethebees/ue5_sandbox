#include "compiler_test_context.h"
#include "test_utilities.h"

#include <lispb/schema/bit_layout.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lispb::codegen {
namespace {

auto placement(schema::BitLayout const& layout, std::string_view path)
    -> schema::BitPlacement const& {
    auto const found{std::ranges::find(layout.placements, path, &schema::BitPlacement::path)};
    EXPECT_NE(found, layout.placements.end());
    if (found == layout.placements.end()) {
        throw std::runtime_error{"Missing bit placement"};
    }
    return *found;
}

TEST(PackedRecord, NestedLayoutIsIndependentOfCppStorage) {
    CompilerTestContext context;
    auto const graph{schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Index :bit-width 24)
        (integer-scalar Kind :bit-width 8)
        (record EntityId :layout packed
          (member index @Index) (member kind @Kind)
          (projections (cpp :storage std::uint64_t)))
        (record Owner :layout packed :bits 64
          (member prefix @Kind) (member target @EntityId)
          (projections (cpp))))
    )"))};
    auto const& child{graph.bit_layout(*find_test_type(graph, "values", "EntityId"))};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Owner"))};
    EXPECT_EQ(child.encoded_bits, 32U);
    EXPECT_EQ(child.storage_bits, 64U);
    EXPECT_EQ(layout.encoded_bits, 64U);
    EXPECT_EQ(placement(layout, "target").offset, 8U);
    EXPECT_EQ(placement(layout, "target").bits, 32U);
    EXPECT_EQ(placement(layout, "target_index").offset, 8U);
    EXPECT_EQ(placement(layout, "target_kind").offset, 32U);
}

TEST(PackedRecord, LogicalRecordsAndArraysHaveFlatConstantOffsets) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Byte :bit-width 8)
        (record Sample (member value @Byte))
        (record Samples :layout packed
          (member samples @Sample :count 3) (projections (cpp))))
    )")};
    auto const graph{schema::resolve_type_graph(manifest)};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Samples"))};
    EXPECT_EQ(layout.encoded_bits, 24U);
    EXPECT_EQ(placement(layout, "samples_0_value").offset, 0U);
    EXPECT_EQ(placement(layout, "samples_1_value").offset, 8U);
    EXPECT_EQ(placement(layout, "samples_2_value").offset, 16U);
    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1U);
    EXPECT_EQ(files.front().content.find("struct Sample\n"), std::string::npos);
    EXPECT_NE(files.front().content.find("samples_2_value()"), std::string::npos);

    for (auto const name_space : {"ioj", "ioj::packed_lookup_fixture", "game"}) {
        SCOPED_TRACE(name_space);
        CompilerTestContext lookup_context;
        auto const source{std::string{R"((module lookup :header "Lookup.h" :namespace )"} +
                          name_space + R"(
          (integer-scalar Byte :bit-width 8 (projections (cpp)))
          (record Value :layout packed (member data @Byte) (projections (cpp)))))"};
        auto const lookup_files{
            render_test_manifest(lookup_context, parse_test_manifest(lookup_context, source))};
        ASSERT_EQ(lookup_files.size(), 1U);
        auto const& output{lookup_files.front().content};
        auto const inside_ioj{std::string_view{name_space}.starts_with("ioj")};
        for (auto const symbol : {"PackedField<", "PackedValueBase<", "PackedStorage<"}) {
            SCOPED_TRACE(symbol);
            EXPECT_TRUE(output.contains(symbol));
            EXPECT_EQ(output.contains("ioj::" + std::string{symbol}), !inside_ioj);
        }

        CompilerTestContext hidden_context;
        auto const hidden_source{std::string{R"((module hidden :header "Hidden.h" :namespace )"} +
                                 name_space + R"(
          (record Hidden :layout packed
            (member packed_insert std::uint8_t) (member packed_extract std::uint8_t)
            (projections (cpp)))))"};
        auto const hidden_files{render_test_manifest(
            hidden_context, parse_test_manifest(hidden_context, hidden_source))};
        ASSERT_EQ(hidden_files.size(), 1U);
        EXPECT_TRUE(
            hidden_files.front().content.contains("insert<decltype(packed_insert_layout)>"));
        EXPECT_TRUE(
            hidden_files.front().content.contains("extract<decltype(packed_extract_layout)>"));
    }
}

TEST(PackedRecord, QualifiedLogicalReferencesNeedNoCppSpelling) {
    CompilerTestContext context;
    auto const graph{schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module logical
        (integer-scalar Index :bit-width 24)
        (record Target (member index @Index)))
      (module owners :header "Owners.h"
        (record Handle :layout packed
          (member target @logical/Target)
          (projections (cpp))))
    )"))};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "owners", "Handle"))};
    EXPECT_EQ(layout.encoded_bits, 24U);
    EXPECT_EQ(placement(layout, "target_index").offset, 0U);
}

TEST(PackedRecord, ProjectedDescendantsAreDefinedBeforeOwnersAcrossLogicalIntermediates) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (record Owner :layout packed
          (member nested @Logical) (projections (cpp)))
        (record Logical (member child @Child))
        (record Child :layout packed
          (member value @Byte) (projections (cpp)))
        (integer-scalar Byte :bit-width 8))
    )"))};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_LT(output.find("struct Child :"), output.find("struct Owner :"));
    EXPECT_NE(output.find("nested_child()"), std::string::npos);
}

TEST(PackedRecord, ReservedAndMostSignificantFirstUseLogicalExtent) {
    CompilerTestContext context;
    auto const graph{schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Version :bit-width 4)
        (integer-scalar Length :bit-width 16)
        (record Header :layout packed :bits 32 :bit-order msb-first :byte-order big
          (member version @Version) (reserved future :bits 12) (member length @Length)
          (projections (cpp :storage std::uint64_t))))
    )"))};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Header"))};
    EXPECT_EQ(placement(layout, "version").offset, 28U);
    EXPECT_EQ(placement(layout, "length").offset, 0U);
    EXPECT_TRUE(placement(layout, "future").reserved);
}

TEST(PackedRecord, UnionAlternativesOverlapAndExplicitExtentPads) {
    CompilerTestContext context;
    auto const graph{schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Small :bit-width 8)
        (integer-scalar Large :bit-width 24)
        (union Target @Small @Large)
        (record Value :layout packed :bits 64
          (member prefix @Small) (member target @Target :bits 40) (member tail @Small)
          (projections (cpp))))
    )"))};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Value"))};
    EXPECT_EQ(placement(layout, "target_small").offset, 8U);
    EXPECT_EQ(placement(layout, "target_large").offset, 8U);
    EXPECT_EQ(placement(layout, "target").bits, 24U);
    EXPECT_EQ(placement(layout, "target").extent, 40U);
    EXPECT_EQ(placement(layout, "tail").offset, 48U);
}

TEST(PackedRecord, TaggedAlternativesResolveGuardsAndDirectNestedAccessors) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Index :bit-width 24)
        (enum Opcode :bit-width 6 (value KillEntity) (value MoveTo) (projections (cpp)))
        (record EntityId :layout packed (member index @Index) (projections (cpp)))
        (record KillEntity (member target @EntityId))
        (record Movement (member destination @Index))
        (union Payload (case KillEntity @KillEntity) (case MoveTo @Movement))
        (record Instruction :layout packed :bits 64
          (member opcode @Opcode) (member payload @Payload :bits 58 :tag opcode)
          (projections (cpp))))
    )")};
    auto const graph{schema::resolve_type_graph(manifest)};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Instruction"))};
    EXPECT_TRUE(placement(layout, "opcode").discriminator);
    auto const& target{placement(layout, "payload_kill_entity_target_index")};
    ASSERT_EQ(target.guards.size(), 1U);
    EXPECT_EQ(target.offset, 6U);
    EXPECT_EQ(target.guards.front().offset, 0U);
    EXPECT_EQ(target.guards.front().bits, 6U);
    EXPECT_EQ(target.guards.front().value, 0U);
    EXPECT_EQ(placement(layout, "payload_move_to_destination").guards.front().value, 1U);
    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("payload_kill_entity_target_index()"), std::string::npos);
    EXPECT_NE(output.find("set_payload_kill_entity_target_index("), std::string::npos);
    EXPECT_NE(output.find("make_payload_kill_entity("), std::string::npos);
    EXPECT_NE(output.find("payload_kill_entity_target()"), std::string::npos);
    EXPECT_EQ(output.find("set_opcode("), std::string::npos);
    EXPECT_EQ(output.find("payload_kill_entity_target().index()"), std::string::npos);
}

TEST(PackedRecord, FixedEncodingsComposeWithoutStandaloneProjection) {
    CompilerTestContext context;
    auto const graph{schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100)
        (fixed-point Velocity :signed true :total-bits 12 :fractional-bits 4)
        (linear-quantized Quantized :source @Health :bits 6 :reserved-codes 1 :clipping clamp)
        (mini-float Position :sign-bits 1 :exponent-bits 5 :significand-bits 6 :bias 15)
        (record State :layout packed
          (member velocity @Velocity) (member health @Quantized) (member position @Position)
          (projections (cpp))))
    )"))};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "State"))};
    EXPECT_EQ(layout.encoded_bits, 30U);
    EXPECT_EQ(placement(layout, "velocity").bits, 12U);
    EXPECT_EQ(placement(layout, "health").offset, 12U);
    EXPECT_EQ(placement(layout, "position").offset, 18U);
}

TEST(PackedRecord, DefaultsIncludeSignedValuesAndReservedBits) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Low :bit-width 3)
        (integer-scalar Delta :bit-width 5 :signed true)
        (record Defaults :layout packed :bits 16
          (member low @Low :default 5) (reserved gap :bits 2)
          (member delta @Delta :default -3) (projections (cpp)))
        (record SameInvalid :layout packed :invalid-value 253
          (member data std::uint8_t) (projections (cpp)))
        (record DistinctInvalid :layout packed :invalid-value 254
          (member data std::uint8_t :default 7) (projections (cpp))))
    )")};
    auto const graph{schema::resolve_type_graph(manifest)};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Defaults"))};
    EXPECT_EQ(layout.default_raw_value, std::uint64_t{5U | (29U << 5)});
    EXPECT_EQ(graph.bit_layout(*find_test_type(graph, "values", "SameInvalid")).default_raw_value,
              253U);
    EXPECT_EQ(
        graph.bit_layout(*find_test_type(graph, "values", "DistinctInvalid")).default_raw_value,
        7U);
    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_EQ(occurrences(output, "933"), 1U);
    EXPECT_EQ(occurrences(output, "253"), 1U);
    for (auto const name : {"defaults", "same_invalid", "distinct_invalid"}) {
        EXPECT_EQ(occurrences(output, std::string{name} + "_default_raw_value"), 2U);
    }
    auto const same_begin{output.find("struct SameInvalid :")};
    ASSERT_NE(same_begin, std::string::npos);
    auto const same{output.substr(same_begin, output.find("\n};", same_begin) - same_begin)};
    EXPECT_TRUE(same.contains("invalid_value{storage.default_raw_value}"));
    auto const distinct_begin{output.find("struct DistinctInvalid :")};
    ASSERT_NE(distinct_begin, std::string::npos);
    auto const distinct{
        output.substr(distinct_begin, output.find("\n};", distinct_begin) - distinct_begin)};
    EXPECT_TRUE(distinct.contains("invalid_value{254"));
    EXPECT_FALSE(distinct.contains("invalid_value{storage.default_raw_value}"));
}

TEST(PackedRecord, SharesTypedValidationHelpersAcrossConstructionMutationAndValidation) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100 (projections (cpp)))
        (integer-scalar Index :bit-width 24 (projections (cpp)))
        (integer-scalar Byte :bit-width 8 (projections (cpp)))
        (integer-scalar Limited :bit-width 12 :maximum 100 (projections (cpp)))
        (record State :layout packed (member health @Health) (projections (cpp)))
        (record Handle :layout packed (member index @Index) (member byte @Byte)
          (projections (cpp)))
        (record Constrained :layout packed (member limited @Limited) (projections (cpp))))
    )"))};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_GE(occurrences(output, "is_valid_value("), 4U);
    EXPECT_GE(occurrences(output, "is_valid_health("), 4U);
    EXPECT_NE(output.find("value_type const value"), std::string::npos);
    EXPECT_NE(output.find("decltype(health_layout)::value_type const value"), std::string::npos);
    EXPECT_EQ(output.find("RawTag"), std::string::npos);
    EXPECT_TRUE(output.contains("using packed_integer_tag = ioj::packed::scalar_tag;"));
    EXPECT_TRUE(output.contains("using packed_integer_tag = ioj::packed::integer_tag;"));
    EXPECT_FALSE(output.contains("struct std::hash"));
    EXPECT_FALSE(output.contains("struct std::formatter"));
    EXPECT_FALSE(output.contains("using health_type"));
    EXPECT_FALSE(output.contains("using health_field"));
    EXPECT_FALSE(output.contains("using value_field"));
    EXPECT_TRUE(output.contains("extract<decltype(health_layout)>()"));
    EXPECT_TRUE(output.contains("insert<decltype(health_layout)>(value)"));
    auto class_output = [&](std::string_view name) {
        auto const begin{output.find("struct " + std::string{name} + " :")};
        EXPECT_NE(begin, std::string::npos);
        if (begin == std::string::npos) {
            return std::string_view{};
        }
        return std::string_view{output}.substr(begin, output.find("\n};", begin) - begin);
    };
    auto method_body = [](std::string_view type, std::string_view signature) {
        auto const method{type.find(signature)};
        EXPECT_NE(method, std::string_view::npos);
        if (method == std::string_view::npos) {
            return std::string_view{};
        }
        auto const begin{type.find('{', method)};
        return type.substr(begin, type.find('}', begin) - begin);
    };
    for (auto const name :
         {"Health", "Index", "Byte", "Limited", "State", "Handle", "Constrained"}) {
        SCOPED_TRACE(name);
        auto const type{class_output(name)};
        EXPECT_EQ(occurrences(output, "PackedValueBase<" + std::string{name} + ","), 1U);
        EXPECT_FALSE(type.contains("PackedValueBase<"));
        auto const forward{output.find("struct " + std::string{name} + ";")};
        auto const base{output.find("using " + std::string{name} + "Base =")};
        auto const definition{output.find("struct " + std::string{name} + " :")};
        EXPECT_LT(forward, base);
        EXPECT_LT(base, definition);
        EXPECT_TRUE(type.contains(" : detail::" + std::string{name} + "Base"));
        EXPECT_TRUE(type.contains("using Base = detail::" + std::string{name} + "Base;"));
        auto const constructor{type.find("constexpr " + std::string{name} + "()")};
        auto const comparison{type.find("operator<=>")};
        EXPECT_LT(type.find("using Base"), constructor);
        EXPECT_FALSE(type.contains("using storage_type"));
        EXPECT_FALSE(type.contains("PackedStorage<"));
        EXPECT_EQ(type.substr(constructor).find("static_cast<std::uint8_t>"),
                  std::string_view::npos);
        EXPECT_LT(constructor, comparison);
        EXPECT_FALSE(type.contains("raw_value() const"));
        EXPECT_FALSE(type.contains("from_raw(storage_type"));
        EXPECT_FALSE(type.contains("storage_type value_"));
        EXPECT_FALSE(type.contains("std::bit_cast"));
    }
    auto const index{class_output("Index")};
    auto const index_constructor{index.find("Index(value_type")};
    ASSERT_NE(index_constructor, std::string_view::npos);
    EXPECT_TRUE(index.substr(index_constructor, index.find("operator<=>") - index_constructor)
                    .contains("is_valid_value(value)"));
    EXPECT_TRUE(method_body(index, "set_value(").contains("is_valid_value(value)"));
    EXPECT_FALSE(index.contains("is_valid() const"));
    auto const handle{class_output("Handle")};
    EXPECT_FALSE(handle.contains("is_valid() const"));
    EXPECT_TRUE(method_body(handle, "set_index(").contains("is_valid_index(value)"));
    EXPECT_FALSE(method_body(handle, "set_byte(").contains("assert("));
    EXPECT_FALSE(handle.contains("is_valid_byte("));
    auto const byte{class_output("Byte")};
    EXPECT_FALSE(byte.contains("assert("));
    EXPECT_TRUE(method_body(byte, "is_valid_value(").contains("return true;"));
    EXPECT_FALSE(byte.contains("is_valid() const"));
    auto const limited{class_output("Limited")};
    EXPECT_TRUE(method_body(limited, "is_valid() const").contains("is_valid_value("));
    EXPECT_FALSE(limited.contains("is_valid_value_domain("));
    auto const constrained{class_output("Constrained")};
    EXPECT_TRUE(method_body(constrained, "is_valid() const").contains("is_valid_limited("));
    EXPECT_FALSE(constrained.contains("is_valid_limited_domain("));
    EXPECT_TRUE(method_body(constrained, "set_limited(").contains("is_valid_limited(value)"));
}

TEST(PackedRecord, DefaultsScalarComparisonOnlyWhenStorageOrderMatchesLogicalValues) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Full :bit-width 8 (projections (cpp)))
        (integer-scalar Padded :bit-width 24 (projections (cpp)))
        (integer-scalar Signed :bit-width 8 :signed true (projections (cpp))))
    )"))};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("operator<=>(Full const&) const noexcept = default"), std::string::npos);
    EXPECT_EQ(output.find("operator<=>(Padded const&) const noexcept = default"),
              std::string::npos);
    EXPECT_EQ(output.find("operator<=>(Signed const&) const noexcept = default"),
              std::string::npos);
    EXPECT_EQ(occurrences(output, "return value() <=> other.value();"), 2U);
}

class InvalidPackedRecord : public testing::TestWithParam<char const*> {};

TEST_P(InvalidPackedRecord, RejectsInvalidSchemaBeforeEmission) {
    CompilerTestContext context;
    EXPECT_THROW(render_test_manifest(context, parse_test_manifest(context, GetParam())),
                 std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    UnifiedValidation,
    InvalidPackedRecord,
    testing::Values(
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member value @Byte :kind unsigned) (projections (cpp)))))",
        R"((module m :header "M.h" (packed-value R :storage std::uint32_t
      (field value std::uint32_t :bits 32))))",
        R"((module m :header "M.h" (tagged-union R :discriminant std::uint8_t
      (alternative value std::uint32_t :tag Value))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed :bits 7 (member value @Byte) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member value @Byte :bits 7) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Wide :bit-width 64)
      (record R :layout packed (member first @Wide) (member second @Wide) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member value @Byte) (projections (cpp :storage std::int32_t)))))",
        R"((module m :header "M.h" (integer-scalar Wide :bit-width 24)
      (record R :layout packed (member value @Wide) (projections (cpp :storage std::uint16_t)))))",
        R"((module m :header "M.h" (record R :layout packed (member value std::string) (projections (cpp)))))",
        R"((module m :header "M.h" (record R :layout packed (member value @Missing) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member raw_value @Byte) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8) (record Nested (member b @Byte))
      (record R :layout packed (member a @Nested) (member a_b @Byte) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member value @Byte :default 256) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100)
      (record R :layout packed (member value @Health :default 101) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8) (union Payload @Byte)
      (record R :layout packed (member payload @Payload :tag missing) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8) (union Payload @Byte)
      (record R :layout packed (member tag @Byte) (member payload @Payload :tag tag) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (enum Tag :bit-width 2 (value First) (projections (cpp))) (union Payload (case Missing @Byte))
      (record R :layout packed (member tag @Tag) (member payload @Payload :tag tag) (projections (cpp)))))",
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (enum Tag :bit-width 2 (value First :value 0) (value Alias :value 0) (projections (cpp)))
      (union Payload (case First @Byte) (case Alias @Byte))
      (record R :layout packed (member tag @Tag) (member payload @Payload :tag tag) (projections (cpp)))))",
        R"((module m :header "M.h" (record R :layout packed (member self @R) (projections (cpp)))))"));

} // namespace
} // namespace lispb::codegen
