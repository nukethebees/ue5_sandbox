#pragma once

#include <SandboxCoreEngine/strings.h>

#include <InputCoreTypes.h>

#include <charconv>
#include <string_view>

namespace ioj {
inline auto binding_key(std::string_view name) -> FKey {
    constexpr std::string_view prefix{"UnknownCharCode_"};
    if (name.starts_with(prefix)) {
        auto const digits{name.substr(prefix.size())};
        uint32 character{};
        auto const end{digits.data() + digits.size()};
        auto const parsed{std::from_chars(digits.data(), end, character)};
        if (parsed.ec == std::errc{} && parsed.ptr == end && character > 32 &&
            character <= 0x10ffff) {
            return FInputKeyManager::Get().GetKeyFromCodes(INDEX_NONE, character);
        }
    }
    return FKey{FName{*to_fstring(name)}};
}
}
