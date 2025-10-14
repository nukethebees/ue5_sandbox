#include "Sandbox/characters/TestEnemy.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/ai_controllers/SimpleAIController.h"

#include "Sandbox/macros/null_checks.hpp"

ATestEnemy::ATestEnemy()
    : body_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")))
    , light(CreateDefaultSubobject<UPointLightComponent>(TEXT("Light")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    body_mesh->SetupAttachment(RootComponent);
    light->SetupAttachment(body_mesh);

    AutoPossessAI = EAutoPossessAI::Spawned;
    if (controller_class) {
        AIControllerClass = controller_class;
    } else {
        AIControllerClass = ASimpleAIController::StaticClass();
    }
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

    if (!Controller && AIControllerClass) {
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

UBehaviorTree* ATestEnemy::get_behaviour_tree_asset() const {
    return behaviour_tree_asset;
}

float ATestEnemy::get_acceptable_radius() const {
    return acceptable_radius;
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
