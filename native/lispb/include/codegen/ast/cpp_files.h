#pragma once

#include <codegen/ast/cpp_file_definition.h>
#include <codegen/ast/cpp_file_flags.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/node_id.h>
#include <lispb/indices.h>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace lispb::codegen {

class CppFiles {
  public:
    auto add(CppFileDefinition const& definition) -> CppFileId;
    // Require a file belonging to this store. Zero denotes an absent file.
    auto index(CppFileId file) const -> std::size_t;
    auto nodes(CppFileId file) const -> std::span<NodeId const>;
    auto prologue(CppFileId file) const -> std::span<StringId const>;
    auto epilogue(CppFileId file) const -> std::span<StringId const>;
    auto include_order(CppFileId file) const -> std::span<StringId const>;
    // Require input spans from outside this store; mutations invalidate returned spans.
    void set_nodes(CppFileId file, std::span<NodeId const> nodes);
    void set_nodes(CppFileId file, std::initializer_list<NodeId> nodes) {
        set_nodes(file, std::span<NodeId const>{nodes});
    }
    void clear();

    std::vector<StringId> path;
    std::vector<StringId> source;
    std::vector<CppFileFlags> flags;
  private:
    auto save_nodes(std::span<NodeId const> nodes) -> ir::Range;
    auto save_text(std::span<StringId const> text) -> ir::Range;
    auto text(ir::Range range) const -> std::span<StringId const>;
    std::vector<ir::Range> nodes_;
    std::vector<ir::Range> prologue_;
    std::vector<ir::Range> epilogue_;
    std::vector<ir::Range> include_order_;
    std::vector<NodeId> node_references_;
    std::vector<StringId> text_references_;
};

}
