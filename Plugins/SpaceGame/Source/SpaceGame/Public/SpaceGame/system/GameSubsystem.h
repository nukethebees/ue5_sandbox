#pragma once

#include "SpaceGame/levels/LevelDefinition.h"
#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSubsystem.generated.h"

namespace ml::ioj {
class USpaceGameUiTheme;

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

struct SPACEGAME_API FLevelSelectRequest {
    FName preferred_level_id{NAME_None};
};

UCLASS()
class SPACEGAME_API UGameSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;

    auto get_platform_capabilities() const -> FGameCapabilities const&;
    auto get_save_game_browser() -> FSaveGameBrowser&;

    auto get_ui_style() const -> FGameUiStyle const&;
    auto set_ui_theme(USpaceGameUiTheme* theme) -> bool;

    void set_pending_level(FLevelDefinition definition,
                           FString source_path,
                           ELevelLaunchMode launch_mode = ELevelLaunchMode::Running);
    auto take_pending_level() -> TOptional<FPendingLevelDefinition>;

    [[nodiscard]] auto return_to_level_select(FName preferred_level_id = NAME_None) -> bool;
    auto take_level_select_request() -> TOptional<FLevelSelectRequest>;
    static auto get_main_menu_level_name() -> FName;

    void set_level_launch_error(FString error);
    auto has_level_launch_error() const noexcept -> bool;
    auto take_level_launch_error() -> FString;
  private:
    void initialize_ui_style();

    FGameCapabilities platform_capabilities_;
    FSaveGameBrowser save_game_browser_;
    TOptional<FPendingLevelDefinition> pending_level_{NullOpt};
    TOptional<FLevelSelectRequest> level_select_request_{NullOpt};
    FString level_launch_error_{};
    bool level_transition_in_progress_{false};

    UPROPERTY(Transient)
    TObjectPtr<USpaceGameUiTheme> ui_theme_{nullptr};
    FGameUiStyle ui_style_{};
};
}
