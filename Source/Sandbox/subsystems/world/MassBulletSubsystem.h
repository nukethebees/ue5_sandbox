#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassBulletSubsystem.generated.h"

class AMassBulletVisualizationActor;
class UBulletDataAsset;

UCLASS()
class SANDBOX_API UMassBulletSubsystem
    : public UMassSubsystemBase
    , public ml::LogMsgMixin<"UMassBulletSubsystem"> {
    GENERATED_BODY()
  public:
    void add_bullet(FTransform const& transform, float bullet_speed);
    void return_bullet(FMassEntityHandle handle);
  protected:
    virtual void OnWorldBeginPlay(UWorld& in_world) override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
  private:
    [[nodiscard]] bool initialise_asset_data();
    void configure_active_bullet(FMassEntityManager& entity_manager,
                                 FMassEntityHandle entity,
                                 FTransform const& transform,
                                 float bullet_speed,
                                 int32 ismc_index);

    TArray<FMassEntityHandle> free_list;
    FMassArchetypeHandle bullet_archetype;
    AMassBulletVisualizationActor* visualization_actor{nullptr};
    FMassArchetypeSharedFragmentValues shared_values{};
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};
};
