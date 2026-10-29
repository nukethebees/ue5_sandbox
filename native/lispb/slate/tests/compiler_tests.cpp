#include "parser.h"
#include "renderer.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_renderer.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/diagnostics.h>
#include <lispb/strings.h>
#include <slate_codegen/document.h>
#include <slate_codegen/indices.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace slate_codegen::detail {
namespace {

struct TestDocument {
    lispb::Strings strings;
    lispb::Diagnostics diagnostics{strings};
    lispb::codegen::sexpr::Sources sources{strings, diagnostics};
    lispb::codegen::sexpr::Syntax syntax{strings, sources};
    Document document;
    lispb::codegen::CppTypes types{strings};
    lispb::codegen::CppIR ir{strings, types};

    explicit TestDocument(std::string_view const source) {
        auto const file{sources.add("test.lispb", source)};
        parse(syntax, lispb::codegen::sexpr::read_forms(syntax, file), document);
    }
    auto render(std::uint32_t const index = 0) -> std::string {
        return lispb::codegen::CppRenderer{ir}.render(
            detail::lower("test.lispb", sources, document, DeclarationId{index}, ir));
    }
};

auto parse_source(std::string_view const source) -> TestDocument {
    return TestDocument{source};
}

auto parse_error(std::string_view const source) -> std::string {
    try {
        [[maybe_unused]] auto document{parse_source(source)};
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        return error.what();
    }
    return {};
}

TEST(SlateDsl, LibrariesRenderInlineFunctionsWithExplicitParameters) {
    auto document{parse_source(R"(
(widget-library Example::Controls
  (function Empty (params) (SBox))
  (function Build
    (params (value label) (callback clicked) (existing content) (factory make_child))
    (let ((padding (0 4)))
      (vbox
        (auto :padding padding (SButton :Text label :OnClicked (callback clicked)))
        (auto (existing content))
        (auto (call make_child label))))))
(widget-class SOwner
  (function Build (params) (assign image_ SImage)))
)")};
    ASSERT_EQ(document.document.declarations.size(), 2);
    auto const output{document.render()};
    EXPECT_TRUE(output.contains("namespace SlateGenerated::Example::Controls {"));
    EXPECT_TRUE(output.contains("inline auto Empty()"));
    EXPECT_TRUE(output.contains(
        "inline auto Build(auto&& label, auto&& clicked, auto&& content, auto&& make_child)"));
    EXPECT_TRUE(output.contains(".OnClicked_Lambda(std::forward<decltype(clicked)>(clicked))"));
    EXPECT_TRUE(output.contains("make_child(label)"));
    EXPECT_FALSE(output.contains("ThisClass"));
    EXPECT_FALSE(output.contains("self_"));
    EXPECT_TRUE(document.render(1).contains("SAssignNew(self_.image_"));
}

TEST(SlateDsl, LibrariesRejectHostDependentForms) {
    for (auto const body : {"(assign image_ SImage)",
                            "(SButton :OnClicked (uobject clicked))",
                            "(SButton :Text (method label))"}) {
        auto const source{std::string{"(widget-library Controls (function Build (params) "} + body +
                          "))"};
        EXPECT_TRUE(parse_error(source).contains("requires a widget-class host"));
    }
}

TEST(SlateDsl, ExplicitParametersDetermineSignatureAndRoles) {
    auto document{parse_source(R"(
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

    ASSERT_EQ(document.document.declarations.size(), 1);
    auto const output{document.render()};
    EXPECT_NE(output.find("auto Build(auto&& label, auto&& make_child, auto&& supplied_child, "
                          "auto&& on_clicked)"),
              std::string::npos);
    EXPECT_NE(output.find("make_child(label)"), output.rfind("make_child(label)"));
    EXPECT_NE(output.find(".Text(label)"), std::string::npos);
}

TEST(SlateDsl, LetBindingsSupportValuesAndMargins) {
    auto document{parse_source(R"(
(widget-class FOwner
  (function Build
    (params)
    (let ((gap (0 0 0 10))
          (width 512.0f))
      (vbox
        (auto :padding gap
          (SBox :WidthOverride width))))))
)")};

    auto const output{document.render()};
    EXPECT_NE(output.find("auto const gap{FMargin{0.0f, 0.0f, 0.0f, 10.0f}};"), std::string::npos);
    EXPECT_NE(output.find("auto const width{512.0f};"), std::string::npos);
    EXPECT_NE(output.find("vbox_auto_slot(FMargin{gap})"), std::string::npos);
    EXPECT_NE(output.find(".WidthOverride(width)"), std::string::npos);
}

TEST(SlateDsl, ValueParametersWorkInLetBindingsAndPadding) {
    auto document{parse_source(R"(
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

    auto const output{document.render()};
    EXPECT_NE(output.find("auto const box_width{width};"), std::string::npos);
    EXPECT_NE(output.find("vbox_auto_slot(FMargin{padding})"), std::string::npos);
    EXPECT_NE(output.find(".WidthOverride(box_width)"), std::string::npos);
}

TEST(SlateDsl, RendersSupportedTreeForms) {
    auto document{parse_source(R"(
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

    auto const output{document.render()};
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
    auto document{parse_source(R"(
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

    auto const output{document.render()};
    EXPECT_NE(output.find("auto BuildControls()"), std::string::npos);
    EXPECT_NE(output.find("auto BuildPreview()"), std::string::npos);
    EXPECT_NE(output.find("vbox_fill_slot(2.0f, FMargin{1.0f, 2.0f})"), std::string::npos);
    EXPECT_NE(output.find(".HAlign(HAlign_Right)"), std::string::npos);
    EXPECT_NE(output.find(".VAlign(VAlign_Bottom)"), std::string::npos);
}

TEST(SlateDsl, RendersValueParametersAsBoxOptions) {
    auto document{parse_source(R"(
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

    auto const output{document.render()};
    EXPECT_NE(output.find("hbox_fill_slot(fill_width)"), std::string::npos);
    EXPECT_NE(output.find(".HAlign(horizontal_alignment)"), std::string::npos);
    EXPECT_NE(output.find(".VAlign(vertical_alignment)"), std::string::npos);
}

struct InvalidParameterCase {
    char const* name;
    char const* parameters;
    char const* body;
    char const* diagnostic;
};

using SlateInvalidParameter = testing::TestWithParam<InvalidParameterCase>;

TEST_P(SlateInvalidParameter, RejectsInvalidDeclarationOrUse) {
    auto const& test{GetParam()};
    auto const source{std::string{"(widget-class FOwner (function Build "} + test.parameters + " " +
                      test.body + "))"};
    EXPECT_NE(parse_error(source).find(test.diagnostic), std::string::npos) << source;
}

INSTANTIATE_TEST_SUITE_P(
    SlateDsl,
    SlateInvalidParameter,
    testing::Values(
        InvalidParameterCase{"RepeatedCallback",
                             "(params (callback handle))",
                             "(SButton :OnClicked (callback handle) :OnPressed (callback handle))",
                             "callback parameter 'handle' may only be used once"},
        InvalidParameterCase{"RepeatedExisting",
                             "(params (existing child))",
                             "(vbox (auto (existing child)) (auto (existing child)))",
                             "existing parameter 'child' may only be used once"},
        InvalidParameterCase{"UndeclaredCallback",
                             "(params)",
                             "(SButton :OnClicked (callback on_clicked))",
                             "undeclared callback parameter 'on_clicked'"},
        InvalidParameterCase{"IncorrectCallbackRole",
                             "(params (existing on_clicked))",
                             "(SButton :OnClicked (callback on_clicked))",
                             "declared as existing, not callback"},
        InvalidParameterCase{"IncorrectValueRole",
                             "(params (existing label))",
                             "(SButton :Text label)",
                             "declared as existing, not value"},
        InvalidParameterCase{
            "MissingDeclaration", "", "(SButton)", "expected 'params' declaration"},
        InvalidParameterCase{"UnusedExisting",
                             "(params (existing child))",
                             "(SButton)",
                             "unused existing parameter 'child'"},
        InvalidParameterCase{"Duplicate",
                             "(params (existing child) (callback child))",
                             "(SButton)",
                             "duplicate parameter declaration 'child'"}),
    [](auto const& info) { return info.param.name; });

}
}
