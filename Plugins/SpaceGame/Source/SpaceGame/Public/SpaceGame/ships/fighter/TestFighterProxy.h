#pragma once

#include <SpaceGamePresentation/entities/TestTeam.h>

#include <GameFramework/Actor.h>

#include "TestFighterProxy.generated.h"

class UStaticMeshComponent;

namespace ioj {
UCLASS(meta = (DisplayName = "Fighter Proxy"))
class SPACEGAME_API ATestFighterProxy : public AActor {
    GENERATED_BODY()

  public:
    ATestFighterProxy();

    auto get_team() const noexcept { return team; }
    void set_team(ETestTeam const value) noexcept { team = value; }

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditInstanceOnly, Category = "Level Authoring", meta = (DisplayName = "Export"))
    bool export_to_script{true};
#endif
    UPROPERTY(EditInstanceOnly,
              Category = "Level Authoring",
              meta = (ClampMin = "0.0", Units = "s"))
    double spawn_time_seconds{};

  private:
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> mesh;

    UPROPERTY(EditInstanceOnly, Category = "Level Authoring")
    ETestTeam team{ETestTeam::White};
};
}
