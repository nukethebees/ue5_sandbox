#pragma once

#include <ioj/input/physical/physical_key.h>

#include <InputCoreTypes.h>

namespace ioj {
auto binding_key(PhysicalKey key) -> FKey;
auto binding_label(PhysicalKey key) -> FText;
}
