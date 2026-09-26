#include "single_allocation_soa_internal.h"

namespace codegen::detail {

auto lower_single_allocation(SoaSchema const& schema,
                             std::map<std::string, SoaSchema const*> const& schemas,
                             TypeRegistry const& types,
                             lispb::schema::TypeGraph const& type_graph,
                             std::string const& module_name,
                             SoaBackend const backend,
                             bool const emit_shared_types) -> LoweredSoa {
    auto model{
        build_single_allocation_model(schema, schemas, types, type_graph, module_name, backend)};
    NodeListBuilder result;
    NodeListBuilder source;

    if (emit_shared_types) {
        result.append(emit_single_allocation_layout(model)).new_lines(2);
    }
    if (emit_shared_types) {
        result.append(emit_single_allocation_views(model, source)).new_lines(1);
    }
    result.add(emit_single_allocation_container(model, source));

    return {result.build(), source.build()};
}

} // namespace codegen::detail
