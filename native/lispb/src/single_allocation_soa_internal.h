#pragma once

#include "soa_internal.h"

#include <lispb/schema/type_graph.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace codegen::detail {

struct SingleAllocationDialect {
    std::string runtime_namespace;
    std::string vector_namespace;
    std::string size_type;
    std::string byte_size_type;
    std::string alignment_argument_type;
    std::string span_template;
    bool span_count_requires_cast{};
    std::string default_allocate_function;
    std::string default_free_function;
    bool default_free_requires_alignment{};
    std::string column_iteration_function;
    std::string column_application_function;
    bool column_iteration_returns_result{};
    std::vector<TypeDependency> dependencies;

    auto span_count(Expr count) const -> Expr;
};

struct SingleAllocationColumn {
    std::string flattened_identifier;
    std::string layout_identifier;
    CppType type;
    std::vector<std::string> member_path;
};

struct CompactVectorShape {
    std::string element_type;
    std::size_t dimensions{};
};

struct SingleAllocationModel {
    SoaSchema const* schema{};
    std::map<std::string, SoaSchema const*> const* schemas{};
    TypeRegistry const* types{};
    SoaBackend backend{};
    SingleAllocationDialect dialect;
    std::string owner_name;
    std::string layout_name;
    std::string view_name;
    std::string const_view_name;
    std::vector<std::string> member_prefix;
    CppType allocate_function;
    CppType free_function;
    bool free_requires_alignment{};
    std::vector<SingleAllocationColumn> columns;
    std::map<std::string, std::size_t> column_indices;
    std::map<std::string, CompactVectorShape> compact_vectors;
    std::map<std::string, std::string> equivalent_constructors;
    std::vector<TypeDependency> dependencies;
};

auto build_single_allocation_model(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   TypeRegistry const& types,
                                   lispb::schema::TypeGraph const& type_graph,
                                   std::string const& module_name,
                                   SoaBackend backend) -> SingleAllocationModel;

auto column_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> SingleAllocationColumn const&;
auto compact_vector_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> CompactVectorShape const*;

auto emit_single_allocation_layout(SingleAllocationModel const& model) -> Nodes;
auto emit_single_allocation_views(SingleAllocationModel const& model, NodeListBuilder& source)
    -> Nodes;
auto emit_single_allocation_container(SingleAllocationModel const& model, NodeListBuilder& source)
    -> Node;

} // namespace codegen::detail
