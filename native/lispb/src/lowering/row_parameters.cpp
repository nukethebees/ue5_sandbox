#include <codegen/lowering/row_parameters.h>

#include <cassert>
namespace lispb::codegen {
auto RowParameters::append() -> RowParameterId {
    RowParameterId const id{static_cast<std::uint32_t>(type.size() + 1)};
    type.emplace_back();
    name.emplace_back();
    column.emplace_back();
    nested.emplace_back();
    return id;
}
auto RowParameters::index(RowParameterId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= type.size());
    return id.value - 1;
}
void RowParameters::clear() {
    type.clear();
    name.clear();
    column.clear();
    nested.clear();
}
}
