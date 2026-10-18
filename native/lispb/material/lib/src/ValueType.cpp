#include <material_gen/ValueType.h>

#include "material_type_rules.h"

#include <cstddef>

namespace material_synth {

auto promote_value_types(ValueType const left, ValueType const right) -> ValueType {
    if (!is_numeric(left) || !is_numeric(right)) {
        return ValueType::invalid;
    }
    if (left == right) {
        return left;
    }
    if (left == ValueType::float1) {
        return right;
    }
    if (right == ValueType::float1) {
        return left;
    }
    return ValueType::invalid;
}

auto is_numeric(ValueType const type) -> bool {
    return type >= ValueType::float1 && type <= ValueType::float4;
}

auto component_count(ValueType const type) -> std::size_t {
    if (!is_numeric(type)) {
        return 0;
    }
    return static_cast<std::size_t>(type) - static_cast<std::size_t>(ValueType::float1) + 1;
}

}
