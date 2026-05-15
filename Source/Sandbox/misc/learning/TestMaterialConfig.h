#pragma once

#include <Math/Color.h>

#include "TestMaterialConfig.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UStaticMeshComponent;

USTRUCT()
struct FTestMaterialConfig {
    GENERATED_BODY()

    FTestMaterialConfig() = default;

    UPROPERTY(EditAnywhere)
    FColor base_colour{FColor::Red};
    UPROPERTY(EditAnywhere)
    float metallic{0.f};
    UPROPERTY(EditAnywhere)
    float specular{0.5f};
    UPROPERTY(EditAnywhere)
    float roughness{0.5f};
    UPROPERTY(EditAnywhere)
    FColor emissive_colour{FColor::Red};
    UPROPERTY(EditAnywhere)
    float emissive_intensity{1.f};
    UPROPERTY(EditAnywhere)
    float opacity{1.f};
};

USTRUCT()
struct FTestMaterialState {
    GENERATED_BODY()

    FTestMaterialState() = default;

    [[nodiscard]]
    auto initialise(UObject* outer) -> bool;
    auto create_dynamic_instance(UMaterialInterface& interface, UObject* outer)
        -> UMaterialInstanceDynamic*;

    void set_mesh_material(UStaticMeshComponent& mesh, int32 slot);
    void initialise_mesh_material(UObject* outer, UStaticMeshComponent& mesh, int32 slot);

    auto update_instance() -> bool;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UMaterialInterface> material_interface{nullptr};
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UMaterialInstanceDynamic> material_instance{nullptr};

    UPROPERTY(EditAnywhere)
    FTestMaterialConfig config{};
  private:
    void configure_instance(UMaterialInstanceDynamic& inst);
};
