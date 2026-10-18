#include <material_gen/NamedNodes.h>

namespace material_synth {
auto NamedNodes::append(lispb::StringId name, NodeHandle node, SourceSpan span) -> NamedNodeId {
    auto const id{NamedNodeId{static_cast<std::uint32_t>(size())}};
    this->name.push_back(name);
    this->node.push_back(node);
    this->span.push_back(span);
    return id;
}
}
