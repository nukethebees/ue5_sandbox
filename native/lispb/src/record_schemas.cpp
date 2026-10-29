#include <codegen/schema/record_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto RecordSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto RecordSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    layout.push_back(LayoutKind::native);
    bits.push_back({});
    invalid_value.push_back({});
    bit_order.push_back({});
    byte_order.push_back({});
    name.push_back({});
    constraint_policy.push_back({});
    generate_visitor.push_back(false);
    s7_tag.push_back({});
    s7_positional.push_back(false);
    s7_constructor.push_back(true);
    s7_reader_includes.push_back({});
    members.push_back({});
    export_specifier.push_back({});
    comparison.push_back(RecordComparison::none);
    comparison_noexcept.push_back(false);
    functions.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::record) << 24) |
                             (index + 1)};
}
auto RecordSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this,
                                    id,
                                    cpp_projection,
                                    layout,
                                    bits,
                                    invalid_value,
                                    bit_order,
                                    byte_order,
                                    name,
                                    constraint_policy,
                                    generate_visitor,
                                    s7_tag,
                                    s7_positional,
                                    s7_constructor,
                                    s7_reader_includes,
                                    members,
                                    export_specifier,
                                    comparison,
                                    comparison_noexcept,
                                    functions);
}
void RecordSchemas::clear() {
    detail::clear_schema_columns(cpp_projection,
                                 layout,
                                 bits,
                                 invalid_value,
                                 bit_order,
                                 byte_order,
                                 name,
                                 constraint_policy,
                                 generate_visitor,
                                 s7_tag,
                                 s7_positional,
                                 s7_constructor,
                                 s7_reader_includes,
                                 members,
                                 export_specifier,
                                 comparison,
                                 comparison_noexcept,
                                 functions);
}

}
