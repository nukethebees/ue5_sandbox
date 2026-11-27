#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "input/BindingCapture.h"
#include "input/BindingKey.h"
#include <ioj/host/input.h>
#include <ioj/input/flight_controls.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GenericPlatform/GenericPlatformInputDeviceMapper.h>
#include <Input/Events.h>
#include <InputCoreTypes.h>
#include <Kismet/GameplayStatics.h>
#include <UserSettings/EnhancedInputUserSettings.h>
#include <Widgets/Input/SEditableText.h>
#include <Widgets/SWindow.h>
#include <Widgets/Text/SMultiLineEditableText.h>

#include <type_traits>
#include <variant>

namespace ioj::host {
namespace {
auto controller() -> ASpaceGamePlayerController* {
    auto* const world{GEngine->GameViewport->GetWorld()};
    return world ? Cast<ASpaceGamePlayerController>(world->GetFirstPlayerController()) : nullptr;
}
auto modifier_state(Modifiers modifiers) -> FModifierKeysState {
    return {modifiers.left_shift(),
            modifiers.right_shift(),
            modifiers.left_control(),
            modifiers.right_control(),
            modifiers.left_alt(),
            modifiers.right_alt(),
            modifiers.left_command(),
            modifiers.right_command(),
            modifiers.caps_lock()};
}
}
auto menu_status() -> MenuStatus {
    auto* const player{controller()};
    auto const widget{FSlateApplication::Get().GetKeyboardFocusedWidget()};
    auto const editing{
        widget &&
        (widget->GetType() == SEditableText::StaticWidgetClass().GetWidgetType() ||
         widget->GetType() == SMultiLineEditableText::StaticWidgetClass().GetWidgetType())};
    auto const* main{player ? player->main_menu() : nullptr};
    return {editing,
            widget && widget->GetMetaData<FBindingCapture>().IsValid(),
            main ? main->focused_item() : std::nullopt};
}
void read_legacy_key_bindings(void* destination, LegacyBindingSink sink) {
    static constexpr TCHAR slot[]{TEXT("SpaceGameInputSettings")};
    if (!UGameplayStatics::DoesSaveGameExist(slot, 0)) {
        return;
    }
    auto* const settings{
        Cast<UEnhancedInputUserSettings>(UGameplayStatics::LoadGameFromSlot(slot, 0))};
    auto* const profile{settings ? settings->GetActiveKeyProfile() : nullptr};
    if (!profile) {
        return;
    }

    for (auto const& control : input::flight_controls()) {
        auto const* row{
            profile->FindKeyMappingRow(FName{*to_fstring(to_serialized_string(control.mapping))})};
        if (!row) {
            continue;
        }
        for (auto const& mapping : row->Mappings) {
            if (mapping.GetSlot() == EPlayerMappableKeySlot::First) {
                auto const& key{mapping.GetCurrentKey()};
                sink(destination,
                     control.mapping,
                     key.IsValid() ? input::parse_key(to_string(key.GetFName().ToString()))
                                   : input::KeyCode{});
            }
        }
    }
}
auto present_pause(PauseCommand command) -> bool {
    auto* player{controller()};
    check(player);
    return player->present_pause(command);
}
void select_benchmark_button(bool end_selected) {
    auto* player{controller()};
    check(player);
    if (auto* hud{player->get_benchmark_hud()}) {
        hud->select_button(end_selected);
    }
}
namespace {
void present_viewer_motion(ViewerMotion const& motion, double delta_seconds) {
    auto* const player{controller()};
    check(player);
    player->present_viewer_motion(motion, delta_seconds);
}
void present_steering(std::optional<Vector2d> steering) {
    auto* const player{controller()};
    check(player);
    TOptional<FVector2D> position;
    if (steering) {
        position.Emplace(steering->x(), steering->y());
    }
    player->present_steering(position);
}
void ui_key(
    input::KeyCode code, std::uint32_t character, bool down, bool repeat, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const key{binding_key(code)};
    if (!key.IsValid()) {
        return;
    }

    auto const device{IPlatformInputDeviceMapper::Get().GetDefaultInputDevice()};
    FKeyEvent const event{key, modifier_state(modifiers), device, repeat, character, 0};
    if (down) {
        slate.ProcessKeyDownEvent(event);
    } else {
        slate.ProcessKeyUpEvent(event);
    }
}
void ui_axis(input::KeyCode code, float value, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const device{IPlatformInputDeviceMapper::Get().GetDefaultInputDevice()};
    slate.ProcessAnalogInputEvent(FAnalogInputEvent{
        binding_key(code), modifier_state(modifiers), device, false, 0, 0, value});
}
void ui_pointer(PointerInput const& event, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const window{GEngine->GameViewport->GetWindow()};
    if (!window) {
        return;
    }
    FVector2D const position{window->GetPositionInScreen() + FVector2D{event.x, event.y}};
    auto const button{binding_key(event.button)};
    auto pressed{slate.GetPressedMouseButtons()};
    if (event.action == PointerAction::Down || event.action == PointerAction::DoubleClick) {
        pressed.Add(button);
    } else if (event.action == PointerAction::Up) {
        pressed.Remove(button);
    }
    FPointerEvent const pointer{FSlateApplication::CursorPointerIndex,
                                position,
                                slate.GetLastCursorPos(),
                                pressed,
                                button,
                                event.wheel,
                                modifier_state(modifiers)};
    switch (event.action) {
        case PointerAction::Down:
            slate.ProcessMouseButtonDownEvent(window->GetNativeWindow(), pointer);
            break;
        case PointerAction::Up:
            slate.ProcessMouseButtonUpEvent(pointer);
            break;
        case PointerAction::DoubleClick:
            slate.ProcessMouseButtonDoubleClickEvent(window->GetNativeWindow(), pointer);
            break;
        case PointerAction::Wheel:
            slate.ProcessMouseWheelOrGestureEvent(pointer, nullptr);
            break;
        case PointerAction::Move:
            slate.ProcessMouseMoveEvent(pointer);
            break;
    }
}
void ui_text(std::string_view text, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const characters{to_fstring(text)};
    for (int32 i{}; i < characters.Len(); ++i) {
        slate.ProcessKeyCharEvent(
            FCharacterEvent{characters[i], modifier_state(modifiers), 0, false});
    }
}
}
void focus_main_item(input::MainMenuItem item) {
    if (auto* player{controller()}) {
        if (auto* main{player->main_menu()}) {
            main->focus_item(item);
        }
    }
}
void activate_main_item(input::MainMenuItem item) {
    if (auto* player{controller()}) {
        if (auto* main{player->main_menu()}) {
            main->activate_item(item);
        }
    }
}
void present_menu_button(MenuButton button) {
    PhysicalKey key{};
    switch (button.command) {
        case input::MenuCommand::Up:
            key = PhysicalKey::Up;
            break;
        case input::MenuCommand::Down:
            key = PhysicalKey::Down;
            break;
        case input::MenuCommand::Left:
            key = PhysicalKey::Left;
            break;
        case input::MenuCommand::Right:
            key = PhysicalKey::Right;
            break;
        case input::MenuCommand::Next:
            button.modifiers.set_left_shift(false);
            button.modifiers.set_right_shift(false);
            key = PhysicalKey::Tab;
            break;
        case input::MenuCommand::Previous:
            if (!button.modifiers.left_shift() && !button.modifiers.right_shift()) {
                button.modifiers.set_left_shift(true);
            }
            key = PhysicalKey::Tab;
            break;
        case input::MenuCommand::Accept:
            key = PhysicalKey::Enter;
            break;
        case input::MenuCommand::Back:
            key = PhysicalKey::Escape;
            break;
    }
    ui_key(key, 0, button.down, button.repeat, button.modifiers);
}
void present_input(PresentationEvent const& event) {
    std::visit(
        [](auto const& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, MenuKey>) {
                ui_key(item.key, item.character, item.down, item.repeat, item.modifiers);
            } else if constexpr (std::is_same_v<T, MenuAxis>) {
                ui_axis(item.key, item.value, item.modifiers);
            } else if constexpr (std::is_same_v<T, MenuText>) {
                ui_text(item.text, item.modifiers);
            } else if constexpr (std::is_same_v<T, MenuPointer>) {
                ui_pointer(item.pointer, item.modifiers);
            } else if constexpr (std::is_same_v<T, ViewerStep>) {
                present_viewer_motion(item.motion, item.seconds);
            } else {
                present_steering(item.direction);
            }
        },
        event);
}

}
#endif
