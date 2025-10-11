#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "Sandbox/data_assets/BulletDataAssetIds.h"
#include "Sandbox/mass_entity/EntityDefinition.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "MassArchetypeSubsystem.generated.h"

// Create cached Mass Entity archetypes.
UCLASS()
class SANDBOX_API UMassArchetypeSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UMassArchetypeSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    auto get_bullet_archetype() const -> FMassArchetypeHandle;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
  private:
    void build_archetypes(FMassEntityManager& entity_manager);
    void build_definitions(FMassEntityManager & entity_manager);
    int32 add_definition(FEntityDefinition definition, FPrimaryAssetId id);

    TArray<FEntityDefinition> definitions{};
    TMap<FPrimaryAssetId, int32> definition_indexes{};
    FMassArchetypeHandle bullet_archetype;
};
