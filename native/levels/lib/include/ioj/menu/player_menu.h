#pragma once

#include <ioj/input/input_actions.h>
#include <ioj/input/logical/flight_controls.h>
#include <ioj/input/physical/physical_key.h>
#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_catalog_category.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/player_settings_view.h>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj {
auto settings_category(MainMenuItem page) -> std::optional<GameSettingCategory>;

enum class MenuControl {
    ReturnToLevels,
    Launch,
    BattleSpeed,
    BattleDuration,
    SimulationOnly,
    CreateProfile,
    RefreshProfiles,
    ActivateProfile,
    ResetProfile,
    ProfileName,
    ConfirmCreate,
    CancelCreate,
    ResetCategory,
    Apply,
    Save,
    Discard,
    Stay,
    KeepDisplay,
    RevertDisplay
};
enum class MenuScreen { Main, Pause, Completion, Closed };
enum class SettingsTransaction { Editing, ConfirmLeave, ConfirmDisplay, Saving };
enum class ProfilePage { Browse, Create };
enum class MenuOperation { Focus, Activate, RefreshSettings, BindingPrompt };

struct MenuTarget {
    enum class Kind {
        Page,
        PauseItem,
        Device,
        Setting,
        Mapping,
        Binding,
        Category,
        Control,
        Level,
        Profile,
        Outcome
    };

    MenuTarget(host::PauseMenuItem item)
        : kind{Kind::PauseItem}
        , index{static_cast<unsigned>(item)} {}
    auto pause_item() const -> host::PauseMenuItem {
        return static_cast<host::PauseMenuItem>(index);
    }
    MenuTarget(MainMenuItem value)
        : kind{Kind::Page}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(GameSettingDevice value)
        : kind{Kind::Device}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(GameSetting value)
        : kind{Kind::Setting}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(FlightMapping value)
        : kind{Kind::Mapping}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(BindingChoice value)
        : kind{Kind::Binding}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(levels::authoring::LevelCatalogCategory value)
        : kind{Kind::Category}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(MenuControl value)
        : kind{Kind::Control}
        , index{static_cast<unsigned>(value)} {}
    MenuTarget(Kind row_kind, std::string row_id)
        : kind{row_kind}
        , id{std::move(row_id)} {}
    auto operator==(MenuTarget const&) const -> bool = default;
    auto is_row() const -> bool {
        return kind == Kind::Level || kind == Kind::Profile || kind == Kind::Outcome;
    }
    auto page() const -> MainMenuItem { return static_cast<MainMenuItem>(index); }
    auto device() const -> GameSettingDevice { return static_cast<GameSettingDevice>(index); }
    auto setting() const -> GameSetting { return static_cast<GameSetting>(index); }
    auto mapping() const -> FlightMapping { return static_cast<FlightMapping>(index); }
    auto binding() const -> BindingChoice { return static_cast<BindingChoice>(index); }
    auto category() const -> levels::authoring::LevelCatalogCategory {
        return static_cast<levels::authoring::LevelCatalogCategory>(index);
    }
    auto control() const -> MenuControl { return static_cast<MenuControl>(index); }

    Kind kind;
    unsigned index{};
    std::string id;
};

struct MenuUpdate {
    MenuTarget target;
    MenuOperation operation;
};

struct CatalogControl {
    std::string id;
    levels::authoring::LevelCatalogCategory category;
    bool unlocked;

    auto visible_in(levels::authoring::LevelCatalogCategory tab) const -> bool {
        using enum levels::authoring::LevelCatalogCategory;
        return category == (tab == Benchmark ? BattleViewer : tab);
    }
};

struct ControlBindingRow {
    FlightControlDescriptor const& control;
    PhysicalKey current_key;
    PhysicalKey default_key;
};

struct MenuPresentation {
    auto navigation_blocked() const -> bool;
    auto binding_rows() const -> std::vector<ControlBindingRow>;
    auto display_seconds_remaining() const -> int;
    auto launch_speed() const -> std::string const&;
    auto speed_value() const -> std::optional<double>;
    auto duration_value() const -> std::optional<double>;
    auto launch_options() const -> std::optional<levels::LaunchOptions>;

    GameSettingsEditState draft;
    MenuScreen screen{MenuScreen::Main};
    MainMenuItem page{MainMenuItem::SelectLevel};
    MenuTarget focus{MainMenuItem::SelectLevel};
    SettingsTransaction transaction{};
    GameSettingDevice device{GameSettingDevice::KeyboardMouse};
    levels::authoring::LevelCatalogCategory category{};
    std::string selected_level;
    std::string selected_profile;
    std::string selected_outcome;
    ProfilePage profile_page{};
    std::chrono::steady_clock::time_point display_deadline{};
    std::string viewer_speed{"1.0"};
    std::string benchmark_speed{"10.0"};
    std::string duration;
    levels::PresentationMode presentation_mode{levels::PresentationMode::Visual};
};

struct PlayerMenu : MenuPresentation {
    explicit PlayerMenu(bool show_reset = false)
        : show_reset_profile{show_reset} {}
    auto pause_item() const -> host::PauseMenuItem;
    auto select_pause_item(host::PauseMenuItem item) -> bool;
    auto navigate_pause(MenuCommand command, bool repeat) -> std::optional<host::PauseMenuItem>;
    void open(MenuScreen screen, bool focus_content = false);
    void select_page(MainMenuItem item);
    auto leave_ready() -> bool;
    auto finish_leave(bool accepted) -> std::optional<MenuUpdate>;
    void set_transaction(SettingsTransaction next);
    void select_profile_page(ProfilePage page);
    auto navigate(MenuCommand command,
                  bool repeat,
                  PlayerSettingsView const& settings,
                  std::span<ScreenResolution const> resolutions) -> std::optional<MenuUpdate>;
    auto edit_text(MenuControl control, std::string text) -> std::optional<MenuUpdate>;
    auto activate(MenuTarget target,
                  PlayerSettingsView const& settings,
                  std::span<ScreenResolution const> resolutions) -> std::optional<MenuUpdate>;
    auto content_targets(PlayerSettingsView const& settings) const -> std::vector<MenuTarget>;
    auto primary_target(PlayerSettingsView const& settings) const -> MenuTarget;
    auto select(MenuTarget target) -> MenuUpdate;

    auto input_mode() const -> MenuInputMode {
        auto const prompt{draft.binding_edit().prompt};
        if (prompt == BindingPrompt::Capture || prompt == BindingPrompt::Unsupported) {
            return MenuInputMode::BindingCapture;
        }
        if (focus.kind == MenuTarget::Kind::Control &&
            (focus.control() == MenuControl::ProfileName ||
             focus.control() == MenuControl::BattleSpeed ||
             focus.control() == MenuControl::BattleDuration)) {
            return MenuInputMode::TextEntry;
        }
        return MenuInputMode::Navigation;
    }

    MenuTarget return_focus{MainMenuItem::SelectLevel};
    std::vector<CatalogControl> levels;
    std::vector<std::string> profiles;
    std::vector<std::string> outcomes;
    std::string active_profile;
    bool const show_reset_profile;
    std::optional<MenuTarget> pending_destination;
    std::string profile_name;
};
}
