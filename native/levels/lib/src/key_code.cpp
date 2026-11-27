#include <ioj/input/key_code.h>

#include <ioj/ascii.h>

#include <bit>
#include <charconv>
#include <utility>

namespace ioj {
auto character_key(std::uint32_t character) -> KeyCode {
    if (character >= 'a' && character <= 'z') {
        character -= 'a' - 'A';
    }
    if (character >= 'A' && character <= 'Z') {
        return static_cast<PhysicalKey>(std::to_underlying(PhysicalKey::A) + character - 'A');
    }
    if (character >= '0' && character <= '9') {
        return static_cast<PhysicalKey>(std::to_underlying(PhysicalKey::Zero) + character - '0');
    }
    switch (character) {
        case ' ':
            return PhysicalKey::SpaceBar;
        case '-':
            return PhysicalKey::Hyphen;
        case '=':
            return PhysicalKey::Equals;
        case '[':
            return PhysicalKey::LeftBracket;
        case ']':
            return PhysicalKey::RightBracket;
        case '\\':
            return PhysicalKey::Backslash;
        case ';':
            return PhysicalKey::Semicolon;
        case '\'':
            return PhysicalKey::Apostrophe;
        case '`':
            return PhysicalKey::Tilde;
        case ',':
            return PhysicalKey::Comma;
        case '.':
            return PhysicalKey::Period;
        case '/':
            return PhysicalKey::Slash;
        case '_':
            return PhysicalKey::Underscore;
        case '&':
            return PhysicalKey::Ampersand;
        case '*':
            return PhysicalKey::Asterix;
        case '^':
            return PhysicalKey::Caret;
        case ':':
            return PhysicalKey::Colon;
        case '$':
            return PhysicalKey::Dollar;
        case '!':
            return PhysicalKey::Exclamation;
        case '(':
            return PhysicalKey::LeftParantheses;
        case ')':
            return PhysicalKey::RightParantheses;
        case '"':
            return PhysicalKey::Quote;
        case 0xe0:
            return PhysicalKey::AAccentGrave;
        case 0xe8:
            return PhysicalKey::EAccentGrave;
        case 0xe9:
            return PhysicalKey::EAccentAigu;
        case 0xe7:
            return PhysicalKey::CCedille;
        case 0xa7:
            return PhysicalKey::Section;
        default:
            return {PhysicalKey::Character, character};
    }
}
auto parse_key(std::string_view name) -> KeyCode {
    for (auto key : EnumTraits<PhysicalKey>::values) {
        if (key != PhysicalKey::Character && equals_ignore_case(name, to_serialized_string(key))) {
            return key;
        }
    }
    constexpr std::string_view prefix{"UnknownCharCode_"};
    if (equals_ignore_case(name.substr(0, prefix.size()), prefix)) {
        std::uint32_t code{};
        auto const end{name.data() + name.size()};
        auto const result{std::from_chars(name.data() + prefix.size(), end, code)};
        if (result.ec == std::errc{} && result.ptr == end && code > 32 && code <= 0x10ffff &&
            !(code >= 0xd800 && code <= 0xdfff)) {
            return character_key(code);
        }
    }
    if (name.empty() || name.size() > 4) {
        return {};
    }
    auto const first{static_cast<unsigned char>(name.front())};
    if (name.size() == 1 && first > 32 && first < 127) {
        return character_key(first);
    }
    auto const length{std::countl_one(first)};
    if (length >= 2 && length <= 4 && name.size() == static_cast<std::size_t>(length)) {
        std::uint32_t code{first & (0x7fu >> length)};
        for (unsigned char byte : name.substr(1)) {
            if ((byte & 0xc0) != 0x80) {
                return {};
            }
            code = (code << 6) | (byte & 0x3fu);
        }
        constexpr std::uint32_t minimum[]{0, 0, 0x80, 0x800, 0x10000};
        if (code >= minimum[length] && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff)) {
            return character_key(code);
        }
    }
    return {};
}
auto serialized_key(KeyCode key) -> std::string {
    if (key.key == PhysicalKey::Character) {
        return "UnknownCharCode_" + std::to_string(key.codepoint);
    }
    return key.key == PhysicalKey::Unbound ? std::string{}
                                           : std::string{to_serialized_string(key.key)};
}
auto parse_mapping(std::string_view name) -> std::optional<FlightMapping> {
    for (auto mapping : EnumTraits<FlightMapping>::values) {
        if (equals_ignore_case(name, to_serialized_string(mapping))) {
            return mapping;
        }
    }
    return std::nullopt;
}
}
