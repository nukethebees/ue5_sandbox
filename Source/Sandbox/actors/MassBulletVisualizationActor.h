#pragma once

#include <atomic>
#include <optional>

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassBulletVisualizationActor.generated.h"

class UBulletDataAsset;

UCLASS()
class SANDBOX_API AMassBulletVisualizationActor
    : public AActor
    , public ml::LogMsgMixin<"AMassBulletVisualizationActor"> {
    GENERATED_BODY()
  public:
    static constexpr float growth_factor{1.5};
    static constexpr std::size_t transform_queue_capacity{100000};

    AMassBulletVisualizationActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ISM")
    int32 preallocate_isms{0};

    std::optional<int32> add_instance(FTransform const& transform);
    void update_instance(int32 instance_index, FTransform const& transform);
    void remove_instance(int32 instance_index);
    void return_instance_to_pool(int32 instance_index) { free_indices.Add(instance_index); }
    bool mark_render_state_as_dirty() {
        if (ismc) {
            ismc->MarkRenderStateDirty();
            return true;
        }
        return false;
    }

    UInstancedStaticMeshComponent* get_ismc() const { return ismc; }
    void enqueue_transform(FTransform const& transform) {
        (void)transform_queue.enqueue(transform);
    }
    FTransform const& get_hidden_transform() const {
        static auto const transform{[]() -> FTransform {
            FRotator const rotation{};
            FVector const infinite_location{FLT_MAX, FLT_MAX, FLT_MAX};
            auto const scale{FVector::ZeroVector};

            return {rotation, infinite_location, scale};
        }()};

        return transform;
    }
  protected:
    virtual void BeginPlay() override;
  private:
    void handle_assets_ready(FPrimaryAssetId primary_asset_id);
    void handle_preallocation();
    TArray<int32> create_instances(int32 n);
    void add_instances(int32 n);
    void grow_instances();
    void register_phase_end_callback();
    void on_phase_end(float delta_time);

    UPROPERTY(VisibleAnywhere)
    UInstancedStaticMeshComponent* ismc{nullptr};

    UPROPERTY()
    UBulletDataAsset* bullet_data;

    TArray<int32> free_indices{};
    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<FTransform>> transform_queue{};
    FDelegateHandle phase_end_delegate_handle{};
};
