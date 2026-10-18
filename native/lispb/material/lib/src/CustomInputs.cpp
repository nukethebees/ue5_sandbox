#include <material_gen/CustomInputs.h>

#include <cstdint>

namespace material_synth {
auto CustomInputs::append(lispb::StringId name, ValueType type) -> CustomInputId {
    auto const id{CustomInputId{static_cast<std::uint32_t>(this->name.size())}};
    this->name.push_back(name);
    this->type.push_back(type);
    return id;
}
}
