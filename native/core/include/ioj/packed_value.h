#pragma once

#include <ioj/min_max.h>

#include <bit>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace ioj {

template <typename T>
concept PackedStorageInteger =
    std::unsigned_integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <typename T, int Bits>
concept PackedEnumValue =
    std::is_enum_v<T> && std::unsigned_integral<std::underlying_type_t<T>> && Bits > 0 &&
    std::numeric_limits<std::underlying_type_t<T>>::digits >= Bits;

template <typename T, int Bits>
concept PackedFieldValue =
    Bits > 0 && ((std::same_as<std::remove_cv_t<T>, bool> && Bits == 1) ||
                 (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> &&
                  std::numeric_limits<T>::digits + (std::signed_integral<T> ? 1 : 0) >= Bits) ||
                 PackedEnumValue<T, Bits>);

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

template <typename T>
concept RawValue = requires(T const& value) {
    typename T::packed_integer_tag;
    requires PackedStorageInteger<typename T::storage_type>;
    { value.raw_value() } noexcept -> std::same_as<typename T::storage_type>;
} && valid_packed_value_layout<T>();

template <typename T>
concept ScalarValue =
    std::same_as<typename T::packed_integer_tag, scalar_tag> && requires(T const& value) {
        requires std::integral<typename T::value_type>;
        { value.value() } noexcept -> std::same_as<typename T::value_type>;
    };
}

template <typename T>
concept PackedInteger =
    packed::RawValue<T> &&
    (std::same_as<typename T::packed_integer_tag, packed::integer_tag> || packed::ScalarValue<T>);

namespace packed {
template <PackedInteger T>
[[nodiscard]] constexpr auto format_value(T const& value) noexcept {
    if constexpr (ScalarValue<T>) {
        return value.value();
    } else {
        return value.raw_value();
    }
}

template <PackedInteger T>
using format_type = decltype(format_value(std::declval<T const&>()));
}

template <PackedStorageInteger Storage, int EncodedBits>
struct PackedStorage {
    using storage_type = Storage;

    static_assert(EncodedBits > 0 && EncodedBits <= std::numeric_limits<Storage>::digits,
                  "Packed encoding must fit in storage");

    inline static constexpr int encoded_bits{EncodedBits};
    inline static constexpr int storage_bits{std::numeric_limits<Storage>::digits};

    Storage default_raw_value{};
};

template <PackedStorageInteger Storage, typename Value, int Offset, int Bits>
struct PackedField {
    using storage_type = Storage;
    using value_type = Value;

    static_assert(Offset >= 0 && Bits > 0 && Bits <= std::numeric_limits<Storage>::digits &&
                      Offset <= std::numeric_limits<Storage>::digits - Bits,
                  "Packed field must fit in storage");
    static_assert(
        PackedFieldValue<Value, Bits>,
        "Packed field requires an integer, one-bit bool, or unsigned enum value type wide enough "
        "for its bits");

    inline static constexpr int offset{Offset};
    inline static constexpr int bits{Bits};
    inline static constexpr int magnitude_bits{Bits - (std::is_signed_v<Value> ? 1 : 0)};
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

template <typename Derived, auto Layout>
struct PackedValueBase {
    using storage_type = typename decltype(Layout)::storage_type;

    static constexpr auto storage{Layout};

    constexpr PackedValueBase() noexcept = default;

    constexpr auto operator<=>(PackedValueBase const&) const noexcept = default;

    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type { return value_; }
    // Clear scalar padding without validating the encoded value's semantic domain.
    [[nodiscard]] static constexpr auto from_raw(storage_type const value) noexcept -> Derived {
        static_assert(valid_packed_value_layout<Derived>());
        return std::bit_cast<Derived>(canonical_raw(value));
    }
    // Require zero scalar padding; semantic validity remains the caller's responsibility.
    [[nodiscard]] static constexpr auto from_canonical_raw(storage_type const value) noexcept
        -> Derived {
        static_assert(valid_packed_value_layout<Derived>());
        assert(value == canonical_raw(value));
        return std::bit_cast<Derived>(value);
    }
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool { return true; }

  private:
    [[nodiscard]] static constexpr auto canonical_raw(storage_type const value) noexcept
        -> storage_type {
        if constexpr (std::same_as<typename Derived::packed_integer_tag, packed::scalar_tag>) {
            using Field = PackedField<storage_type, storage_type, 0, storage.encoded_bits>;
            return static_cast<storage_type>(value & Field::value_mask);
        } else {
            return value;
        }
    }

  protected:
    template <typename Field>
    [[nodiscard]] constexpr auto extract() const noexcept -> typename Field::value_type {
        return packed_extract<Field>(value_);
    }
    template <typename Field>
    constexpr void insert(typename Field::value_type const value) noexcept {
        value_ = packed_insert<Field>(value_, value);
    }

    storage_type value_{canonical_raw(storage.default_raw_value)};
};

} // namespace ioj

template <ioj::PackedInteger T>
struct std::hash<T> {
    [[nodiscard]] auto operator()(T const& value) const noexcept -> std::size_t {
        return std::hash<typename T::storage_type>{}(value.raw_value());
    }
};

template <ioj::PackedInteger T, typename Char>
struct std::formatter<T, Char> : std::formatter<ioj::packed::format_type<T>, Char> {
    template <typename FormatContext>
    auto format(T const& value, FormatContext& context) const -> typename FormatContext::iterator {
        return std::formatter<ioj::packed::format_type<T>, Char>::format(
            ioj::packed::format_value(value), context);
    }
};
