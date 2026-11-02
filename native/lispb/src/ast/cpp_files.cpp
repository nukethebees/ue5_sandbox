#include <codegen/ast/cpp_files.h>

#include <codegen/ast/ir/range_storage.h>

#include <cassert>
#include <cstdint>

namespace lispb::codegen {

auto CppFiles::add(CppFileDefinition const& definition) -> CppFileId {
    auto const result{CppFileId{static_cast<std::uint32_t>(path.size() + 1)}};
    path.push_back(definition.path);
    source.push_back(definition.source);
    auto const options{definition.flags};
    flags.emplace_back(options.pragma_once, options.clang_format_off, options.format_generated);
    nodes_.push_back(save_nodes(definition.nodes));
    prologue_.push_back(save_text(definition.prologue));
    epilogue_.push_back(save_text(definition.epilogue));
    include_order_.push_back(save_text(definition.include_order));
    return result;
}

auto CppFiles::index(CppFileId file) const -> std::size_t {
    assert(file.value != 0 && file.value <= path.size());
    return file.value - 1;
}
auto CppFiles::nodes(CppFileId file) const -> std::span<NodeId const> {
    return ir::range_values(node_references_, nodes_[index(file)]);
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
void CppFiles::set_nodes(CppFileId file, std::span<NodeId const> nodes) {
    nodes_[index(file)] = save_nodes(nodes);
}
auto CppFiles::save_nodes(std::span<NodeId const> nodes) -> ir::Range {
    return ir::save_range(node_references_, nodes);
}
auto CppFiles::save_text(std::span<StringId const> text) -> ir::Range {
    return ir::save_range(text_references_, text);
}
auto CppFiles::text(ir::Range range) const -> std::span<StringId const> {
    return ir::range_values(text_references_, range);
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
