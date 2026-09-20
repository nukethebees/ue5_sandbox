#pragma once

#include <codegen/schema/fixed_point_schema.h>
#include <codegen/schema/integer_varint_schema.h>
#include <codegen/schema/linear_quantized_schema.h>
#include <codegen/schema/mini_float_schema.h>
#include <codegen/schema/module_settings.h>
#include <codegen/schema/optional_presence_bit_schema.h>
#include <codegen/schema/optional_sentinel_schema.h>

#include <vector>

namespace codegen {

struct RepresentationModuleSchema {
    ModuleSettings settings;
    std::vector<LinearQuantizedSchema> linear_quantized;
    std::vector<IntegerVarintSchema> integer_varints;
    std::vector<FixedPointSchema> fixed_points;
    std::vector<OptionalSentinelSchema> optional_sentinels;
    std::vector<OptionalPresenceBitSchema> optional_presence_bits{};
    std::vector<MiniFloatSchema> mini_floats{};
};

} // namespace codegen
