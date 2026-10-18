#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class SoaStorage : std::uint8_t { vector, single_allocation, both };

constexpr auto soa_storage_name(SoaStorage const storage) -> char const* {
    switch (storage) {
        case SoaStorage::vector: {
            return "vector";
        }
        case SoaStorage::single_allocation: {
            return "single-allocation";
        }
        case SoaStorage::both: {
            return "both";
        }
    }
    return "";
}

} // namespace lispb::codegen
