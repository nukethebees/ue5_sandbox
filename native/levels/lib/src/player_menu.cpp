#include <ioj/menu/player_menu.h>

#include <ioj/input/logical/flight_controls.h>
#include <ioj/settings/setting_control_kind.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <string_view>
#include <system_error>

namespace ioj {
auto settings_category(MainMenuItem page) -> std::optional<GameSettingCategory> {
    switch (page) {
        case MainMenuItem::Video:
            return GameSettingCategory::Video;
        case MainMenuItem::Audio:
            return GameSettingCategory::Audio;
        case MainMenuItem::Gameplay:
            return GameSettingCategory::Gameplay;
        case MainMenuItem::Accessibility:
            return GameSettingCategory::Accessibility;
        case MainMenuItem::Controls:
            return GameSettingCategory::Controls;
        default:
            return {};
    }
}

namespace {
auto parse_number(std::string_view text) -> std::optional<double> {
    if (text.starts_with('+')) {
        text.remove_prefix(1);
    }
    double value{};
    auto const end{text.data() + text.size()};
    auto const result{std::from_chars(text.data(), end, value, std::chars_format::fixed)};
    if (result.ec != std::errc{} || result.ptr != end) {
        return {};
    }
    return value;
}
}
auto MenuPresentation::launch_speed() const -> std::string const& {
    return category == levels::authoring::LevelCatalogCategory::Benchmark ? benchmark_speed
                                                                          : viewer_speed;
}
auto MenuPresentation::speed_value() const -> std::optional<double> {
    auto value{parse_number(launch_speed())};
    return value && levels::launch::TimeScale::valid(*value) ? value : std::nullopt;
}
auto MenuPresentation::duration_value() const -> std::optional<double> {
    auto value{parse_number(duration)};
    return value && PositiveFinite<double>::valid(*value) ? value : std::nullopt;
}
auto MenuPresentation::launch_options() const -> std::optional<levels::LaunchOptions> {
    levels::LaunchOptions options;
    using Category = levels::authoring::LevelCatalogCategory;
    if (category == Category::Playable) {
        return options;
    }
    auto const speed{speed_value()};
    auto const seconds{duration_value()};
    if (!speed || (!duration.empty() && !seconds)) {
        return {};
    }
    options.requested_time_scale = levels::launch::TimeScale::make(*speed);
    if (seconds) {
        options.simulated_duration_seconds = PositiveFinite<double>::make(*seconds);
    }
    options.presentation_mode = presentation_mode;
    options.control_context = category == Category::Benchmark ? levels::ControlContext::Benchmark
                                                              : levels::ControlContext::Observer;
    return options;
}

auto MenuPresentation::binding_rows() const -> std::vector<ControlBindingRow> {
    std::vector<ControlBindingRow> rows;
    for (auto const& control : flight_controls()) {
        if (is_gamepad_mapping(control.mapping) == (device == GameSettingDevice::Controller)) {
            rows.push_back({control,
                            draft.pending().controls.bindings[control.mapping],
                            draft.defaults().controls.bindings[control.mapping]});
        }
    }
    return rows;
}

auto MenuPresentation::navigation_blocked() const -> bool {
    return transaction != SettingsTransaction::Editing ||
           draft.binding_edit().prompt != BindingPrompt::Closed ||
           profile_page == ProfilePage::Create;
}
auto PlayerMenu::pause_item() const -> host::PauseMenuItem {
    return page == MainMenuItem::Controls ? host::PauseMenuItem::Controls
                                          : host::PauseMenuItem::Audio;
}
auto PlayerMenu::select_pause_item(host::PauseMenuItem item) -> bool {
    if (screen != MenuScreen::Pause || navigation_blocked() || item == pause_item() ||
        focus == MenuTarget{item}) {
        return false;
    }
    focus = item;
    return true;
}
auto PlayerMenu::navigate_pause(MenuCommand command, bool repeat)
    -> std::optional<host::PauseMenuItem> {
    if (navigation_blocked()) {
        return {};
    }
    using enum MenuCommand;
    switch (command) {
        case Back:
            if (!repeat) {
                return host::PauseMenuItem::Resume;
            }
            break;
        case Accept:
            if (!repeat) {
                return focus.pause_item();
            }
            break;
        case Right:
            return pause_item();
        case Left:
            break;
        default:
            do {
                focus = command == Up || command == Previous ? cycle_previous(focus.pause_item())
                                                             : cycle_next(focus.pause_item());
            } while (focus.pause_item() == pause_item());
            break;
    }
    return {};
}
void PlayerMenu::open(MenuScreen next, bool content) {
    screen = next;
    page = next == MenuScreen::Pause ? MainMenuItem::Audio : MainMenuItem::SelectLevel;
    focus =
        next == MenuScreen::Completion ? MenuTarget{MenuControl::ReturnToLevels} : MenuTarget{page};
    if (next == MenuScreen::Pause) {
        focus = host::PauseMenuItem::Resume;
    }
    if (content && next == MenuScreen::Main) {
        focus = MenuTarget{MenuTarget::Kind::Level, selected_level};
    }
    category = levels::authoring::LevelCatalogCategory::Playable;
    pending_destination.reset();
    transaction = {};
    profile_page = ProfilePage::Browse;
    draft.cancel();
}

void PlayerMenu::select_page(MainMenuItem item) {
    page = item;
    if (screen == MenuScreen::Main) {
        focus = item;
    }
}

auto PlayerMenu::leave_ready() -> bool {
    if (draft.is_dirty()) {
        set_transaction(SettingsTransaction::ConfirmLeave);
        return false;
    }
    return true;
}

auto PlayerMenu::finish_leave(bool accepted) -> std::optional<MenuUpdate> {
    auto const destination{std::exchange(pending_destination, {})};
    if (!destination || !accepted) {
        return {};
    }
    if (destination->kind == MenuTarget::Kind::Page && destination->page() != MainMenuItem::Quit) {
        select_page(destination->page());
    }
    return MenuUpdate{*destination, MenuOperation::Activate};
}

auto MenuPresentation::display_seconds_remaining() const -> int {
    return static_cast<int>(std::max<std::int64_t>(
        0,
        std::chrono::ceil<std::chrono::seconds>(display_deadline - std::chrono::steady_clock::now())
            .count()));
}

void PlayerMenu::set_transaction(SettingsTransaction next) {
    if (next == transaction) {
        return;
    }
    if (next == SettingsTransaction::Editing) {
        focus = return_focus;
    } else {
        if (transaction == SettingsTransaction::Editing) {
            return_focus = focus;
        }
        if (next != SettingsTransaction::Saving) {
            focus = next == SettingsTransaction::ConfirmLeave ? MenuControl::Save
                                                              : MenuControl::KeepDisplay;
        }
    }
    transaction = next;
}

void PlayerMenu::select_profile_page(ProfilePage next) {
    profile_page = next;
    profile_name.clear();
    focus = next == ProfilePage::Create ? MenuControl::ProfileName : MenuControl::CreateProfile;
}

auto PlayerMenu::select(MenuTarget target) -> MenuUpdate {
    focus = std::move(target);
    return {focus, MenuOperation::Focus};
}

auto PlayerMenu::content_targets(PlayerSettingsView const& settings) const
    -> std::vector<MenuTarget> {
    using enum MenuControl;
    if (screen == MenuScreen::Completion) {
        return {ReturnToLevels};
    }
    if (transaction == SettingsTransaction::ConfirmLeave) {
        return {Save, Discard, Stay};
    }
    if (transaction == SettingsTransaction::ConfirmDisplay) {
        return {KeepDisplay, RevertDisplay};
    }
    if (profile_page == ProfilePage::Create) {
        return {ProfileName, CancelCreate, ConfirmCreate};
    }
    std::vector<MenuTarget> targets;
    if (page == MainMenuItem::SelectLevel) {
        using Category = levels::authoring::LevelCatalogCategory;
        targets = {Category::Playable, Category::BattleViewer, Category::Benchmark};
        bool can_launch{};
        for (auto const& level : levels) {
            if (level.visible_in(category)) {
                targets.emplace_back(MenuTarget{MenuTarget::Kind::Level, level.id});
                if (level.id == selected_level) {
                    can_launch = level.unlocked;
                }
            }
        }
        if (category != Category::Playable) {
            targets.insert(targets.end(), {BattleSpeed, BattleDuration, SimulationOnly});
        }
        if (can_launch && launch_options()) {
            targets.emplace_back(Launch);
        }
    } else if (page == MainMenuItem::DataArchive) {
        for (auto const& id : profiles) {
            targets.emplace_back(MenuTarget{MenuTarget::Kind::Profile, id});
        }
        for (auto const& id : outcomes) {
            targets.emplace_back(MenuTarget{MenuTarget::Kind::Outcome, id});
        }
        if (!selected_profile.empty() && selected_profile != active_profile) {
            targets.emplace_back(ActivateProfile);
        }
        targets.emplace_back(CreateProfile);
        if (show_reset_profile) {
            targets.emplace_back(ResetProfile);
        }
        targets.emplace_back(RefreshProfiles);
    } else if (auto group{settings_category(page)}) {
        if (page == MainMenuItem::Controls) {
            targets = {GameSettingDevice::KeyboardMouse, GameSettingDevice::Controller};
        }
        for (auto const& row : game_setting_descriptors()) {
            if (row.category == *group &&
                (row.device == GameSettingDevice::Shared || row.device == device) &&
                row.control_kind != SettingControlKind::Custom &&
                settings.is_available(draft.pending(), row.id)) {
                targets.emplace_back(row.id);
            }
        }
        if (page == MainMenuItem::Controls) {
            for (auto const& row : flight_controls()) {
                if (is_gamepad_mapping(row.mapping) == (device == GameSettingDevice::Controller)) {
                    targets.emplace_back(row.mapping);
                }
            }
        }
        if (!draft.is_at_defaults(*group)) {
            targets.emplace_back(ResetCategory);
        }
        if (draft.is_dirty()) {
            targets.emplace_back(Apply);
        }
    }
    return targets;
}

auto PlayerMenu::primary_target(PlayerSettingsView const& settings) const -> MenuTarget {
    if (page == MainMenuItem::SelectLevel && transaction == SettingsTransaction::Editing) {
        return selected_level.empty() ? MenuTarget{category}
                                      : MenuTarget{MenuTarget::Kind::Level, selected_level};
    }
    if (page == MainMenuItem::DataArchive && profile_page == ProfilePage::Browse &&
        !selected_profile.empty()) {
        return MenuTarget{MenuTarget::Kind::Profile, selected_profile};
    }
    auto targets{content_targets(settings)};
    return targets.empty() ? MenuTarget{page} : std::move(targets.front());
}

auto PlayerMenu::edit_text(MenuControl control, std::string text) -> std::optional<MenuUpdate> {
    if (screen == MenuScreen::Closed || transaction == SettingsTransaction::Saving) {
        return {};
    }
    if (control == MenuControl::ProfileName && profile_page == ProfilePage::Create) {
        profile_name = std::move(text);
    } else if (page == MainMenuItem::SelectLevel &&
               category != levels::authoring::LevelCatalogCategory::Playable) {
        auto const last{text.find_last_not_of(" \t\r\n")};
        if (last == std::string::npos) {
            text.clear();
        } else {
            text.erase(last + 1);
            text.erase(0, text.find_first_not_of(" \t\r\n"));
        }
        if (control == MenuControl::BattleDuration) {
            duration = std::move(text);
        } else if (control == MenuControl::BattleSpeed) {
            (category == levels::authoring::LevelCatalogCategory::Benchmark ? benchmark_speed
                                                                            : viewer_speed) =
                std::move(text);
        } else {
            return {};
        }
        return MenuUpdate{control, MenuOperation::RefreshSettings};
    }
    return {};
}

auto PlayerMenu::activate(MenuTarget target,
                          PlayerSettingsView const& settings,
                          std::span<ScreenResolution const> resolutions)
    -> std::optional<MenuUpdate> {
    if (transaction == SettingsTransaction::Saving || screen == MenuScreen::Closed) {
        return {};
    }
    if (target.kind == MenuTarget::Kind::Binding) {
        auto const choice{target.binding()};
        if (!draft.binding_action_available(choice)) {
            return {};
        }
        draft.choose_binding_action(choice);
        return MenuUpdate{draft.binding_edit().mapping, MenuOperation::BindingPrompt};
    }
    if (target.kind == MenuTarget::Kind::Page) {
        if (screen != MenuScreen::Main || navigation_blocked()) {
            return {};
        }
        auto const item{target.page()};
        if (item != page && !leave_ready()) {
            pending_destination = item;
            return MenuUpdate{focus, MenuOperation::Focus};
        }
        if (item != MainMenuItem::Quit) {
            select_page(item);
        }
    } else if (draft.binding_edit().prompt != BindingPrompt::Closed ||
               !std::ranges::contains(content_targets(settings), target)) {
        return {};
    }
    focus = target;
    if (target.kind == MenuTarget::Kind::Control) {
        using enum MenuControl;
        switch (target.control()) {
            case ResetCategory: {
                auto const category{settings_category(page)};
                if (category) {
                    draft.reset_category(*category);
                    if (*category == GameSettingCategory::Video) {
                        if (settings.launch_mode != LaunchDisplayMode::Preferences) {
                            draft.set_setting(GameSetting::WindowMode,
                                              draft.applied().video.window_mode);
                        } else if (draft.pending().video.window_mode ==
                                   GameWindowMode::Fullscreen) {
                            draft.set_setting(GameSetting::WindowMode, GameWindowMode::Borderless);
                        }
                        if (!settings.is_available(draft.pending(), GameSetting::Resolution)) {
                            draft.set_setting(GameSetting::Resolution,
                                              draft.applied().video.resolution);
                        }
                    }
                }
                return MenuUpdate{target, MenuOperation::RefreshSettings};
            }
            default:
                break;
        }
    }
    if (target == MenuTarget{MenuControl::SimulationOnly}) {
        presentation_mode = presentation_mode == levels::PresentationMode::Visual
                              ? levels::PresentationMode::SimulationOnly
                              : levels::PresentationMode::Visual;
    }
    if (target == MenuTarget{MenuControl::CreateProfile} ||
        target == MenuTarget{MenuControl::CancelCreate}) {
        select_profile_page(target == MenuTarget{MenuControl::CreateProfile} ? ProfilePage::Create
                                                                             : ProfilePage::Browse);
    }
    if (target.kind == MenuTarget::Kind::Setting) {
        auto const setting{target.setting()};
        auto const& row{game_setting_descriptor(setting)};
        if (row.control_kind == SettingControlKind::Toggle) {
            settings.set_setting(
                draft, setting, !std::get<bool>(draft.value(setting)), resolutions);
            return MenuUpdate{target, MenuOperation::RefreshSettings};
        }
        if (row.control_kind == SettingControlKind::Choice) {
            return navigate(MenuCommand::Right, false, settings, resolutions);
        }
        return {};
    }
    if (target.kind == MenuTarget::Kind::Mapping) {
        auto const mapping{target.mapping()};
        draft.manage_binding(mapping);
    }
    if (target.kind == MenuTarget::Kind::Device) {
        auto const selected{target.device()};
        device = selected;
    }
    if (target.kind == MenuTarget::Kind::Category) {
        auto const selected{target.category()};
        category = selected;
        auto first{std::ranges::find_if(
            levels, [this](auto const& level) { return level.visible_in(category); })};
        selected_level = first == levels.end() ? std::string{} : first->id;
    }
    if (target.is_row()) {
        switch (target.kind) {
            case MenuTarget::Kind::Level:
                selected_level = target.id;
                break;
            case MenuTarget::Kind::Profile:
                selected_profile = target.id;
                break;
            case MenuTarget::Kind::Outcome:
                selected_outcome = target.id;
                break;
            default:
                std::unreachable();
        }
    }
    return MenuUpdate{std::move(target), MenuOperation::Activate};
}

auto PlayerMenu::navigate(MenuCommand command,
                          bool repeat,
                          PlayerSettingsView const& settings,
                          std::span<ScreenResolution const> resolutions)
    -> std::optional<MenuUpdate> {
    using enum MenuCommand;
    if (transaction == SettingsTransaction::Saving || screen == MenuScreen::Closed ||
        (repeat && (command == Accept || command == Back))) {
        return {};
    }
    if (command == Accept) {
        if (focus == MenuTarget{MenuControl::ProfileName}) {
            return activate(MenuControl::ConfirmCreate, settings, resolutions);
        }
        if (focus == MenuTarget{MenuControl::BattleSpeed} ||
            focus == MenuTarget{MenuControl::BattleDuration}) {
            return navigate(Next, false, settings, resolutions);
        }
        return activate(focus, settings, resolutions);
    }
    if (command == Back) {
        if (transaction == SettingsTransaction::ConfirmLeave) {
            return activate(MenuControl::Stay, settings, resolutions);
        }
        if (transaction == SettingsTransaction::ConfirmDisplay) {
            return activate(MenuControl::RevertDisplay, settings, resolutions);
        }
        if (profile_page == ProfilePage::Create) {
            return activate(MenuControl::CancelCreate, settings, resolutions);
        }
        if (screen == MenuScreen::Main) {
            return select(page);
        }
        return {};
    }
    auto const backward{command == Previous || command == Up || command == Left};
    if (focus.kind == MenuTarget::Kind::Page) {
        auto const item{focus.page()};
        if (command == Right) {
            return select(primary_target(settings));
        }
        if (command == Left) {
            return {};
        }
        return select(backward ? cycle_previous(item) : cycle_next(item));
    }
    auto targets{content_targets(settings)};
    if (targets.empty()) {
        return {};
    }
    if (command == Left || command == Right) {
        if (focus.kind == MenuTarget::Kind::Setting) {
            auto const setting{focus.setting()};
            auto const& row{game_setting_descriptor(setting)};
            auto value{draft.value(setting)};
            if (row.control_kind == SettingControlKind::FloatRange) {
                value = static_cast<float>(
                    std::clamp(std::get<float>(value) + (backward ? -row.step : row.step),
                               row.minimum,
                               row.maximum));
            } else if (row.control_kind == SettingControlKind::Choice) {
                auto const choices{
                    settings.choices(draft.pending(), row.options_provider, resolutions)};
                auto it{std::ranges::find(choices, value, &GameSettingChoice::value)};
                if (choices.empty()) {
                    return {};
                }
                auto const index{it == choices.end() ? 0 : it - choices.begin()};
                value =
                    choices[(index + choices.size() + (backward ? -1 : 1)) % choices.size()].value;
            } else if (row.control_kind == SettingControlKind::Toggle) {
                value = !backward;
            }
            settings.set_setting(draft, setting, value, resolutions);
            return MenuUpdate{focus, MenuOperation::RefreshSettings};
        }
        using Category = levels::authoring::LevelCatalogCategory;
        if (focus.kind == MenuTarget::Kind::Category) {
            auto const tab{focus.category()};
            auto index{static_cast<int>(tab) + (backward ? -1 : 1)};
            if (index >= 0 && index <= static_cast<int>(Category::Benchmark)) {
                return activate(static_cast<Category>(index), settings, resolutions);
            }
        }
        if (focus.is_row()) {
            auto const* row{&focus};
            if (!backward && row->kind == MenuTarget::Kind::Profile && !outcomes.empty()) {
                return select(MenuTarget{MenuTarget::Kind::Outcome, selected_outcome});
            }
            if (backward && row->kind == MenuTarget::Kind::Outcome) {
                return select(MenuTarget{MenuTarget::Kind::Profile, selected_profile});
            }
        }
        if (backward && screen == MenuScreen::Main && transaction == SettingsTransaction::Editing &&
            profile_page == ProfilePage::Browse) {
            return select(page);
        }
    }
    if (command == Down && focus.kind == MenuTarget::Kind::Category) {
        return select(primary_target(settings));
    }
    if (command == Up || command == Down) {
        if (focus.is_row()) {
            auto const* row{&focus};
            std::erase_if(targets, [row](auto const& target) { return target.kind != row->kind; });
            if (row->kind == MenuTarget::Kind::Level && !targets.empty() && command == Up &&
                focus == targets.front()) {
                return select(category);
            }
        }
    }
    auto it{std::ranges::find(targets, focus)};
    if (it == targets.end()) {
        return select(primary_target(settings));
    }
    auto index{static_cast<std::size_t>(it - targets.begin())};
    index = (index + (backward ? targets.size() - 1 : 1)) % targets.size();
    return select(std::move(targets[index]));
}
}
