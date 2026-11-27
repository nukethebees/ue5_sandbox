#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "input/BindingKey.h"
#include <ioj/host/input.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <Framework/Application/SlateApplication.h>
#include <GenericPlatform/GenericPlatformInputDeviceMapper.h>
#include <Input/Events.h>
#include <InputCoreTypes.h>
#include <Widgets/SWindow.h>

namespace ioj::host {
namespace {
auto modifier_state(Modifiers modifiers) -> FModifierKeysState {
    return {modifiers.left_shift,
            modifiers.right_shift,
            modifiers.left_control,
            modifiers.right_control,
            modifiers.left_alt,
            modifiers.right_alt,
            modifiers.left_command,
            modifiers.right_command,
            modifiers.caps_lock};
}
auto mouse_button(int button) -> FKey {
    switch (button) {
        case 1:
            return EKeys::LeftMouseButton;
        case 2:
            return EKeys::MiddleMouseButton;
        case 3:
            return EKeys::RightMouseButton;
        case 4:
            return EKeys::ThumbMouseButton;
        case 5:
            return EKeys::ThumbMouseButton2;
        default:
            return EKeys::Invalid;
    }
}
}
void ui_key(
    std::string_view name, std::uint32_t character, bool down, bool repeat, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const key{binding_key(name)};
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
void ui_axis(std::string_view name, float value, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const device{IPlatformInputDeviceMapper::Get().GetDefaultInputDevice()};
    slate.ProcessAnalogInputEvent(FAnalogInputEvent{
        FKey{FName{*to_fstring(name)}}, modifier_state(modifiers), device, false, 0, 0, value});
}
void ui_pointer(PointerInput const& event, Modifiers modifiers) {
    auto& slate{FSlateApplication::Get()};
    auto const window{GEngine->GameViewport->GetWindow()};
    if (!window) {
        return;
    }
    FVector2D const position{window->GetPositionInScreen() + FVector2D{event.x, event.y}};
    auto const button{mouse_button(event.button)};
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
auto editing_text() -> bool {
    auto const widget{FSlateApplication::Get().GetKeyboardFocusedWidget()};
    return widget && (widget->GetType() == TEXT("SEditableText") ||
                      widget->GetType() == TEXT("SMultiLineEditableText"));
}
}
#endif
