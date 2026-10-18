#include <lispb/schema/editable_document_state.h>

#include <codegen/schema/manifest.h>

#include <utility>

namespace lispb::schema {

EditableDocumentState::EditableDocumentState(codegen::Manifest manifest)
    : manifest_{std::move(manifest)} {}

} // namespace lispb::schema
