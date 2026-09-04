#pragma once

#include "SpaceGame/levels/LevelDefinition.h"
#include "SpaceGame/persistence/SaveGameBrowser.h"

#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSubsystem.generated.h"

namespace ml::ioj {
struct SPACEGAME_API FGameCapabilities {
    bool supports_large_pages{false};
};

enum class ELevelLaunchMode : uint8 {
    Running,
    Paused,
};

struct SPACEGAME_API FPendingLevelDefinition {
    FLevelDefinition definition{};
    FString source_path{};
    ELevelLaunchMode launch_mode{ELevelLaunchMode::Running};
};

UCLASS()
class SPACEGAME_API UGameSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;

    auto get_platform_capabilities() const -> FGameCapabilities const&;
    auto get_save_game_browser() -> FSaveGameBrowser&;

    void set_pending_level(FLevelDefinition definition,
                           FString source_path,
                           ELevelLaunchMode launch_mode = ELevelLaunchMode::Running);
    auto take_pending_level() -> TOptional<FPendingLevelDefinition>;

    void set_level_launch_error(FString error);
    auto has_level_launch_error() const noexcept -> bool;
    auto take_level_launch_error() -> FString;
  private:
    FGameCapabilities platform_capabilities_;
    FSaveGameBrowser save_game_browser_;
    TOptional<FPendingLevelDefinition> pending_level_{NullOpt};
    FString level_launch_error_{};
};
}
