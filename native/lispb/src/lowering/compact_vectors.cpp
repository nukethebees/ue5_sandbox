#include <codegen/lowering/compact_vectors.h>

#include <cassert>
namespace lispb::codegen {
auto CompactVectors::append() -> CompactVectorId {
    CompactVectorId const id{static_cast<std::uint32_t>(element_type.size() + 1)};
    element_type.emplace_back();
    components.emplace_back();
    kind.emplace_back();
    equivalent_type.emplace_back();
    return id;
}
auto CompactVectors::index(CompactVectorId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= element_type.size());
    return id.value - 1;
}
void CompactVectors::clear() {
    element_type.clear();
    components.clear();
    kind.clear();
    equivalent_type.clear();
}
}
