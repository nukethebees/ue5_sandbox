#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <string_view>

namespace lispb::codegen::sexpr {

class Syntax;

class Parser {
  public:
    explicit Parser(Syntax& syntax);
    auto syntax() const -> Syntax&;
    [[noreturn]] void fail(FormId form, std::string_view message) const;
    [[noreturn]] void fail(SourceSpan span, std::string_view message) const;
    auto text(FormId form, std::string_view purpose) const -> std::string_view;
    auto text_id(FormId form, std::string_view purpose) const -> StringId;
    auto identifier(FormId form, std::string_view purpose) const -> IdentifierId;
    auto boolean(FormId form, std::string_view purpose) const -> bool;
    auto integer(FormId form, std::string_view purpose) const -> int;
    auto number(FormId form, std::string_view purpose) const -> double;
  private:
    Syntax& syntax_;
};

}
