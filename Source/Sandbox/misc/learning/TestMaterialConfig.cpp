#include "TestMaterialConfig.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <Components/StaticMeshComponent.h>
#include <HAL/Platform.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>
#include <UObject/Object.h>

auto FTestMaterialConfig::initialise(UObject* outer) -> bool {
    if (!material_interface) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("material_interface is nullptr."));

        return false;
    }

    material_instance = create_dynamic_instance(*material_interface, outer);

    if (!material_instance) {
        return false;
    }

    return true;
}
auto FTestMaterialConfig::create_dynamic_instance(UMaterialInterface& interface, UObject* outer)
    -> UMaterialInstanceDynamic* {
    auto* mat{UMaterialInstanceDynamic::Create(&interface, outer)};

    if (!mat) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Could not make material instance"));
        return nullptr;
    }

    configure_instance(*mat);

    return mat;
}
void FTestMaterialConfig::configure_instance(UMaterialInstanceDynamic& inst) {
    inst.SetVectorParameterValue(TEXT("base_colour"), base_colour);

    inst.SetScalarParameterValue(TEXT("metallic"), metallic);
    inst.SetScalarParameterValue(TEXT("specular"), specular);
    inst.SetScalarParameterValue(TEXT("roughness"), roughness);

    inst.SetVectorParameterValue(TEXT("emissive_colour"), emissive_colour);
    inst.SetScalarParameterValue(TEXT("emissive_intensity"), emissive_intensity);

    inst.SetScalarParameterValue(TEXT("opacity"), opacity);
}
void FTestMaterialConfig::set_mesh_material(UStaticMeshComponent& mesh, int32 slot) {
    if (!material_instance) {
        return;
    }

    mesh.SetMaterial(slot, material_instance);
}
void FTestMaterialConfig::initialise_mesh_material(UObject* outer,
                                                   UStaticMeshComponent& mesh,
                                                   int32 slot) {
    if (!initialise(outer)) {
        return;
    }

    mesh.SetMaterial(slot, material_instance);
}
