#include <Sandbox/misc/learning/RegistryEntityHandle.h>

auto FRegistryEntityHandle::is_valid() const noexcept -> bool {
    return index >= 0 && generation >= 0;
}
auto FRegistryEntityHandle::to_string() const -> FString {
    if (is_valid()) { return FString::Printf(TEXT("%d [Gen: %d]"), index, generation); }

    return FString::Printf(TEXT("%d [Gen: %d][invalid]"), index, generation);
}
