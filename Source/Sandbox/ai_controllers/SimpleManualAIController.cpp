#include "Sandbox/ai_controllers/SimpleManualAIController.h"

#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/utilities/navigation.h"

#include "Sandbox/macros/null_checks.hpp"

ASimpleManualAIController::ASimpleManualAIController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ai_perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

    sight_config = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    sight_config->SightRadius = 800.0f;
    sight_config->LoseSightRadius = 900.0f;
    sight_config->PeripheralVisionAngleDegrees = 270.0f;
    sight_config->SetMaxAge(5.0f);

    sight_config->DetectionByAffiliation.bDetectEnemies = true;
    sight_config->DetectionByAffiliation.bDetectFriendlies = true;
    sight_config->DetectionByAffiliation.bDetectNeutrals = true;

    ai_perception->ConfigureSense(*sight_config);
    ai_perception->SetDominantSense(sight_config->GetSenseImplementation());

    // Auto-update blackboard when target is spotted/lost
    ai_perception->OnTargetPerceptionUpdated.AddDynamic(
        this, &ASimpleManualAIController::on_target_perception_updated);

    SetPerceptionComponent(*ai_perception);
}

void ASimpleManualAIController::BeginPlay() {
    Super::BeginPlay();

    update_fsm(0.0f);
}
void ASimpleManualAIController::Tick(float delta_time) {
    update_fsm(delta_time);
}

void ASimpleManualAIController::on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus) {
    constexpr auto LOG{NestedLogger<"on_target_perception_updated">()};

    if (!Actor) {
        LOG.log_warning(TEXT("Actor is nullptr."));
        return;
    }

    if (Stimulus.WasSuccessfullySensed()) {
        memory.target = Actor;
        LOG.log_display(TEXT("Spotted: %s"), *Actor->GetName());
    } else {
        memory.target = nullptr;
        LOG.log_display(TEXT("Lost sight of: %s"), *Actor->GetName());
    }
}

void ASimpleManualAIController::update_fsm(float delta_time) {
    constexpr auto LOG{NestedLogger<"update_fsm">()};

    switch (memory.state) {
        using enum ESimpleManualAIState;
        case Idle: {
            fsm_wandering(delta_time);
            break;
        }
        case Wandering: {
            fsm_wandering(delta_time);
            break;
        }
        case Following: {
            fsm_following(delta_time);
            break;
        }
        case Stuck: {
            fsm_stuck(delta_time);
            break;
        }
        default: {
            log_warning(TEXT("Unhandled FSM state."));
            break;
        }
    }

    memory.state = memory.next_state;
}
void ASimpleManualAIController::move_to_state(ESimpleManualAIState new_state) {
    UE_LOGFMT(LogTemp,
              Verbose,
              "Moving from {old_state} to {new_state}.",
              ("old_state", *UEnum::GetValueAsString(memory.state)),
              ("new_state", *UEnum::GetValueAsString(new_state)));

    switch (memory.state) {
        using enum ESimpleManualAIState;
        case Idle: {
            SetActorTickEnabled(true);
            break;
        }
        case Wandering: {
            SetActorTickEnabled(true);
            break;
        }
        case Following: {
            SetActorTickEnabled(true);
            break;
        }
        case Stuck: {
            SetActorTickEnabled(false);
            break;
        }
        default: {
            log_warning(TEXT("Unhandled FSM state."));
            break;
        }
    }
}
void ASimpleManualAIController::fsm_idle(float delta_time) {
    using enum ESimpleManualAIState;
    constexpr auto LOG{NestedLogger<"fsm_idle">()};

    move_to_state(memory.target ? Following : Wandering);
}
void ASimpleManualAIController::fsm_wandering(float delta_time) {
    // Move to a random location then wait
    using enum ESimpleManualAIState;
    constexpr auto LOG{NestedLogger<"fsm_wandering">()};
    LOG.log_verbose(TEXT("Start"));

    auto const random_point{ml::get_random_nav_point(GetPawn(), config.wander_radius)};
    if (!random_point) {
        LOG.log_warning(TEXT("No point found."));
        return;
    }

    FAIMoveRequest move_request{random_point->Location};
    move_request.SetAcceptanceRadius(config.acceptable_move_radius);

    auto const result{MoveTo(move_request)};

    if (result.Code == EPathFollowingRequestResult::RequestSuccessful) {
        SetActorTickEnabled(false);

        constexpr bool repeat{false};
        GetWorld()->GetTimerManager().SetTimer(
            wander_timer, [this]() { move_to_state(Idle); }, config.wander_wait_time, repeat);

        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("MoveTo command failed."));
    move_to_state(Idle);
}
void ASimpleManualAIController::fsm_following(float delta_time) {
    // Follow target for X seconds then wait
    constexpr auto LOG{NestedLogger<"fsm_following">()};
}
void ASimpleManualAIController::fsm_stuck(float delta_time) {
    constexpr auto LOG{NestedLogger<"fsm_stuck">()};
}
