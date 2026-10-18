#include <codegen/schema/semantic_relation_unit.h>

#include <string_view>

namespace lispb::codegen {

auto semantic_relation_unit_name(SemanticRelationUnit const unit) -> std::string_view {
    switch (unit) {
        case SemanticRelationUnit::elements:
            return "elements";
        case SemanticRelationUnit::bytes:
            return "bytes";
    }
    return "elements";
}

} // namespace lispb::codegen
