#pragma once

#include <ioj/min_max.h>

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace ioj {

template <typename Storage, int ExpectedBits>
[[nodiscard]] constexpr auto valid_packed_storage() noexcept -> bool {
    return std::is_integral_v<Storage> && std::is_unsigned_v<Storage> &&
           !std::is_same_v<std::remove_cv_t<Storage>, bool> &&
           std::numeric_limits<Storage>::digits == ExpectedBits;
}

template <typename Enum, int Bits>
[[nodiscard]] constexpr auto valid_packed_enum() noexcept -> bool {
    if constexpr (std::is_enum_v<Enum>) {
        using Underlying = std::underlying_type_t<Enum>;
        return std::is_unsigned_v<Underlying> && std::numeric_limits<Underlying>::digits >= Bits;
    } else {
        return false;
    }
}

template <typename T>
[[nodiscard]] consteval auto valid_packed_value_layout() noexcept -> bool {
    if constexpr (requires {
                      sizeof(T);
                      sizeof(typename T::storage_type);
                      alignof(T);
                      alignof(typename T::storage_type);
                  }) {
        return sizeof(T) == sizeof(typename T::storage_type) &&
               alignof(T) == alignof(typename T::storage_type) && std::is_trivially_copyable_v<T> &&
               std::is_standard_layout_v<T>;
    } else {
        return false;
    }
}

namespace packed {
struct integer_tag {};
struct scalar_tag {};
}

template <typename T>
concept PackedInteger =
    requires(T const& value) {
        typename T::packed_integer_tag;
        typename T::storage_type;
        requires std::same_as<typename T::packed_integer_tag, packed::integer_tag> ||
                     std::same_as<typename T::packed_integer_tag, packed::scalar_tag>;
        requires std::unsigned_integral<typename T::storage_type>;
        requires (!std::same_as<typename T::storage_type, bool>);
        { value.raw_value() } noexcept -> std::same_as<typename T::storage_type>;
    } && valid_packed_value_layout<T>() &&
    (std::same_as<typename T::packed_integer_tag, packed::integer_tag> || requires(T const& value) {
        typename T::value_type;
        requires std::integral<typename T::value_type>;
        { value.value() } noexcept -> std::same_as<typename T::value_type>;
    });

namespace packed {
template <PackedInteger T>
[[nodiscard]] constexpr auto comparison_value(T const& value) noexcept {
    if constexpr (std::same_as<typename T::packed_integer_tag, scalar_tag>) {
        return value.value();
    } else {
        return value.raw_value();
    }
}

template <PackedInteger T>
using comparison_type = decltype(comparison_value(std::declval<T const&>()));
}

template <typename Storage, int EncodedBits>
struct PackedStorage {
    using storage_type = Storage;

    static_assert(valid_packed_storage<Storage, std::numeric_limits<Storage>::digits>(),
                  "Packed storage requires unsigned integer storage other than bool");
    static_assert(EncodedBits > 0 && EncodedBits <= std::numeric_limits<Storage>::digits,
                  "Packed encoding must fit in storage");

    inline static constexpr int encoded_bits{EncodedBits};
    inline static constexpr int storage_bits{std::numeric_limits<Storage>::digits};

    Storage default_raw_value{};
};

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
        []<typename UnqualifiedValue> {
            if constexpr (std::is_same_v<UnqualifiedValue, bool>) {
                return Bits == 1;
            } else if constexpr (std::is_integral_v<UnqualifiedValue>) {
                return std::numeric_limits<UnqualifiedValue>::digits +
                           (std::is_signed_v<UnqualifiedValue> ? 1 : 0) >=
                       Bits;
            } else if constexpr (std::is_enum_v<UnqualifiedValue>) {
                return valid_packed_enum<UnqualifiedValue, Bits>();
            } else {
                return false;
            }
        }.template operator()<std::remove_cv_t<Value>>(),
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

    MinMax<Value> range{[] {
        if constexpr (std::is_signed_v<Value>) {
            auto const maximum{static_cast<Value>(value_mask >> 1)};
            return MinMax<Value>{static_cast<Value>(-maximum - Value{1}), maximum};
        } else {
            return MinMax<Value>{Value{}, static_cast<Value>(value_mask)};
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

} // namespace ioj

template <ioj::PackedInteger T>
struct std::hash<T> {
    [[nodiscard]] auto operator()(T const& value) const noexcept -> std::size_t {
        return std::hash<ioj::packed::comparison_type<T>>{}(ioj::packed::comparison_value(value));
    }
};

template <ioj::PackedInteger T, typename Char>
struct std::formatter<T, Char> : std::formatter<ioj::packed::comparison_type<T>, Char> {
    template <typename FormatContext>
    auto format(T const& value, FormatContext& context) const -> typename FormatContext::iterator {
        return std::formatter<ioj::packed::comparison_type<T>, Char>::format(
            ioj::packed::comparison_value(value), context);
    }
};
