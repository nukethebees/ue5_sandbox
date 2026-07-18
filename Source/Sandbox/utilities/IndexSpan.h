#pragma once

#include <CoreMinimal.h>
#include <HAL/Platform.h>

#include <compare>

struct FIndexSpan {
    int32 offset{0};
    int32 count{0};

    auto operator<=>(FIndexSpan const&) const noexcept = default;

    bool is_empty() const noexcept { return count == 0; }
    auto start() const noexcept { return offset; }
    auto end() const noexcept { return offset + count; }
    auto to_string() const -> FString {
        return FString::Printf(TEXT("IndexSpan(%d, %d)"), offset, count);
    }
    auto to_compact_string() const -> FString {
        return FString::Printf(TEXT("(%d, %d)"), offset, count);
    }
};

inline auto LexToString(FIndexSpan const span) -> FString {
    return span.to_string();
}
