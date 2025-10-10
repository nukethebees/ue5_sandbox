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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    int32 preallocate_isms{0};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    int32 cull_min_distance{0};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    int32 cull_max_distance{50000};
  protected:
    virtual void BeginPlay() override;
  private:
    void handle_assets_ready(FPrimaryAssetId primary_asset_id);
    void handle_preallocation();
    void create_instances(int32 n);
    void add_instances(int32 n);
    void grow_instances();
    void register_phase_end_callback();
    void on_phase_end(float delta_time);

    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    UInstancedStaticMeshComponent* ismc{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    UBulletDataAsset* bullet_data;

    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<FTransform>> transform_queue{};
    FDelegateHandle phase_end_delegate_handle{};
    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    int32 current_instance_count{0};

// Debug properties
#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    int32 num_flying{0};
#endif
};
