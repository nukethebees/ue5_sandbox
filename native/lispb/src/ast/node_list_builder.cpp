#include <codegen/ast/node_list_builder.h>

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>

#include <utility>

namespace lispb::codegen {

NodeListBuilder::NodeListBuilder(CppIR& ir)
    : ir_{ir} {}
auto NodeListBuilder::add(NodeId value) -> NodeListBuilder& {
    nodes_.push_back(value);
    return *this;
}
auto NodeListBuilder::add(NodeId value, int trailing_newlines) -> NodeListBuilder& {
    return add(value).new_lines(trailing_newlines);
}
auto NodeListBuilder::append(Nodes const& values) -> NodeListBuilder& {
    nodes_.insert(nodes_.end(), values.begin(), values.end());
    return *this;
}
auto NodeListBuilder::append(std::initializer_list<Nodes> groups) -> NodeListBuilder& {
    for (auto const& values : groups) {
        append(values);
    }
    return *this;
}
auto NodeListBuilder::new_lines(int count) -> NodeListBuilder& {
    return add(ir_.lines(count));
}
auto NodeListBuilder::build() -> Nodes {
    return std::move(nodes_);
}

} // namespace lispb::codegen
