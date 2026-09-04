#include "parser.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

namespace slate_codegen::detail {
namespace {

class Parser {
  public:
    Parser(std::string_view const path, std::vector<Token> tokens)
        : path_{path}, tokens_{std::move(tokens)} {}

    auto parse() -> Child {
        auto result{parse_widget()};
        expect(TokenKind::end, "expected end of file");
        return result;
    }

  private:
    auto current() const -> Token const& { return tokens_[index_]; }

    auto previous() const -> Token const& { return tokens_[index_ - 1]; }

    auto at(TokenKind const kind) const -> bool { return current().kind == kind; }

    auto at_keyword(std::string_view const keyword) const -> bool {
        return at(TokenKind::identifier) && current().text == keyword;
    }

    auto consume(TokenKind const kind) -> bool {
        if (!at(kind)) {
            return false;
        }
        ++index_;
        return true;
    }

    auto consume_keyword(std::string_view const keyword) -> bool {
        if (!at_keyword(keyword)) {
            return false;
        }
        ++index_;
        return true;
    }

    auto expect(TokenKind const kind, std::string_view const message) -> Token const& {
        if (!consume(kind)) {
            fail(current().span, message);
        }
        return previous();
    }

    auto expect_identifier(std::string_view const message) -> Token const& {
        return expect(TokenKind::identifier, message);
    }

    void expect_keyword(std::string_view const keyword) {
        if (!consume_keyword(keyword)) {
            fail(current().span, "expected '" + std::string{keyword} + "'");
        }
    }

    [[noreturn]] void fail(SourceSpan const span, std::string_view const message) const {
        throw SourceError{path_, span, std::string{message}};
    }

    auto parse_widget() -> Child {
        expect_keyword("widget");
        auto const span{previous().span};
        Widget widget{.type = parse_type_name(), .span = span};
        expect(TokenKind::left_brace, "expected '{' after widget type");

        std::set<std::string> argument_names;
        std::set<std::string> slot_names;
        bool saw_child{false};
        while (!at(TokenKind::right_brace)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected '}' after widget body");
            }
            if (consume_keyword("content")) {
                auto const content_span{previous().span};
                saw_child = true;
                if (widget.content) {
                    fail(content_span, "duplicate default content");
                }
                widget.content_span = content_span;
                widget.content = parse_child_block();
                continue;
            }
            if (consume_keyword("slot")) {
                auto const slot_span{previous().span};
                saw_child = true;
                auto const& name{expect_identifier("expected named slot name")};
                if (!slot_names.insert(name.text).second) {
                    fail(name.span, "duplicate named slot '" + name.text + "'");
                }
                widget.named_slots.push_back(
                    WidgetSlot{name.text, parse_child_block(), slot_span});
                continue;
            }
            if (saw_child) {
                fail(current().span, "widget arguments must appear before child slots");
            }
            auto argument{parse_argument()};
            if (!argument_names.insert(argument.name).second) {
                fail(argument.span, "duplicate widget argument '" + argument.name + "'");
            }
            widget.arguments.push_back(std::move(argument));
        }
        expect(TokenKind::right_brace, "expected '}' after widget body");
        return Child{std::move(widget), span};
    }

    auto parse_type_name() -> std::string {
        std::string result{parse_qualified_identifier("expected widget type")};
        if (!consume(TokenKind::less)) {
            return result;
        }
        result += '<';
        result += parse_type_name();
        while (consume(TokenKind::comma)) {
            result += ", ";
            result += parse_type_name();
        }
        expect(TokenKind::greater, "expected '>' after template arguments");
        result += '>';
        return result;
    }

    auto parse_qualified_identifier(std::string_view const message) -> std::string {
        std::string result{expect_identifier(message).text};
        while (consume(TokenKind::scope)) {
            result += "::";
            result += expect_identifier("expected identifier after '::'").text;
        }
        return result;
    }

    auto parse_argument() -> Argument {
        auto const& name{expect_identifier("expected widget argument or child slot")};
        auto const span{name.span};
        auto const name_text{name.text};
        expect(TokenKind::equal, "expected '=' after widget argument name");
        return Argument{name_text, parse_value(), span};
    }

    auto parse_value() -> Value {
        auto const span{current().span};
        if (consume(TokenKind::number)) {
            return Value{ValueKind::number, previous().text, span};
        }
        if (consume(TokenKind::string)) {
            return Value{ValueKind::text, previous().text, span};
        }
        if (at_keyword("true") || at_keyword("false")) {
            ++index_;
            return Value{ValueKind::boolean, previous().text, span};
        }
        if (at(TokenKind::identifier)) {
            return Value{ValueKind::symbol,
                         parse_qualified_identifier("expected literal value"),
                         span};
        }
        fail(span, "expected number, boolean, text, or qualified identifier");
    }

