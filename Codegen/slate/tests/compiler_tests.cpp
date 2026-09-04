#include "lexer.h"
#include "parser.h"
#include "renderer.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace slate_codegen::detail {
namespace {

auto parse_source(std::string_view const source) -> Document {
    return parse("test.sbxslate", lex("test.sbxslate", source));
}

auto parse_error(std::string_view const source) -> std::string {
    try {
        [[maybe_unused]] auto const document{parse_source(source)};
    } catch (SourceError const& error) {
        return error.what();
    }
    return {};
}

TEST(SlateDsl, ExplicitParametersDetermineSignatureAndRoles) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params
      (factory make_child)
      (existing supplied_child)
      (callback on_clicked))
    (vbox
      (auto (call make_child 1))
      (auto (call make_child 2))
      (auto (existing supplied_child))
      (auto (SButton :OnClicked (callback on_clicked))))))
)")};

    ASSERT_EQ(document.widget_classes.size(), 1);
    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("auto Build(auto&& make_child, auto&& supplied_child, auto&& on_clicked)"),
              std::string::npos);
    EXPECT_NE(output.find("make_child(1)"), std::string::npos);
    EXPECT_NE(output.find("make_child(2)"), std::string::npos);
}

TEST(SlateDsl, LetBindingsSupportValuesAndMargins) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params)
    (let ((gap (0 0 0 10))
          (width 512.0f))
      (vbox
        (auto :padding gap
          (SBox :WidthOverride width))))))
)")};

    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("auto const gap{FMargin{0.0f, 0.0f, 0.0f, 10.0f}};"),
              std::string::npos);
    EXPECT_NE(output.find("auto const width{512.0f};"), std::string::npos);
    EXPECT_NE(output.find("vbox_auto_slot(FMargin{gap})"), std::string::npos);
    EXPECT_NE(output.find(".WidthOverride(width)"), std::string::npos);
}

TEST(SlateDsl, RejectsUndeclaredAndIncorrectParameterUses) {
    auto const undeclared{parse_error(R"(
(widget-class FOwner
  (function Build
    (params)
    (SButton :OnClicked (callback on_clicked))))
)")};
    EXPECT_NE(undeclared.find("undeclared callback parameter 'on_clicked'"), std::string::npos);

    auto const incorrect_role{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (existing on_clicked))
    (SButton :OnClicked (callback on_clicked))))
)")};
    EXPECT_NE(incorrect_role.find("declared as existing, not callback"), std::string::npos);
}

TEST(SlateDsl, RejectsUnusedAndDuplicateParameters) {
    auto const missing{parse_error(R"(
(widget-class FOwner
  (function Build
    (SButton)))
)")};
    EXPECT_NE(missing.find("expected 'params' declaration"), std::string::npos);

    auto const unused{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (existing child))
    (SButton)))
)")};
    EXPECT_NE(unused.find("unused existing parameter 'child'"), std::string::npos);

    auto const duplicate{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (existing child) (callback child))
    (SButton)))
)")};
    EXPECT_NE(duplicate.find("duplicate parameter declaration 'child'"), std::string::npos);
}

}
}
