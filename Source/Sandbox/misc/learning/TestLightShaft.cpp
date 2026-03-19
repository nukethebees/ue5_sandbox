#include "Sandbox/misc/learning/TestLightShaft.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATestLightShaft::ATestLightShaft()
    : light_shaft_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("light_shaft"))} {
    PrimaryActorTick.bCanEverTick = false;
}

void ATestLightShaft::BeginPlay() {
    Super::BeginPlay();
}
void ATestLightShaft::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    RETURN_IF_NULLPTR(light_shaft_mesh);
    RETURN_IF_NULLPTR(light_material_base);

    light_material_instance = UMaterialInstanceDynamic::Create(light_material_base, this);
    RETURN_IF_NULLPTR(light_material_instance);
    light_shaft_mesh->SetMaterial(0, light_material_instance);

    TRY_INIT_PTR(underlying_mesh, light_shaft_mesh->GetStaticMesh());
    auto const bbox{underlying_mesh->GetBoundingBox()};
    auto const mesh_bounds{bbox.GetExtent() * 2.f};
    auto const max_len_cm{FMath::Max3(mesh_bounds.X, mesh_bounds.Y, mesh_bounds.Z)};
    auto const max_len_m{max_len_cm / 100.f};
    light_material_instance->SetScalarParameterValue(TEXT("beam_length_m"), max_len_m);

    auto const light_origin{light_shaft_mesh->GetComponentLocation()};
    light_material_instance->SetVectorParameterValue(TEXT("light_origin"), light_origin);

    UE_LOG(LogSandboxLearning, Display, TEXT("Beam origin: %s"), *light_origin.ToCompactString());
    UE_LOG(LogSandboxLearning, Display, TEXT("BBox extent: %s"), *bbox.ToCompactString());
    UE_LOG(LogSandboxLearning, Display, TEXT("Mesh bounds: %s"), *mesh_bounds.ToCompactString());
    UE_LOG(LogSandboxLearning, Display, TEXT("Beam length (m): %.2f"), max_len_m);
}
