#pragma once
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/levels/level_source.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/session/launch_state.h>

#include "Subsystems/GameInstanceSubsystem.h"

#include <memory_resource>

#include "GameSubsystem.generated.h"

namespace ioj::memory {
class BumpResource;
}

namespace ioj {
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

UCLASS()
class SPACEGAME_API UGameSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    /* **************************************** */
    // Lifecycle and services
    /* **************************************** */
    UGameSubsystem(FObjectInitializer const& object_initializer = FObjectInitializer::Get());
    UGameSubsystem(FVTableHelper& helper);
    ~UGameSubsystem() override;

    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    static auto get(UGameInstance const* game_instance) -> UGameSubsystem*;
    auto get_memory_resource() noexcept -> std::pmr::memory_resource&;
    void reset_level_memory();
    auto get_platform_capabilities() const -> FGameCapabilities const&;
    auto get_save_game_browser() -> profiles::SaveGameBrowser&;
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
    void set_pending_level(levels::LevelSource source,
                           FString source_path,
                           levels::LaunchOptions options = {});
    auto take_pending_level() -> std::optional<session::PendingLevel>;

    [[nodiscard]] auto return_to_level_select(FName preferred_level_id = NAME_None) -> bool;
    [[nodiscard]] auto return_to_telemetry(std::expected<std::string, std::string> result) -> bool;
    auto take_menu_request() -> std::optional<session::MenuRequest>;
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
    TUniquePtr<memory::BumpResource> local_memory_;
    memory::BumpResource* memory_resource_{};
    profiles::SaveGameBrowser save_game_browser_;
    session::LaunchState launch_state_;
};
}
