#include "test_utilities.h"

#include <lispb/schema/enum_domains.h>
#include <lispb/schema/source_renderer.h>

#include <gtest/gtest.h>

#include <format>
#include <string>
#include <vector>

namespace lispb::codegen {
namespace {
auto names(CompilerTestContext const& ctx, schema::TypeGraph const& graph)
    -> std::vector<std::string> {
    auto const type{*find_test_type(graph, "test", "Example")};
    auto const& domains{graph.enum_domains()};
    auto const& declarations{ctx.storage.declarations};
    std::vector<std::string> result;
    for (auto const value : domains.values(domains.find(graph.source_declaration(type)))) {
        result.emplace_back(ctx.storage.strings.view(declarations.enumerators.name[value]));
    }
    return result;
}

TEST(EnumGroups, ExplicitPrioritiesPreserveTiesAndRoundTripAuthoredOrder) {
    CompilerTestContext ctx;
    auto const manifest{parse_test_manifest(ctx, R"(
      (module test :header "Test.h"
        (enum Example std::uint8_t :enum-array true :ordering (B C A)
          (value One :groups (A B)) (value Two :groups (A C))
          (value Three :groups (A B C)) (value Four :groups (B))
          (value Five :groups (C)) (value Six :groups (A))
          (value Seven :groups (A B)) (projections (cpp)))))")};
    auto const graph{schema::resolve_type_graph(manifest)};
    EXPECT_EQ(names(ctx, graph),
              (std::vector<std::string>{"Three", "One", "Seven", "Four", "Two", "Five", "Six"}));
    auto const source{
        schema::render_module_source(ctx.storage.declarations, manifest.modules.front())};
    EXPECT_LT(source.find("(value One"), source.find("(value Three"));
    EXPECT_NE(source.find(":ordering (B C A)"), std::string::npos);
    CompilerTestContext roundtrip;
    auto const parsed{parse_test_manifest(roundtrip, source)};
    EXPECT_EQ(names(roundtrip, schema::resolve_type_graph(parsed)), names(ctx, graph));
}

TEST(EnumGroups, ManualAndAbsentOrderingPreserveCodesAndUngroupedEnums) {
    for (auto const ordering : {"", ":ordering manual"}) {
        CompilerTestContext ctx;
        auto const manifest{parse_test_manifest(ctx,
                                                std::format(R"(
          (module test :header "Test.h" (enum Example std::int8_t {}
            (value First :value -3 :groups (selected))
            (value Second :value 4) (value Third :value -2 :groups (selected))
            (projections (cpp)))))",
                                                            ordering))};
        auto const graph{schema::resolve_type_graph(manifest)};
        EXPECT_EQ(names(ctx, graph), (std::vector<std::string>{"First", "Second", "Third"}));
        auto const files{render_test_manifest(ctx, manifest)};
        EXPECT_NE(files.front().content.find("is_selected"), std::string::npos);
        EXPECT_NE(files.front().content.find("example_selected_values"), std::string::npos);
        EXPECT_NE(files.front().content.find("Example::First"), std::string::npos);
    }
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
      (module test :header "Test.h" (enum Example std::uint8_t
        (value First) (value Second) (projections (cpp)))))"))};
    EXPECT_EQ(files.front().content.find("is_"), std::string::npos);
}

TEST(EnumGroups, AutomaticOrderingIsDeterministicAndDoesNotIncreaseFragmentation) {
    auto const source{R"((module test :header "Test.h" (enum Example std::uint8_t :ordering auto
      (value Zero) (value One :groups (A)) (value Two :groups (B))
      (value Three :groups (A)) (value Four :groups (B)) (value Five :groups (A B))
      (projections (cpp)))))"};
    CompilerTestContext first;
    auto const manifest{parse_test_manifest(first, source)};
    auto const graph{schema::resolve_type_graph(manifest)};
    auto const ordered{names(first, graph)};
    EXPECT_LT(std::ranges::find(ordered, "One"), std::ranges::find(ordered, "Three"));
    auto const& decls{first.storage.declarations};
    auto const type{*find_test_type(graph, "test", "Example")};
    auto const& domains{graph.enum_domains()};
    std::size_t ranges{};
    for (auto const group : {"A", "B"}) {
        bool previous{};
        for (auto const value : domains.values(domains.find(graph.source_declaration(type)))) {
            auto const member{
                std::ranges::any_of(decls.items(decls.enumerators.groups[value]), [&](auto id) {
                    return first.storage.strings.view(id) == group;
                })};
            ranges += member && !previous;
            previous = member;
        }
    }
    EXPECT_LT(ranges, 5U);
    CompilerTestContext second;
    auto const repeat{parse_test_manifest(second, source)};
    EXPECT_EQ(ordered, names(second, schema::resolve_type_graph(repeat)));
    CompilerTestContext roundtrip;
    auto const canonical{schema::render_module_source(decls, manifest.modules.front())};
    auto const reparsed{parse_test_manifest(roundtrip, canonical)};
    EXPECT_EQ(ordered, names(roundtrip, schema::resolve_type_graph(reparsed)));
}

