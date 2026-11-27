#include <ioj/input/dispatch/player_menu.h>

#include <ioj/input/logical/flight_controls.h>
#include <ioj/settings/setting_control_kind.h>

#include <algorithm>
#include <cassert>

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

auto PlayerMenu::binding_rows() const -> std::vector<ControlBindingRow> {
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

void PlayerMenu::open(MenuScreen next, bool content) {
    screen = next;
    page = next == MenuScreen::Pause ? MainMenuItem::Audio : MainMenuItem::SelectLevel;
    focus =
        next == MenuScreen::Completion ? MenuTarget{MenuControl::ReturnToLevels} : MenuTarget{page};
    if (content && next == MenuScreen::Main) {
        focus = MenuRow{MenuRow::Kind::Level, selected_level};
    }
    category = levels::authoring::LevelCatalogCategory::Playable;
    pending_page.reset();
    prompt = {};
    create_profile = false;
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
        set_prompt(OptionsPrompt::Dirty);
        return false;
    }
    return true;
}

auto PlayerMenu::finish_main_leave(bool accepted) -> std::optional<MenuUpdate> {
    auto const destination{std::exchange(pending_page, {})};
    if (!destination || !accepted) {
        return {};
    }
    if (*destination != MainMenuItem::Quit) {
        select_page(*destination);
    }
    return MenuUpdate{*destination, MenuOperation::Activate};
}

auto PlayerMenu::display_seconds_remaining() const -> int {
    return static_cast<int>(std::max<std::int64_t>(
        0,
        std::chrono::ceil<std::chrono::seconds>(display_deadline - std::chrono::steady_clock::now())
            .count()));
}

void PlayerMenu::set_prompt(OptionsPrompt next) {
    if (next == prompt) {
        return;
    }
    if (next == OptionsPrompt::None) {
        focus = return_focus;
    } else {
        if (prompt == OptionsPrompt::None) {
            return_focus = focus;
        }
        focus = next == OptionsPrompt::Dirty ? MenuControl::Save : MenuControl::KeepDisplay;
    }
    prompt = next;
}

