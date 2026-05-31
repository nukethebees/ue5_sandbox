#pragma once

#include <SandboxCore/Public/generation_index.h>

bool FGenerationIndex::is_valid() const noexcept {
    return index >= 0 && generation >= 0;
}
