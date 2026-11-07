#pragma once

#include <codegen/ast/cpp_keyword.h>
#include <lispb/strings.h>

#include <cctype>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace lispb {

enum class IdentifierPolicy { Lexical, Keywords, Declaration };
enum class IdentifierError { Empty, Malformed, Reserved };

[[nodiscard]] inline auto identifier_error_message(IdentifierError const error,
                                                   std::string_view const value,
                                                   std::string_view const ctx) -> std::string {
    if (error == IdentifierError::Empty) {
        return std::format("{} must not be empty", ctx);
    }
    return std::format("{} {}: {}",
                       ctx,
                       error == IdentifierError::Malformed
                           ? "must be a C++ identifier"
                           : "must not be a reserved C++ identifier",
                       value);
}

[[nodiscard]] inline auto identifier_error(std::string_view const value,
                                           IdentifierPolicy const policy)
    -> std::optional<IdentifierError> {
    if (value.empty()) {
        return IdentifierError::Empty;
    }
    if (std::isalpha(static_cast<unsigned char>(value.front())) == 0 && value.front() != '_') {
        return IdentifierError::Malformed;
    }
    for (auto const character : value.substr(1)) {
        if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_') {
            return IdentifierError::Malformed;
        }
    }
    if (policy == IdentifierPolicy::Lexical) {
        return std::nullopt;
    }
    if (auto const keyword{codegen::try_parse_serialized_cpp_keyword(value)};
        keyword && (policy == IdentifierPolicy::Declaration || !codegen::is_contextual(*keyword))) {
        return IdentifierError::Reserved;
    }
    if (policy == IdentifierPolicy::Declaration &&
        (value.starts_with("__") || (value.size() > 1 && value.front() == '_' &&
                                     std::isupper(static_cast<unsigned char>(value[1])) != 0))) {
        return IdentifierError::Reserved;
    }
    return std::nullopt;
}

[[nodiscard]] inline auto is_cpp_identifier(std::string_view const value,
                                            IdentifierPolicy const policy) -> bool {
    return !identifier_error(value, policy);
}

[[nodiscard]] inline auto is_qualified_cpp_identifier(std::string_view value,
                                                      IdentifierPolicy const policy) -> bool {
    while (true) {
        auto const separator{value.find("::")};
        if (!is_cpp_identifier(value.substr(0, separator), policy)) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        value.remove_prefix(separator + 2);
    }
}

template <IdentifierPolicy Policy = IdentifierPolicy::Declaration>
class CppIdentifier {
  public:
    [[nodiscard]] static auto try_make(Strings const& strings, StringId const id)
        -> std::expected<CppIdentifier, IdentifierError> {
        if (auto const error{identifier_error(strings.view(id), Policy)}) {
            return std::unexpected{*error};
        }
        return CppIdentifier{id};
    }

    [[nodiscard]] operator StringId() const noexcept { return id_; }

  private:
    explicit CppIdentifier(StringId const id)
        : id_{id} {}

    StringId id_;
};

}
