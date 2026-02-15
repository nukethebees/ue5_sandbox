#include "Sandbox/players/TestEnemyController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/core/SandboxDeveloperSettings.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/CombatActor.h"
#include "Sandbox/players/SandboxMobInterface.h"
#include "Sandbox/players/TestEnemy.h"
#include "Sandbox/players/TestEnemyBlackboardConstants.h"
#include "Sandbox/utilities/array.h"
#include "Sandbox/utilities/geometry.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

using C = TestEnemyBlackboardConstants::FName;

#define LOG(VERBOSITY, MESSAGE, ...) UE_LOG(LogSandboxAI, VERBOSITY, TEXT(MESSAGE), __VA_ARGS__)

ATestEnemyController::ATestEnemyController()
    : ai_perception(CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")))
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
    ai_perception->OnTargetPerceptionForgotten.AddDynamic(
        this, &ATestEnemyController::on_target_perception_forgotten);

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
    RETURN_IF_NULLPTR(behaviour_tree_asset);

    auto* bb_ptr{Blackboard.Get()};
    UseBlackboard(behaviour_tree_asset->BlackboardAsset, bb_ptr);
    RunBehaviorTree(behaviour_tree_asset);

    auto const team_if{Cast<IGenericTeamAgentInterface>(pawn)};
    RETURN_IF_NULLPTR(team_if);
    SetGenericTeamId(team_if->GetGenericTeamId());

    TRY_INIT_PTR(test_enemy, Cast<ATestEnemy>(pawn));
    TRY_INIT_PTR(mob_interface, Cast<ISandboxMobInterface>(pawn));
    TRY_INIT_PTR(combat_interface, Cast<ICombatActor>(pawn));

    set_bb_value(C::acceptable_radius(), mob_interface->get_acceptable_radius());
    set_bb_value(C::attack_radius(), mob_interface->get_attack_acceptable_radius());

    ai_state = mob_interface->get_default_ai_state();
    set_bb_value(C::default_ai_state(), ai_state);
    set_ai_state(ai_state);

    auto const attack_profile{combat_interface->get_combat_profile()};
    set_bb_value(C::mob_attack_mode(), attack_profile.attack_mode);
    set_bb_value(C::defend_actor(), static_cast<UObject*>(test_enemy->get_defend_actor()));
    set_bb_value(C::defend_position(), test_enemy->get_defend_position());

#if WITH_EDITOR
    auto const new_label{FString::Printf(TEXT("TestEnemyController_%s"), *pawn->GetActorLabel())};
    SetActorLabel(new_label);
#endif
}
void ATestEnemyController::OnUnPossess() {
    Super::OnUnPossess();
}

void ATestEnemyController::on_target_perception_updated(AActor* actor, FAIStimulus stimulus) {
    using C = TestEnemyBlackboardConstants::FName;

    RETURN_IF_NULLPTR(Blackboard);

    auto const display_name{ml::get_best_display_name(*actor)};
    LOG(VeryVerbose, "Perception updated for: %s.", *display_name);

    if (stimulus.WasSuccessfullySensed()) {
        LOG(VeryVerbose, "Successfully sensed: %s.", *display_name);

        auto const attitude{GetTeamAttitudeTowards(*actor)};
        if (attitude == ETeamAttitude::Hostile) {
            LOG(VeryVerbose, "Spotted enemy: %s", *display_name);
            on_enemy_spotted.Broadcast();

            set_ai_state(EAIState::Combat);
            set_bb_value(C::target_actor(), static_cast<UObject*>(actor));
            set_bb_value(C::last_known_location(), actor->GetActorLocation());
        } else {
            LOG(VeryVerbose, "Not hostile: %s", *display_name);
        }
    }
}
void ATestEnemyController::on_target_perception_forgotten(AActor* actor) {
    auto const display_name{ml::get_best_display_name(*actor)};
    LOG(VeryVerbose, "Forgot: %s", *display_name);

    auto* tgt_actor_obj{Blackboard->GetValueAsObject(C::target_actor())};
    if (!tgt_actor_obj) {
        return;
    }

    if (actor == tgt_actor_obj) {
        set_ai_state(EAIState::Investigate);
        Blackboard->ClearValue(C::target_actor());
    }
}
void ATestEnemyController::visualise_vision_cone() {
    check(sight_config);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(pawn, GetPawn());

    auto const angle_deg{sight_config->PeripheralVisionAngleDegrees};
    auto const origin{pawn->GetActorLocation()};
    auto const fwd{pawn->GetActorForwardVector()};

    int32 const n_segments{8};
    int32 const n_lines{n_segments + 1};

    constexpr auto map_fn{[](float x) { return FRotator{0.0f, x, 0.0f}; }};
    auto const angles{ml::subdivide_arc_into_segments(-angle_deg, angle_deg * 2.0f, n_segments)};
    auto rots{ml::map_array<map_fn>(angles)};

    auto draw_vision{[&](float len, FColor col, float thickness) {
        auto outs{ml::map_array(rots, [&](FRotator const& rot) {
            return origin + rot.RotateVector(fwd).GetSafeNormal() * len;
        })};

        auto draw_line{[&](FVector const& from, FVector const& to) -> void {
            DrawDebugLine(world, from, to, col, false, -1.f, 0, thickness);
        }};

        for (auto const& out : outs) {
            draw_line(origin, out);
        }
        for (int32 i{1}; i < n_lines; i++) {
            draw_line(outs[i - 1], outs[i]);
        }
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
            LOG(Verbose, "Sensed %s near pawn.", *ml::get_best_display_name(enemy));
            return true;
        }
    }

    return false;
}
