#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/explosion/ExplosionConfig.h"
#include "ShooterGame/health/DeathHandler.h"
#include "ShooterGame/interaction/Describable.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "ExplosiveProp.generated.h"

class UStaticMeshComponent;
class USphereComponent;

class UHealthComponent;

UCLASS()
class SHOOTERGAME_API AExplosiveProp
    : public AActor
    , public IDeathHandler
    , public ml::LogMsgMixin<"AExplosiveProp", LogShooterGameActor>
    , public IDescribable {
    GENERATED_BODY()
  public:
    AExplosiveProp();

    // IDeathHandler implementation
    void handle_death() override;

    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Explosive Prop"))};
        return desc;
    }
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    UHealthComponent* health_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    FExplosionConfig explosion_config{};
};
