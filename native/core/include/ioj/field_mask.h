#pragma once

#include <ioj/enum_traits.h>

#include <cstdint>
#include <type_traits>

namespace ioj {

template <typename Field, std::size_t N = enum_count<Field>()>
class FieldMask {
  public:
    static constexpr std::size_t field_count{N};
    static_assert(field_count <= 64, "Field mask exceeds 64 bits.");

    using storage_type = std::conditional_t<
        field_count <= 8,
        std::uint8_t,
        std::conditional_t<field_count <= 16,
                           std::uint16_t,
                           std::conditional_t<field_count <= 32, std::uint32_t, std::uint64_t>>>;

    constexpr FieldMask() noexcept = default;
    constexpr explicit FieldMask(storage_type value) noexcept
        : value_{value} {}

    [[nodiscard]] constexpr auto value() const noexcept -> storage_type { return value_; }
    [[nodiscard]] constexpr auto is_empty() const noexcept -> bool { return value_ == 0; }
    [[nodiscard]] constexpr auto has(Field field) const noexcept -> bool {
        return (value_ & bit(field)) != 0;
    }
    [[nodiscard]] static constexpr auto index(Field field) noexcept -> std::int32_t {
        return static_cast<std::int32_t>(field);
    }

    constexpr void set(Field field) noexcept {
        value_ = static_cast<storage_type>(value_ | bit(field));
    }
    template <typename Self>
        requires (!std::is_const_v<Self>)
    constexpr void set(this Self& self, Self const fields) noexcept {
        self.value_ = static_cast<storage_type>(self.value_ | fields.value_);
    }
    constexpr void clear(Field field) noexcept {
        value_ = static_cast<storage_type>(value_ & ~bit(field));
    }
  private:
    [[nodiscard]] static constexpr auto bit(Field field) noexcept -> storage_type {
        return static_cast<storage_type>(std::uint64_t{1} << static_cast<std::uint8_t>(field));
    }

    storage_type value_{};
};

} // namespace ioj
