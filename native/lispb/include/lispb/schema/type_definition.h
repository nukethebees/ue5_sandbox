#pragma once

#include <lispb/schema/enum_type.h>
#include <lispb/schema/external_type.h>
#include <lispb/schema/facade_type.h>
#include <lispb/schema/fixed_point_type.h>
#include <lispb/schema/homogeneous_storage_type.h>
#include <lispb/schema/integer_scalar_type.h>
#include <lispb/schema/integer_varint_type.h>
#include <lispb/schema/linear_quantized_type.h>
#include <lispb/schema/mini_float_type.h>
#include <lispb/schema/optional_presence_bit_type.h>
#include <lispb/schema/optional_sentinel_type.h>
#include <lispb/schema/packed_type.h>
#include <lispb/schema/record_type.h>
#include <lispb/schema/soa_type.h>
#include <lispb/schema/static_table_type.h>
#include <lispb/schema/tagged_union_type.h>
#include <lispb/schema/union_type.h>

#include <variant>

namespace lispb::schema {
using TypeDefinition = std::variant<ExternalType,
                                    EnumType,
                                    IntegerScalarType,
                                    LinearQuantizedType,
                                    IntegerVarintType,
                                    FixedPointType,
                                    MiniFloatType,
                                    OptionalSentinelType,
                                    OptionalPresenceBitType,
                                    PackedType,
                                    RecordType,
                                    UnionType,
                                    TaggedUnionType,
                                    SoaType,
                                    StaticTableType,
                                    FacadeType,
                                    HomogeneousStorageType>;
}
