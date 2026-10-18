#pragma once

#include <cstdint>
namespace lispb::codegen::sexpr {

struct TokenRange {
    std::uint32_t offset{};
    std::uint32_t count{};
};

}