void PlayerMenu::set_create_profile(bool open) {
    create_profile = open;
    focus = open ? MenuControl::ProfileName : MenuControl::CreateProfile;
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
    if (prompt == OptionsPrompt::Dirty) {
        return {Save, Discard, Stay};
    }
    if (prompt == OptionsPrompt::Display) {
        return {KeepDisplay, RevertDisplay};
    }
    if (create_profile) {
        return {ProfileName, CancelCreate, ConfirmCreate};
    }
    std::vector<MenuTarget> targets;
    if (page == MainMenuItem::SelectLevel) {
        using Category = levels::authoring::LevelCatalogCategory;
        targets = {Category::Playable, Category::BattleViewer, Category::Benchmark};
        bool can_launch{};
        for (auto const& level : levels) {
            if (level.visible_in(category)) {
                targets.emplace_back(MenuRow{MenuRow::Kind::Level, level.id});
                if (level.id == selected_level) {
                    can_launch = level.unlocked;
                }
            }
        }
        if (category != Category::Playable) {
            targets.insert(targets.end(), {BattleSpeed, BattleDuration, SimulationOnly});
        }
        if (can_launch && launch_options_valid) {
            targets.emplace_back(Launch);
        }
    } else if (page == MainMenuItem::DataArchive) {
        for (auto const& id : profiles) {
            targets.emplace_back(MenuRow{MenuRow::Kind::Profile, id});
        }
        for (auto const& id : outcomes) {
            targets.emplace_back(MenuRow{MenuRow::Kind::Outcome, id});
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
    if (page == MainMenuItem::SelectLevel && prompt == OptionsPrompt::None) {
        return selected_level.empty() ? MenuTarget{category}
                                      : MenuTarget{MenuRow{MenuRow::Kind::Level, selected_level}};
    }
    if (page == MainMenuItem::DataArchive && !create_profile && !selected_profile.empty()) {
        return MenuRow{MenuRow::Kind::Profile, selected_profile};
    }
    auto targets{content_targets(settings)};
    return targets.empty() ? MenuTarget{page} : std::move(targets.front());
}

auto PlayerMenu::activate(MenuTarget target,
                          PlayerSettingsView const& settings,
                          std::span<ScreenResolution const> resolutions)
    -> std::optional<MenuUpdate> {
    if (saving || screen == MenuScreen::Closed) {
        return {};
    }
    if (auto choice{std::get_if<BindingChoice>(&target)}) {
        if (!draft.binding_action_available(*choice)) {
            return {};
        }
        draft.choose_binding_action(*choice);
        return MenuUpdate{draft.binding_edit().mapping, MenuOperation::BindingPrompt};
    }
    if (std::holds_alternative<MainMenuItem>(target)) {
        if (screen != MenuScreen::Main || prompt != OptionsPrompt::None || create_profile ||
            draft.binding_edit().prompt != BindingPrompt::Closed) {
            return {};
        }
        auto const item{std::get<MainMenuItem>(target)};
        if (item != page && !leave_ready()) {
            pending_page = item;
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
    if (target == MenuTarget{MenuControl::CreateProfile} ||
        target == MenuTarget{MenuControl::CancelCreate}) {
        set_create_profile(target == MenuTarget{MenuControl::CreateProfile});
    }
    if (auto setting{std::get_if<GameSetting>(&target)}) {
        auto const& row{game_setting_descriptor(*setting)};
        if (row.control_kind == SettingControlKind::Toggle) {
            settings.set_setting(
                draft, *setting, !std::get<bool>(draft.value(*setting)), resolutions);
            return MenuUpdate{target, MenuOperation::RefreshSettings};
        }
        if (row.control_kind == SettingControlKind::Choice) {
            return navigate(MenuCommand::Right, false, settings, resolutions);
        }
        return {};
    }
    if (auto mapping{std::get_if<FlightMapping>(&target)}) {
        draft.manage_binding(*mapping);
    }
    if (auto selected{std::get_if<GameSettingDevice>(&target)}) {
        device = *selected;
    }
    if (auto selected{std::get_if<levels::authoring::LevelCatalogCategory>(&target)}) {
        category = *selected;
        auto first{std::ranges::find_if(
            levels, [this](auto const& level) { return level.visible_in(category); })};
        selected_level = first == levels.end() ? std::string{} : first->id;
    }
    if (auto row{std::get_if<MenuRow>(&target)}) {
        switch (row->kind) {
            case MenuRow::Kind::Level:
                selected_level = row->id;
                break;
            case MenuRow::Kind::Profile:
                selected_profile = row->id;
                break;
            case MenuRow::Kind::Outcome:
                selected_outcome = row->id;
                break;
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
    if (saving || screen == MenuScreen::Closed ||
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
        if (prompt == OptionsPrompt::Dirty) {
            return activate(MenuControl::Stay, settings, resolutions);
        }
        if (prompt == OptionsPrompt::Display) {
            return activate(MenuControl::RevertDisplay, settings, resolutions);
        }
        if (create_profile) {
            return activate(MenuControl::CancelCreate, settings, resolutions);
        }
        if (screen == MenuScreen::Main) {
            return select(page);
        }
        return {};
    }
    auto const backward{command == Previous || command == Up || command == Left};
    if (auto item{std::get_if<MainMenuItem>(&focus)}) {
        if (command == Right) {
            return select(primary_target(settings));
        }
        if (command == Left) {
            return {};
        }
        return select(backward ? cycle_previous(*item) : cycle_next(*item));
    }
    auto targets{content_targets(settings)};
    if (targets.empty()) {
        return {};
    }
    if (command == Left || command == Right) {
        if (auto setting{std::get_if<GameSetting>(&focus)}) {
            auto const& row{game_setting_descriptor(*setting)};
            auto value{draft.value(*setting)};
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
            settings.set_setting(draft, *setting, value, resolutions);
            return MenuUpdate{focus, MenuOperation::RefreshSettings};
        }
        using Category = levels::authoring::LevelCatalogCategory;
        if (auto tab{std::get_if<Category>(&focus)}) {
            auto index{static_cast<int>(*tab) + (backward ? -1 : 1)};
            if (index >= 0 && index <= static_cast<int>(Category::Benchmark)) {
                return activate(static_cast<Category>(index), settings, resolutions);
            }
        }
        if (auto row{std::get_if<MenuRow>(&focus)}) {
            if (!backward && row->kind == MenuRow::Kind::Profile && !outcomes.empty()) {
                return select(MenuRow{MenuRow::Kind::Outcome, selected_outcome});
            }
            if (backward && row->kind == MenuRow::Kind::Outcome) {
                return select(MenuRow{MenuRow::Kind::Profile, selected_profile});
            }
        }
        if (backward && screen == MenuScreen::Main && prompt == OptionsPrompt::None &&
            !create_profile) {
            return select(page);
        }
    }
    if (command == Down && std::holds_alternative<levels::authoring::LevelCatalogCategory>(focus)) {
        return select(primary_target(settings));
    }
    if (command == Up || command == Down) {
        if (auto const* row{std::get_if<MenuRow>(&focus)}) {
            std::erase_if(targets, [row](auto const& target) {
                auto const* candidate{std::get_if<MenuRow>(&target)};
                return !candidate || candidate->kind != row->kind;
            });
            if (row->kind == MenuRow::Kind::Level && !targets.empty() && command == Up &&
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
