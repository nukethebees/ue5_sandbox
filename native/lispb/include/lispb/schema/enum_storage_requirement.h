#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {
struct EnumStorageRequirement {
    bool signedness{};
    std::uint8_t bit_width{};
};

struct EnumDomains;
auto derive_enum_storage_requirement(EnumDomains const& domains,
                                     EnumDomainId domain,
                                     std::optional<std::uint8_t> declared_bit_width)
    -> std::optional<EnumStorageRequirement>;
}
