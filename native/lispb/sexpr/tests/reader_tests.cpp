#include <codegen/sexpr/lexer.h>
#include <codegen/sexpr/reader.h>

#include <gtest/gtest.h>

namespace codegen::sexpr {
namespace {

TEST(SexprReader, ReadsNestedFormsAndPreservesSpans) {
    auto const forms{read_forms("fixture.scm", "(outer\n  (inner value))")};
    ASSERT_EQ(forms.size(), 1);
    EXPECT_EQ(forms[0].head(), "outer");
    ASSERT_EQ(forms[0].children.size(), 2);
    EXPECT_EQ(forms[0].children[1].head(), "inner");
    EXPECT_EQ(forms[0].children[1].token.span.path, "fixture.scm");
    EXPECT_EQ(forms[0].children[1].token.span.line, 2);
    EXPECT_EQ(forms[0].children[1].token.span.column, 3);
    EXPECT_EQ(forms[0].closing.span.column, 16);
}

TEST(SexprReader, ReadsEscapedStrings) {
    auto const forms{read_forms("fixture.scm", R"((value "line\n\"quoted\""))")};
    ASSERT_EQ(forms.size(), 1);
    ASSERT_EQ(forms[0].children.size(), 2);
    EXPECT_EQ(forms[0].children[1].token.kind, TokenKind::string);
    EXPECT_EQ(forms[0].children[1].token.text, "line\n\"quoted\"");
}

TEST(SexprReader, ReadsOpaqueTaggedRawLiterals) {
    auto const source{R"lisp((value #cpp{if (ready) {
    auto const path = "C:\\generated\\file";
    // Braces in comments are opaque: { }
    use(path);
}}cpp#))lisp"};
    auto const forms{read_forms("fixture.lispb", source)};

    ASSERT_EQ(forms.size(), 1);
    ASSERT_EQ(forms[0].children.size(), 2);
    auto const& token{forms[0].children[1].token};
    EXPECT_EQ(token.kind, TokenKind::raw_literal);
    EXPECT_EQ(token.tag, "cpp");
    EXPECT_EQ(token.text,
              "if (ready) {\n"
              "    auto const path = \"C:\\\\generated\\\\file\";\n"
              "    // Braces in comments are opaque: { }\n"
              "    use(path);\n"
              "}");
}

TEST(SexprReader, PreservesRawLiteralWhitespaceAndSupportsGenericTags) {
    auto const forms{read_forms("fixture.lispb", "#hlsl{\r\n  return Value;\r\n}hlsl#")};

    ASSERT_EQ(forms.size(), 1);
    EXPECT_EQ(forms[0].token.kind, TokenKind::raw_literal);
    EXPECT_EQ(forms[0].token.tag, "hlsl");
    EXPECT_EQ(forms[0].token.text, "\r\n  return Value;\r\n");

    auto const empty{read_forms("fixture.lispb", "#cpp{}cpp#")};
    ASSERT_EQ(empty.size(), 1);
    EXPECT_TRUE(empty[0].token.text.empty());
}

TEST(SexprReader, PreservesSourcePositionAfterRawLiteral) {
    auto const tokens{lex("fixture.lispb", "#cpp{first\nsecond}cpp#\n(next)")};

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[1].kind, TokenKind::left_parenthesis);
    EXPECT_EQ(tokens[1].span.line, 3);
    EXPECT_EQ(tokens[1].span.column, 1);
}

TEST(SexprReader, RawLiteralEndsAtFirstExactClosingDelimiter) {
    auto const tokens{lex("fixture.lispb", "#cpp{before}cpp#after")};

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::raw_literal);
    EXPECT_EQ(tokens[0].text, "before");
    EXPECT_EQ(tokens[1].kind, TokenKind::atom);
    EXPECT_EQ(tokens[1].text, "after");
}

TEST(SexprReader, RejectsUnterminatedRawLiteralAtOpeningSpan) {
    try {
        static_cast<void>(read_forms("fixture.lispb", "\n  #cpp{unterminated"));
        FAIL() << "Expected source error";
    } catch (SourceError const& error) {
        EXPECT_EQ(error.span().line, 2);
        EXPECT_EQ(error.span().column, 3);
        EXPECT_TRUE(std::string{error.what()}.contains("expected '}cpp#'"));
    }
}

TEST(SexprReader, DoesNotRecognizeCPlusPlusAsARawTag) {
    auto const tokens{lex("fixture.lispb", "#c++{value}c++#")};

    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, TokenKind::atom);
    EXPECT_EQ(tokens[0].text, "#c++{value}c++#");
}

TEST(SexprReader, RejectsMissingClosingParenthesis) {
    EXPECT_THROW(static_cast<void>(read_forms("fixture.scm", "(outer (inner)")), SourceError);
}

TEST(SexprReader, RejectsUnexpectedClosingParenthesis) {
    EXPECT_THROW(static_cast<void>(read_forms("fixture.scm", ")")), SourceError);
}

}
}
