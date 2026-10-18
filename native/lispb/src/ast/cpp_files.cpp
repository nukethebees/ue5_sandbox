#include <codegen/ast/cpp_files.h>

#include <cassert>
#include <cstdint>

namespace lispb::codegen {

auto CppFiles::append(StringId file_path, std::span<NodeId const> file_nodes) -> CppFileId {
    auto const result{CppFileId{static_cast<std::uint32_t>(path.size() + 1)}};
    path.push_back(file_path);
    source.emplace_back();
    flags.emplace_back();
    flags.back().set_pragma_once(true);
    nodes_.push_back(save_nodes(file_nodes));
    prologue_.emplace_back();
    epilogue_.emplace_back();
    include_order_.emplace_back();
    return result;
}
auto CppFiles::index(CppFileId file) const -> std::size_t {
    assert(file.value != 0 && file.value <= path.size());
    return file.value - 1;
}
auto CppFiles::nodes(CppFileId file) const -> std::span<NodeId const> {
    auto const range{nodes_[index(file)]};
    return std::span<NodeId const>{node_references_}.subspan(range.begin, range.count);
}
auto CppFiles::prologue(CppFileId file) const -> std::span<StringId const> {
    return text(prologue_[index(file)]);
}
auto CppFiles::epilogue(CppFileId file) const -> std::span<StringId const> {
    return text(epilogue_[index(file)]);
}
auto CppFiles::include_order(CppFileId file) const -> std::span<StringId const> {
    return text(include_order_[index(file)]);
}
void CppFiles::set_nodes(CppFileId file, std::span<NodeId const> values) {
    nodes_[index(file)] = save_nodes(values);
}
void CppFiles::set_prologue(CppFileId file, std::span<StringId const> values) {
    prologue_[index(file)] = save_text(values);
}
void CppFiles::set_epilogue(CppFileId file, std::span<StringId const> values) {
    epilogue_[index(file)] = save_text(values);
}
void CppFiles::set_include_order(CppFileId file, std::span<StringId const> values) {
    include_order_[index(file)] = save_text(values);
}
auto CppFiles::save_nodes(std::span<NodeId const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(node_references_.size()),
                          static_cast<std::uint32_t>(values.size())};
    node_references_.insert(node_references_.end(), values.begin(), values.end());
    return range;
}
auto CppFiles::save_text(std::span<StringId const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(text_references_.size()),
                          static_cast<std::uint32_t>(values.size())};
    text_references_.insert(text_references_.end(), values.begin(), values.end());
    return range;
}
auto CppFiles::text(ir::Range range) const -> std::span<StringId const> {
    return std::span<StringId const>{text_references_}.subspan(range.begin, range.count);
}
void CppFiles::clear() {
    path.clear();
    source.clear();
    flags.clear();
    nodes_.clear();
    prologue_.clear();
    epilogue_.clear();
    include_order_.clear();
    node_references_.clear();
    text_references_.clear();
}

}
