#include <ioj/layout/declaration_kind.hpp>

namespace ioj::layout {
auto declaration_kind_label(DeclarationKind const kind) -> char const* {
    switch (kind) {
        case DeclarationKind::external:
            return "external";
        case DeclarationKind::enumeration:
            return "enum";
        case DeclarationKind::integer_scalar:
            return "integer scalar";
        case DeclarationKind::linear_quantized:
            return "linear quantized";
        case DeclarationKind::integer_varint:
            return "integer varint";
        case DeclarationKind::fixed_point:
            return "fixed point";
        case DeclarationKind::mini_float:
            return "mini float";
        case DeclarationKind::optional_sentinel:
            return "optional sentinel";
        case DeclarationKind::optional_presence_bit:
            return "optional presence bit";
        case DeclarationKind::packed:
            return "packed record";
        case DeclarationKind::record:
            return "record";
        case DeclarationKind::union_:
            return "union";
        case DeclarationKind::soa:
            return "SoA";
        case DeclarationKind::static_table:
            return "table";
        case DeclarationKind::facade:
            return "facade";
    }
    return "unknown";
}
}
