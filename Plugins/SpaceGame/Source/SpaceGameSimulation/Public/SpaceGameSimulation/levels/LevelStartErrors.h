#pragma once

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAMESIMULATION_API FLevelStartErrors {
    void add(FString message) { messages_.Add(MoveTemp(message)); }

    void append(TArray<FString> messages) {
        for (auto& message : messages) {
            add(MoveTemp(message));
        }
    }

    auto has_errors() const noexcept -> bool { return !messages_.IsEmpty(); }

    auto format() const -> FString { return FString::Join(messages_, TEXT("\n")); }
  private:
    TArray<FString> messages_{};
};
}
