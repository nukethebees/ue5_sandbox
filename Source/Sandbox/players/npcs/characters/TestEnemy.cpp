#include "Sandbox/players/npcs/characters/TestEnemy.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/players/npcs/ai_controllers/SimpleAIController.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATestEnemy::ATestEnemy()
    : body_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")))
    , light(CreateDefaultSubobject<UPointLightComponent>(TEXT("Light")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    body_mesh->SetupAttachment(RootComponent);
    light->SetupAttachment(body_mesh);

    AutoPossessAI = EAutoPossessAI::Spawned;
}

void ATestEnemy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    apply_light_colours();

    RETURN_IF_NULLPTR(body_mesh);
    RETURN_IF_NULLPTR(body_mesh->GetMaterial(0));

    dynamic_material = body_mesh->CreateDynamicMaterialInstance(0);
    apply_material_colours();
}
void ATestEnemy::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};
    Super::BeginPlay();

    if (controller_class) {
        AIControllerClass = controller_class;
    }

    if (!Controller) {
        RETURN_IF_NULLPTR(AIControllerClass);
        SpawnDefaultController();
    }
}

void ATestEnemy::handle_death() {
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
    auto const distance_to_target{FVector::Dist(GetActorLocation(), target->GetActorLocation())};
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
    return combat_profile.get_attack_range();
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
void ATestEnemy::apply_light_colours() {
    constexpr auto logger{NestedLogger<"apply_light_colours">()};

    logger.log_verbose(TEXT("Setting light colour."));

    RETURN_IF_NULLPTR(light);
    light->SetLightColor(light_colour);
}
