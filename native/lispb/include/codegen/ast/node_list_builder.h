#pragma once

#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>

namespace lispb::codegen {

class CppIR;

class NodeListBuilder {
  public:
    explicit NodeListBuilder(CppIR& ir);
    auto add(NodeId value) -> NodeListBuilder&;
    auto add(NodeId value, int trailing_newlines) -> NodeListBuilder&;
    auto append(Nodes values) -> NodeListBuilder&;
    auto new_lines(int count = 1) -> NodeListBuilder&;
    auto build() -> Nodes;
  private:
    CppIR& ir_;
    Nodes nodes_;
};

} // namespace lispb::codegen
