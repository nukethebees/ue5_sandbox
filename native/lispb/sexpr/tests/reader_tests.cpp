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

TEST(SexprReader, RejectsMissingClosingParenthesis) {
    EXPECT_THROW(static_cast<void>(read_forms("fixture.scm", "(outer (inner)")), SourceError);
}

TEST(SexprReader, RejectsUnexpectedClosingParenthesis) {
    EXPECT_THROW(static_cast<void>(read_forms("fixture.scm", ")")), SourceError);
}

}
}
