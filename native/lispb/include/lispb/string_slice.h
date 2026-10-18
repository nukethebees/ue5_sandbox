#pragma once

#include <cstdint>
namespace lispb {

struct StringSlice {
    std::uint32_t offset{};
    std::uint32_t length{};
};

}
