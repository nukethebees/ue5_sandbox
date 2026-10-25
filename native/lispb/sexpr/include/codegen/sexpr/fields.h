#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
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

    template <typename Property>
        requires std::is_enum_v<Property>
    [[nodiscard]] auto optional(Property name) const -> std::optional<FormId> {
        return optional(to_serialized_string(name));
    }

    template <typename Property>
        requires std::is_enum_v<Property>
    [[nodiscard]] auto required(Property name) const -> FormId {
        return required(to_serialized_string(name));
    }

    [[nodiscard]] auto declarations() const -> std::span<FormId const>;

    void validate(std::initializer_list<std::string_view> properties,
                  std::initializer_list<std::string_view> declarations = {}) const;

    template <typename Property, std::size_t Count>
        requires std::is_enum_v<Property>
    void validate(Property const (&properties)[Count],
                  std::initializer_list<std::string_view> declarations = {}) const {
        std::array<std::string_view, Count> names{};
        for (std::size_t index{}; index < Count; ++index) {
            names[index] = to_serialized_string(properties[index]);
        }
        validate_names(names, declarations);
    }
  private:
    void validate_names(std::span<std::string_view const> properties,
                        std::span<std::string_view const> declarations) const;

    Syntax const& syntax_;
    FormId form_;
    Parser const& parser_;
    std::vector<StringId> properties_;
    std::vector<FormId> values_;
    std::vector<SourceSpan> property_spans_;
    std::vector<FormId> declarations_;
};

} // namespace lispb::codegen::sexpr
