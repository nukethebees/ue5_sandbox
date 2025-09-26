#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "TiledCubeActor.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float width{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float height{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float depth{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float tile_width{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    float tile_height{1.0f};
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* cube_mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    UMaterialInterface* face_material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiled Cube")
    UMaterialInterface* edge_material;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* face_material_inst;
  private:
    static FName const face_material_name;
    static FName const edge_material_name;
};
