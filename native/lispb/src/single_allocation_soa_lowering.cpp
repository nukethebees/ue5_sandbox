#include "single_allocation_soa_internal.h"

namespace codegen::detail {

auto lower_single_allocation_nodes(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   std::map<std::string, CppType> const& types,
                                   lispb::schema::TypeGraph const& type_graph,
                                   std::string const& module_name,
                                   SoaBackend const backend) -> Nodes {
    auto model{
        build_single_allocation_model(schema, schemas, types, type_graph, module_name, backend)};
    NodeListBuilder result;

    if (model.emit_shared_types) {
        result.append(emit_single_allocation_layout(model)).new_lines(2);
    }
    if (model.emit_shared_types) {
        result.append(emit_single_allocation_views(model)).new_lines(1);
    }
    result.add(emit_single_allocation_container(model));

    return result.build();
}

} // namespace codegen::detail
