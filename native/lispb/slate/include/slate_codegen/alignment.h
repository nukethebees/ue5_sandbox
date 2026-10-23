#pragma once

#include <lispb/indices.h>
#include <slate_codegen/alignment_kind.h>

namespace slate_codegen::detail {

struct Alignment {
    lispb::StringId parameter{};
    AlignmentKind kind{};
};

}
