#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSubsystem.generated.h"

namespace ml::ioj {
struct SPACEGAME_API FGameCapabilities {
    bool supports_large_pages{false};
};

UCLASS()
class SPACEGAME_API UGameSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;

    auto get_platform_capabilities() const -> FGameCapabilities const&;
  private:
    FGameCapabilities platform_capabilities_;
};
}
