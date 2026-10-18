#pragma once

#include <cstdint>

namespace lispb::schema {
enum class SourceCommentPlacement : std::uint8_t {
    leading,
    trailing,
    before_close,
    file_header,
    file_trailer,
};
}
