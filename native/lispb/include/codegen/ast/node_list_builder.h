#pragma once

#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>

#include <initializer_list>
#include <span>

namespace lispb::codegen {

class CppIR;

class NodeListBuilder {
  public:
    explicit NodeListBuilder(CppIR& ir);
    auto add(NodeId value) -> NodeListBuilder&;
    auto add(NodeId value, int trailing_newlines) -> NodeListBuilder&;
    // Consume ranges immediately. Braced groups preserve construction order and borrow their nodes.
    auto append(std::span<NodeId const> values) -> NodeListBuilder&;
    auto append(std::initializer_list<std::span<NodeId const>> groups) -> NodeListBuilder&;
    auto new_lines(int count = 1) -> NodeListBuilder&;
    auto build() -> Nodes;
  private:
    CppIR& ir_;
    Nodes nodes_;
};

} // namespace lispb::codegen
