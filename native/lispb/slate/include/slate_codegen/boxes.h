#pragma once

#include <codegen/sexpr/source_span.h>
#include <slate_codegen/box_orientation.h>
#include <slate_codegen/range.h>

#include <vector>
namespace slate_codegen::detail {

struct Boxes {
    std::vector<BoxOrientation> orientations;
    std::vector<Range> slots;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
