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
    static constexpr float growth_factor{1.5};

    AMassBulletVisualizationActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ISM")
    int32 preallocate_isms{0};

    std::optional<int32> add_instance(FTransform const& transform);
    void update_instance(int32 instance_index, FTransform const& transform);
    void remove_instance(int32 instance_index);

    UInstancedStaticMeshComponent* get_ismc() const { return ismc; }
  protected:
    virtual void BeginPlay() override;
  private:
    void handle_assets_ready(FPrimaryAssetId primary_asset_id);
    void handle_preallocation();
    TArray<int32> create_instances(int32 n);
    void add_instances(int32 n);
    void grow_instances();
    FTransform const& get_hidden_transform() const;

    UPROPERTY(VisibleAnywhere)
    UInstancedStaticMeshComponent* ismc{nullptr};

    UPROPERTY()
    UBulletDataAsset* bullet_data;

    TArray<int32> free_indices{};
};
