#pragma once

#include <CoreMinimal.h>

#include <sandbox/simulation/index_span.h>

inline auto to_string(FIndexSpan const span) -> FString {
    return FString::Printf(TEXT("IndexSpan(%d, %d)"), span.offset, span.count);
}

inline auto to_compact_string(FIndexSpan const span) -> FString {
    return FString::Printf(TEXT("(%d, %d)"), span.offset, span.count);
}

inline auto LexToString(FIndexSpan const span) -> FString {
    return to_string(span);
}
