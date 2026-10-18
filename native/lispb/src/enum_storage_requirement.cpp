#include <lispb/schema/enum_storage_requirement.h>

#include <lispb/schema/enum_domains.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {
auto derive_enum_storage_requirement(EnumDomains const& domains,
                                     EnumDomainId const domain,
                                     std::optional<std::uint8_t> const declared_bit_width)
    -> std::optional<EnumStorageRequirement> {
    auto const row{domains.index(domain)};
    auto const effective_width{declared_bit_width.has_value() ? declared_bit_width
                                                              : domains.minimum_required_bits[row]};
    if (domains.signedness[row] == EnumSignedness::unspecified || !effective_width.has_value() ||
        *effective_width == 0 || *effective_width > 64) {
        return std::nullopt;
    }

    auto const storage_width{*effective_width <= 8    ? 8U
                             : *effective_width <= 16 ? 16U
                             : *effective_width <= 32 ? 32U
                                                      : 64U};
    return EnumStorageRequirement{.signedness = domains.signedness[row] == EnumSignedness::signed_,
                                  .bit_width = static_cast<std::uint8_t>(storage_width)};
}
}