    auto parse_child_block() -> std::shared_ptr<Child> {
        expect(TokenKind::left_brace, "expected '{' before child widget");
        auto child{std::make_shared<Child>(parse_child())};
        expect(TokenKind::right_brace, "expected '}' after child widget");
        return child;
    }

    auto parse_child() -> Child {
        if (at_keyword("widget")) {
            return parse_widget();
        }
        if (at_keyword("vbox")) {
            return parse_vbox();
        }
        fail(current().span, "expected 'widget' or 'vbox'");
    }

    auto parse_vbox() -> Child {
        expect_keyword("vbox");
        auto const span{previous().span};
        expect(TokenKind::left_brace, "expected '{' after vbox");
        VBox box{.span = span};
        while (!at(TokenKind::right_brace)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected '}' after vbox body");
            }
            box.slots.push_back(parse_box_slot());
        }
        expect(TokenKind::right_brace, "expected '}' after vbox body");
        if (box.slots.empty()) {
            fail(span, "vbox must contain at least one slot");
        }
        return Child{std::move(box), span};
    }

    auto parse_box_slot() -> BoxSlot {
        if (!at_keyword("auto") && !at_keyword("fill")) {
            fail(current().span, "expected 'auto' or 'fill' vbox slot");
        }
        auto const mode{current()};
        ++index_;
        BoxSlot slot{.fill = mode.text == "fill", .span = mode.span};
        bool has_padding{false};
        bool has_horizontal_alignment{false};
        bool has_vertical_alignment{false};

        while (!at(TokenKind::left_brace)) {
            auto const& option{expect_identifier("expected vbox slot option or '{'")};
            auto const option_text{option.text};
            expect(TokenKind::equal, "expected '=' after vbox slot option");
            if (option_text == "weight") {
                if (!slot.fill) {
                    fail(option.span, "weight is only valid on fill slots");
                }
                if (slot.weight) {
                    fail(option.span, "duplicate fill weight");
                }
                slot.weight = parse_number("expected fill weight");
            } else if (option_text == "padding") {
                if (has_padding) {
                    fail(option.span, "duplicate slot padding");
                }
                has_padding = true;
                slot.padding = parse_margin();
            } else if (option_text == "halign") {
                if (has_horizontal_alignment) {
                    fail(option.span, "duplicate horizontal alignment");
                }
                has_horizontal_alignment = true;
                slot.horizontal_alignment = parse_alignment(true);
            } else if (option_text == "valign") {
                if (has_vertical_alignment) {
                    fail(option.span, "duplicate vertical alignment");
                }
                has_vertical_alignment = true;
                slot.vertical_alignment = parse_alignment(false);
            } else {
                fail(option.span, "unsupported vbox slot option '" + option_text + "'");
            }
        }
        slot.child = parse_child_block();
        return slot;
    }

    auto parse_number(std::string_view const message) -> std::string {
        return expect(TokenKind::number, message).text;
    }

    auto parse_margin() -> std::vector<std::string> {
        if (!consume(TokenKind::left_parenthesis)) {
            return {parse_number("expected padding value")};
        }
        std::vector<std::string> values;
        values.push_back(parse_number("expected padding value"));
        while (consume(TokenKind::comma)) {
            values.push_back(parse_number("expected padding value after ','"));
        }
        expect(TokenKind::right_parenthesis, "expected ')' after padding");
        if (values.size() != 2 && values.size() != 4) {
            fail(previous().span, "padding tuple must contain two or four values");
        }
        return values;
    }

    auto parse_alignment(bool const horizontal) -> std::string {
        auto const& value{expect_identifier("expected alignment value")};
        auto const valid{horizontal ? value.text == "left" || value.text == "center" ||
                                          value.text == "right" || value.text == "fill"
                                    : value.text == "top" || value.text == "center" ||
                                          value.text == "bottom" || value.text == "fill"};
        if (!valid) {
            fail(value.span,
                 horizontal ? "expected left, center, right, or fill"
                            : "expected top, center, bottom, or fill");
        }
        return value.text;
    }

    std::string_view path_;
    std::vector<Token> tokens_;
    std::size_t index_{};
};

}

auto parse(std::string_view const path, std::vector<Token> tokens) -> Child {
    return Parser{path, std::move(tokens)}.parse();
}

}
