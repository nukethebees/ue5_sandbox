#pragma once

#include <SpaceGame/ships/player/TestSpaceShip.h>

#include "S7LevelAuthoringTestActor.generated.h"

UCLASS()
class AS7LevelAuthoringTestActor final : public ATestSpaceShip {
    GENERATED_BODY()
  public:
    int32 modifications{};
    int32 constructions{};

    bool Modify(bool const always_mark_dirty = true) override {
        ++modifications;
        return Super::Modify(always_mark_dirty);
    }
    void OnConstruction(FTransform const& transform) override {
        ++constructions;
        Super::OnConstruction(transform);
    }
};