TEST(EnumGroups, EmitsOneComparisonForUnsignedZeroBasedPrefixAndAcceptsMatchingExplicitValues) {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
      (module test :header "Test.h" (enum Example std::uint8_t :ordering (actionable)
        :enum-array true (value Other :value 2) (value First :value 0 :groups (actionable))
        (value Last :value 1 :groups (actionable)) (projections (cpp)))))"))};
    EXPECT_NE(files.front().content.find("return value <= ::Example::Last;"), std::string::npos)
        << files.front().content;
}

class InvalidEnumGroups : public testing::TestWithParam<char const*> {};
TEST_P(InvalidEnumGroups, RejectsInvalidDeclarations) {
    CompilerTestContext ctx;
    auto const source{std::format(
        "(module test :header \"Test.h\" (enum Example std::uint8_t {} (projections (cpp))))",
        GetParam())};
    EXPECT_THROW(static_cast<void>(render_test_manifest(ctx, parse_test_manifest(ctx, source))),
                 std::exception);
}
INSTANTIATE_TEST_SUITE_P(
    Validation,
    InvalidEnumGroups,
    testing::Values(":ordering (missing) (value One :groups (A))",
                    ":ordering (A A) (value One :groups (A))",
                    ":ordering (A) (value One :groups (A B))",
                    ":ordering () (value One :groups (A))",
                    ":ordering unknown (value One)",
                    ":ordering priority (value One)",
                    "(value One :groups A)",
                    "(value One :groups (A A))",
                    "(value One :groups (bad-name))",
                    ":ordering (A) (value One :value 0) (value Two :value 1 :groups (A))",
                    ":ordering auto (value One :value 9 :groups (A))",
                    ":ordering (A) (value One :value \"1 << 2\" :groups (A))",
                    "(value One :value 0 :groups (A)) (value Two :value 0)",
                    ":enum-array true :ordering manual (value One :value 2 :groups (A))"));

TEST(EnumGroups, UnrealProjectionAndConversionsShareOrderedValues) {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
      (module test :header "Test.h" :namespace test
        (enum Example std::uint8_t :native-api true :helpers () :ordering (selected)
          (value Other) (value First :groups (selected)) (value Last :groups (selected))
          (projections (cpp) (unreal-projection EExample :header "UE.h"
            :header-include "UE.h" :conversion-header "Conversions.h"
            :native-header-include "Test.h" :enum-array true)))))"))};
    ASSERT_EQ(files.size(), 3U);
    for (auto const& file : files) {
        if (file.path.filename() == "Test.h" || file.path.filename() == "UE.h") {
            EXPECT_LT(file.content.find("First,"), file.content.find("Other"));
            EXPECT_NE(file.content.find("is_selected"), std::string::npos);
            auto const array_name{file.path.filename() == "Test.h" ? "example_selected_values"
                                                                   : "e_example_selected_values"};
            EXPECT_NE(file.content.find(array_name), std::string::npos);
        }
    }
}
}
}
