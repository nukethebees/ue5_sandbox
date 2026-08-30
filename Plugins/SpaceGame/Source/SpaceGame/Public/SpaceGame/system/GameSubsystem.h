#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

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
    auto get_save_game_browser() -> FSaveGameBrowser&;
  private:
    FGameCapabilities platform_capabilities_;
    FSaveGameBrowser save_game_browser_;
};
}
