#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MothershipBoss.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class UMaterialInstance;
class UMaterialInstanceDynamic;

UCLASS()
class AMothershipBoss : public AActor {
    GENERATED_BODY()
  public:
    AMothershipBoss();

    inline static constexpr int32 n_hatches{4};
    inline static constexpr int32 n_search_lights{8};
    inline static constexpr int32 n_point_lights{8};

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Boss")
    UStaticMeshComponent* mesh_component{nullptr};

    // Hatches
    UPROPERTY(EditAnywhere, Category = "Boss|Hatches")
    UStaticMesh* hatch_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Boss|Hatches")
    TArray<UStaticMeshComponent*> hatch_meshes{};

    // Search lights
    UPROPERTY(EditAnywhere, Category = "Boss|Search light")
    UStaticMesh* search_light_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Boss|Search light")
    TArray<UStaticMeshComponent*> search_light_meshes{};
    UPROPERTY(EditAnywhere, Category = "Boss|Search light")
    UMaterialInstance* search_light_mesh_material{nullptr};

    // Point lights
    UPROPERTY(EditAnywhere, Category = "Boss|Point lights")
    TArray<UPointLightComponent*> point_lights{};
    UPROPERTY(EditAnywhere, Category = "Boss|Point lights")
    FLinearColor point_light_colour{FLinearColor::Red};
    UPROPERTY(EditAnywhere, Category = "Boss|Point lights")
    float point_light_brightness{100e6};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Boss|Movement")
    FRotator rotation_speed{FRotator::ZeroRotator};
  private:
    template <int32 N, typename T>
    void add_n_components(TArray<T*>& components, FString const& name_base);
    template <int32 N, typename T>
    void attach_components(UStaticMeshComponent& parent,
                           UStaticMesh& parent_mesh,
                           TArray<T*>& components,
                           FString const& socket_base);
    void set_meshes(UStaticMesh* mesh, TArray<UStaticMeshComponent*>& components);
    void config_point_lights();
};
