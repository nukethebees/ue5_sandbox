#include "UnifiedPacked.h"

#include <cstddef>
#include <cstdint>

using codegen_compile_fixture::unified::EntityId;
using codegen_compile_fixture::unified::Instruction;
using codegen_compile_fixture::unified::NestedId;

extern "C" auto packed_nested_index(NestedId value) -> std::uint32_t {
    return value.target_index();
}

extern "C" auto handwritten_nested_index(std::uint64_t value) -> std::uint32_t {
    return static_cast<std::uint32_t>((value >> 8) & 0xffffff);
}

extern "C" auto packed_set_nested_index(NestedId value, std::uint32_t index) -> std::uint64_t {
    value.set_target_index(index);
    return value.raw_value();
}

extern "C" auto handwritten_set_nested_index(std::uint64_t value, std::uint32_t index)
    -> std::uint64_t {
    return (value & ~(0xffffffULL << 8)) | ((std::uint64_t{index} & 0xffffff) << 8);
}

extern "C" auto packed_kill_target_index(Instruction value) -> std::uint32_t {
    return value.payload_kill_entity_target_index();
}

extern "C" auto handwritten_kill_target_index(std::uint64_t value) -> std::uint32_t {
    return static_cast<std::uint32_t>((value >> 6) & 0xffffff);
}

extern "C" auto packed_make_kill(EntityId target) -> std::uint64_t {
    return Instruction::make_payload_kill_entity(target).raw_value();
}

extern "C" auto handwritten_make_kill(std::uint32_t target) -> std::uint64_t {
    return std::uint64_t{target} << 6;
}

extern "C" auto packed_sum_nested_indices(NestedId const* values, std::size_t count)
    -> std::uint64_t {
    std::uint64_t sum{};
    for (std::size_t i{}; i < count; ++i) {
        sum += values[i].target_index();
    }
    return sum;
}

extern "C" auto handwritten_sum_nested_indices(std::uint64_t const* values, std::size_t count)
    -> std::uint64_t {
    std::uint64_t sum{};
    for (std::size_t i{}; i < count; ++i) {
        sum += (values[i] >> 8) & 0xffffff;
    }
    return sum;
}
