#pragma once

#include <codegen/sexpr/failure_handler.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::sexpr {

class Sources;
class Syntax;

[[noreturn]] void
    throw_source_error(Sources const& sources, SourceSpan const& span, std::string const& message);

class Fields {
  public:
    Fields(Syntax const& syntax,
           FormId form,
           std::string_view expected_head,
           std::size_t positionals,
           FailureHandler failure = throw_source_error);

    [[nodiscard]] auto positional(std::size_t index) const -> FormId;
    [[nodiscard]] auto optional(std::string_view name) const -> std::optional<FormId>;
    [[nodiscard]] auto required(std::string_view name) const -> FormId;
    [[nodiscard]] auto declarations() const -> std::span<FormId const>;

    void validate(std::initializer_list<std::string_view> properties,
                  std::initializer_list<std::string_view> declarations = {}) const;
  private:
    Syntax const& syntax_;
    FormId form_;
    FailureHandler failure_;
    std::vector<IdentifierId> properties_;
    std::vector<FormId> values_;
    std::vector<SourceSpan> property_spans_;
    std::vector<FormId> declarations_;
};

[[nodiscard]] auto
    text(Syntax const& syntax, FormId const form, std::string_view purpose, FailureHandler failure)
        -> std::string_view;
[[nodiscard]] auto raw_text(Syntax const& syntax,
                            FormId const form,
                            std::string_view expected_tag,
                            std::string_view purpose,
                            FailureHandler failure) -> std::string_view;
[[nodiscard]] auto boolean(Syntax const& syntax,
                           FormId const form,
                           std::string_view purpose,
                           FailureHandler failure) -> bool;
[[nodiscard]] auto integer(Syntax const& syntax,
                           FormId const form,
                           std::string_view purpose,
                           FailureHandler failure) -> int;
[[nodiscard]] auto number(Syntax const& syntax,
                          FormId const form,
                          std::string_view purpose,
                          FailureHandler failure) -> double;
[[nodiscard]] auto text_list(Syntax const& syntax,
                             FormId const form,
                             std::string_view purpose,
                             FailureHandler failure) -> std::vector<std::string>;

} // namespace lispb::codegen::sexpr
