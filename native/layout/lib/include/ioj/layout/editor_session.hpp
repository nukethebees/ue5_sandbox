#pragma once

#include <ioj/layout/document.hpp>
#include <ioj/layout/document_id.hpp>
#include <ioj/layout/document_source.hpp>

#include <lispb/compiler_storage.h>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace ioj::layout {

// Own shared stores for all documents in an editing session. Outlive every document and query.
class EditorSession {
  public:
    auto from_source(std::string_view source, std::string_view registry = {}) -> Document;
    auto from_sources(std::span<DocumentSource const> sources, std::string_view registry = {})
        -> Document;
    auto load(std::filesystem::path const& project_path, std::string_view target) -> Document;

    auto storage() -> lispb::CompilerStorage&;
    auto diagnostics() const -> lispb::Diagnostics const&;
    auto format_diagnostic(lispb::DiagnosticId diagnostic) const -> std::string;
    auto last_diagnostic_begin() const -> std::size_t;
    void begin_operation();
    void record_error(std::string_view message);
  private:
    friend class Document;
    auto allocate_document_id() -> DocumentId;

    lispb::CompilerStorage storage_;
    DocumentId next_document_id_{1};
    std::size_t diagnostic_begin_{};
};

}
