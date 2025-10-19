#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Color.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"

#include "TiledCubeActor.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EWallDirection : uint8 {
    Width UMETA(DisplayName = "Width"),
    Height UMETA(DisplayName = "Height"),
    Depth UMETA(DisplayName = "Depth"),
};

USTRUCT(BlueprintType)
struct FTiledCubeActorMaterialParams {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    FLinearColor base_colour{0.1f, 0.1f, 0.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    FLinearColor emissive_colour{0.0f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float metallic{0.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float specular{0.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float roughness{0.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    UTexture2D* face_texture{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    UMaterialInterface* face_material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    UMaterialInterface* edge_material{nullptr};
};

UCLASS()
class SANDBOX_API ATiledCubeActor
    : public AActor
    , public ml::LogMsgMixin<"ATiledCubeActor"> {
    GENERATED_BODY()
  public:
    ATiledCubeActor();
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float tile_width{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float tile_height{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float tile_depth{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float n_tiles_width{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float n_tiles_depth{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float n_tiles_height{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    EWallDirection u_direction{EWallDirection::Width};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    EWallDirection v_direction{EWallDirection::Height};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    FTiledCubeActorMaterialParams material{};
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* cube_mesh{nullptr};

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* face_material_inst;
  private:
    auto get_n(EWallDirection dir) {
        switch (dir) {
            using enum EWallDirection;
            case Width:
                return n_tiles_width;
            case Depth:
                return n_tiles_depth;
            case Height:
                return n_tiles_height;
            default:
                break;
        }

        return 0.0f;
    }

    static FName const face_material_name;
    static FName const edge_material_name;
};
