#include <codegen/lowering/lowering_paths.h>

#include <cassert>
#include <cstdint>
namespace lispb::codegen {
auto LoweringPaths::append(LoweringPathId parent, StringId name) -> LoweringPathId {
    auto const key{std::pair{parent, name}};
    if (auto const found{paths_.find(key)}; found != paths_.end()) {
        return found->second;
    }
    auto const begin{static_cast<std::uint32_t>(names_.size())};
    auto const parent_items{items(parent)};
    auto const count{static_cast<std::uint32_t>(parent_items.size())};
    names_.reserve(names_.size() + count + 1);
    auto const previous{items(parent)};
    for (std::uint32_t index{}; index < count; ++index) {
        names_.push_back(previous[index]);
    }
    names_.push_back(name);
    ranges_.push_back({begin, count + 1});
    LoweringPathId const result{static_cast<std::uint32_t>(ranges_.size())};
    paths_.emplace(key, result);
    return result;
}
auto LoweringPaths::items(LoweringPathId path) const -> std::span<StringId const> {
    if (path.value == 0) {
        return {};
    }
    assert(path.value <= ranges_.size());
    auto const range{ranges_[path.value - 1]};
    return std::span{names_}.subspan(range.offset, range.count);
}
void LoweringPaths::clear() {
    ranges_.clear();
    names_.clear();
    paths_.clear();
}
}
