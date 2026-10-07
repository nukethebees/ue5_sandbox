#include <ioj/s7/ast.h>
#include <ioj/s7/interpreter.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace ioj::s7::tests {
TEST(SchemeAst, OwnsValuesAfterInterpreterAndSourceAreDestroyed) {
    Ast ast;
    {
        Interpreter interpreter;
        std::string source{
            "(list 'symbol :keyword \"text\" 9007199254740993 2/3 1.25 #t '() '(a (b)))"};
        auto result{interpreter.evaluate_ast(source)};
        ASSERT_TRUE(result);
        ast = std::move(*result);
    }

    auto moved{std::move(ast)};
    auto const values{moved.children(moved.root)};
    ASSERT_EQ(values.size(), 9u);
    EXPECT_EQ(moved.node(values[0]).kind, NodeKind::Symbol);
    EXPECT_EQ(moved.text(values[0]), "symbol");
    EXPECT_EQ(moved.node(values[1]).kind, NodeKind::Keyword);
    EXPECT_EQ(moved.text(values[1]), "keyword");
    EXPECT_EQ(moved.node(values[2]).kind, NodeKind::String);
    EXPECT_EQ(moved.text(values[2]), "text");
    EXPECT_EQ(moved.node(values[3]).kind, NodeKind::Integer);
    EXPECT_EQ(moved.node(values[3]).integer, INT64_C(9007199254740993));
    EXPECT_EQ(moved.node(values[4]).kind, NodeKind::Ratio);
    EXPECT_EQ(moved.node(values[4]).ratio.numerator, 2);
    EXPECT_EQ(moved.node(values[4]).ratio.denominator, 3);
    EXPECT_EQ(moved.node(values[5]).kind, NodeKind::Real);
    EXPECT_DOUBLE_EQ(moved.node(values[5]).real, 1.25);
    EXPECT_TRUE(moved.node(values[6]).boolean);
    EXPECT_TRUE(moved.children(values[7]).empty());
    auto const nested{moved.children(values[8])};
    ASSERT_EQ(nested.size(), 2u);
    EXPECT_EQ(moved.text(moved.children(nested[1])[0]), "b");
}

TEST(SchemeAst, RejectsUnsupportedAndMalformedData) {
    Interpreter interpreter;
    struct Case {
        std::string_view source;
        AstErrorCode code;
    };
    Case const cases[]{
        {"(lambda () 1)", AstErrorCode::UnsupportedValue},
        {"(vector 1 2)", AstErrorCode::UnsupportedValue},
        {"'(1 . 2)", AstErrorCode::ImproperList},
        {"(let ((x (list 1))) (set-cdr! x x) x)", AstErrorCode::CyclicStructure},
        {"(let ((x (list 1))) (set-car! x x) x)", AstErrorCode::CyclicStructure},
        {"(error 'test \"bad source\")", AstErrorCode::EvaluationFailed},
    };
    for (auto const& test : cases) {
        auto const result{interpreter.evaluate_ast(test.source)};
        ASSERT_FALSE(result) << test.source;
        ASSERT_EQ(result.error().size(), 1u);
        EXPECT_EQ(result.error()[0].code, test.code) << test.source;
        EXPECT_FALSE(result.error()[0].node_path.empty());
        EXPECT_FALSE(result.error()[0].message.empty());
    }
    EXPECT_TRUE(interpreter.evaluate_ast("'(1 2)"));
}

TEST(SchemeAst, AcceptsSharedAcyclicLists) {
    Interpreter interpreter;
    auto const result{interpreter.evaluate_ast("(let ((tail (list 'x))) (cons tail tail))")};
    ASSERT_TRUE(result);
    auto const values{result->children(result->root)};
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(result->text(result->children(values[0])[0]), "x");
    EXPECT_EQ(result->text(values[1]), "x");
}

TEST(SchemeAst, EnforcesLimitsAtBoundaries) {
    Interpreter interpreter;
    auto limits{
        AstLimits{.max_depth = 1, .max_nodes = 3, .max_child_indices = 2, .max_text_bytes = 2}};
    EXPECT_TRUE(interpreter.evaluate_ast("'(a b)", limits));
    for (auto const source : {"'((a))", "'(a b c)", "\"abc\""}) {
        auto const result{interpreter.evaluate_ast(source, limits)};
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error()[0].code, AstErrorCode::LimitExceeded);
    }
    limits.max_nodes = 0;
    EXPECT_FALSE(interpreter.evaluate_ast("'()", limits));
}

TEST(SchemeAst, PreservesEmbeddedNullsInStrings) {
    Interpreter interpreter;
    auto const result{interpreter.evaluate_ast("(string #\\a (integer->char 0) #\\b)")};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->text(result->root), std::string_view("a\0b", 3));
}
}
