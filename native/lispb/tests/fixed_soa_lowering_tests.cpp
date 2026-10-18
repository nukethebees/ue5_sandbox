#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lispb::codegen {
namespace {

auto render_fixed(CompilerTestContext& context,
                  std::span<std::string_view const> const containers = {}) -> std::string {
    std::string source{R"((module fixed :header "Fixed.h" :source "Fixed.cpp"
        (struct FChild
            (member names array FString)
            (member weights array float)
            (fixed TChildStorage))
        (struct FRows :equivalent-type FRow
            (member ids array int32)
            (member children nested @child :fixed-schema FChild)
            (fixed TRowsStorage :containers ()"};
    if (containers.empty()) {
        source += "TFixedRows";
    } else {
        for (auto const container : containers) {
            source += std::string{container} + " ";
        }
    }
    source += "))))";
    auto const files{render_test_manifest(
        context,
        parse_test_manifest(
            context, source, {{"child", context.storage.cpp_types.add("FChild")}}))};
    EXPECT_EQ(files.size(), 2);
    return files.front().content;
}

TEST(FixedSoaLowering, PreservesRecursiveLeafOrderAndNestedStorage) {
    CompilerTestContext context;
    auto const header{render_fixed(context)};

    EXPECT_NE(header.find("TChildStorage<Capacity> children_;"), std::string::npos);
    EXPECT_NE(header.find("template <typename TArg0, typename TArg1, typename TArg2>"),
              std::string::npos);
    EXPECT_NE(header.find("new_ids, TArg1&& new_children_names, "
                          "TArg2&& new_children_weights"),
              std::string::npos);
    EXPECT_NE(header.find("children_.construct_at(index, "
                          "std::forward<TArg1>(new_children_names), "
                          "std::forward<TArg2>(new_children_weights));"),
              std::string::npos);

    auto const destroy_children{header.find("children_.destroy_at(index);")};
    auto const destroy_ids{header.find("ids_.destroy_at(index);", destroy_children)};
    ASSERT_NE(destroy_children, std::string::npos);
    ASSERT_NE(destroy_ids, std::string::npos);
    EXPECT_LT(destroy_children, destroy_ids);
}

TEST(FixedSoaLowering, GivesEachOwningContainerOneSizeAndFullValueSemantics) {
    CompilerTestContext context;
    auto const header{
        render_fixed(context, std::array<std::string_view, 2>{"TFirstRows", "TSecondRows"})};

    EXPECT_EQ(occurrences(header, "size_type size_{};"), 2);
    for (auto const* container : {"TFirstRows", "TSecondRows"}) {
        EXPECT_NE(header.find(std::string{"struct "} + container), std::string::npos);
        EXPECT_NE(header.find(std::string{container} + "(" + container + " const& other)"),
                  std::string::npos);
        EXPECT_NE(header.find(std::string{container} + "(" + container + "&& other)"),
                  std::string::npos);
        EXPECT_NE(header.find(std::string{"~"} + container + "() { reset(); }"), std::string::npos);
    }
    EXPECT_NE(header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_NE(header.find("= delete;"), std::string::npos);
}

TEST(FixedSoaLowering, EmitsCheckedEquivalentAccessAndBoundedMutation) {
    CompilerTestContext context;
    auto const header{render_fixed(context)};

    EXPECT_NE(header.find("using equivalent_type = FRow;"), std::string::npos);
    EXPECT_NE(header.find("auto operator[](size_type const index) const -> equivalent_type"),
              std::string::npos);
    EXPECT_NE(header.find("check_index(index);\n        return (*this)[index];"),
              std::string::npos);
    EXPECT_NE(header.find("check(count <= capacity() - size_);"), std::string::npos);
    EXPECT_NE(header.find("check(index + count <= size_);"), std::string::npos);
    EXPECT_NE(header.find("destroy_from(size_ - count);"), std::string::npos);
}

TEST(FixedSoaLowering, GeneratesNamedCapabilitiesForFixedOperations) {
    CompilerTestContext context;
    auto const header{render_fixed(context)};

    EXPECT_NE(header.find("inline static constexpr bool supports_copy_construction"),
              std::string::npos);
    EXPECT_NE(header.find("inline static constexpr bool supports_move_construction"),
              std::string::npos);
    EXPECT_NE(header.find("inline static constexpr bool supports_nothrow_move_construction"),
              std::string::npos);
    EXPECT_NE(header.find("inline static constexpr bool supports_default_construction"),
              std::string::npos);
    EXPECT_NE(header.find("inline static constexpr bool supports_uninitialised_storage"),
              std::string::npos);
    EXPECT_NE(header.find("inline static constexpr bool supports_element_construction_from"),
              std::string::npos);
    EXPECT_EQ(occurrences(header, "std::is_trivially_copyable_v<int32>"), 1);
    EXPECT_EQ(occurrences(header, "std::is_trivially_copyable_v<FString>"), 1);
    EXPECT_EQ(occurrences(header, "std::is_trivially_destructible_v<int32>"), 1);
    EXPECT_EQ(occurrences(header, "std::is_trivially_destructible_v<FString>"), 1);
    EXPECT_EQ(occurrences(header, "requires supports_uninitialised_storage"), 3);
    EXPECT_EQ(occurrences(header, "requires supports_default_construction"), 3);
    EXPECT_EQ(occurrences(header, "requires supports_element_construction_from<"), 2);
    EXPECT_NE(header.find("noexcept(supports_nothrow_move_construction)"), std::string::npos);
    EXPECT_NE(header.find("void set_num_uninitialised(size_type const new_size)"),
              std::string::npos);
    EXPECT_NE(header.find("void add_uninitialised(size_type const count)"), std::string::npos);
}

} // namespace
} // namespace lispb::codegen
