#include "Sandbox/players/npcs/ai_controllers/SimpleManualAIController.h"

#include "Logging/StructuredLog.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/players/common/utilities/navigation.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void FSimpleManualAIControllerMemory::print_states() const {
    UE_LOGFMT(LogTemp,
              Verbose,
              "States:\n    Previous state: {0}\n    Current state: {1}\n    Next state: {2}",
              *UEnum::GetValueAsString(previous_state),
              *UEnum::GetValueAsString(state),
              *UEnum::GetValueAsString(next_state));
}

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
        memory.last_known_location = Actor->GetActorLocation();
        LOG.log_display(TEXT("Spotted: %s"), *Actor->GetName());
    } else {
        memory.target = nullptr;
        LOG.log_display(TEXT("Lost sight of: %s"), *Actor->GetName());
    }
}

void ASimpleManualAIController::OnMoveCompleted(FAIRequestID RequestID,
                                                FPathFollowingResult const& Result) {
    Super::OnMoveCompleted(RequestID, Result);
    constexpr auto LOG{NestedLogger<"OnMoveCompleted">()};
    using enum ESimpleManualAIState;

    LOG.log_verbose(TEXT("Finished moving."));

    if (memory.state == ESimpleManualAIState::Moving) {
        if (Result.IsSuccess()) {
            handle_successful_move();
        } else {
            LOG.log_warning(TEXT("Move failed."));
            move_to_state(Idle);
        }
    }
}

void ASimpleManualAIController::update_fsm(float delta_time) {
    constexpr auto LOG{NestedLogger<"update_fsm">()};

#define STATE_CASE(ENUM_NAME, FN_NAME) \
    case ENUM_NAME: {                  \
        FN_NAME(delta_time);           \
        break;                         \
    }

    switch (memory.state) {
        using enum ESimpleManualAIState;

        STATE_CASE(Idle, fsm_idle);
        STATE_CASE(Wandering, fsm_wandering);
        STATE_CASE(Following, fsm_following);
        STATE_CASE(Moving, fsm_moving);
        STATE_CASE(MovingToLastKnownLocation, fsm_moving_to_last_known_location);
        STATE_CASE(Stuck, fsm_stuck);

        default: {
            log_warning(TEXT("Unhandled FSM state."));
            break;
        }
    }

#undef STATE_CASE

    memory.state = memory.next_state;
}
void ASimpleManualAIController::move_to_state(ESimpleManualAIState new_state) {
    memory.next_state = new_state;
    move_to_next_state();
}
void ASimpleManualAIController::move_to_next_state() {
    constexpr auto LOG{NestedLogger<"move_to_next_state">()};

    LOG.log_verbose(TEXT("\nMoving state:\n    From: %s\n    To  : %s."),
                    *UEnum::GetValueAsString(memory.state),
                    *UEnum::GetValueAsString(memory.next_state));

    memory.previous_state = memory.state;
    memory.state = memory.next_state;

#define STATE_CASE(ENUM_NAME, BOOL_STATE) \
    case ENUM_NAME: {                     \
        SetActorTickEnabled(BOOL_STATE);  \
        break;                            \
    }

    switch (memory.state) {
        using enum ESimpleManualAIState;
        STATE_CASE(Idle, true);
        STATE_CASE(Wandering, true);
        STATE_CASE(Following, true);
        STATE_CASE(Moving, false);
        STATE_CASE(MovingToLastKnownLocation, true);
        STATE_CASE(Stuck, false);
        default: {
            LOG.log_warning(TEXT("Unhandled FSM state."));
            break;
        }
    }
#undef STATE_CASE
}

void ASimpleManualAIController::fsm_idle(float delta_time) {
    using enum ESimpleManualAIState;
    constexpr auto LOG{NestedLogger<"fsm_idle">()};

    if (memory.target) {
        move_to_state(Following);
    } else if (memory.last_known_location.IsSet()) {
        move_to_state(MovingToLastKnownLocation);
    } else {
        move_to_state(Wandering);
    }
}
void ASimpleManualAIController::fsm_wandering(float delta_time) {
    // Move to a random location then wait
    using enum ESimpleManualAIState;
    constexpr auto LOG{NestedLogger<"fsm_wandering">()};
    LOG.log_verbose(TEXT("Start"));

    TRY_INIT_PTR(pawn, GetPawn());

    auto const random_point{ml::get_random_nav_point(*pawn, config.wander_radius)};
    if (!random_point) {
        LOG.log_warning(TEXT("No point found."));
        return;
    }

    move_to_location(random_point->Location);
}
void ASimpleManualAIController::fsm_following(float delta_time) {
    move_to_location(memory.target->GetActorLocation());
}
void ASimpleManualAIController::fsm_moving(float delta_time) {
    return;
}
void ASimpleManualAIController::fsm_moving_to_last_known_location(float delta_time) {
    move_to_location(memory.last_known_location.GetValue());
}
void ASimpleManualAIController::fsm_stuck(float delta_time) {
    constexpr auto LOG{NestedLogger<"fsm_stuck">()};
}

void ASimpleManualAIController::move_to_location(FVector location) {
    constexpr auto LOG{NestedLogger<"move_to_location">()};
    using enum ESimpleManualAIState;

    FAIMoveRequest move_request{location};
    move_request.SetAcceptanceRadius(config.acceptable_move_radius);

    if (auto const result{MoveTo(move_request)};
        result.Code == EPathFollowingRequestResult::RequestSuccessful) {
        move_to_state(Moving);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("MoveTo command failed."));
    move_to_state(Idle);
}
void ASimpleManualAIController::handle_successful_move() {
    constexpr auto LOG{NestedLogger<"handle_successful_move">()};
    using enum ESimpleManualAIState;

    switch (memory.previous_state) {
        case MovingToLastKnownLocation: {
            // Let the perception component see if it found the target
            memory.last_known_location = NullOpt;
            [[fallthrough]];
        }
        case Following: {
            [[fallthrough]];
        }
        case Wandering: {
            wait_for_time([this]() { move_to_state(Idle); }, config.wander_wait_time);
            break;
        }
        default: {
            move_to_state(Idle);
            LOG.log_warning(TEXT("Unhandled previous state: %s"),
                            *UEnum::GetValueAsString(memory.previous_state));
        }
    }
}
