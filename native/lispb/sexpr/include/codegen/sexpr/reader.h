#pragma once

#include <codegen/sexpr/syntax.h>

#include <span>
#include <string_view>
#include <vector>

namespace codegen::sexpr {

struct Form {
    Token token;
    std::vector<Form> children;
    Token closing;

    [[nodiscard]] auto is_list() const -> bool;
    [[nodiscard]] auto head() const -> std::string_view;
};

auto read_form(std::span<Token const> tokens, std::size_t& index) -> Form;
auto read_forms(std::string_view path, std::string_view source) -> std::vector<Form>;

}
