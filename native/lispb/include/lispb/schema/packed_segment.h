#pragma once

#include <lispb/schema/packed_field.h>
#include <lispb/schema/packed_reserved_bits.h>

#include <variant>

namespace lispb::schema {
using PackedSegment = std::variant<PackedField, PackedReservedBits>;
}
