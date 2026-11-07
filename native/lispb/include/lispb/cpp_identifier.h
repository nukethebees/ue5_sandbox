#pragma once

#include <codegen/ast/cpp_keyword.h>

#include <cctype>
#include <cstddef>
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
    auto const& keywords{ioj::enum_strings<codegen::CppKeyword, ioj::Serialized>};
    auto const count{keywords.offsets.size() - 1};
    for (std::size_t index{}; index < count; ++index) {
        auto const keyword{static_cast<codegen::CppKeyword>(index)};
        if (keywords(keyword) == value &&
            (policy == IdentifierPolicy::Declaration || !codegen::is_contextual(keyword))) {
            return IdentifierError::Reserved;
        }
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

}
