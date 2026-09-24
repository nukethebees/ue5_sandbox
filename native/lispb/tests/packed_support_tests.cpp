#include <sandbox/core/packed_value.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace packed_support_tests {

template <typename Storage>
constexpr auto check_storage_width() -> bool {
    constexpr auto width{std::numeric_limits<Storage>::digits};
    constexpr auto maximum{(std::numeric_limits<Storage>::max)()};
    using Full = ml::PackedField<Storage, Storage, 0, width>;
    using Top = ml::PackedField<Storage, Storage, width - 3, 3>;
    using Flag = ml::PackedField<Storage, bool, width - 1, 1>;
    using Low = ml::PackedField<Storage, std::uint64_t, 0, 3>;
    using Signed = ml::PackedField<Storage, std::make_signed_t<Storage>, 0, width>;
    using SignedValue = typename Signed::value_type;
    constexpr auto minimum{std::numeric_limits<SignedValue>::min()};
    constexpr auto signed_maximum{(std::numeric_limits<SignedValue>::max)()};

    static_assert(ml::valid_packed_storage<Storage, width>());
    static_assert(!ml::valid_packed_storage<Storage, width + 1>());
    static_assert(Full::value_mask == maximum && Full::mask == maximum);
    static_assert(Top::offset == width - 3 && Top::bits == 3);
    static_assert(Top::value_mask == 7);
    static_assert(Top::mask == static_cast<Storage>(Storage{7} << (width - 3)));
    static_assert(std::is_same_v<typename Full::storage_type, Storage>);
    static_assert(std::is_same_v<typename Low::value_type, std::uint64_t>);
    static_assert(ml::packed_extract<Full>(maximum) == maximum);
    static_assert(ml::packed_pack<Full>(maximum) == maximum);
    static_assert(ml::packed_insert<Full>(maximum, 0) == 0);
    static_assert(ml::packed_pack<Flag>(true) == Flag::mask);
    static_assert(ml::packed_extract<Flag>(maximum));
    static_assert(ml::packed_insert<Top>(maximum, 0) == static_cast<Storage>(maximum >> 3));
    static_assert(ml::packed_insert<Low>(Top::mask, 5) == static_cast<Storage>(Top::mask | 5));
    static_assert(ml::packed_pack<Low>((std::numeric_limits<std::uint64_t>::max)()) == 7);
    static_assert(ml::packed_extract<Signed>(ml::packed_pack<Signed>(minimum)) == minimum);
    static_assert(ml::packed_extract<Signed>(ml::packed_pack<Signed>(signed_maximum)) ==
                  signed_maximum);
    static_assert(ml::packed_extract<Signed>(maximum) == -1);
    return true;
}

static_assert(check_storage_width<std::uint8_t>());
static_assert(check_storage_width<std::uint16_t>());
static_assert(check_storage_width<std::uint32_t>());
static_assert(check_storage_width<std::uint64_t>());
static_assert(!ml::valid_packed_storage<bool, 1>());
static_assert(!ml::valid_packed_storage<bool const, 1>());
static_assert(!ml::valid_packed_storage<std::int32_t, 31>());

enum class State : std::uint8_t { zero = 0, maximum = 7 };
using EnumField = ml::PackedField<std::uint16_t, State, 13, 3>;
static_assert(ml::packed_extract<EnumField>(0xffff) == State::maximum);
static_assert(ml::packed_pack<EnumField>(State::maximum) == 0xe000);
static_assert(ml::packed_insert<EnumField>(0xffff, State::zero) == 0x1fff);

enum class WideState : std::uint64_t { maximum = (std::numeric_limits<std::uint64_t>::max)() };
using WideEnumField = ml::PackedField<std::uint64_t, WideState, 0, 64>;
static_assert(ml::packed_extract<WideEnumField>(
                  ml::packed_pack<WideEnumField>(WideState::maximum)) == WideState::maximum);
using ConstField = ml::PackedField<std::uint16_t, std::int16_t const, 3, 5>;
static_assert(ml::packed_extract<ConstField>(ml::packed_pack<ConstField>(-16)) == -16);
using ConstEnumField = ml::PackedField<std::uint8_t, State const, 0, 3>;
static_assert(ml::packed_extract<ConstEnumField>(7) == State::maximum);
using CvFlag = ml::PackedField<std::uint8_t, bool const volatile, 0, 1>;
static_assert(CvFlag::bits == 1);
using CvInteger = ml::PackedField<std::uint16_t, std::uint16_t const volatile, 0, 16>;
static_assert(CvInteger::value_mask == 0xffff);

TEST(PackedSupport, ExhaustivelyDecodesSignedFieldsAndPreservesOtherBits) {
    using Field = ml::PackedField<std::uint8_t, std::int16_t, 2, 4>;
    for (unsigned raw{}; raw <= 0xffu; ++raw) {
        auto const encoded{static_cast<int>((raw >> 2) & 0xfu)};
        auto const expected{encoded >= 8 ? encoded - 16 : encoded};
        auto const storage{static_cast<std::uint8_t>(raw)};
        EXPECT_EQ(ml::packed_extract<Field>(storage), expected);
        for (std::int16_t value{-8}; value < 8; ++value) {
            auto const changed{ml::packed_insert<Field>(storage, value)};
            EXPECT_EQ(ml::packed_extract<Field>(changed), value);
            EXPECT_EQ(changed & 0xc3u, raw & 0xc3u);
        }
    }
}

} // namespace packed_support_tests
