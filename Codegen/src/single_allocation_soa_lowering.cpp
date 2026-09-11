#include "single_allocation_soa_internal.h"

namespace codegen::detail {

auto lower_single_allocation_nodes(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   std::map<std::string, CppType> const& types,
                                   bool const native) -> Nodes {
    auto model{build_single_allocation_model(schema, schemas, types, native)};
    NodeListBuilder result;

    if (model.emit_shared_types) {
        result.append(emit_single_allocation_layout(model)).new_lines(2);
    }
    result.add(emit_single_allocation_storage(model));
    if (model.emit_shared_types) {
        result.new_lines(2).append(emit_single_allocation_views(model)).new_lines(1);
    } else {
        result.new_lines(2);
    }
    result.add(emit_single_allocation_container(model));

    return result.build();
}

} // namespace codegen::detail
