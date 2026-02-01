#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/explosion/ExplosionConfig.h"
#include "Sandbox/health/interfaces/DeathHandler.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "ExplosiveProp.generated.h"

class UStaticMeshComponent;
class USphereComponent;

class UHealthComponent;

UCLASS()
class SANDBOX_API AExplosiveProp
    : public AActor
    , public IDeathHandler
    , public ml::LogMsgMixin<"AExplosiveProp", LogSandboxActor>
    , public IDescribable {
    GENERATED_BODY()
  public:
    AExplosiveProp();

    // IDeathHandler implementation
    virtual void handle_death() override;

    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Explosive Prop"))};
        return desc;
    }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    UHealthComponent* health_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosive")
    FExplosionConfig explosion_config{};
};
