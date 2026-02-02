#pragma once

#include "Sandbox/combat/bullets/MassBulletSubsystemData.h"
#include "Sandbox/combat/bullets/BulletTypeIndex.h"
#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/environment/utilities/world.h"
#include "Sandbox/health/HealthChange.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/mass_entity/EntityDefinition.h"

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"

#include <atomic>
#include <cstddef>

#include "MassBulletSubsystem.generated.h"

class AMassBulletVisualizationActor;
class UBulletDataAsset;
class AActor;

struct FBulletSpawnRequest {
    struct Data {
        FTransform transform;
        float speed;
        FHealthChange damage;
        TWeakObjectPtr<AActor> shooter;

        Data(FTransform const& t, float s, FHealthChange damage, TWeakObjectPtr<AActor> shooter)
            : transform(t)
            , speed(s)
            , damage(damage)
            , shooter(shooter) {}
    };
    Data data;
    FPrimaryAssetId bullet_type;

    FBulletSpawnRequest(Data d, FPrimaryAssetId const& bt)
        : data(d)
        , bullet_type(bt) {}
};

struct FBulletSpawnRequestView {
    std::span<FTransform> transforms;
    std::span<float> speeds;
    std::span<FHealthChange> damages;
    std::span<TWeakObjectPtr<AActor>> shooters;
    std::span<FPrimaryAssetId> bullet_types;

    FBulletSpawnRequestView(std::span<FTransform> t,
                            std::span<float> s,
                            std::span<FHealthChange> d,
                            std::span<TWeakObjectPtr<AActor>> shtrs,
                            std::span<FPrimaryAssetId> bt)
        : transforms(t)
        , speeds(s)
        , damages(d)
        , shooters(shtrs)
        , bullet_types(bt) {}
};

UCLASS()
class SANDBOX_API UMassBulletSubsystem
    : public UMassSubsystemBase
    , public ml::LogMsgMixin<"UMassBulletSubsystem", LogSandboxMassEntity> {
    GENERATED_BODY()
  public:
    static constexpr std::size_t n_queue_elements{30000};

    void add_bullet(FBulletSpawnRequest r) {
        (void)spawn_queue.enqueue(
            r.data.transform, r.data.speed, r.data.damage, r.data.shooter, r.bullet_type);
    }
    void add_bullet(FBulletSpawnRequest::Data data, FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        check(i >= 0);
        check(i < indexed_bullet_types.Num());
        add_bullet({data, indexed_bullet_types[i]});
    }
    void destroy_bullet(FMassEntityHandle handle, FPrimaryAssetId const& bullet_type) {
        (void)destroy_queue.enqueue(handle, bullet_type);
    }
    void destroy_bullet(FMassEntityHandle handle, FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        check(i >= 0 && i < indexed_bullet_types.Num());
        destroy_bullet(handle, indexed_bullet_types[i]);
    }

    AMassBulletSubsystemData* get_data_actor() {
        if (auto* world{GetWorld()}) {
            return ml::get_first_actor<AMassBulletSubsystemData>(*world);
        }
        return nullptr;
    }

    auto get_bullet_type_index(FPrimaryAssetId const& bullet_type) const -> FBulletTypeIndex {
        if (auto const* index{bullet_type_indices.Find(bullet_type)}) {
            return *index;
        }
        return FBulletTypeIndex{-1};
    }
  protected:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
  private:
    void on_archetypes_ready();

    [[nodiscard]] bool initialise_asset_data();
    void configure_active_bullet(FMassEntityManager& entity_manager,
                                 FMassEntityHandle entity,
                                 FTransform const& transform,
                                 float bullet_speed,
                                 FHealthChange damage);
    struct FBulletDestroyRequest {
        FMassEntityHandle entity;
        FPrimaryAssetId bullet_type;
    };
    struct FBulletDestroyRequestView {
        std::span<FMassEntityHandle> entities;
        std::span<FPrimaryAssetId> bullet_types;

        FBulletDestroyRequestView(std::span<FMassEntityHandle> e, std::span<FPrimaryAssetId> bt)
            : entities(e)
            , bullet_types(bt) {}
    };

    void on_end_frame();
    void consume_lifecycle_requests(FBulletSpawnRequestView const& spawn_requests,
                                    FBulletDestroyRequestView const& destroy_requests);

    TArray<FMassEntityHandle> free_list;
    TMap<FPrimaryAssetId, FEntityDefinition> bullet_definitions;
    TMap<FPrimaryAssetId, FBulletTypeIndex> bullet_type_indices;
    TArray<FPrimaryAssetId> indexed_bullet_types;
    TArray<FMassEntityHandle> new_entities{};

    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueueSoA<FBulletSpawnRequestView,
                                                            FTransform,
                                                            float,
                                                            FHealthChange,
                                                            TWeakObjectPtr<AActor>,
                                                            FPrimaryAssetId>>
        spawn_queue;
    ml::MonitoredLockFreeMPSCQueue<
        ml::LockFreeMPSCQueueSoA<FBulletDestroyRequestView, FMassEntityHandle, FPrimaryAssetId>>
        destroy_queue;
};
