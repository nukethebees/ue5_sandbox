#include <ioj/sim/entities/entity_id.h>

#include <cstddef>
#include <cstdint>

extern "C" auto generated_index(ioj::sim::EntityId value) -> std::uint32_t {
    return value.index();
}

extern "C" auto handwritten_index(std::uint32_t value) -> std::uint32_t {
    return value & 0xffffffu;
}

extern "C" void
    generated_indices(ioj::sim::EntityId const* source, std::uint32_t* output, std::size_t count) {
    for (std::size_t index{}; index < count; ++index) {
        output[index] = source[index].index();
    }
}

extern "C" void
    handwritten_indices(std::uint32_t const* source, std::uint32_t* output, std::size_t count) {
    for (std::size_t index{}; index < count; ++index) {
        output[index] = source[index] & 0xffffffu;
    }
}
