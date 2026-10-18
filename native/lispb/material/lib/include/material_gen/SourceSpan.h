#pragma once

#include <lispb/indices.h>

#include <cstdint>
namespace material_synth {

struct SourceSpan {
    std::uint32_t line{1};
    std::uint32_t column{1};
    lispb::StringId path;
    lispb::StringId expansion;
};

}
