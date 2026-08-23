from Codegen.cpp import NewLines, Node
from Codegen.soa.fixed_container import fixed_array_struct
from Codegen.soa.fixed_layout import build_fixed_layout
from Codegen.soa.fixed_storage import fixed_storage_projection
from Codegen.soa.model import SoAStruct


def fixed_soa_nodes(soa: SoAStruct) -> tuple[Node, ...]:
    if soa.fixed is None:
        return ()
    layout = build_fixed_layout(soa)
    nodes: list[Node] = [fixed_storage_projection(layout)]
    for declaration in layout.containers:
        nodes.extend((NewLines(2), fixed_array_struct(layout, declaration)))
    return tuple(nodes)
