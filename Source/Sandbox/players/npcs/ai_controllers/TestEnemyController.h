#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "Sandbox/players/npcs/data/npc_delegates.h"
#include "Sandbox/players/npcs/enums/AIState.h"

#include "Sandbox/players/npcs/blackboard_utils.hpp"

#include "TestEnemyController.generated.h"

class UWorld;
class APawn;
class UStateTreeComponent;
class UAIPerceptionComponent;
class UBehaviorTree;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class ETestEnemyControllerAiMode : uint8 { BehaviourTree, StateTree };

UCLASS()
class SANDBOX_API ATestEnemyController : public AAIController {
    GENERATED_BODY()
  public:
    ATestEnemyController();

    FOnEnemySpotted on_enemy_spotted;

    virtual void Tick(float delta_time) override;

    auto get_ai_state() const { return ai_state; }
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);
    UFUNCTION()
    void on_target_perception_forgotten(AActor* Actor);
    void set_bb_value(FName const& name, auto const& value) {
        check(Blackboard);
        ml::set_bb_value(*Blackboard, name, value);
    }
    void visualise_vision_cone();
    void set_ai_state(EAIState state);

    auto scan_around_pawn(UWorld& world, FVector scan_location) const -> TArray<FHitResult>;
    auto check_for_enemy_nearby(UWorld& world, FVector scan_location, AActor& enemy) const -> bool;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UStateTreeComponent* state_tree{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UAISenseConfig_Sight* sight_config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EAIState ai_state;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETestEnemyControllerAiMode state_tree_mode{ETestEnemyControllerAiMode::BehaviourTree};
};
