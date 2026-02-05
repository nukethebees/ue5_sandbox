#include "Sandbox/players/npcs/SimpleCharacter.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/health/HealthComponent.h"
#include "Sandbox/players/npcs/SimpleAIController.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ASimpleCharacter::ASimpleCharacter()
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

void ASimpleCharacter::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    apply_light_colours();

    RETURN_IF_NULLPTR(body_mesh);
    RETURN_IF_NULLPTR(body_mesh->GetMaterial(0));

    dynamic_material = body_mesh->CreateDynamicMaterialInstance(0);
    apply_material_colours();
}
void ASimpleCharacter::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};
    Super::BeginPlay();

    if (!Controller && AIControllerClass) {
        SpawnDefaultController();
    }
}

void ASimpleCharacter::handle_death() {
    SetLifeSpan(0.1f);
}

FGenericTeamId ASimpleCharacter::GetGenericTeamId() const {
    return FGenericTeamId(static_cast<uint8>(team_id));
}

void ASimpleCharacter::SetGenericTeamId(FGenericTeamId const& TeamID) {
    team_id = static_cast<ETeamID>(TeamID.GetId());
}

float ASimpleCharacter::get_acceptable_radius() const {
    return acceptable_radius;
}

void ASimpleCharacter::apply_material_colours() {
    constexpr auto logger{NestedLogger<"apply_material_colours">()};

    RETURN_IF_NULLPTR(dynamic_material);
    dynamic_material->SetVectorParameterValue(FName("base_colour"), mesh_base_colour);
    dynamic_material->SetVectorParameterValue(FName("emissive_colour"), mesh_emissive_colour);
}
void ASimpleCharacter::apply_light_colours() {
    constexpr auto logger{NestedLogger<"apply_light_colours">()};

    RETURN_IF_NULLPTR(light);
    light->SetLightColor(light_colour);
}
