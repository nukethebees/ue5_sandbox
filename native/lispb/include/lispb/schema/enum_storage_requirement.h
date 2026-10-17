#pragma once

#include <cstdint>
#include <optional>

namespace lispb::schema {
struct EnumStorageRequirement {
    bool signedness{};
    std::uint32_t bit_width{};
};

struct EnumDomain;
auto derive_enum_storage_requirement(EnumDomain const& domain,
                                     std::optional<std::uint32_t> declared_bit_width)
    -> std::optional<EnumStorageRequirement>;
}
