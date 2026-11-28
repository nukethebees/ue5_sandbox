#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "input/HostInput.h"

#include <SpaceGame/UnrealEndpoint.h>

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
auto slate_key(PhysicalKey key) -> FKey {
    if (key == PhysicalKey::Unbound) {
        return EKeys::Invalid;
    }
    if (key == PhysicalKey::NumPadEnter) {
        return EKeys::Enter;
    }
    if (is_keyboard_key(key)) {
        auto const label{to_fstring(unreal_endpoint().keyboard().label(key))};
        if (label.Len() == 1) {
            auto const logical{FInputKeyManager::Get().GetKeyFromCodes(0, label[0])};
            if (logical.IsValid()) {
                return logical;
            }
        }
    }
    return FKey{FName{*to_fstring(to_serialized_string(key))}};
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
void present_input(MenuKey const& entry) {
    auto& slate{FSlateApplication::Get()};
    auto const key{slate_key(entry.key)};
    if (!key.IsValid()) {
        return;
    }

    auto const device{IPlatformInputDeviceMapper::Get().GetDefaultInputDevice()};
    FKeyEvent const event{key, modifier_state(entry.modifiers), device, entry.repeat, 0, 0};
    if (entry.down) {
        slate.ProcessKeyDownEvent(event);
    } else {
        slate.ProcessKeyUpEvent(event);
    }
}
void present_input(MenuPointer const& event) {
    auto& slate{FSlateApplication::Get()};
    auto const window{GEngine->GameViewport->GetWindow()};
    if (!window) {
        return;
    }
    FVector2D const position{window->GetPositionInScreen() + FVector2D{event.x, event.y}};
    auto const button{slate_key(event.button)};
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
                                modifier_state(event.modifiers)};
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
void present_input(MenuText const& entry) {
    auto& slate{FSlateApplication::Get()};
    auto const characters{to_fstring(entry.text)};
    for (int32 i{}; i < characters.Len(); ++i) {
        slate.ProcessKeyCharEvent(
            FCharacterEvent{characters[i], modifier_state(entry.modifiers), 0, false});
    }
}
}
#endif
