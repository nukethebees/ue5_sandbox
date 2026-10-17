#pragma once

#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>

namespace lispb::schema {
struct OptionalPresenceBitType {
    ResolvedTypeRef source;
    std::uint32_t payload_bits{};
    std::uint32_t encoded_bits{};
};
}
