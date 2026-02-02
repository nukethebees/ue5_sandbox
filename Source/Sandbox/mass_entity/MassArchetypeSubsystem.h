#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "Sandbox/combat/bullets/BulletDataAssetIds.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/mass_entity/EntityDefinition.h"

#include "MassArchetypeSubsystem.generated.h"

class UWorld;

DECLARE_MULTICAST_DELEGATE(FOnMassArchetypeSubsystemReady);

// Create cached Mass Entity archetypes.
UCLASS()
class SANDBOX_API UMassArchetypeSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UMassArchetypeSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    auto get_bullet_archetype() const -> FMassArchetypeHandle;
    auto get_definition(FPrimaryAssetId id) -> std::optional<FEntityDefinition>;

    FOnMassArchetypeSubsystemReady on_mass_archetype_subsystem_ready;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& in_world) override;
    virtual void Deinitialize() override;
  private:
    void build_archetypes(FMassEntityManager& entity_manager);
    void build_definitions(FMassEntityManager& entity_manager);
    int32 add_definition(FEntityDefinition definition, FPrimaryAssetId id);

    TArray<FEntityDefinition> definitions{};
    TMap<FPrimaryAssetId, int32> definition_indexes{};
    FMassArchetypeHandle bullet_archetype;
};
