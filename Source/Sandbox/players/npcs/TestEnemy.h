#pragma once

#include "Sandbox/health/DeathHandler.h"
#include "Sandbox/health/HealthChange.h"
#include "Sandbox/interaction/Describable.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/player_delegates.h"
#include "Sandbox/players/common/TeamID.h"
#include "Sandbox/players/npcs/AIState.h"
#include "Sandbox/players/npcs/CombatActor.h"
#include "Sandbox/players/npcs/CombatProfile.h"
#include "Sandbox/players/npcs/MobAttackMode.h"
#include "Sandbox/players/npcs/SandboxMobInterface.h"
#include "Sandbox/utilities/enums.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "TestEnemy.generated.h"

class UWorld;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UArrowComponent;

class AAIController;
class UHealthComponent;
class UNpcPatrolComponent;
class ABulletActor;

USTRUCT(BlueprintType)
struct FTestEnemyRangedConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "AI")
    TSubclassOf<ABulletActor> bullet_actor_class;

    UPROPERTY(EditAnywhere, Category = "AI")
    float bullet_speed{2000.0f};

    UPROPERTY(EditAnywhere, Category = "AI")
    FHealthChange bullet_damage{5.f, EHealthChangeType::Damage};
};

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
    auto get_defend_actor() const -> AActor* { return defend_actor; }
    auto get_defend_position() const { return defend_position; }

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

    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    UMaterialInterface* mesh_base_material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(VisibleAnywhere, Category = "Visuals")
    UMaterialInstanceDynamic* dynamic_material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Enemy};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    FTestEnemyRangedConfig ranged_config;

    UPROPERTY(EditAnywhere, Category = "AI")
    AActor* defend_actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "AI")
    FVector defend_position{FVector::ZeroVector};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    float last_attack_time{0.0f};
  private:
    void apply_material_colours(UMaterialInstanceDynamic& dyn_material);
};
