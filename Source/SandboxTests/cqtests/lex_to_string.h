#pragma once

#include "CoreMinimal.h"

auto LexToString(FVector3f const& vec) -> FString;

namespace ml {
template <typename T>
concept supports_lex_to_string = requires(T const& t) {
    { ::LexToString(t) } -> std::convertible_to<FString>;
};
}
