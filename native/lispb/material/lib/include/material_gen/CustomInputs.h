#pragma once

#include <lispb/indices.h>
#include <material_gen/Indices.h>
#include <material_gen/ValueType.h>

#include <vector>

namespace material_synth {
struct CustomInputs {
    auto append(lispb::StringId name, ValueType type) -> CustomInputId;

    std::vector<lispb::StringId> name;
    std::vector<ValueType> type;
};
}
