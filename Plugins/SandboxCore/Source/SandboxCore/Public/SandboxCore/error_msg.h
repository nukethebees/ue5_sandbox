#pragma once

#include "CoreMinimal.h"

namespace ml {
struct FErrorMsg {
    auto operator*() const noexcept -> TCHAR const* { return *message; }

    explicit operator bool() const noexcept { return has_error(); }

    bool has_error() const noexcept { return !message.IsEmpty(); }

    FString message;
};
}
