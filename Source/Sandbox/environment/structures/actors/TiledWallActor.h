#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Color.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "TiledWallActor.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

UENUM(BlueprintType)
enum class EWallMeshType : uint8 {
    Floor UMETA(DisplayName = "Floor"),
    Ceiling UMETA(DisplayName = "Ceiling"),
    Wall_PosX UMETA(DisplayName = "Wall +X"),
    Wall_NegX UMETA(DisplayName = "Wall -X"),
    Wall_PosY UMETA(DisplayName = "Wall +Y"),
    Wall_NegY UMETA(DisplayName = "Wall -Y"),
};

UENUM(BlueprintType)
enum class EWorldDimension : uint8 {
    X UMETA(DisplayName = "X"),
    Y UMETA(DisplayName = "Y"),
    Z UMETA(DisplayName = "Z"),
};

USTRUCT(BlueprintType)
struct FWallMeshConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    UStaticMesh* mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    EWorldDimension u_maps_to{EWorldDimension::X};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    EWorldDimension v_maps_to{EWorldDimension::Z};
};

USTRUCT(BlueprintType)
struct FTiledWallActorMaterialParams {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    FLinearColor base_colour{0.1f, 0.1f, 0.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    FLinearColor emissive_colour{0.0f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    float metallic{0.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    float specular{0.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    float roughness{0.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    UTexture2D* face_texture{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    UMaterialInterface* face_material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    UMaterialInterface* edge_material{nullptr};
};

UCLASS()
class SANDBOX_API ATiledWallActor
    : public AActor
    , public ml::LogMsgMixin<"ATiledWallActor"> {
    GENERATED_BODY()
  public:
    ATiledWallActor();
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
  public:
    struct Constants {
#define MAKE_CONST(NAME)                \
    static auto NAME() {                \
        static FName text{TEXT(#NAME)}; \
        return text;                    \
    }
        MAKE_CONST(tiling_u)
        MAKE_CONST(tiling_v)

        MAKE_CONST(face_texture)

        MAKE_CONST(metallic)
        MAKE_CONST(specular)
        MAKE_CONST(roughness)

        MAKE_CONST(base_colour)
        MAKE_CONST(emissive_colour)
#undef MAKE_CONST
    };

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    EWallMeshType mesh_type{EWallMeshType::Floor};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    TMap<EWallMeshType, FWallMeshConfig> mesh_configs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    float scale_u{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    float scale_v{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Wall")
    FTiledWallActorMaterialParams material{};
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* wall_mesh{nullptr};

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* face_material_inst{nullptr};
  private:
    auto get_scale_for_dimension(EWorldDimension dim) const -> float {
        auto const* mesh_config{mesh_configs.Find(mesh_type)};
        if (!mesh_config) {
            return 1.0f;
        }

        if (dim == mesh_config->u_maps_to) {
            return scale_u;
        }
        if (dim == mesh_config->v_maps_to) {
            return scale_v;
        }
        return 1.0f;
    }

    static FName const face_material_name;
    static FName const edge_material_name;
};
