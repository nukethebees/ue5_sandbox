#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace lispb::codegen::sexpr {

class Sources;
class Syntax;

class Parser;

class Fields {
  public:
    Fields(Parser const& parser,
           FormId form,
           std::string_view expected_head,
           std::size_t positionals);

    [[nodiscard]] auto positional(std::size_t index) const -> FormId;
    [[nodiscard]] auto optional(std::string_view name) const -> std::optional<FormId>;
    [[nodiscard]] auto required(std::string_view name) const -> FormId;
    [[nodiscard]] auto declarations() const -> std::span<FormId const>;

    void validate(std::initializer_list<std::string_view> properties,
                  std::initializer_list<std::string_view> declarations = {}) const;
  private:
    Syntax const& syntax_;
    FormId form_;
    Parser const& parser_;
    std::vector<StringId> properties_;
    std::vector<FormId> values_;
    std::vector<SourceSpan> property_spans_;
    std::vector<FormId> declarations_;
};

} // namespace lispb::codegen::sexpr
