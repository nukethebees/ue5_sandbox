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
#else
#error Select an invalid packed field
#endif

static_assert(sizeof(InvalidField) > 0);
