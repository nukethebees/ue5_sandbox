#include "Sandbox/utilities/string.h"

namespace ml {
auto without_class_prefix(FString const& input) -> FString {
    FString out{};

    for (auto c : input.Reverse()) {
        if (c == TEXT(':')) {
            break;
        }
        out += c;
    }

    out.ReverseString();
    return out;
}
}
