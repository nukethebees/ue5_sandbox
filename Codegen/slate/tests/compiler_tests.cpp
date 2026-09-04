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
      (value label)
      (factory make_child)
      (existing supplied_child)
      (callback on_clicked))
    (vbox
      (auto (call make_child label))
      (auto (call make_child label))
      (auto (existing supplied_child))
      (auto (SButton :Text label :OnClicked (callback on_clicked))))))
)")};

    ASSERT_EQ(document.widget_classes.size(), 1);
    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find(
                  "auto Build(auto&& label, auto&& make_child, auto&& supplied_child, auto&& on_clicked)"),
              std::string::npos);
    EXPECT_NE(output.find("make_child(label)"), output.rfind("make_child(label)"));
    EXPECT_NE(output.find(".Text(label)"), std::string::npos);
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

TEST(SlateDsl, ValueParametersWorkInLetBindingsAndPadding) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params
      (value padding)
      (value width))
    (let ((box_width width))
      (vbox
        (auto :padding padding
          (SBox :WidthOverride box_width))))))
)")};

    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("auto const box_width{width};"), std::string::npos);
    EXPECT_NE(output.find("vbox_auto_slot(FMargin{padding})"), std::string::npos);
    EXPECT_NE(output.find(".WidthOverride(box_width)"), std::string::npos);
}

TEST(SlateDsl, RejectsRepeatedSingleUseParameters) {
    auto const repeated_callback{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (callback handle))
    (SButton
      :OnClicked (callback handle)
      :OnPressed (callback handle))))
)")};
    EXPECT_NE(repeated_callback.find("callback parameter 'handle' may only be used once"),
              std::string::npos);

    auto const repeated_existing{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (existing child))
    (vbox
      (auto (existing child))
      (auto (existing child)))))
)")};
    EXPECT_NE(repeated_existing.find("existing parameter 'child' may only be used once"),
              std::string::npos);
}

TEST(SlateDsl, RendersSupportedTreeForms) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params (value title))
    (SSectionPanel
      :Title title
      :Description (loc "Tests" "Description" "Description text")
      :State (method current_state)
      :OnClicked (uobject handle_clicked)
      (slot Header
        (STextBlock :Text title))
      (slot Body
        (assign child_ SBox)))))
)")};

    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("NSLOCTEXT(\"Tests\", \"Description\", \"Description text\")"),
              std::string::npos);
    EXPECT_NE(output.find(".State(&self_, &ThisClass::current_state)"), std::string::npos);
    EXPECT_NE(output.find(".OnClicked_UObject(&self_, &ThisClass::handle_clicked)"),
              std::string::npos);
    EXPECT_NE(output.find(".Header()"), std::string::npos);
    EXPECT_NE(output.find(".Body()"), std::string::npos);
    EXPECT_NE(output.find("SAssignNew(self_.child_, SBox)"), std::string::npos);
}

TEST(SlateDsl, RendersBoxOptionsAndMultipleFunctions) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function BuildControls
    (params)
    (vbox
      (fill :weight 2 :padding (1 2) :halign right :valign bottom
        (SButton))))
  (function BuildPreview
    (params)
    (SImage)))
)")};

    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("auto BuildControls()"), std::string::npos);
    EXPECT_NE(output.find("auto BuildPreview()"), std::string::npos);
    EXPECT_NE(output.find("vbox_fill_slot(2.0f, FMargin{1.0f, 2.0f})"), std::string::npos);
    EXPECT_NE(output.find(".HAlign(HAlign_Right)"), std::string::npos);
    EXPECT_NE(output.find(".VAlign(VAlign_Bottom)"), std::string::npos);
}

TEST(SlateDsl, RendersValueParametersAsBoxOptions) {
    auto const document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params
      (value fill_width)
      (value horizontal_alignment)
      (value vertical_alignment))
    (hbox
      (fill :weight fill_width :halign horizontal_alignment :valign vertical_alignment
        (SButton))))
)
)")};

    auto const output{render("test.sbxslate", document.widget_classes.front())};
    EXPECT_NE(output.find("hbox_fill_slot(fill_width)"), std::string::npos);
    EXPECT_NE(output.find(".HAlign(horizontal_alignment)"), std::string::npos);
    EXPECT_NE(output.find(".VAlign(vertical_alignment)"), std::string::npos);
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

    auto const incorrect_value_role{parse_error(R"(
(widget-class FOwner
  (function Build
    (params (existing label))
    (SButton :Text label)))
)")};
    EXPECT_NE(incorrect_value_role.find("declared as existing, not value"), std::string::npos);
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
