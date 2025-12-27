#include "Sandbox/utilities/string.h"

namespace ml {
auto without_class_prefix(FString const& input) -> FString {
    FString out{};

    auto const n_full{input.Len()};
    int32 n{0};
    for (int32 i{n_full - 1}; i >= 0; --i) {
        auto const c{input[i]};
        if (c == TEXT(':')) {
            break;
        }
        n++;
    }

    return input.Right(n);
}
}
