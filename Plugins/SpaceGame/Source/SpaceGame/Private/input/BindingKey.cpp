#include "input/BindingKey.h"

#include <ioj/input/dispatch/events.h>

#include <SandboxCoreEngine/strings.h>

#include <Containers/Map.h>

namespace ioj {
namespace {
auto keyboard_labels() -> TMap<PhysicalKey, FString>& {
    static TMap<PhysicalKey, FString> labels;
    return labels;
}
}
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
void host::present_keyboard_layout(std::span<KeyboardLabel const> labels) {
    auto& names{keyboard_labels()};
    names.Reset();
    for (auto const& label : labels) {
        names.Add(label.key, to_fstring(label.name));
    }
}
#endif
auto binding_key(PhysicalKey key) -> FKey {
    if (key == PhysicalKey::Unbound) {
        return EKeys::Invalid;
    }
    if (key == PhysicalKey::NumPadEnter) {
        return EKeys::Enter;
    }
    if (auto const* name{keyboard_labels().Find(key)}; name && name->Len() == 1) {
        auto const logical{FInputKeyManager::Get().GetKeyFromCodes(0, (*name)[0])};
        if (logical.IsValid()) {
            return logical;
        }
    }
    return FKey{FName{*to_fstring(to_serialized_string(key))}};
}
auto binding_label(PhysicalKey key) -> FText {
    if (key == PhysicalKey::Unbound) {
        return NSLOCTEXT("OptionsMenu", "UnboundControl", "Unbound");
    }
    if (auto const* name{keyboard_labels().Find(key)}; name && !name->IsEmpty()) {
        return FText::FromString(*name);
    }
    return binding_key(key).GetDisplayName();
}
}
