#include "compiler_test_context.h"
#include "test_utilities.h"
#include "text_expectations.h"

#include <lispb/schema/bit_layout.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lispb::codegen {
using lispb::test_support::expect_contains_all;
using lispb::test_support::expect_contains_none;
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
    EXPECT_FALSE(files.front().content.contains("struct Sample\n"));
    EXPECT_TRUE(files.front().content.contains("samples_2_value()"));

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
        expect_contains_all(
            hidden_files.front().content,
            {"insert<decltype(packed_insert_layout)>", "extract<decltype(packed_extract_layout)>"});
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

    CompilerTestContext duplicate_context;
    auto const duplicates{parse_test_manifest(duplicate_context, R"(
      (module first :header "First.h" :namespace shared
        (integer-scalar Value :bit-width 8 (projections (cpp))))
      (module second :header "Second.h" :namespace shared
        (integer-scalar Value :bit-width 16 (projections (cpp))))
    )")};
    EXPECT_NO_THROW(schema::resolve_type_graph(duplicates));
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
    EXPECT_TRUE(output.contains("nested_child()"));
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
    expect_contains_all(output,
                        {"payload_kill_entity_target_index()",
                         "set_payload_kill_entity_target_index(",
                         "make_payload_kill_entity(",
                         "payload_kill_entity_target()"});
    expect_contains_none(output, {"set_opcode(", "payload_kill_entity_target().index()"});
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
          (member data std::uint8_t :default 7) (projections (cpp)))
        (enum Kind :bit-width 1 (value First) (value Second) (projections (cpp)))
        (union Payload (case First @Low) (case Second @Delta))
        (record TaggedInvalid :layout packed :bits 8 :invalid-value 255
          (member kind @Kind) (member data @Payload :tag kind) (projections (cpp))))
    )")};
    auto const graph{schema::resolve_type_graph(manifest)};
    auto const& layout{graph.bit_layout(*find_test_type(graph, "values", "Defaults"))};
    EXPECT_EQ(layout.default_raw_value, std::uint64_t{5U | (29U << 5)});
    EXPECT_EQ(graph.bit_layout(*find_test_type(graph, "values", "SameInvalid")).default_raw_value,
              253U);
    EXPECT_EQ(
        graph.bit_layout(*find_test_type(graph, "values", "DistinctInvalid")).default_raw_value,
        7U);
    EXPECT_EQ(graph.bit_layout(*find_test_type(graph, "values", "TaggedInvalid")).default_raw_value,
              255U);
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
        R"((module m :header "M.h" (integer-scalar Byte :bit-width 8)
      (record R :layout packed (member from_canonical_raw @Byte) (projections (cpp)))))",
        R"((module m :header "M.h"
      (record FooBar :layout packed (member data std::uint8_t :default 1) (projections (cpp)))
      (record Foo_Bar :layout packed (member data std::uint8_t :default 2) (projections (cpp)))))",
        R"((module m :header "M.h"
      (integer-scalar detail :bit-width 8 (projections (cpp)))))",
        R"((module m :header "M.h"
      (record detail (member data std::uint8_t) (projections (cpp)))
      (integer-scalar Byte :bit-width 8 (projections (cpp)))))",
        R"((module m :header "M.h"
      (integer-scalar Byte :bit-width 8 (projections (cpp)))
      (record detail (member data std::uint8_t) (projections (cpp)))))",
        R"((module a :header "A.h" :namespace shared
      (record FooBar :layout packed (member data std::uint8_t :default 1) (projections (cpp))))
      (module b :header "B.h" :namespace shared
      (record Foo_Bar :layout packed (member data std::uint8_t :default 2) (projections (cpp)))))",
        R"((module a :header "A.h" :namespace shared::detail
      (record ByteBase (member data std::uint8_t) (projections (cpp))))
      (module b :header "B.h" :namespace shared
      (integer-scalar Byte :bit-width 8 (projections (cpp)))))",
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
