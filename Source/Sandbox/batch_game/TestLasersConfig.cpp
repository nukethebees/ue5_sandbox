#include "TestLasersConfig.h"

auto UTestLasersConfig::is_ready() const noexcept -> bool {
    return (mesh != nullptr) && (material != nullptr);
}
