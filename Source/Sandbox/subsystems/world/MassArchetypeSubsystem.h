#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassArchetypeSubsystem.generated.h"

// Create cached Mass Entity archetypes.
UCLASS()
class SANDBOX_API UMassArchetypeSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UMassArchetypeSubsystem"> {
    GENERATED_BODY()
  public:
    auto get_bullet_archetype() const -> FMassArchetypeHandle;
  protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
  private:
    void build_archetypes(FMassEntityManager& entity_manager);

    FMassArchetypeHandle bullet_archetype;
};
