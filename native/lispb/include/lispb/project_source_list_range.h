#pragma once
#include <codegen/sexpr/handle_range.h>

#include <cstddef>

namespace lispb {

struct ProjectSourceListRange {
    std::size_t begin_offset{};
    std::size_t end_offset{};
    std::size_t closing_offset{};
    std::size_t item_indentation{};
    codegen::sexpr::HandleRange original_sources;
    codegen::sexpr::HandleRange original_items;
};

}
