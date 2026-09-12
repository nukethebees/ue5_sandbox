#pragma once

#include <codegen/sexpr/reader.h>

#include <initializer_list>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace codegen::sexpr {

using FailureHandler = void (*)(SourceSpan const&, std::string const&);

[[noreturn]] void throw_source_error(SourceSpan const& span, std::string const& message);

class Fields {
  public:
    Fields(Form const& form,
           std::string_view expected_head,
           std::size_t positionals,
           FailureHandler failure = throw_source_error);

    [[nodiscard]] auto positional(std::size_t index) const -> Form const&;
    [[nodiscard]] auto optional(std::string_view name) const -> Form const*;
    [[nodiscard]] auto required(std::string_view name) const -> Form const&;
    [[nodiscard]] auto declarations() const -> std::span<Form const* const>;

    void validate(std::initializer_list<std::string_view> properties,
                  std::initializer_list<std::string_view> declarations = {}) const;
  private:
    Form const& form_;
    FailureHandler failure_;
    std::map<std::string, Form const*, std::less<>> properties_;
    std::map<std::string, SourceSpan, std::less<>> property_spans_;
    std::vector<Form const*> declarations_;
};

[[nodiscard]] auto text(Form const& form, std::string_view purpose, FailureHandler failure)
    -> std::string;
[[nodiscard]] auto boolean(Form const& form, std::string_view purpose, FailureHandler failure)
    -> bool;
[[nodiscard]] auto integer(Form const& form, std::string_view purpose, FailureHandler failure)
    -> int;
[[nodiscard]] auto number(Form const& form, std::string_view purpose, FailureHandler failure)
    -> double;
[[nodiscard]] auto text_list(Form const& form, std::string_view purpose, FailureHandler failure)
    -> std::vector<std::string>;

} // namespace codegen::sexpr
