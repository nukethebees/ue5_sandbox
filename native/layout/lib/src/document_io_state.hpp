#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>

#include <map>
#include <string>
#include <vector>

namespace ioj::layout {
class DocumentIoState {
  public:
    lispb::codegen::sexpr::SourceFileId project_source;
    lispb::codegen::sexpr::SourceSpan project_sources_span;
    std::string saved_project_text;
    std::string registry_text;
    std::vector<std::string> saved_source_order;
    mutable std::map<lispb::codegen::sexpr::SourceFileId, std::string> rendered_sources;
    std::map<std::string, std::string> saved_sources;
    std::map<std::string, std::string> persisted_sources;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::codegen::sexpr::SourceFileId>
        source_origins;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::codegen::sexpr::SourceFileId>
        render_origins;
    std::map<lispb::codegen::sexpr::SourceFileId, std::string> saved_source_paths;
};
}
