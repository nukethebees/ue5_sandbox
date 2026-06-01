#include <SandboxCore/generation_index.h>

auto FGenerationIndex::is_valid() const noexcept -> bool {
    return index >= 0 && generation >= 0;
}
auto FGenerationIndex::to_string() const -> FString {
    if (is_valid()) {
        return FString::Printf(TEXT("%d [Gen: %d]"), index, generation);
    }

    return FString::Printf(TEXT("%d [Gen: %d][invalid]"), index, generation);
}
