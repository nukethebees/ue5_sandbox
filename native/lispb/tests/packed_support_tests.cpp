#include <ioj/packed_value.h>

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace packed_support_tests {

template <typename Storage>
constexpr auto check_storage_width() -> bool {
    constexpr auto width{std::numeric_limits<Storage>::digits};
    constexpr auto maximum{(std::numeric_limits<Storage>::max)()};
    using Full = ioj::PackedField<Storage, Storage, 0, width>;
    using Top = ioj::PackedField<Storage, Storage, width - 3, 3>;
    using Flag = ioj::PackedField<Storage, bool, width - 1, 1>;
    using Low = ioj::PackedField<Storage, std::uint64_t, 0, 3>;
    using Signed = ioj::PackedField<Storage, std::make_signed_t<Storage>, 0, width>;
    using SignedValue = typename Signed::value_type;
    constexpr ioj::PackedStorage<Storage, width> storage{maximum};
    constexpr Full full{};
    constexpr Signed signed_field{};
    constexpr Flag flag{};
    constexpr auto minimum{std::numeric_limits<SignedValue>::min()};
    constexpr auto signed_maximum{(std::numeric_limits<SignedValue>::max)()};

    static_assert(ioj::PackedStorageInteger<Storage>);
    static_assert(ioj::PackedFieldValue<Storage, width>);
    static_assert(!ioj::PackedFieldValue<Storage, width + 1>);
    static_assert(storage.encoded_bits == width && storage.storage_bits == width);
    static_assert(storage.default_raw_value == maximum);
    static_assert(full.range.min == 0 && full.range.max == maximum);
    static_assert(signed_field.range.min == minimum && signed_field.range.max == signed_maximum);
    static_assert(!flag.range.min && flag.range.max);
    static_assert(Full::value_mask == maximum && Full::mask == maximum);
    static_assert(Top::offset == width - 3 && Top::bits == 3);
    static_assert(Top::value_mask == 7);
    static_assert(Top::mask == static_cast<Storage>(Storage{7} << (width - 3)));
    static_assert(std::is_same_v<typename Full::storage_type, Storage>);
    static_assert(std::is_same_v<typename Low::value_type, std::uint64_t>);
    static_assert(ioj::packed_extract<Full>(maximum) == maximum);
    static_assert(ioj::packed_pack<Full>(maximum) == maximum);
    static_assert(ioj::packed_insert<Full>(maximum, 0) == 0);
    static_assert(ioj::packed_pack<Flag>(true) == Flag::mask);
    static_assert(ioj::packed_extract<Flag>(maximum));
    static_assert(ioj::packed_insert<Top>(maximum, 0) == static_cast<Storage>(maximum >> 3));
    static_assert(ioj::packed_insert<Low>(Top::mask, 5) == static_cast<Storage>(Top::mask | 5));
    static_assert(ioj::packed_pack<Low>((std::numeric_limits<std::uint64_t>::max)()) == 7);
    static_assert(ioj::packed_extract<Signed>(ioj::packed_pack<Signed>(minimum)) == minimum);
    static_assert(ioj::packed_extract<Signed>(ioj::packed_pack<Signed>(signed_maximum)) ==
                  signed_maximum);
    static_assert(ioj::packed_extract<Signed>(maximum) == -1);
    return true;
}

static_assert(check_storage_width<std::uint8_t>());
static_assert(check_storage_width<std::uint16_t>());
static_assert(check_storage_width<std::uint32_t>());
static_assert(check_storage_width<std::uint64_t>());
static_assert(!ioj::PackedStorageInteger<bool>);
static_assert(!ioj::PackedStorageInteger<bool const>);
static_assert(!ioj::PackedStorageInteger<std::int32_t>);
static_assert(!ioj::PackedStorageInteger<float>);
static_assert(ioj::PackedFieldValue<bool const volatile, 1>);
static_assert(!ioj::PackedFieldValue<bool, 2>);
static_assert(ioj::PackedFieldValue<std::int8_t, 8>);
static_assert(!ioj::PackedFieldValue<std::uint8_t, 0>);
static_assert(!ioj::PackedFieldValue<float, 8>);
static_assert(!ioj::PackedEnumValue<std::uint8_t, 8>);

enum class State : std::uint8_t { zero = 0, maximum = 7 };
static_assert(ioj::PackedEnumValue<State const, 8>);
static_assert(!ioj::PackedEnumValue<State, 9>);
enum class SignedState : std::int8_t { negative = -1 };
static_assert(!ioj::PackedEnumValue<SignedState, 8>);
using EnumField = ioj::PackedField<std::uint16_t, State, 13, 3>;
static_assert(ioj::packed_extract<EnumField>(0xffff) == State::maximum);
static_assert(ioj::packed_pack<EnumField>(State::maximum) == 0xe000);
static_assert(ioj::packed_insert<EnumField>(0xffff, State::zero) == 0x1fff);

enum class WideState : std::uint64_t { maximum = (std::numeric_limits<std::uint64_t>::max)() };
using WideEnumField = ioj::PackedField<std::uint64_t, WideState, 0, 64>;
static_assert(ioj::packed_extract<WideEnumField>(
                  ioj::packed_pack<WideEnumField>(WideState::maximum)) == WideState::maximum);
using ConstField = ioj::PackedField<std::uint16_t, std::int16_t const, 3, 5>;
static_assert(ioj::packed_extract<ConstField>(ioj::packed_pack<ConstField>(-16)) == -16);
using ConstEnumField = ioj::PackedField<std::uint8_t, State const, 0, 3>;
static_assert(ioj::packed_extract<ConstEnumField>(7) == State::maximum);
using CvFlag = ioj::PackedField<std::uint8_t, bool const volatile, 0, 1>;
static_assert(CvFlag::bits == 1);
using CvInteger = ioj::PackedField<std::uint16_t, std::uint16_t const volatile, 0, 16>;
static_assert(CvInteger::value_mask == 0xffff);

TEST(PackedSupport, ExhaustivelyDecodesSignedFieldsAndPreservesOtherBits) {
    struct Incomplete;
    struct Untagged {
        using storage_type = std::uint32_t;
        [[maybe_unused]] auto raw_value() const noexcept -> storage_type { return raw; }
        storage_type raw;
    };
    struct WrongTag : Untagged {
        using packed_integer_tag = void;
    };
    struct SignedStorage {
        using packed_integer_tag = ioj::packed::integer_tag;
        using storage_type = std::int32_t;
        [[maybe_unused]] auto raw_value() const noexcept -> storage_type { return raw; }
        storage_type raw;
    };
    struct MissingDecodedValue : Untagged {
        using packed_integer_tag = ioj::packed::scalar_tag;
    };
    struct ThrowingRaw {
        using packed_integer_tag = ioj::packed::integer_tag;
        using storage_type = std::uint32_t;
        [[maybe_unused]] auto raw_value() const -> storage_type { return raw; }
        storage_type raw;
    };
    struct Oversized {
        using packed_integer_tag = ioj::packed::integer_tag;
        using storage_type = std::uint32_t;
        [[maybe_unused]] auto raw_value() const noexcept -> storage_type { return raw[0]; }
        storage_type raw[2];
    };
    static_assert(!ioj::PackedInteger<int>);
    static_assert(!ioj::PackedInteger<Incomplete>);
    static_assert(!ioj::PackedInteger<Untagged>);
    static_assert(!ioj::PackedInteger<WrongTag>);
    static_assert(!ioj::PackedInteger<SignedStorage>);
    static_assert(!ioj::PackedInteger<MissingDecodedValue>);
    static_assert(!ioj::PackedInteger<ThrowingRaw>);
    static_assert(!ioj::PackedInteger<Oversized>);
    static_assert(!ioj::valid_packed_value_layout<Incomplete>());
    static_assert(!ioj::valid_packed_value_layout<int>());
    constexpr ioj::PackedField<std::uint32_t, std::uint32_t, 0, 24> narrow{};
    static_assert(narrow.range.min == 0 && narrow.range.max == 0xffffff);
    constexpr EnumField enumeration{};
    static_assert(enumeration.range.min == State::zero);
    static_assert(enumeration.range.max == State::maximum);
    using Field = ioj::PackedField<std::uint8_t, std::int16_t, 2, 4>;
    constexpr Field constrained{{-3, 5}};
    static_assert(constrained.range.min == -3 && constrained.range.max == 5);
    for (unsigned raw{}; raw <= 0xffu; ++raw) {
        auto const encoded{static_cast<int>((raw >> 2) & 0xfu)};
        auto const expected{encoded >= 8 ? encoded - 16 : encoded};
        auto const storage{static_cast<std::uint8_t>(raw)};
        EXPECT_EQ(ioj::packed_extract<Field>(storage), expected);
        for (std::int16_t value{-8}; value < 8; ++value) {
            auto const changed{ioj::packed_insert<Field>(storage, value)};
            EXPECT_EQ(ioj::packed_extract<Field>(changed), value);
            EXPECT_EQ(changed & 0xc3u, raw & 0xc3u);
        }
    }
}

} // namespace packed_support_tests
