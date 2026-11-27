#pragma once

#include <ioj/input/key_code.h>

#include <SandboxCoreEngine/strings.h>

#include <InputCoreTypes.h>

#include <array>
#include <utility>

namespace ioj {
inline auto binding_key(input::KeyCode code) -> FKey {
    if (code.key == PhysicalKey::Character) {
        return FInputKeyManager::Get().GetKeyFromCodes(INDEX_NONE, code.codepoint);
    }

    static auto const keys = [] {
        std::array<FKey, EnumTraits<PhysicalKey>::values.size()> result{};
        for (auto key : EnumTraits<PhysicalKey>::values) {
            if (key != PhysicalKey::Unbound && key != PhysicalKey::Character) {
                result[std::to_underlying(key)] =
                    FKey{FName{*to_fstring(to_serialized_string(key))}};
            }
        }
        return result;
    }();
    return keys[std::to_underlying(code.key)];
}
}
