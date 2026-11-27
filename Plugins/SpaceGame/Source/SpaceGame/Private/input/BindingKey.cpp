#include "input/BindingKey.h"

#include <SandboxCoreEngine/strings.h>

namespace ioj {
auto binding_key(PhysicalKey key) -> FKey {
    if (key == PhysicalKey::Unbound) {
        return EKeys::Invalid;
    }
    if (key == PhysicalKey::NumPadEnter) {
        return EKeys::Enter;
    }
    return FKey{FName{*to_fstring(to_serialized_string(key))}};
}
auto binding_label(PhysicalKey key) -> FText {
    if (key == PhysicalKey::Unbound) {
        return NSLOCTEXT("OptionsMenu", "UnboundControl", "Unbound");
    }
    return binding_key(key).GetDisplayName();
}
}
