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

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

struct RenderedHomogeneousModule {
    std::string header;
    std::string source;
};

auto render_homogeneous(
    CompilerTestContext& context,
    std::string_view const layouts = R"((layout Values :components (xs ys)
        :export-specifier PROJECT_API (value-type float f)))",
    std::string_view const settings = {},
    std::vector<std::pair<std::string_view, CppTypeId>> const& registrations = {})
    -> RenderedHomogeneousModule {
    auto const source{R"((module values :header "Values.h" :source "Values.cpp"
        :header-include "Project/Values.h" )" +
                      std::string{settings} + " " + std::string{layouts} + ")"};
    auto const files{
        render_test_manifest(context, parse_test_manifest(context, source, registrations))};
    EXPECT_EQ(files.size(), 2);
    return {files[0].content, files[1].content};
}

TEST(HomogeneousLowering, EmitsCompleteViewAndStorageApisForEveryComponent) {
    CompilerTestContext context;
    auto const output{render_homogeneous(context)};

    EXPECT_NE(output.header.find("template <typename T>\nstruct TValuesView"), std::string::npos);
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
    EXPECT_NE(output.header.find("auto add(value_type const x, value_type const y) -> "
                                 "size_type"),
              std::string::npos);

    EXPECT_NE(output.source.find("#include \"Project/Values.h\""), std::string::npos);
    EXPECT_NE(output.source.find("void FValuesf::apply_permutation"), std::string::npos);
    EXPECT_NE(output.source.find("validate_array_sizes();\n    check(indices.Num() == num());"),
              std::string::npos);
    EXPECT_NE(output.source.find("ml::apply_permutation(xs, indices);"), std::string::npos);
    EXPECT_NE(output.source.find("ml::apply_permutation(ys, indices);"), std::string::npos);
}

TEST(HomogeneousLowering, EmitsEquivalentAndInputTypeApis) {
    CompilerTestContext context;
    auto const output{render_homogeneous(
        context,
        R"(
        (layout Values :components (xs ys) :input-members (U V) :export-specifier PROJECT_API
            (value-type float f :equivalent-type @vector :input-types (@vector @point))))",
        {},
        {{"vector",
          context.storage.cpp_types.add(
              "FVector2f",
              {context.storage.cpp_types.dependency("FVector2f", "Project/Vector.h")})},
         {"point",
          context.storage.cpp_types.add(
              "FPoint2f",
              {context.storage.cpp_types.dependency("FPoint2f", "Project/Point.h")})}})};

    EXPECT_NE(output.header.find("template <typename T>\nstruct TValuesEquivalentType;"),
              std::string::npos);
    EXPECT_NE(output.header.find("struct TValuesEquivalentType<float>"), std::string::npos);
    EXPECT_NE(output.header.find("using type = FVector2f;"), std::string::npos);
    EXPECT_NE(output.header.find(
                  "using equivalent_type = typename TValuesEquivalentType<value_type>::type;"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto operator[](size_type const index) const -> "
                                 "equivalent_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("using equivalent_type = FVector2f;"), std::string::npos);
    EXPECT_NE(output.header.find("auto add(value_type const x, value_type const y) -> "
                                 "size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto add(FVector2f const& value) -> size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto add(FPoint2f const& value) -> size_type"),
              std::string::npos);
    EXPECT_NE(output.header.find("void set(size_type const index, value_type const x, "
                                 "value_type const y) const"),
              std::string::npos);
    EXPECT_NE(output.header.find("requires (!std::is_const_v<T>)"), std::string::npos);
    EXPECT_NE(
        output.header.find("void set(size_type const index, equivalent_type const& value) const"),
        std::string::npos);
    EXPECT_NE(output.header.find("void set(size_type const index, FVector2f const& value)"),
              std::string::npos);
    EXPECT_NE(output.header.find("set(index, value.U, value.V);"), std::string::npos);
    EXPECT_NE(output.header.find("return add(value.U, value.V);"), std::string::npos);
    EXPECT_NE(output.header.find("#include \"Project/Vector.h\""), std::string::npos);
    EXPECT_NE(output.header.find("#include \"Project/Point.h\""), std::string::npos);
}

TEST(HomogeneousLowering, EmitsEveryLayoutAndValueTypeExactlyOnce) {
    CompilerTestContext context;
    auto const output{render_homogeneous(context, R"(
        (layout Values :components (xs ys) :export-specifier PROJECT_API
            (value-type float f) (value-type double d))
        (layout Scalars :components (values) (value-type int32 i32)))")};

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
    CompilerTestContext context;
    auto const output{render_homogeneous(
        context,
        R"((layout Values :components (xs ys) :export-specifier PROJECT_API (value-type float f)))",
        R"(:namespace project::generated :prelude ("class FForward;") :include-order ("Project/"))")};

    EXPECT_NE(output.header.find("class FForward;"), std::string::npos);
    EXPECT_NE(output.header.find("namespace project::generated {"), std::string::npos);
    EXPECT_NE(output.header.find("} // namespace project::generated"), std::string::npos);
    EXPECT_NE(output.source.find("namespace project::generated {"), std::string::npos);
    EXPECT_NE(output.source.find("} // namespace project::generated"), std::string::npos);
}

} // namespace
} // namespace lispb::codegen
