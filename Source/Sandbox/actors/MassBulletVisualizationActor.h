#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassBulletVisualizationActor.generated.h"

class UInstancedStaticMeshComponent;
class UBulletDataAsset;

UCLASS()
class SANDBOX_API AMassBulletVisualizationActor
    : public AActor
    , public ml::LogMsgMixin<"AMassBulletVisualizationActor"> {
    GENERATED_BODY()
  public:
    AMassBulletVisualizationActor();

    std::optional<int32> add_instance(FTransform const& transform);
    void update_instance(int32 instance_index, FTransform const& transform);
    void remove_instance(int32 instance_index);

    UInstancedStaticMeshComponent* get_ismc() const { return ismc; }
  protected:
    virtual void BeginPlay() override;
  private:
    void handle_assets_ready(FPrimaryAssetId primary_asset_id);

    UPROPERTY(VisibleAnywhere)
    UInstancedStaticMeshComponent* ismc{nullptr};

    UPROPERTY()
    UBulletDataAsset* bullet_data;

    TArray<int32> free_indices{};
};
