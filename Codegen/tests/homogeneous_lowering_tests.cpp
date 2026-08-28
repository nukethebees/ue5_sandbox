#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

namespace codegen {
namespace {

struct RenderedHomogeneousModule {
    std::string header;
    std::string source;
};

auto render_homogeneous(HomogeneousModuleSchema module,
                        std::map<std::string, CppType> types = {})
    -> RenderedHomogeneousModule {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = 1,
        .types = std::move(types),
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 2);
    return RenderedHomogeneousModule{files[0].content, files[1].content};
}

auto homogeneous_module(std::vector<HomogeneousValueSchema> value_types = {
                            HomogeneousValueSchema{TypeRef{"float"}, "f"}})
    -> HomogeneousModuleSchema {
    return HomogeneousModuleSchema{
        .settings = ModuleSettings{
            .name = "values",
            .header = "Values.h",
            .source = "Values.cpp",
            .header_include = "Project/Values.h",
        },
        .layouts = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs", "ys"},
            .value_types = std::move(value_types),
            .export_specifier = "PROJECT_API",
        }},
    };
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

TEST(HomogeneousLowering, EmitsCompleteViewAndStorageApisForEveryComponent) {
    auto const output{render_homogeneous(homogeneous_module())};

    EXPECT_NE(output.header.find("template <typename T>\nstruct TValuesView"),
              std::string::npos);
    EXPECT_NE(output.header.find("TArrayView<T> xs;"), std::string::npos);
    EXPECT_NE(output.header.find("TArrayView<T> ys;"), std::string::npos);
    EXPECT_NE(output.header.find("return TValuesView{xs.Slice(offset, count), "
                                 "ys.Slice(offset, count)};"),
              std::string::npos);
    EXPECT_NE(output.header.find("struct PROJECT_API FValuesf"), std::string::npos);
    EXPECT_NE(output.header.find("check(ys.Num() == xs.Num());"), std::string::npos);
    EXPECT_NE(output.header.find("xs[dst_i] = src.xs[src_i];"), std::string::npos);
    EXPECT_NE(output.header.find("ys[dst_i] = src.ys[src_i];"), std::string::npos);
    EXPECT_NE(output.header.find("xs.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("ys.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("validate_array_sizes();\n        auto const n{num()};"),
              std::string::npos);

    EXPECT_NE(output.source.find("#include \"Project/Values.h\""), std::string::npos);
    EXPECT_NE(output.source.find("void FValuesf::apply_permutation"), std::string::npos);
    EXPECT_NE(output.source.find("validate_array_sizes();\n    check(indices.Num() == num());"),
              std::string::npos);
    EXPECT_NE(output.source.find("ml::apply_permutation(xs, indices);"),
              std::string::npos);
    EXPECT_NE(output.source.find("ml::apply_permutation(ys, indices);"),
              std::string::npos);
}

TEST(HomogeneousLowering, EmitsEquivalentAndInputTypeApis) {
    auto module{homogeneous_module({HomogeneousValueSchema{
        .type = TypeRef{"float"},
        .suffix = "f",
        .equivalent_type = TypeRef{"@vector"},
        .input_types = {TypeRef{"@vector"}, TypeRef{"@point"}},
    }})};
    auto const output{render_homogeneous(
        std::move(module),
        {{"vector", CppType{"FVector2f", "Project/Vector.h"}},
         {"point", CppType{"FPoint2f", "Project/Point.h"}}})};

    EXPECT_NE(output.header.find("template <typename T>\nstruct TValuesEquivalentType;"),
              std::string::npos);
    EXPECT_NE(output.header.find("struct TValuesEquivalentType<float>"),
              std::string::npos);
    EXPECT_NE(output.header.find("using type = FVector2f;"), std::string::npos);
    EXPECT_NE(output.header.find(
                  "using equivalent_type = typename TValuesEquivalentType<value_type>::type;"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto operator[](size_type const index) const -> "
                                 "equivalent_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("using equivalent_type = FVector2f;"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto add(value_type const x, value_type const y) -> "
                                 "size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto add(FVector2f const& value) -> size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto add(FPoint2f const& value) -> size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("return add(value.X, value.Y);"), std::string::npos);
    EXPECT_NE(output.header.find("#include \"Project/Vector.h\""), std::string::npos);
    EXPECT_NE(output.header.find("#include \"Project/Point.h\""), std::string::npos);
}

TEST(HomogeneousLowering, EmitsEveryLayoutAndValueTypeExactlyOnce) {
    auto module{homogeneous_module({HomogeneousValueSchema{TypeRef{"float"}, "f"},
                                    HomogeneousValueSchema{TypeRef{"double"}, "d"}})};
    module.layouts.push_back(HomogeneousLayoutSchema{
        .name = "Scalars",
        .components = {"values"},
        .value_types = {HomogeneousValueSchema{TypeRef{"int32"}, "i32"}},
    });

    auto const output{render_homogeneous(std::move(module))};

    EXPECT_EQ(occurrences(output.header, "struct TValuesView"), 1);
    EXPECT_EQ(occurrences(output.header, "struct TScalarsView"), 1);
    EXPECT_EQ(occurrences(output.header, "struct PROJECT_API FValuesf"), 1);
    EXPECT_EQ(occurrences(output.header, "struct PROJECT_API FValuesd"), 1);
    EXPECT_EQ(occurrences(output.header, "struct FScalarsi32"), 1);
    EXPECT_EQ(occurrences(output.source, "void FValuesf::apply_permutation"), 1);
    EXPECT_EQ(occurrences(output.source, "void FValuesd::apply_permutation"), 1);
    EXPECT_EQ(occurrences(output.source, "void FScalarsi32::apply_permutation"), 1);
}

TEST(HomogeneousLowering, AppliesPreludeNamespaceAndIncludeOrdering) {
    auto module{homogeneous_module()};
    module.settings.namespace_name = "project::generated";
    module.settings.prelude_lines = {"class FForward;"};
    module.settings.include_order = {"Project/"};

    auto const output{render_homogeneous(std::move(module))};

    EXPECT_NE(output.header.find("class FForward;"), std::string::npos);
    EXPECT_NE(output.header.find("namespace project::generated {"), std::string::npos);
    EXPECT_NE(output.header.find("} // namespace project::generated"), std::string::npos);
    EXPECT_NE(output.source.find("namespace project::generated {"), std::string::npos);
    EXPECT_NE(output.source.find("} // namespace project::generated"), std::string::npos);
}

} // namespace
} // namespace codegen
