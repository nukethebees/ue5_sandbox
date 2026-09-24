#include <sandbox/core/packed_value.h>

#include <cstdint>
#include <limits>

#if defined(CODEGEN_BAD_SIGNED_STORAGE)
using InvalidField = ml::PackedField<std::int8_t, std::uint8_t, 0, 1>;
#elif defined(CODEGEN_BAD_BOOL_STORAGE)
using InvalidField = ml::PackedField<bool, bool, 0, 1>;
#elif defined(CODEGEN_BAD_ZERO_BITS)
using InvalidField = ml::PackedField<std::uint8_t, std::uint8_t, 0, 0>;
#elif defined(CODEGEN_BAD_NEGATIVE_BITS)
using InvalidField =
    ml::PackedField<std::uint8_t, std::uint8_t, 0, std::numeric_limits<int>::min()>;
#elif defined(CODEGEN_BAD_NEGATIVE_OFFSET)
using InvalidField = ml::PackedField<std::uint8_t, std::uint8_t, -1, 1>;
#elif defined(CODEGEN_BAD_OVERFLOW)
using InvalidField = ml::PackedField<std::uint64_t, std::uint64_t, 1, 64>;
#elif defined(CODEGEN_BAD_FULL_OFFSET)
using InvalidField = ml::PackedField<std::uint64_t, std::uint64_t, 64, 1>;
#elif defined(CODEGEN_BAD_UNSIGNED_VALUE)
using InvalidField = ml::PackedField<std::uint16_t, std::uint8_t, 0, 16>;
#elif defined(CODEGEN_BAD_SIGNED_VALUE)
using InvalidField = ml::PackedField<std::uint16_t, std::int8_t, 0, 9>;
#elif defined(CODEGEN_BAD_BOOL_VALUE)
using InvalidField = ml::PackedField<std::uint8_t, bool, 0, 2>;
#elif defined(CODEGEN_BAD_ENUM_WIDTH)
enum class Narrow : std::uint8_t { zero };
using InvalidField = ml::PackedField<std::uint16_t, Narrow, 0, 16>;
#elif defined(CODEGEN_BAD_ENUM_SIGNED)
enum class Signed : std::int8_t { zero };
using InvalidField = ml::PackedField<std::uint8_t, Signed, 0, 4>;
#elif defined(CODEGEN_BAD_FLOAT_VALUE)
using InvalidField = ml::PackedField<std::uint8_t, float, 0, 4>;
#elif defined(CODEGEN_BAD_POINTER_VALUE)
using InvalidField = ml::PackedField<std::uint8_t, int*, 0, 4>;
#elif defined(CODEGEN_BAD_REFERENCE_VALUE)
using InvalidField = ml::PackedField<std::uint8_t, int&, 0, 4>;
#else
#error Select an invalid packed field
#endif

static_assert(sizeof(InvalidField) > 0);
