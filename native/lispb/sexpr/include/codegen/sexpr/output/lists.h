#pragma once
#include <codegen/sexpr/output/range.h>

#include <vector>

namespace lispb::codegen::sexpr::output_detail {
struct Lists {
    std::vector<Range> header;
    std::vector<Range> rows;
};
}
