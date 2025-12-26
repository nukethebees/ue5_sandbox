#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/npcs/data/npc_delegates.h"
#include "Sandbox/players/npcs/enums/AIState.h"

#include "Sandbox/players/npcs/blackboard_utils.hpp"

#include "TestEnemyController.generated.h"

class UWorld;
class APawn;

class UAIPerceptionComponent;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;
class UAISenseConfig_Sight;

UCLASS()
class SANDBOX_API ATestEnemyController
    : public AAIController
    , public ml::LogMsgMixin<"ATestEnemyController", LogSandboxController> {
    GENERATED_BODY()
  public:
    ATestEnemyController();

    FOnEnemySpotted on_enemy_spotted;

    virtual void Tick(float delta_time) override;
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);
    void set_bb_value(FName const& name, auto const& value) {
        check(blackboard_component);
        ml::set_bb_value(*blackboard_component, name, value);
    }
    void visualise_vision_cone();
    void set_ai_state(EAIState state);

    auto scan_around_pawn(UWorld& world, FVector scan_location) const -> TArray<FHitResult>;
    auto check_for_enemy_nearby(UWorld& world, FVector scan_location, AActor& enemy) const -> bool;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTreeComponent* behavior_tree_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBlackboardComponent* blackboard_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UAISenseConfig_Sight* sight_config;
};
