#include "fixed_soa_internal.h"

namespace codegen::detail {
namespace {

auto fixed_nodes(FixedLayout const& layout,
                 std::map<std::string, CppType> const& types) -> Nodes {
    NodeListBuilder result;
    result.add(fixed_storage_node(layout));
    for (auto const& container : layout.schema->fixed->containers) {
        result.new_lines(2).add(fixed_container_node(layout, container, types));
    }
    return result.build();
}

} // namespace

auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       std::map<std::string, CppType> const& types) -> Nodes {
    return fixed_nodes(build_fixed_layout(schema, schemas, types), types);
}

} // namespace codegen::detail
