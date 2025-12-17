#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Engine.h"

class print_msg_mixin {
  protected:
    template <typename Self>
    void print_msg(this Self&& self, FString const& msg) {
        auto const fmsg{FString::Printf(TEXT("%s: %s"), *self.GetName(), *msg)};
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, fmsg);
    }
};
