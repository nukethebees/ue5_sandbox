#pragma once

#include <cstdint>

namespace lispb::schema {

enum class EnumSignedness : std::uint8_t { unsigned_, signed_, unspecified };

} // namespace lispb::schema
