#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

namespace codegen {
namespace {

auto render_fixed(std::vector<SoaSchema> schemas,
                  std::map<std::string, CppType> types = {}) -> std::string {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .types = std::move(types),
        .modules = {SoaModuleSchema{
            .settings = ModuleSettings{
                .name = "fixed", .header = "Fixed.h", .source = "Fixed.cpp"},
            .structs = std::move(schemas),
        }},
    }))};
    EXPECT_EQ(files.size(), 2);
    return files.front().content;
}

auto occurrences(std::string const& text, std::string const& value) -> std::size_t {
    std::size_t count{};
    auto position{text.find(value)};
    while (position != std::string::npos) {
        ++count;
        position = text.find(value, position + value.size());
    }
    return count;
}

auto child_schema() -> SoaSchema {
    return SoaSchema{
        .name = "FChild",
        .members = {
            SoaMemberSchema{"names", SoaMemberKind::array, TypeRef{"FString"}},
            SoaMemberSchema{"weights", SoaMemberKind::array, TypeRef{"float"}},
        },
        .fixed = FixedSoaSchema{"TChildStorage", {}},
    };
}

auto parent_schema(std::vector<std::string> containers = {"TFixedRows"}) -> SoaSchema {
    return SoaSchema{
        .name = "FRows",
        .members = {
            SoaMemberSchema{"ids", SoaMemberKind::array, TypeRef{"int32"}},
            SoaMemberSchema{"children", SoaMemberKind::nested, TypeRef{"@child"}, "FChild"},
        },
        .equivalent_type = TypeRef{"FRow"},
        .fixed = FixedSoaSchema{"TRowsStorage", std::move(containers)},
    };
}

auto fixed_types() -> std::map<std::string, CppType> {
    return {{"child", CppType{"FChild"}}};
}

TEST(FixedSoaLowering, PreservesRecursiveLeafOrderAndNestedStorage) {
    auto const header{
        render_fixed({child_schema(), parent_schema()}, fixed_types())};

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
    auto const header{render_fixed(
        {child_schema(), parent_schema({"TFirstRows", "TSecondRows"})}, fixed_types())};

    EXPECT_EQ(occurrences(header, "size_type size_{};"), 2);
    for (auto const* container : {"TFirstRows", "TSecondRows"}) {
        EXPECT_NE(header.find(std::string{"struct "} + container), std::string::npos);
        EXPECT_NE(header.find(std::string{container} + "(" + container + " const& other)"),
                  std::string::npos);
        EXPECT_NE(header.find(std::string{container} + "(" + container + "&& other)"),
                  std::string::npos);
        EXPECT_NE(header.find(std::string{"~"} + container + "() { reset(); }"),
                  std::string::npos);
    }
    EXPECT_NE(header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_NE(header.find("= delete;"), std::string::npos);
}

TEST(FixedSoaLowering, EmitsCheckedEquivalentAccessAndBoundedMutation) {
    auto const header{
        render_fixed({child_schema(), parent_schema()}, fixed_types())};

    EXPECT_NE(header.find("using equivalent_type = FRow;"), std::string::npos);
    EXPECT_NE(header.find("auto operator[](size_type const index) const -> equivalent_type"),
              std::string::npos);
    EXPECT_NE(header.find("check_index(index);\n        return (*this)[index];"),
              std::string::npos);
    EXPECT_NE(header.find("check(count <= capacity() - size_);"), std::string::npos);
    EXPECT_NE(header.find("check(index + count <= size_);"), std::string::npos);
    EXPECT_NE(header.find("destroy_from(size_ - count);"), std::string::npos);
}

TEST(FixedSoaLowering, GeneratesTrivialOnlyUninitialisedOperations) {
    auto const header{
        render_fixed({child_schema(), parent_schema()}, fixed_types())};

    EXPECT_NE(header.find("std::is_trivially_copyable_v<int32>"), std::string::npos);
    EXPECT_NE(header.find("std::is_trivially_copyable_v<FString>"), std::string::npos);
    EXPECT_NE(header.find("void set_num_uninitialised(size_type const new_size)"),
              std::string::npos);
    EXPECT_NE(header.find("void add_uninitialised(size_type const count)"),
              std::string::npos);
}

} // namespace
} // namespace codegen
