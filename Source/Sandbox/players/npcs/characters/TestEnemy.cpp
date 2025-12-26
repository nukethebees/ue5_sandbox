#include "Sandbox/players/npcs/characters/TestEnemy.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/players/npcs/actor_components/NpcPatrolComponent.h"
#include "Sandbox/players/npcs/ai_controllers/SimpleAIController.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATestEnemy::ATestEnemy()
    : body_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health")))
    , patrol_state(CreateDefaultSubobject<UNpcPatrolComponent>(TEXT("PatrolState")))
    , ai_state{default_ai_state} {
    PrimaryActorTick.bCanEverTick = false;

    body_mesh->SetupAttachment(RootComponent);

    AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ATestEnemy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    RETURN_IF_NULLPTR(body_mesh);
    RETURN_IF_NULLPTR(body_mesh->GetMaterial(0));

    dynamic_material = body_mesh->CreateDynamicMaterialInstance(0);
    apply_material_colours();

    if (controller_class) {
        AIControllerClass = controller_class;
    }
}
void ATestEnemy::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};
    Super::BeginPlay();
}

void ATestEnemy::handle_death() {
    on_player_killed.Broadcast();
    SetLifeSpan(0.1f);
}

FGenericTeamId ATestEnemy::GetGenericTeamId() const {
    return FGenericTeamId(static_cast<uint8>(team_id));
}

void ATestEnemy::SetGenericTeamId(FGenericTeamId const& TeamID) {
    team_id = static_cast<ETeamID>(TeamID.GetId());
}

bool ATestEnemy::attack_actor(AActor* target) {
    constexpr auto logger{NestedLogger<"attack_actor">()};

    RETURN_VALUE_IF_NULLPTR(target, false);
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);

    // Check cooldown
    auto const current_time{world->GetTimeSeconds()};
    auto const delta_time{current_time - last_attack_time};

    if (delta_time < combat_profile.melee_cooldown) {
        logger.log_verbose(TEXT("Attack on cooldown"));
        return false;
    }

    // Check distance
    // Use a hitscan to take enemy size into account
    // We hit their edge, not their middle
    FHitResult hit;
    auto const start{GetActorLocation()};
    auto const end{target->GetActorLocation()};
    FCollisionQueryParams query_params;
    query_params.AddIgnoredActor(this);

    if (!world->LineTraceSingleByChannel(hit, start, end, ECC_Pawn, query_params)) {
        logger.log_verbose(TEXT("Hit missed"));
        return false;
    }

    auto const distance_to_target{hit.Distance};
    if (distance_to_target > combat_profile.melee_range) {
        logger.log_verbose(TEXT("Target out of attack range: %.2f > %.2f"),
                           distance_to_target,
                           combat_profile.melee_range);
        return false;
    }

    // Find health component
    INIT_PTR_OR_RETURN_VALUE(
        target_health, target->FindComponentByClass<UHealthComponent>(), false);

    // Apply damage
    logger.log_verbose(
        TEXT("Attacking %s for %.2f damage"), *target->GetName(), combat_profile.melee_damage);
    target_health->modify_health(
        FHealthChange{combat_profile.melee_damage, EHealthChangeType::Damage});
    last_attack_time = current_time;

    return true;
}

UBehaviorTree* ATestEnemy::get_behaviour_tree_asset() const {
    return behaviour_tree_asset;
}

float ATestEnemy::get_acceptable_radius() const {
    return 50.0f;
}
float ATestEnemy::get_attack_acceptable_radius() const {
    return combat_profile.get_attack_range() / 2.0f;
}

void ATestEnemy::apply_material_colours() {
    constexpr auto logger{NestedLogger<"apply_material_colours">()};

    RETURN_IF_NULLPTR(dynamic_material);
    dynamic_material->SetVectorParameterValue(FName("base_colour"), mesh_base_colour);
    dynamic_material->SetVectorParameterValue(FName("emissive_colour"), mesh_emissive_colour);
}
