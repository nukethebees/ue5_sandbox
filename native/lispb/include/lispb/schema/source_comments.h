#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/schema/source_comment_placement.h>

#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
struct CompilerStorage;
namespace schema {

// Copy this small provenance table with an editing draft. The syntax and source stores
// retain original forms; replacements inherit their origins, including child renames.
class SourceComments {
  public:
    void capture(CompilerStorage const& storage,
                 std::span<codegen::ModuleHandle const> modules,
                 codegen::sexpr::SourceFileId file = {});
    void inherit(codegen::DeclarationHandle before, codegen::DeclarationHandle after);
    void inherit(codegen::ModuleHandle before, codegen::ModuleHandle after);
    void rename_child(codegen::DeclarationHandle declaration,
                      std::string_view head,
                      std::string_view before,
                      std::string_view after);
    void remove_child(codegen::DeclarationHandle declaration,
                      std::string_view head,
                      std::string_view name);

    [[nodiscard]] auto apply(CompilerStorage const& storage,
                             std::span<codegen::ModuleHandle const> modules,
                             codegen::sexpr::SourceFileId file,
                             std::string canonical) const -> std::string;
  private:
    std::map<codegen::ModuleHandle, codegen::sexpr::FormId> modules_;
    std::map<codegen::DeclarationHandle, codegen::sexpr::FormId> declarations_;
    std::set<codegen::DeclarationHandle> authored_comments_;
    std::map<codegen::DeclarationHandle, std::map<std::string, std::string>> renamed_children_;
    std::set<codegen::sexpr::SourceFileId> captured_files_;

    std::vector<codegen::sexpr::FormId> owners_;
    std::vector<codegen::sexpr::FormId> property_parents_;
    std::vector<SourceCommentPlacement> placements_;
    std::vector<codegen::sexpr::SourceSpan> spans_;
};
}
}
