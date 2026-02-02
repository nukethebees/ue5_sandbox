#pragma once

#include <atomic>
#include <optional>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/bullets/BulletTypeIndex.h"
#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "MassBulletVisualizationActor.generated.h"

class UInstancedStaticMeshComponent;

class UBulletDataAsset;

UCLASS()
class SANDBOX_API AMassBulletVisualizationActor
    : public AActor
    , public ml::LogMsgMixin<"AMassBulletVisualizationActor", LogSandboxMassEntity> {
    GENERATED_BODY()
  public:
    static constexpr float growth_factor{1.5};
    static constexpr std::size_t transform_queue_capacity{100000};

    AMassBulletVisualizationActor();

    void enqueue_transform(FBulletTypeIndex index, FTransform const& transform) {
        auto const i{index.get_value()};
        check(i >= 0 && i < transform_queues.Num());
        (void)transform_queues[i].enqueue(transform);
    }
    void enqueue_transform(FPrimaryAssetId id, FTransform const& transform) {
        if (auto* i{lookup.Find(id)}) {
            enqueue_transform(FBulletTypeIndex{*i}, transform);
        }
    }
    void increment_killed_count(FBulletTypeIndex index) {
        auto const i{index.get_value()};
        check(i >= 0 && i < to_be_hidden.Num());
        to_be_hidden[i].fetch_add(1, std::memory_order_relaxed);
    }
    void increment_killed_count(FPrimaryAssetId id) {
        if (auto* i{lookup.Find(id)}) {
            increment_killed_count(FBulletTypeIndex{*i});
        }
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
    int32 register_new_projectile(UBulletDataAsset& bullet_data);

    int32 get_num_ismcs() const { return ismcs.Num(); }
    void handle_preallocation();
    void add_instances(int32 mesh_index, int32 n);
    void grow_instances(int32 mesh_index, int32 min_required);
    void register_phase_end_callback();
    void on_phase_end(float delta_time);
    int32 consume_killed_count(int32 i) {
        return to_be_hidden[i].exchange(0, std::memory_order_relaxed);
    }

    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    TMap<FPrimaryAssetId, int32> lookup;

    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    TArray<UInstancedStaticMeshComponent*> ismcs;

    FDelegateHandle phase_end_delegate_handle{};

    TArray<ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<FTransform>>> transform_queues{};
    UPROPERTY(VisibleAnywhere, Category = "Bullets")
    TArray<int32> current_instance_counts{};
    TArray<std::atomic<int32>> to_be_hidden{};
};
