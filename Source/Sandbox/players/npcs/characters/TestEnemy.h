#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/health/interfaces/DeathHandler.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/common/player_delegates.h"
#include "Sandbox/players/npcs/data/CombatProfile.h"
#include "Sandbox/players/npcs/enums/AIState.h"
#include "Sandbox/players/npcs/enums/MobAttackMode.h"
#include "Sandbox/players/npcs/interfaces/CombatActor.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"
#include "Sandbox/utilities/enums.h"

#include "TestEnemy.generated.h"

class UWorld;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UArrowComponent;

class AAIController;
class UHealthComponent;
class UNpcPatrolComponent;
class ABulletActor;

UCLASS()
class SANDBOX_API ATestEnemy
    : public ACharacter
    , public ml::LogMsgMixin<"ATestEnemy", LogSandboxCharacter>
    , public ICombatActor
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface
    , public IDescribable {
    GENERATED_BODY()
  public:
    ATestEnemy();

    bool attack_actor_melee(UWorld& world, AActor& target);
    bool attack_actor_ranged(UWorld& world, AActor& target);

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ICombatActor
    virtual bool attack_actor(AActor& target) override;
    virtual auto get_combat_profile() const -> FCombatProfile override { return combat_profile; }

    // ISandboxMobInterface
    virtual float get_acceptable_radius() const override;
    virtual float get_attack_acceptable_radius() const override;
    virtual EAIState get_default_ai_state() const override { return default_ai_state; }

    // IDescribable
    virtual FText get_description() const override {
        auto const attack_mode{ml::to_string_without_type_prefix(combat_profile.attack_mode)};

        auto const fmt{FText::FromName(TEXT("Test NPC ({0})"))};
        auto const description{FText::Format(fmt, FText::FromString(attack_mode))};
        return description;
    }

    FOnPlayerKilled on_player_killed;
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    // IDeathHandler
    virtual void handle_death() override;

    UPROPERTY(EditAnywhere, Category = "Turret")
    UArrowComponent* muzzle_point{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UHealthComponent* health{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UNpcPatrolComponent* patrol_state{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    FCombatProfile combat_profile{EMobAttackMode::None, 150.0f, 1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EAIState default_ai_state{EAIState::RandomlyMove};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Enemy};

    UPROPERTY(EditAnywhere, Category = "AI")
    TSubclassOf<ABulletActor> bullet_actor_class;
  private:
    void apply_material_colours();

    UMaterialInstanceDynamic* dynamic_material{nullptr};
    float last_attack_time{0.0f};
};
