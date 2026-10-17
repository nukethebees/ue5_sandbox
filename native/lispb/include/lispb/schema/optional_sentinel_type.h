#pragma once

#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>
#include <string>

namespace lispb::schema {
struct OptionalSentinelType {
    ResolvedTypeRef source;
    std::string sentinel_name;
    codegen::PackedIntegerValue sentinel_value;
    std::uint32_t bit_width{};
};
}
