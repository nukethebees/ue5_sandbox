#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassArchetypeSubsystem.generated.h"

// Create cached Mass Entity archetypes.
UCLASS()
class SANDBOX_API UMassArchetypeSubsystem
    : public UGameInstanceSubsystem
    , public ml::LogMsgMixin<"UMassArchetypeSubsystem"> {
    GENERATED_BODY()
  public:
    auto get_bullet_archetype() const -> FMassArchetypeHandle;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
  private:
    void on_post_world_initialisation(UWorld* world,
                                      UWorld::InitializationValues const initialisation_values);
    void build_archetypes(FMassEntityManager& entity_manager);

    FMassArchetypeHandle bullet_archetype;
};
