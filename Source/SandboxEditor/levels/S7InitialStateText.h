#pragma once

#include "CoreMinimal.h"

namespace ml::editor {
inline auto count_text(int32 const count, FStringView const singular, FStringView const plural)
    -> FString {
    auto const text{count == 1 ? singular : plural};
    return FString::Printf(TEXT("%d %.*s"), count, text.Len(), text.GetData());
}
}
