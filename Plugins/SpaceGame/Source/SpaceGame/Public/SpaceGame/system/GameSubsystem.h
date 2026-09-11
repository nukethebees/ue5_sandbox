#pragma once

#include "SpaceGame/levels/LevelDefinition.h"
#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/ships/player/PlayerControlContext.h"
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSubsystem.generated.h"

namespace ml::ioj {
class USpaceGameUiTheme;

struct SPACEGAME_API FCpuSimdCapabilities {
    bool available{};
    bool sse{};
    bool sse2{};
    bool sse3{};
    bool ssse3{};
    bool sse4_1{};
    bool sse4_2{};
    bool sse4a{};
    bool avx{};
    bool avx2{};
    bool avx_vnni{};
    bool avx512_f{};
    bool avx512_cd{};
    bool avx512_bw{};
    bool avx512_dq{};
    bool avx512_vl{};
    bool amx_tile{};
    bool amx_bf16{};
    bool amx_int8{};
    bool amx_fp16{};
};

#if PLATFORM_WINDOWS
enum class ELargePageAccessStatus : uint8 {
    Unsupported,
    Enabled,
    PrivilegeUnavailable,
    QueryFailed,
};

struct SPACEGAME_API FWindowsGameCapabilities {
    uint64 large_page_minimum_bytes{};
    ELargePageAccessStatus large_page_access_status{ELargePageAccessStatus::QueryFailed};
};
#endif

struct SPACEGAME_API FGameCapabilities {
    FString platform_name{};
    FString host_architecture{};
    FString operating_system_version{};
    FString operating_system_subversion{};
    FString cpu_vendor{};
    FString cpu_brand{};
    FString primary_gpu_brand{};
    int32 physical_core_count{};
    int32 logical_core_count{};
    uint64 total_physical_memory_bytes{};
    FCpuSimdCapabilities cpu_simd{};

#if PLATFORM_WINDOWS
    FWindowsGameCapabilities windows{};
#endif
};

enum class ELevelLaunchMode : uint8 {
    Running,
    Paused,
};

enum class ELevelPresentationMode : uint8 {
    Visual,
    SimulationOnly,
};

enum class ELevelResultsNavigation : uint8 {
    None,
    Telemetry,
};

namespace level_launch {
inline constexpr double default_time_scale{1.0};
inline constexpr double maximum_time_scale{100.0};

SPACEGAME_API auto is_valid_time_scale(double value) noexcept -> bool;
}

struct SPACEGAME_API FLevelLaunchOptions {
    ELevelLaunchMode launch_mode{ELevelLaunchMode::Running};
    double requested_time_scale{level_launch::default_time_scale};
    ELevelPresentationMode presentation_mode{ELevelPresentationMode::Visual};
    TOptional<double> simulated_duration_seconds{};
    bool stop_when_battle_resolved{};
    bool detailed_timing{true};
    ELevelResultsNavigation results_navigation{ELevelResultsNavigation::None};
    EPlayerControlContext control_context{EPlayerControlContext::Player};
};

struct SPACEGAME_API FPendingLevelDefinition {
    FLevelDefinition definition{};
    FString source_path{};
    FString source_sha256{};
    FLevelLaunchOptions options{};
};

enum class EMainMenuDestination : uint8 {
    LevelSelect,
    Telemetry,
};

struct SPACEGAME_API FLevelSelectRequest {
    FName preferred_level_id{NAME_None};
    EMainMenuDestination destination{EMainMenuDestination::LevelSelect};
    FString selected_telemetry_run_id{};
    FString telemetry_error{};
};

UCLASS()
class SPACEGAME_API UGameSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    /* **************************************** */
    // Lifecycle and services
    /* **************************************** */
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    auto get_platform_capabilities() const -> FGameCapabilities const&;
    auto get_save_game_browser() -> FSaveGameBrowser&;
    auto get_audio() -> FGameAudioFacade;
    void start_menu_ambience();
    void stop_menu_ambience();
    void start_player_ship_ambience();
    void stop_player_ship_ambience();

    /* **************************************** */
    // UI configuration
    /* **************************************** */
    auto get_ui_style() const -> FGameUiStyle const&;
    auto set_ui_theme(USpaceGameUiTheme* theme) -> bool;

    /* **************************************** */
    // Level launch and navigation
    /* **************************************** */
    void set_pending_level(FLevelDefinition definition,
                           FString source_path,
                           FString source_sha256,
                           FLevelLaunchOptions options = {});
    auto take_pending_level() -> TOptional<FPendingLevelDefinition>;

    [[nodiscard]] auto return_to_level_select(FName preferred_level_id = NAME_None) -> bool;
    [[nodiscard]] auto return_to_telemetry(FString run_id, FString error = {}) -> bool;
    auto take_level_select_request() -> TOptional<FLevelSelectRequest>;
    static auto get_main_menu_level_name() -> FName;

    /* **************************************** */
    // Launch errors
    /* **************************************** */
    void set_level_launch_error(FString error);
    auto has_level_launch_error() const noexcept -> bool;
    auto take_level_launch_error() -> FString;
  private:
    void update_audio_settings();

    /* **************************************** */
    // State
    /* **************************************** */
    FGameAudio audio_;
    FGameCapabilities platform_capabilities_;
    FSaveGameBrowser save_game_browser_;
    TOptional<FPendingLevelDefinition> pending_level_{NullOpt};
    TOptional<FLevelSelectRequest> level_select_request_{NullOpt};
    FString level_launch_error_{};
    bool level_transition_in_progress_{false};
};
}
