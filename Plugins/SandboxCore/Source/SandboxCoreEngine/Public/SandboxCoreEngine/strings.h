#pragma once

#include <Containers/StringConv.h>
#include <CoreMinimal.h>

#include <string_view>

namespace ml {
inline auto to_fstring(std::string_view const value) -> FString {
    if (value.empty()) {
        return {};
    }

    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}
} // namespace ml
