#pragma once

#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/homogeneous_storage_types.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/union_alternatives.h>
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
    PackedTypes packed;
    RecordTypes records;
    UnionTypes unions;
    TaggedUnionTypes tagged_unions;
    SoaTypes soas;
    StaticTableTypes static_tables;
    FacadeTypes facades;
    HomogeneousStorageTypes homogeneous_storage;
    RecordMembers record_members;
    UnionAlternatives union_alternatives;
    TaggedUnionAlternatives tagged_union_alternatives;
    SoaColumns soa_columns;
    PackedFields packed_fields;
    PackedReservedSegments packed_reserved_bits;
};

} // namespace lispb::schema
