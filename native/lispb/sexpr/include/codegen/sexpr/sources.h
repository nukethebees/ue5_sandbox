#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_file_kind.h>
#include <codegen/sexpr/source_file_state.h>
#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace lispb {
class Strings;
}

namespace lispb::codegen::sexpr {

class Sources {
  public:
    explicit Sources(Strings& strings);

    [[nodiscard]] auto add(std::string_view path, std::string_view text) -> SourceFileId;
    [[nodiscard]] auto load(std::filesystem::path const& path, std::string_view display_path = {})
        -> SourceFileId;
    [[nodiscard]] auto overlay(std::filesystem::path const& path, std::string_view text)
        -> SourceFileId;
    [[nodiscard]] auto path(SourceFileId file) const -> std::string_view;
    [[nodiscard]] auto text(SourceFileId file) const -> std::string_view;
    [[nodiscard]] auto path_id(SourceFileId file) const -> StringId;
    [[nodiscard]] auto text_id(SourceFileId file) const -> StringId;
    [[nodiscard]] auto filesystem_path(SourceFileId file) const -> std::filesystem::path;
    [[nodiscard]] auto kind(SourceFileId file) const -> SourceFileKind;
    void set_kind(SourceFileId file, SourceFileKind kind);
    void set_includes(SourceFileId file,
                      std::span<SourceFileId const> targets,
                      std::span<SourceSpan const> spans);
    [[nodiscard]] auto include_targets(SourceFileId file) const -> std::span<SourceFileId const>;
    [[nodiscard]] auto include_spans(SourceFileId file) const -> std::span<SourceSpan const>;
    [[nodiscard]] auto span(SourceFileId file, std::uint32_t begin, std::uint32_t end) const
        -> SourceSpan;
    [[nodiscard]] auto location(SourceSpan span) const -> SourceLocation;
    [[nodiscard]] auto add_expansion(IdentifierId name,
                                     SourceSpan invocation,
                                     SourceSpan definition) -> ExpansionId;
    [[nodiscard]] auto expansion_text(SourceSpan span) const -> std::string;
    [[nodiscard]] auto strings() const -> Strings& { return *strings_; }

    void set_path(SourceFileId file, std::string_view path);
    // Recheck disk-backed files on their next load; preserve unsaved document overlays.
    void refresh();
    void clear_expansions();
    void rebind(Strings& strings) { strings_ = &strings; }
  private:
    Strings* strings_;
    std::vector<StringId> paths_{{}};
    std::vector<StringId> texts_{{}};
    std::vector<HandleRange> lines_{{}};
    std::vector<SourceFileKind> kinds_{{}};
    std::vector<SourceFileState> states_{{}};
    std::vector<HandleRange> includes_{{}};
    std::vector<SourceFileId> include_targets_;
    std::vector<SourceSpan> include_spans_;
    std::vector<std::uint32_t> line_offsets_;
    std::vector<IdentifierId> expansion_names_{{}};
    std::vector<SourceSpan> invocations_{{}};
    std::vector<SourceSpan> definitions_{{}};

    std::unordered_map<IdentifierId, SourceFileId> loaded_files_;
};

}
