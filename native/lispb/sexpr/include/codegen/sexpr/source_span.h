#pragma once

#include <codegen/sexpr/indices.h>

#include <cstdint>
namespace lispb::codegen::sexpr {

struct SourceSpan {
    SourceFileId file;
    std::uint32_t offset{};
    std::uint32_t end_offset{};
    ExpansionId expansion;
};

}
