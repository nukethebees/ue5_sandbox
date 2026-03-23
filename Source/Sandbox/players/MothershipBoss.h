#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MothershipBoss.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class USpotLightComponent;
class UMaterialInstance;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FSpotLightSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FLinearColor colour{FLinearColor::Yellow};
    UPROPERTY(EditAnywhere)
    float source_radius{1000.f};
    UPROPERTY(EditAnywhere)
    float attenuation_radius{1000.f};
    UPROPERTY(EditAnywhere)
    float brightness{10e6};
    UPROPERTY(EditAnywhere)
    float inner_cone_angle{30.f};
    UPROPERTY(EditAnywhere)
    float outer_cone_angle{60.f};
};

USTRUCT(BlueprintType)
struct FPointLightSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FLinearColor colour{FLinearColor::Red};
    UPROPERTY(EditAnywhere)
    float source_radius{1000.f};
    UPROPERTY(EditAnywhere)
    float attenuation_radius{1000.f};
    UPROPERTY(EditAnywhere)
    float brightness{10e6};
};

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
    UPROPERTY(EditAnywhere, Category = "Boss|Hatches")
    TArray<USpotLightComponent*> hatch_lights{};
    UPROPERTY(EditAnywhere, Category = "Boss|Hatches")
    FSpotLightSettings hatch_light_settings{};
    UPROPERTY(EditAnywhere, Category = "Boss|Hatches")
    FRotator hatch_light_rotation_speed{FRotator::ZeroRotator};


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
    FPointLightSettings point_light_settings{};

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
    void config_hatch_lights();
};
