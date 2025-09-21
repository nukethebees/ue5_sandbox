#pragma once

#include <span>

#include "CoreMinimal.h"

struct FTriggerResults {
    TArray<AActor*> actors; // [triggered...][not_triggered...]
    int32 n_triggered{0};   // Boundary marker

    template <typename Self>
    auto get_triggered(this Self&& self) {
        auto const n{self.n_triggered};
        return std::span(std::forward<Self>(self).actors.GetData(), n);
    }
    template <typename Self>
    auto get_not_triggered(this Self&& self) {
        auto const n{self.n_triggered};
        auto const count{self.actors.Num() - n};
        auto const start{std::forward<Self>(self).actors.GetData() + n};

        return std::span(start, count);
    }

    bool any_triggered() const { return n_triggered > 0; }
    bool any_not_triggered() const { return n_triggered < actors.Num(); }
    int32 num_not_triggered() const { return actors.Num() - n_triggered; }
};
