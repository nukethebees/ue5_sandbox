#include <codegen/lowering/compact_vector_kind.h>

#include <utility>

namespace lispb::codegen {

auto compact_vector_prefix(CompactVectorKind kind) -> std::string_view {
    switch (kind) {
        case CompactVectorKind::vector2:
            return "Vector2";
        case CompactVectorKind::vector3:
            return "Vector3";
        case CompactVectorKind::rotator:
            return "RotatorSoA";
    }
    std::unreachable();
}

}
