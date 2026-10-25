#include <codegen/schema/schema_property.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/lexer.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <codegen/sexpr/token_range.h>
#include <lispb/diagnostics.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace lispb::codegen::sexpr {
namespace {

struct Fixture {
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    Sources sources{strings, diagnostics};
    Syntax syntax{strings, sources};

    auto read(std::string_view path, std::string_view source) -> std::span<FormId const> {
        return syntax.forms(read_forms(syntax, sources.add(path, source)));
    }

    auto tokens(std::string_view path, std::string_view source) -> TokenRange {
        return lex(syntax, sources.add(path, source));
    }
};

TEST(SexprReader, GeneratedPropertyNamesSupportTypedLookup) {
    Fixture fixture;
    auto const form{fixture.read("fixture.lispb", "(range :minimum 2 :maximum 5)").front()};
    Parser parser{fixture.syntax};
    Fields fields{parser, form, "range", 0};

    fields.validate({SchemaProperty::minimum, SchemaProperty::maximum});
    EXPECT_EQ(parser.integer(fields.required(SchemaProperty::minimum), "minimum"), 2);
    EXPECT_EQ(fields.optional(SchemaProperty::maximum), fields.optional("maximum"));
    EXPECT_FALSE(fields.optional(SchemaProperty::step));
}

struct PropertyDiagnosticCase {
    char const* name;
    std::string_view source;
    std::string_view message;
};

using SexprPropertyDiagnostic = ::testing::TestWithParam<PropertyDiagnosticCase>;

TEST_P(SexprPropertyDiagnostic, PreservesDiagnosticTextAndPath) {
    auto const& test{GetParam()};
    SCOPED_TRACE(test.source);
    Fixture fixture;
    auto const form{fixture.read("fixture.lispb", test.source).front()};
    Parser parser{fixture.syntax};
    try {
        Fields fields{parser, form, "range", 0};
        fields.validate({SchemaProperty::minimum, SchemaProperty::maximum});
        static_cast<void>(fields.required(SchemaProperty::minimum));
        FAIL() << "Expected source error";
    } catch (SourceError const& error) {
        EXPECT_EQ(fixture.diagnostics.message(error.diagnostic()), test.message);
        EXPECT_EQ(fixture.sources.location(fixture.diagnostics.span(error.diagnostic())).path,
                  "fixture.lispb");
    }
}

INSTANTIATE_TEST_SUITE_P(
    InvalidProperties,
    SexprPropertyDiagnostic,
    ::testing::Values(
        PropertyDiagnosticCase{"Unknown", "(range :typo 1)", "unknown property ':typo'"},
        PropertyDiagnosticCase{"Disallowed", "(range :header 1)", "unknown property ':header'"},
        PropertyDiagnosticCase{
            "Duplicate", "(range :minimum 1 :minimum 2)", "duplicate property ':minimum'"},
        PropertyDiagnosticCase{
            "Missing", "(range :maximum 2)", "missing required property ':minimum'"},
        PropertyDiagnosticCase{
            "MissingValue", "(range :minimum)", "property ':minimum' requires a value"},
        PropertyDiagnosticCase{"NestedDeclaration",
                               "(range :minimum 1 (child))",
                               "unexpected nested declaration 'child'"}),
    [](auto const& info) { return info.param.name; });

TEST(SexprReader, ReadsNestedFormsAndPreservesSpans) {
    Fixture fixture;
    auto const forms{fixture.read("fixture.scm", "(outer\n  (inner value))")};
    ASSERT_EQ(forms.size(), 1);
    EXPECT_EQ(fixture.syntax.head(forms[0]), "outer");
    ASSERT_EQ(fixture.syntax.children(forms[0]).size(), 2);
    EXPECT_EQ(fixture.syntax.head(fixture.syntax.children(forms[0])[1]), "inner");
    auto const location{
        fixture.sources.location(fixture.syntax.token_span(fixture.syntax.children(forms[0])[1]))};
    EXPECT_EQ(location.path, "fixture.scm");
    EXPECT_EQ(location.line, 2);
    EXPECT_EQ(location.column, 3);
    EXPECT_EQ(fixture.sources.location(fixture.syntax.closing_span(forms[0])).column, 16);
    EXPECT_EQ(fixture.syntax.span(forms[0]).end_offset, 23);
}

TEST(SexprReader, ReadsEscapedStrings) {
    Fixture fixture;
    auto const forms{fixture.read("fixture.scm", R"((value "line\n\"quoted\""))")};
    ASSERT_EQ(forms.size(), 1);
    ASSERT_EQ(fixture.syntax.children(forms[0]).size(), 2);
    EXPECT_EQ(fixture.syntax.kind(fixture.syntax.children(forms[0])[1]), TokenKind::string);
    EXPECT_EQ(fixture.syntax.text(fixture.syntax.children(forms[0])[1]), "line\n\"quoted\"");
    EXPECT_EQ(fixture.syntax.span(fixture.syntax.children(forms[0])[1]).end_offset, 25);
}

TEST(SexprReader, ReadsOpaqueTaggedRawLiterals) {
    Fixture fixture;
    auto const source{R"lisp((value #cpp{if (ready) {
    auto const path = "C:\\generated\\file";
    // Braces in comments are opaque: { }
    use(path);
}}cpp#))lisp"};
    auto const forms{fixture.read("fixture.lispb", source)};

