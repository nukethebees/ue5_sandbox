#pragma once

#include <limits>
#include <type_traits>

namespace ml {

template <typename Storage, int ExpectedBits>
[[nodiscard]] constexpr auto valid_packed_storage() noexcept -> bool {
    return std::is_integral_v<Storage> && std::is_unsigned_v<Storage> &&
           !std::is_same_v<std::remove_cv_t<Storage>, bool> &&
           std::numeric_limits<Storage>::digits == ExpectedBits;
}

template <typename Storage, typename Value, int Offset, int Bits>
struct PackedField {
    using storage_type = Storage;
    using value_type = Value;

    static_assert(valid_packed_storage<Storage, std::numeric_limits<Storage>::digits>(),
                  "Packed field requires unsigned integer storage other than bool");
    static_assert(Bits > 0, "Packed field requires positive width");
    static_assert(Offset >= 0, "Packed field requires nonnegative offset");
    static_assert(Bits > 0 && Bits <= std::numeric_limits<Storage>::digits &&
                      Offset <= std::numeric_limits<Storage>::digits - Bits,
                  "Packed field must fit in storage");
    static_assert(
        [] {
            using UnqualifiedValue = std::remove_cv_t<Value>;
            if constexpr (std::is_same_v<UnqualifiedValue, bool>) {
                return Bits == 1;
            } else if constexpr (std::is_integral_v<UnqualifiedValue>) {
                return std::numeric_limits<UnqualifiedValue>::digits +
                           (std::is_signed_v<UnqualifiedValue> ? 1 : 0) >=
                       Bits;
            } else if constexpr (std::is_enum_v<UnqualifiedValue>) {
                using Underlying = std::underlying_type_t<UnqualifiedValue>;
                return std::is_unsigned_v<Underlying> &&
                       std::numeric_limits<Underlying>::digits >= Bits;
            } else {
                return false;
            }
        }(),
        "Packed field requires an integer, one-bit bool, or unsigned enum value type wide enough "
        "for its bits");

    inline static constexpr int offset{Offset};
    inline static constexpr int bits{Bits};
    inline static constexpr storage_type value_mask{[] {
        if constexpr (Bits > 0 && Bits < std::numeric_limits<Storage>::digits) {
            return static_cast<Storage>((Storage{1} << Bits) - Storage{1});
        } else {
            return (std::numeric_limits<Storage>::max)();
        }
    }()};
    inline static constexpr storage_type mask{[] {
        if constexpr (Offset >= 0 && Offset < std::numeric_limits<Storage>::digits) {
            return static_cast<Storage>(value_mask << Offset);
        } else {
            return Storage{};
        }
    }()};
};

template <typename Field>
[[nodiscard]] constexpr auto packed_extract(typename Field::storage_type const raw) noexcept ->
    typename Field::value_type {
    using Storage = typename Field::storage_type;
    using Value = typename Field::value_type;
    auto const encoded{static_cast<Storage>((raw >> Field::offset) & Field::value_mask)};

    if constexpr (std::is_signed_v<Value>) {
        auto const sign_bit{static_cast<Storage>(Storage{1} << (Field::bits - 1))};
        if ((encoded & sign_bit) != 0) {
            // The complemented magnitude fits even when the decoded value is the signed minimum.
            auto const complement{static_cast<Storage>(~encoded & Field::value_mask)};
            return static_cast<Value>(-static_cast<Value>(complement) - Value{1});
        }
    }

    return static_cast<Value>(encoded);
}

template <typename Field>
[[nodiscard]] constexpr auto packed_pack(typename Field::value_type const value) noexcept ->
    typename Field::storage_type {
    // Schema-specific range checks belong to the caller; packing retains only the field's bits.
    using Storage = typename Field::storage_type;
    auto const encoded{static_cast<Storage>(value)};
    return static_cast<Storage>((encoded & Field::value_mask) << Field::offset);
}

template <typename Field>
[[nodiscard]] constexpr auto packed_insert(typename Field::storage_type const raw,
                                           typename Field::value_type const value) noexcept ->
    typename Field::storage_type {
    using Storage = typename Field::storage_type;
    auto const cleared{static_cast<Storage>(raw & static_cast<Storage>(~Field::mask))};
    return static_cast<Storage>(cleared | packed_pack<Field>(value));
}

} // namespace ml
