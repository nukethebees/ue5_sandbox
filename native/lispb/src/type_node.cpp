#include <lispb/schema/type_node.h>

namespace lispb::schema {
auto integer_domain(TypeNode const& node) -> IntegerScalarType const* {
    if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
        return scalar;
    }
    if (auto const* external{std::get_if<ExternalType>(&node.definition)}) {
        return std::get_if<IntegerScalarType>(&external->semantics);
    }
    return nullptr;
}

auto packed_integer_domain(TypeNode const& node, codegen::PackedFieldSchema const& field)
    -> IntegerScalarType const* {
    if (field.bits.has_value() && !std::holds_alternative<IntegerScalarType>(node.definition)) {
        return nullptr;
    }
    return integer_domain(node);
}
}
