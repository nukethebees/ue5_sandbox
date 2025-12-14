#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/health/interfaces/DeathHandler.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/common/player_delegates.h"
#include "Sandbox/players/npcs/data/CombatProfile.h"
#include "Sandbox/players/npcs/enums/DefaultAIState.h"
#include "Sandbox/players/npcs/enums/MobAttackMode.h"
#include "Sandbox/players/npcs/interfaces/CombatActor.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"

#include "TestEnemy.generated.h"

class UHealthComponent;
class UNpcPatrolComponent;

UCLASS()
class SANDBOX_API ATestEnemy
    : public ACharacter
    , public ml::LogMsgMixin<"ATestEnemy", LogSandboxCharacter>
    , public ICombatActor
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface {
    GENERATED_BODY()
  public:
    ATestEnemy();

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ICombatActor
    virtual bool attack_actor(AActor* target) override;
    virtual auto get_combat_profile() const -> FCombatProfile override { return combat_profile; }

    // ISandboxMobInterface
    virtual UBehaviorTree* get_behaviour_tree_asset() const override;
    virtual float get_acceptable_radius() const override;
    virtual float get_attack_acceptable_radius() const override;
    virtual EDefaultAIState get_default_ai_state() const override { return default_ai_state; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor light_colour{FLinearColor::White};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Enemy};

    FOnPlayerKilled on_player_killed;
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    // IDeathHandler
    virtual void handle_death() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    FCombatProfile combat_profile{EMobAttackMode::None, 150.0f, 1000.0f};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UHealthComponent* health{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UNpcPatrolComponent* patrol_state{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EDefaultAIState default_ai_state{EDefaultAIState::RandomlyMove};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EDefaultAIState ai_state;
  private:
    void apply_material_colours();
    void apply_light_colours();

    UMaterialInstanceDynamic* dynamic_material{nullptr};
    float last_attack_time{0.0f};
};
