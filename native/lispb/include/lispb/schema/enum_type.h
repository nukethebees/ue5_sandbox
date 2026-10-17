#pragma once

#include <lispb/schema/enumerator.h>
#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
struct EnumType {
    std::optional<ResolvedTypeRef> underlying_type;
    std::optional<std::uint32_t> bit_width;
    std::optional<bool> signedness;
    std::vector<Enumerator> enumerators;
    std::optional<std::string> count;
    bool enum_array{};
};
}
