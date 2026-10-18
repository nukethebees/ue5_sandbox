#pragma once

#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen::detail {

enum class SoaReceiver : std::uint8_t { owner, const_view, mutable_view };

}
