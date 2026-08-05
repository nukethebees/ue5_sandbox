#include "TestMaterialConfig.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <Components/StaticMeshComponent.h>
#include <HAL/Platform.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>
#include <UObject/Object.h>

auto FTestMaterialState::initialise(UObject* outer) -> bool {
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
auto FTestMaterialState::create_dynamic_instance(UMaterialInterface& interface, UObject* outer)
    -> UMaterialInstanceDynamic* {
    auto* mat{UMaterialInstanceDynamic::Create(&interface, outer)};

    if (!mat) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Could not make material instance"));
        return nullptr;
    }

    configure_instance(*mat);

    return mat;
}
void FTestMaterialState::configure_instance(UMaterialInstanceDynamic& inst) {
    inst.SetVectorParameterValue(TEXT("base_colour"), config.base_colour);

    inst.SetScalarParameterValue(TEXT("metallic"), config.metallic);
    inst.SetScalarParameterValue(TEXT("specular"), config.specular);
    inst.SetScalarParameterValue(TEXT("roughness"), config.roughness);

    inst.SetVectorParameterValue(TEXT("emissive_colour"), config.emissive_colour);
    inst.SetScalarParameterValue(TEXT("emissive_intensity"), config.emissive_intensity);

    inst.SetScalarParameterValue(TEXT("opacity"), config.opacity);
}
void FTestMaterialState::set_mesh_material(UStaticMeshComponent& mesh, int32 slot) {
    if (!material_instance) {
        return;
    }

    mesh.SetMaterial(slot, material_instance);
}
void FTestMaterialState::initialise_mesh_material(UObject* outer,
                                                  UStaticMeshComponent& mesh,
                                                  int32 slot) {
    if (!initialise(outer)) {
        return;
    }

    mesh.SetMaterial(slot, material_instance);
}

auto FTestMaterialState::update_instance() -> bool {
    if (!material_instance) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("material_instance is nullptr."));
        return false;
    }

    configure_instance(*material_instance);

    return true;
}
