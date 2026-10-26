#pragma once

#include <lispb/schema/child_occurrences.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/union_types.h>

namespace lispb::schema {

struct TypeDefinitions {
    ExternalTypes externals;
    EnumTypes enums;
    IntegerScalarTypes integer_scalars;
    LinearQuantizedTypes linear_quantized;
    IntegerVarintTypes integer_varints;
    FixedPointTypes fixed_points;
    MiniFloatTypes mini_floats;
    OptionalSentinelTypes optional_sentinels;
    OptionalPresenceBitTypes optional_presence_bits;
    ChildOccurrences children;
    RecordTypes records;
    UnionTypes unions;
    SoaTypes soas;
    StaticTableTypes static_tables;
    FacadeTypes facades;
    SoaColumns soa_columns;
};

} // namespace lispb::schema
