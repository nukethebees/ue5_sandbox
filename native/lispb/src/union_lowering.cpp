#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

class UnionLowering : public CppIRBuilder {
  public:
    explicit UnionLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_union(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
        return {.header = {union_node(ctx, schema)}};
    }
  private:
    auto union_node(LoweringContext& ctx, DeclarationHandle schema) -> NodeId {
        auto const& unions{ctx.declarations.unions};
        auto const& schemas{ctx.declarations.child_occurrences};
        auto const row{unions.index(schema)};
        auto alternatives{node_list()};
        for (auto const alternative : ctx.declarations.items(unions.alternatives[row])) {
            auto const alternative_row{schemas.index(alternative)};
            alternatives.add(instance_member(lower_member_type(ctx, alternative),
                                             schemas.name[alternative_row]));
        }
        auto const created_record{add_record({.name = this->name(unions.name[row]),
                                              .kind = RecordKind::union_,
                                              .children = alternatives.build(),
                                              .export_specifier = unions.export_specifier[row]})};
        return created_record;
    }
};

} // namespace

auto lower_union(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return UnionLowering{ctx.ir}.lower_union(ctx, schema);
}

} // namespace lispb::codegen::detail
