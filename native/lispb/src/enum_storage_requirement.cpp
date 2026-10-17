#include <lispb/schema/enum_storage_requirement.h>

#include <lispb/schema/enum_domain.h>

namespace lispb::schema {
auto derive_enum_storage_requirement(EnumDomain const& domain,
                                     std::optional<std::uint32_t> const declared_bit_width)
    -> std::optional<EnumStorageRequirement> {
    auto const effective_width{declared_bit_width.has_value() ? declared_bit_width
                                                              : domain.minimum_required_bits};
    if (!domain.signed_domain.has_value() || !effective_width.has_value() ||
        *effective_width == 0 || *effective_width > 64) {
        return std::nullopt;
    }

    auto const storage_width{*effective_width <= 8    ? 8U
                             : *effective_width <= 16 ? 16U
                             : *effective_width <= 32 ? 32U
                                                      : 64U};
    return EnumStorageRequirement{.signedness = *domain.signed_domain, .bit_width = storage_width};
}
}
