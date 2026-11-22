#pragma once

#include <SpaceGamePresentation/entities/TestTeam.h>

#include <GameFramework/Actor.h>

#include "TestFighterProxy.generated.h"

class UInstancedStaticMeshComponent;

namespace ioj {
UCLASS(meta = (DisplayName = "Fighter Proxy"))
class SPACEGAME_API ATestFighterProxy : public AActor {
    GENERATED_BODY()

  public:
    ATestFighterProxy();

    auto get_team() const noexcept { return team; }
    void set_team(ETestTeam const value) noexcept { team = value; }

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditInstanceOnly, Category = "Level Authoring", meta = (DisplayName = "Custom Name"))
    FName custom_name{NAME_None};

    UPROPERTY(EditInstanceOnly, Category = "Level Authoring", meta = (DisplayName = "Export"))
    bool export_to_script{true};
#endif
#if WITH_EDITOR
    bool IsActorLabelEditable() const override { return false; }
#endif
    UPROPERTY(EditInstanceOnly,
              Category = "Level Authoring",
              meta = (ClampMin = "0.0", Units = "s"))
    double spawn_time_seconds{};

  private:
    void OnConstruction(FTransform const& transform) override;
#if WITH_EDITOR
    void PostRegisterAllComponents() override;
#endif
    void refresh_preview();

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> mesh;

    UPROPERTY(EditInstanceOnly, Category = "Level Authoring")
    ETestTeam team{ETestTeam::White};
};
}
