#pragma once

#include <codegen/source_position_types.h>

namespace lispb::schema {
struct SourceRange {
    codegen::SourceFileIndex source_file_index{};
    codegen::SourceOffset begin_offset{};
    codegen::SourceOffset end_offset{};
    codegen::LineNum line{1};
    codegen::ColumnNum column{1};

    auto operator==(SourceRange const&) const -> bool = default;
};
}
