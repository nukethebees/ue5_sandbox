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
    auto append(LoweringPathId parent, StringId name) -> LoweringPathId;
    auto items(LoweringPathId path) const -> std::span<StringId const>;
    void clear();
  private:
    std::vector<SchemaRange<StringId>> ranges_;
    std::vector<StringId> names_;
    std::map<std::pair<LoweringPathId, StringId>, LoweringPathId> paths_;
};
}
