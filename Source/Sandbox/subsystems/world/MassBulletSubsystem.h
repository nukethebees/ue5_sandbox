#pragma once

#include <atomic>
#include <cstddef>

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassBulletSubsystem.generated.h"

class AMassBulletVisualizationActor;
class UBulletDataAsset;

struct FBulletSpawnRequest {
    FTransform transform;
    float speed;

    FBulletSpawnRequest(FTransform const& t, float s)
        : transform(t)
        , speed(s) {}
};

struct FBulletSpawnRequestView {
    std::span<FTransform> transforms;
    std::span<float> speeds;

    FBulletSpawnRequestView(std::span<FTransform> t, std::span<float> s)
        : transforms(t)
        , speeds(s) {}
};

UCLASS()
class SANDBOX_API UMassBulletSubsystem
    : public UMassSubsystemBase
    , public ml::LogMsgMixin<"UMassBulletSubsystem"> {
    GENERATED_BODY()
  public:
    static constexpr std::size_t n_queue_elements{30000};

    void add_bullet(FTransform const& spawn_transform, float bullet_speed) {
        (void)spawn_queue.enqueue(spawn_transform, bullet_speed);
    }
    void destroy_bullet(FMassEntityHandle handle) { (void)destroy_queue.enqueue(handle); }
  protected:
    virtual void OnWorldBeginPlay(UWorld& in_world) override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
  private:
    [[nodiscard]] bool initialise_asset_data();
    void configure_active_bullet(FMassEntityManager& entity_manager,
                                 FMassEntityHandle entity,
                                 FTransform const& transform,
                                 float bullet_speed);
    void on_end_frame();
    void consume_lifecycle_requests(FBulletSpawnRequestView const& spawn_requests,
                                    std::span<FMassEntityHandle> destroy_requests);

    TArray<FMassEntityHandle> free_list;
    FMassArchetypeHandle bullet_archetype;
    AMassBulletVisualizationActor* visualization_actor{nullptr};
    FMassArchetypeSharedFragmentValues shared_values{};
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    ml::MonitoredLockFreeMPSCQueue<
        ml::LockFreeMPSCQueueSoA<FBulletSpawnRequestView, FTransform, float>>
        spawn_queue;
    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<FMassEntityHandle>> destroy_queue;
};
