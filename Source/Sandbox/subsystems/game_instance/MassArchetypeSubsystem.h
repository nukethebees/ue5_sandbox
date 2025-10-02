#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassArchetypeSubsystem.generated.h"

UCLASS()
class SANDBOX_API UMassArchetypeSubsystem
    : public UGameInstanceSubsystem
    , public ml::LogMsgMixin<"UMassArchetypeSubsystem"> {
    GENERATED_BODY()
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    auto get_bullet_archetype() const -> FMassArchetypeHandle;
  private:
    void build_archetypes(FMassEntityManager& entity_manager);

    FMassArchetypeHandle bullet_archetype;
};
