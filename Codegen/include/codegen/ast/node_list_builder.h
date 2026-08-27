#pragma once

#include <codegen/ast/node.h>

namespace codegen {

class NodeListBuilder {
public:
    auto add(Node node) -> NodeListBuilder&;
    auto add(Node node, int trailing_new_lines) -> NodeListBuilder&;
    auto append(Nodes nodes) -> NodeListBuilder&;
    auto new_lines(int count = 1) -> NodeListBuilder&;
    auto build() -> Nodes;

private:
    Nodes nodes_;
};

} // namespace codegen
