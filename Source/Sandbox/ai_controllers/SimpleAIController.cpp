#include "Sandbox/ai_controllers/SimpleAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Sandbox/macros/null_checks.hpp"

ASimpleAIController::ASimpleAIController() {}

void ASimpleAIController::BeginPlay() {
    Super::BeginPlay();

    behavior_tree_component =
        CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
    blackboard_component =
        CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
    ai_perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

    TRY_INIT_PTR(world, GetWorld());

    constexpr float timer_delay{1.0f};
    constexpr bool repeat_timer{false};
    world->GetTimerManager().SetTimer(
        wander_timer, this, &ASimpleAIController::choose_new_target, timer_delay, repeat_timer);
}
void ASimpleAIController::OnMoveCompleted(FAIRequestID RequestID,
                                          FPathFollowingResult const& Result) {
    Super::OnMoveCompleted(RequestID, Result);

    constexpr auto LOG{NestedLogger<"OnMoveCompleted">()};
    LOG.log_verbose(TEXT("Done"));

    constexpr bool repeat_timer{false};

    GetWorld()->GetTimerManager().SetTimer(
        wander_timer, this, &ASimpleAIController::choose_new_target, wander_interval, repeat_timer);
}

void ASimpleAIController::choose_new_target() {
    constexpr auto LOG{NestedLogger<"choose_new_target">()};

    LOG.log_verbose(TEXT("start"));

    TRY_INIT_PTR(pawn, GetPawn());
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(nav_sys, UNavigationSystemV1::GetCurrent(world));

    FNavLocation random_point;

    if (nav_sys->GetRandomReachablePointInRadius(
            pawn->GetActorLocation(), wander_radius, random_point)) {
        MoveToLocation(random_point.Location);
    } else {
        log_warning(TEXT("Couldn't find a target."));
    }
}