    ASSERT_EQ(forms.size(), 1);
    ASSERT_EQ(fixture.syntax.children(forms[0]).size(), 2);
    auto const& token{fixture.syntax.token(fixture.syntax.children(forms[0])[1])};
    EXPECT_EQ(fixture.syntax.kind(token), TokenKind::raw_literal);
    EXPECT_EQ(fixture.syntax.tag(token), "cpp");
    EXPECT_EQ(fixture.syntax.text(token),
              "if (ready) {\n"
              "    auto const path = \"C:\\\\generated\\\\file\";\n"
              "    // Braces in comments are opaque: { }\n"
              "    use(path);\n"
              "}");
}

TEST(SexprReader, PreservesRawLiteralWhitespaceAndSupportsGenericTags) {
    Fixture fixture;
    auto const forms{fixture.read("fixture.lispb", "#hlsl{\r\n  return Value;\r\n}hlsl#")};

    ASSERT_EQ(forms.size(), 1);
    EXPECT_EQ(fixture.syntax.kind(forms[0]), TokenKind::raw_literal);
    EXPECT_EQ(fixture.syntax.tag(forms[0]), "hlsl");
    EXPECT_EQ(fixture.syntax.text(forms[0]), "\r\n  return Value;\r\n");

    auto const empty{fixture.read("fixture.lispb", "#cpp{}cpp#")};
    ASSERT_EQ(empty.size(), 1);
    EXPECT_TRUE(fixture.syntax.text(empty[0]).empty());
}

TEST(SexprReader, PreservesSourcePositionAfterRawLiteral) {
    Fixture fixture;
    auto const tokens{fixture.tokens("fixture.lispb", "#cpp{first\nsecond}cpp#\n(next)")};

    ASSERT_GE(tokens.count, 3);
    auto const token{TokenId{tokens.offset + 1}};
    EXPECT_EQ(fixture.syntax.kind(token), TokenKind::left_parenthesis);
    EXPECT_EQ(fixture.sources.location(fixture.syntax.token_span(token)).line, 3);
    EXPECT_EQ(fixture.sources.location(fixture.syntax.token_span(token)).column, 1);
}

TEST(SexprReader, RawLiteralEndsAtFirstExactClosingDelimiter) {
    Fixture fixture;
    auto const tokens{fixture.tokens("fixture.lispb", "#cpp{before}cpp#after")};

    ASSERT_GE(tokens.count, 3);
    auto const raw{TokenId{tokens.offset}};
    auto const atom{TokenId{tokens.offset + 1}};
    EXPECT_EQ(fixture.syntax.kind(raw), TokenKind::raw_literal);
    EXPECT_EQ(fixture.syntax.text(raw), "before");
    EXPECT_EQ(fixture.syntax.token_span(raw).end_offset, 16);
    EXPECT_EQ(fixture.syntax.kind(atom), TokenKind::atom);
    EXPECT_EQ(fixture.syntax.text(atom), "after");
}

TEST(SexprReader, RejectsUnterminatedRawLiteralAtOpeningSpan) {
    Fixture fixture;
    try {
        static_cast<void>(fixture.read("fixture.lispb", "\n  #cpp{unterminated"));
        FAIL() << "Expected source error";
    } catch (SourceError const& error) {
        EXPECT_EQ(fixture.sources.location(fixture.diagnostics.span(error.diagnostic())).line, 2);
        EXPECT_EQ(fixture.sources.location(fixture.diagnostics.span(error.diagnostic())).column, 3);
        EXPECT_TRUE(std::string{error.what()}.contains("expected '}cpp#'"));
    }
}

TEST(SexprReader, DoesNotRecognizeCPlusPlusAsARawTag) {
    Fixture fixture;
    auto const tokens{fixture.tokens("fixture.lispb", "#c++{value}c++#")};

    ASSERT_GE(tokens.count, 2);
    auto const token{TokenId{tokens.offset}};
    EXPECT_EQ(fixture.syntax.kind(token), TokenKind::atom);
    EXPECT_EQ(fixture.syntax.text(token), "#c++{value}c++#");
}

struct InvalidParenthesisCase {
    char const* name;
    std::string_view source;
};

using SexprParenthesis = ::testing::TestWithParam<InvalidParenthesisCase>;

TEST_P(SexprParenthesis, RejectsInvalidClosingParenthesis) {
    Fixture fixture;
    EXPECT_THROW(static_cast<void>(fixture.read("fixture.scm", GetParam().source)), SourceError);
}

INSTANTIATE_TEST_SUITE_P(InvalidForms,
                         SexprParenthesis,
                         ::testing::Values(InvalidParenthesisCase{"Missing", "(outer (inner)"},
                                           InvalidParenthesisCase{"Unexpected", ")"}),
                         [](auto const& info) { return info.param.name; });

}
}
