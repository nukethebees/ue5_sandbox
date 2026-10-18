#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <map>
#include <span>
#include <utility>
#include <vector>
namespace lispb::codegen {
class LoweringPaths {
  public:
    auto append(LoweringPathId parent, IdentifierId name) -> LoweringPathId;
    auto items(LoweringPathId path) const -> std::span<IdentifierId const>;
    void clear();
  private:
    std::vector<SchemaRange<IdentifierId>> ranges_;
    std::vector<IdentifierId> names_;
    std::map<std::pair<LoweringPathId, IdentifierId>, LoweringPathId> paths_;
};
}
