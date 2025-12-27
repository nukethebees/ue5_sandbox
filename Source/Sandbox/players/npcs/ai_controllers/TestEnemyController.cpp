#include "Sandbox/players/npcs/ai_controllers/TestEnemyController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/core/SandboxDeveloperSettings.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/players/npcs/data/TestEnemyBlackboardConstants.h"
#include "Sandbox/players/npcs/interfaces/CombatActor.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

using C = TestEnemyBlackboardConstants::FName;

ATestEnemyController::ATestEnemyController()
    : ai_perception(CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")))
    , behavior_tree_component(
          CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent")))
    , blackboard_component(
          CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent")))
    , sight_config(CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"))) {
    sight_config->SightRadius = 800.0f;
    sight_config->LoseSightRadius = 900.0f;
    sight_config->PeripheralVisionAngleDegrees = 90.0f;
    sight_config->SetMaxAge(5.0f);

    sight_config->DetectionByAffiliation.bDetectEnemies = true;
    sight_config->DetectionByAffiliation.bDetectFriendlies = true;
    sight_config->DetectionByAffiliation.bDetectNeutrals = true;

    ai_perception->ConfigureSense(*sight_config);
    ai_perception->SetDominantSense(sight_config->GetSenseImplementation());

    // Auto-update blackboard when target is spotted/lost
    ai_perception->OnTargetPerceptionUpdated.AddDynamic(
        this, &ATestEnemyController::on_target_perception_updated);

    SetPerceptionComponent(*ai_perception);
}

void ATestEnemyController::Tick(float delta_time) {
    Super::Tick(delta_time);

    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    if (settings->visualise_ai_vision_cones) {
        visualise_vision_cone();
    }
}

void ATestEnemyController::BeginPlay() {
    Super::BeginPlay();
}
void ATestEnemyController::OnPossess(APawn* pawn) {
    Super::OnPossess(pawn);

    RETURN_IF_NULLPTR(pawn);
    RETURN_IF_NULLPTR(blackboard_component);
    RETURN_IF_NULLPTR(behaviour_tree_asset);

    auto const team_if{Cast<IGenericTeamAgentInterface>(pawn)};
    RETURN_IF_NULLPTR(team_if);
    SetGenericTeamId(team_if->GetGenericTeamId());

    TRY_INIT_PTR(mob_interface, Cast<ISandboxMobInterface>(pawn));
    TRY_INIT_PTR(combat_interface, Cast<ICombatActor>(pawn));

    ai_state = mob_interface->get_default_ai_state();

    set_bb_value(C::acceptable_radius(), mob_interface->get_acceptable_radius());
    set_bb_value(C::attack_radius(), mob_interface->get_attack_acceptable_radius());
    set_bb_value(C::default_ai_state(), ai_state);
    set_ai_state(ai_state);

    auto const attack_profile{combat_interface->get_combat_profile()};
    set_bb_value(C::mob_attack_mode(), attack_profile.attack_mode);

    UseBlackboard(behaviour_tree_asset->BlackboardAsset, blackboard_component);
    RunBehaviorTree(behaviour_tree_asset);
}
void ATestEnemyController::OnUnPossess() {
    Super::OnUnPossess();

    if (behavior_tree_component) {
        behavior_tree_component->StopTree();
    }
}

void ATestEnemyController::on_target_perception_updated(AActor* actor, FAIStimulus stimulus) {
    constexpr auto LOG{NestedLogger<"on_target_perception_updated">()};

    UE_LOG(LogSandboxController, VeryVerbose, TEXT("Perception updated."));

    RETURN_IF_NULLPTR(actor);
    RETURN_IF_NULLPTR(blackboard_component);

    using C = TestEnemyBlackboardConstants::FName;

    if (stimulus.WasSuccessfullySensed()) {
        auto const attitude{GetTeamAttitudeTowards(*actor)};

        UE_LOG(LogSandboxController,
               VeryVerbose,
               TEXT("Sensed: %s."),
               *ml::get_best_display_name(*actor));

        if (attitude == ETeamAttitude::Hostile) {
            on_enemy_spotted.Broadcast();

            set_bb_value(C::target_actor(), static_cast<UObject*>(actor));
            set_bb_value(C::last_known_location(), actor->GetActorLocation());
            UE_LOG(LogSandboxController,
                   Verbose,
                   TEXT("Spotted enemy: %s"),
                   *ml::get_best_display_name(*actor));
        } else {
            UE_LOG(LogSandboxController, Verbose, TEXT("Not hostile"));
        }
    } else {
        if (auto* old_actor{
                Cast<AActor>(blackboard_component->GetValueAsObject(C::target_actor()))}) {
            bool actor_found{false};

            // Do a raycast, if the enemy is within X metres then assume we still sense it
            TRY_INIT_PTR(pawn, GetPawn());
            TRY_INIT_PTR(world, GetWorld());
            auto const scan_location{pawn->GetActorLocation()};

            if (!check_for_enemy_nearby(*world, scan_location, *old_actor)) {
                UE_LOG(LogSandboxController,
                       Verbose,
                       TEXT("Lost sight of: %s"),
                       *ml::get_best_display_name(*actor));
                blackboard_component->ClearValue(C::target_actor());
            }
        }
    }
}
void ATestEnemyController::visualise_vision_cone() {
    check(sight_config);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(pawn, GetPawn());

    auto const angle_deg{sight_config->PeripheralVisionAngleDegrees};
    auto const angle_rad{FMath::DegreesToRadians(angle_deg)};
    auto const origin{pawn->GetActorLocation()};
    auto const fwd{pawn->GetActorForwardVector()};

    FRotator const yaw_rotation_right(0.0f, angle_deg, 0.0f);
    FRotator const yaw_rotation_left(0.0f, -angle_deg, 0.0f);

    auto draw_vision{[&](float len, FColor col, float thickness) {
        auto const rot_right{yaw_rotation_right.RotateVector(fwd) * len};
        auto const rot_left{yaw_rotation_left.RotateVector(fwd) * len};

        auto const loc_front{origin + fwd * len};
        auto const loc_left{origin + rot_left};
        auto const loc_right{origin + rot_right};

        auto draw_line{[&](FVector const& from, FVector const& to) -> void {
            DrawDebugLine(world, from, to, col, false, -1.f, 0, thickness);
        }};

        draw_line(origin, loc_left);
        draw_line(origin, loc_front);
        draw_line(origin, loc_right);
        draw_line(loc_front, loc_left);
        draw_line(loc_front, loc_right);
    }};

    draw_vision(sight_config->SightRadius, FColor::Green, 5.f);
    draw_vision(sight_config->LoseSightRadius, FColor::Blue, 2.f);
}
void ATestEnemyController::set_ai_state(EAIState state) {
    ai_state = state;
    set_bb_value(C::ai_state(), state);
}
auto ATestEnemyController::scan_around_pawn(UWorld& world, FVector scan_location) const
    -> TArray<FHitResult> {
    TArray<FHitResult> hits;
    constexpr float h{200.0f};

    auto const capsule{FCollisionShape::MakeCapsule(h, h)};
    FCollisionQueryParams params;
    params.AddIgnoredActor(Owner);
    params.AddIgnoredActor(this);

    auto sweep_hit{world.SweepMultiByChannel(
        hits, scan_location, scan_location, FQuat::Identity, ECC_Pawn, capsule, params)};

    DrawDebugCapsule(&world, scan_location, h, h, FQuat::Identity, FColor::Purple);

    return hits;
}
auto ATestEnemyController::check_for_enemy_nearby(UWorld& world,
                                                  FVector scan_location,
                                                  AActor& enemy) const -> bool {
    auto const hits{scan_around_pawn(world, scan_location)};

    for (auto const& hit : hits) {
        if (hit.GetActor() == &enemy) {
            UE_LOG(LogSandboxController,
                   Verbose,
                   TEXT("Sensed %s near pawn."),
                   *ml::get_best_display_name(enemy));
            return true;
        }
    }

    return false;
}
