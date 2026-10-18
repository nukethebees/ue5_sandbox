#pragma once

#include <ioj/layout/revision.hpp>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <map>
#include <vector>

namespace ioj::layout {
class DocumentIoState {
  public:
    lispb::codegen::sexpr::SourceFileId project_source;
    lispb::codegen::sexpr::SourceSpan project_sources_span;
    lispb::StringId saved_project_text;
    lispb::StringId registry_text;
    std::vector<lispb::IdentifierId> saved_source_order;
    mutable Revision rendered_revision{invalid_revision};
    mutable std::map<lispb::codegen::sexpr::SourceFileId, lispb::StringId> rendered_sources;
    std::map<lispb::IdentifierId, lispb::StringId> saved_sources;
    std::map<lispb::IdentifierId, lispb::StringId> persisted_sources;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::codegen::sexpr::SourceFileId>
        source_origins;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::codegen::sexpr::SourceFileId>
        render_origins;
    std::map<lispb::codegen::sexpr::SourceFileId, lispb::IdentifierId> saved_source_paths;
};
}
