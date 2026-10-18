#pragma once
#include <lispb/indices.h>

#include <cstdint>
#include <string>
#include <vector>

namespace lispb::codegen::sexpr::output_detail {
struct Texts {
    std::vector<StringId> source;
    std::vector<std::uint32_t> begin;
    std::vector<std::uint32_t> size;
    std::string bytes;
};
}
