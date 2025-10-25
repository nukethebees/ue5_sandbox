#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/effects/data/ExplosionConfig.h"
#include "Sandbox/players/npcs/characters/TestEnemy.h"

#include "TestExplodingEnemy.generated.h"

UCLASS()
class SANDBOX_API ATestExplodingEnemy : public ATestEnemy {
    GENERATED_BODY()
  public:
    ATestExplodingEnemy();

    // ICombatActor
    virtual bool attack_actor(AActor* target) override;
  protected:
    // IDeathHandler
    virtual void handle_death() override;
    void explode(UWorld& world);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    FExplosionConfig explosion_config{};
};
