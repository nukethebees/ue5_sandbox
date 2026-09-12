#pragma once

#include "fixed_soa_internal.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace codegen::detail {

struct SingleAllocationLayoutPolicy {
    std::size_t capacity_granularity{64};
    std::size_t column_gap{192};
    std::size_t minimum_alignment{64};
};

struct SingleAllocationDialect {
    std::string runtime_namespace;
    std::string vector_namespace;
    std::string size_type;
    std::string byte_size_type;
    std::string alignment_argument_type;
    std::string copy_function;
    std::string copy_header;
    std::string span_template;
    bool span_count_requires_cast{};
    std::string source_data_member;
    std::string default_construct_prefix;
    std::string default_construct_type_suffix;
    std::string default_construct_header;
    std::string default_allocate_function;
    std::string default_free_function;
    bool default_free_requires_alignment{};
    std::string column_iteration_function;
    std::string column_application_function;
    bool column_iteration_returns_result{};
    std::vector<TypeDependency> dependencies;

    auto span_type(std::string const& element_type, bool is_const) const -> std::string;
    auto span_count(Expr count) const -> Expr;
};

struct SingleAllocationColumn {
    std::string flattened_identifier;
    std::string layout_identifier;
    CppType type;
    std::vector<std::string> member_path;
    std::string byte_count_identifier;
};

struct SingleAllocationUniqueType {
    CppType type;
    std::vector<std::string> first_member_path;
    std::string byte_count_identifier;
};

struct CompactVectorShape {
    std::string element_type;
    std::size_t dimensions{};
};

struct SingleAllocationModel {
    SoaSchema const* schema{};
    std::map<std::string, SoaSchema const*> const* schemas{};
    SingleAllocationDialect dialect;
    SingleAllocationLayoutPolicy layout_policy;
    std::string owner_name;
    std::string storage_name;
    std::string layout_name;
    std::string view_name;
    std::string const_view_name;
    CppType allocate_function;
    CppType free_function;
    bool free_requires_alignment{};
    bool emit_shared_types{};
    std::vector<SingleAllocationColumn> columns;
    std::vector<SingleAllocationUniqueType> unique_types;
    std::map<std::string, std::size_t> column_indices;
    std::map<std::string, CompactVectorShape> compact_vectors;
    std::vector<TypeDependency> dependencies;
};

auto build_single_allocation_model(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   std::map<std::string, CppType> const& types,
                                   bool native) -> SingleAllocationModel;

auto column_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> SingleAllocationColumn const&;
auto compact_vector_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> CompactVectorShape const*;

auto emit_single_allocation_layout(SingleAllocationModel const& model) -> Nodes;
auto emit_single_allocation_storage(SingleAllocationModel const& model) -> Node;
auto emit_single_allocation_views(SingleAllocationModel const& model) -> Nodes;
auto emit_single_allocation_container(SingleAllocationModel const& model) -> Node;

} // namespace codegen::detail
