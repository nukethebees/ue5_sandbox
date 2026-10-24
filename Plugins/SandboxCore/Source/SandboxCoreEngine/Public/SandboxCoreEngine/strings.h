#pragma once

#include <Containers/StringConv.h>
#include <CoreMinimal.h>

#include <string>
#include <string_view>

namespace ioj {
inline auto to_string(FString const& value) -> std::string {
    auto const converted{FTCHARToUTF8{*value, value.Len()}};
    return {converted.Get(), static_cast<std::size_t>(converted.Length())};
}

inline auto to_fstring(std::string_view const value) -> FString {
    if (value.empty()) {
        return {};
    }

    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}
} // namespace ioj
