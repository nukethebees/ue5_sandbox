#pragma once

#include <atomic>
#include <cstddef>

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"

#include "Sandbox/actors/MassBulletSubsystemData.h"
#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/generated/strong_typedefs/BulletTypeIndex.h"
#include "Sandbox/mass_entity/EntityDefinition.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"
#include "Sandbox/utilities/world.h"

#include "MassBulletSubsystem.generated.h"

class AMassBulletVisualizationActor;
class UBulletDataAsset;

struct FBulletSpawnRequest {
    FTransform transform;
    float speed;
    FPrimaryAssetId bullet_type;

    FBulletSpawnRequest(FTransform const& t, float s, FPrimaryAssetId const& bt)
        : transform(t)
        , speed(s)
        , bullet_type(bt) {}
};

struct FBulletSpawnRequestView {
    std::span<FTransform> transforms;
    std::span<float> speeds;
    std::span<FPrimaryAssetId> bullet_types;

    FBulletSpawnRequestView(std::span<FTransform> t,
                            std::span<float> s,
                            std::span<FPrimaryAssetId> bt)
        : transforms(t)
        , speeds(s)
        , bullet_types(bt) {}
};

UCLASS()
class SANDBOX_API UMassBulletSubsystem
    : public UMassSubsystemBase
    , public ml::LogMsgMixin<"UMassBulletSubsystem", LogSandboxMassEntity> {
    GENERATED_BODY()
  public:
    static constexpr std::size_t n_queue_elements{30000};

    void add_bullet(FTransform const& spawn_transform,
                    float bullet_speed,
                    FPrimaryAssetId const& bullet_type) {
        (void)spawn_queue.enqueue(spawn_transform, bullet_speed, bullet_type);
    }
    void add_bullet(FTransform const& spawn_transform,
                    float bullet_speed,
                    FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        check(i >= 0 && i < indexed_bullet_types.Num());
        add_bullet(spawn_transform, bullet_speed, indexed_bullet_types[i]);
    }
    bool add_bullet_checked(FTransform const& spawn_transform,
                            float bullet_speed,
                            FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        if (i < 0 || i >= indexed_bullet_types.Num()) {
            log_error(TEXT("Invalid bullet type index: %d (valid range: 0-%d)"),
                      i,
                      indexed_bullet_types.Num() - 1);
            return false;
        }
        add_bullet(spawn_transform, bullet_speed, indexed_bullet_types[i]);
        return true;
    }
    void destroy_bullet(FMassEntityHandle handle, FPrimaryAssetId const& bullet_type) {
        (void)destroy_queue.enqueue(handle, bullet_type);
    }
    void destroy_bullet(FMassEntityHandle handle, FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        check(i >= 0 && i < indexed_bullet_types.Num());
        destroy_bullet(handle, indexed_bullet_types[i]);
    }
    bool destroy_bullet_checked(FMassEntityHandle handle, FBulletTypeIndex bullet_type_index) {
        auto const i{bullet_type_index.get_value()};
        if (i < 0 || i >= indexed_bullet_types.Num()) {
            log_error(TEXT("Invalid bullet type index: %d (valid range: 0-%d)"),
                      i,
                      indexed_bullet_types.Num() - 1);
            return false;
        }
        destroy_bullet(handle, indexed_bullet_types[i]);
        return true;
    }

    AMassBulletSubsystemData* get_data_actor() {
        if (auto* world{GetWorld()}) {
            return ml::get_first_actor<AMassBulletSubsystemData>(*world);
        }
        return nullptr;
    }

    FBulletTypeIndex get_bullet_type_index(FPrimaryAssetId const& bullet_type) const {
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
                                 float bullet_speed);
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
    AMassBulletVisualizationActor* visualization_actor{nullptr};
    TArray<FMassEntityHandle> new_entities{};

    ml::MonitoredLockFreeMPSCQueue<
        ml::LockFreeMPSCQueueSoA<FBulletSpawnRequestView, FTransform, float, FPrimaryAssetId>>
        spawn_queue;
    ml::MonitoredLockFreeMPSCQueue<
        ml::LockFreeMPSCQueueSoA<FBulletDestroyRequestView, FMassEntityHandle, FPrimaryAssetId>>
        destroy_queue;
};
