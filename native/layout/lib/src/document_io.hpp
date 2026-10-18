#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_span.h>

#include <string_view>

namespace ioj::layout {
class DocumentState;

namespace document_io {
void append_source(DocumentState& state, lispb::codegen::sexpr::SourceFileId file);
void finish_load(DocumentState& state);
auto sources_span(DocumentState& state,
                  lispb::codegen::sexpr::SourceFileId file,
                  std::string_view target_name) -> lispb::codegen::sexpr::SourceSpan;
}
}
