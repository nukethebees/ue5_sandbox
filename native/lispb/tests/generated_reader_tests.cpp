#include <ioj/s7/interpreter.h>

#include <gtest/gtest.h>
#include <ReaderDependency_reader.h>

namespace lispb::tests {
using namespace ioj::s7;
using namespace codegen_compile_fixture;

template <typename T>
auto read_source(std::string_view const source) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read<T>(Reader{*ast}, ast->root, "record");
}

auto read_record_source(std::string_view const source) -> ReadResult<ReadableRecord> {
    return read_source<ReadableRecord>(source);
}

TEST(GeneratedS7Reader, ReadsEmptyRecursiveAndCrossModuleRecords) {
    EXPECT_EQ(*read_source<ReaderEnum>("'MixedCase"), ReaderEnum::Mixed);
    EXPECT_EQ(*read_source<ReaderEnum>("'mixedcase"), ReaderEnum::Mixed);
    EXPECT_TRUE(read_source<EmptyReadable>("'(:empty)"));
    EXPECT_FALSE(read_source<EmptyReadable>("'(:empty :extra 1)"));
    auto const recursive{read_source<RecursiveReadable>(
        "'(:recursive :children ((:recursive :children ((:recursive)))))")};
    ASSERT_TRUE(recursive);
    ASSERT_EQ(recursive->children.size(), 1u);
    EXPECT_EQ(recursive->children.front().children.size(), 1u);
    auto const dependency{
        read_source<ReaderDependency>(R"('(:dependent :child (:child :name "nested")))")};
    ASSERT_TRUE(dependency);
    EXPECT_EQ(dependency->child.name.get_ref(), "nested");
}

TEST(GeneratedS7Reader, PreservesExactIntegerBounds) {
    auto const minimum{read_source<BoundedInteger>("'(:integer :value -9223372036854775808)")};
    ASSERT_TRUE(minimum);
    EXPECT_EQ(minimum->value, std::numeric_limits<std::int64_t>::min());
    auto const maximum{read_source<BoundedInteger>("'(:integer :value 9223372036854775806)")};
    ASSERT_TRUE(maximum);
    EXPECT_EQ(maximum->value, std::numeric_limits<std::int64_t>::max() - 1);
    EXPECT_FALSE(read_source<BoundedInteger>("'(:integer :value 9223372036854775807)"));
}

TEST(GeneratedS7Reader, ComposesRecordsContainersEnumsAndConstrainedValues) {
    auto const result{read_record_source(R"('(:record :child (:child :name "root")
        :values ((:child :name "first" :count 0) (:child :name "second"))
        :samples (1 2 3) :enabled #f :maybe :none :mode readable_name))")};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->child.name.get_ref(), "root");
    EXPECT_EQ(result->child.count, 2);
    ASSERT_EQ(result->values.size(), 2u);
    EXPECT_EQ(result->values[0].count, 0);
    EXPECT_EQ(result->values[1].name.get_ref(), "second");
    EXPECT_EQ(result->points, (std::array{1.f, 2.f, 3.f}));
    EXPECT_FALSE(result->enabled);
    EXPECT_FALSE(result->maybe);
    EXPECT_EQ(result->mode, EPlainFixture::ReadableName);
    EXPECT_EQ(result->title, "untitled");
    EXPECT_EQ(result->scale, 1.f);
}

TEST(GeneratedS7Reader, PreservesDefaultsAndInclusiveConstraints) {
    for (auto const scale : {"0.5", "5"}) {
        SCOPED_TRACE(scale);
        auto const result{read_record_source(
            std::format(R"('(:record :child (:child :name "root") :scale {}))", scale))};
        ASSERT_TRUE(result);
        EXPECT_EQ(result->maybe, 7);
        EXPECT_TRUE(result->values.empty());
    }
}

TEST(GeneratedS7Reader, ReportsStructuralAndValueErrorsWithFieldPaths) {
    struct Case {
        std::string_view fields;
        AstErrorCode code;
        std::string_view path;
    };
    for (auto const& test :
         {Case{":child (:child)", AstErrorCode::MissingProperty, "record.child.name"},
          Case{":maybe", AstErrorCode::MissingValue, "record[3]"},
          Case{":unknown 1", AstErrorCode::UnknownProperty, "record[3]"},
          Case{":enabled #t :enabled #f", AstErrorCode::DuplicateProperty, "record[5]"},
          Case{":scale 0", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale +inf.0", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale 6", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale 0.25", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":offset 0", AstErrorCode::InvalidNumber, "record.offset"},
          Case{":title \"\"", AstErrorCode::InvalidType, "record.title"},
          Case{":samples (1 2)", AstErrorCode::InvalidType, "record.samples"},
          Case{":values ((:child :name \"\"))", AstErrorCode::InvalidType, "record.values[0].name"},
          Case{":mode unknown", AstErrorCode::UnknownSymbol, "record.mode"}}) {
        SCOPED_TRACE(test.fields);
        auto const source{
            test.fields.starts_with(":child")
                ? std::format("'(:record {})", test.fields)
                : std::format(R"('(:record :child (:child :name "root") {}))", test.fields)};
        auto const result{read_record_source(source)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(std::ranges::any_of(result.error(), [&](AstDiagnostic const& error) {
            return error.code == test.code && error.node_path == test.path &&
                   !error.message.empty();
        }));
    }
    auto const absent{read_record_source("'(:record)")};
    ASSERT_FALSE(absent);
    EXPECT_EQ(absent.error().front().node_path, "record.child");
    EXPECT_FALSE(read_record_source("'(:wrong)"));
}
}
