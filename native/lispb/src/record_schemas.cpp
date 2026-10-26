#include <codegen/schema/record_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    cpp_projection[destination] = cpp_projection[source_index];
    layout[destination] = layout[source_index];
    bits[destination] = bits[source_index];
    invalid_value[destination] = invalid_value[source_index];
    bit_order[destination] = bit_order[source_index];
    byte_order[destination] = byte_order[source_index];
    name[destination] = name[source_index];
    constraint_policy[destination] = constraint_policy[source_index];
    generate_visitor[destination] = generate_visitor[source_index];
    s7_tag[destination] = s7_tag[source_index];
    s7_positional[destination] = s7_positional[source_index];
    s7_constructor[destination] = s7_constructor[source_index];
    s7_reader_includes[destination] = s7_reader_includes[source_index];
    members[destination] = members[source_index];
    export_specifier[destination] = export_specifier[source_index];
    comparison[destination] = comparison[source_index];
    comparison_noexcept[destination] = comparison_noexcept[source_index];
    functions[destination] = functions[source_index];
    return result;
}
void RecordSchemas::clear() {
    cpp_projection.clear();
    layout.clear();
    bits.clear();
    invalid_value.clear();
    bit_order.clear();
    byte_order.clear();
    name.clear();
    constraint_policy.clear();
    generate_visitor.clear();
    s7_tag.clear();
    s7_positional.clear();
    s7_constructor.clear();
    s7_reader_includes.clear();
    members.clear();
    export_specifier.clear();
    comparison.clear();
    comparison_noexcept.clear();
    functions.clear();
}

}
