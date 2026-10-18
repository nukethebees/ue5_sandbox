#pragma once

#include <ioj/layout/document_source.hpp>

#include <filesystem>
#include <span>

namespace ioj::layout {
class DocumentState;
namespace document_io {
void initialize_inputs(DocumentState& state,
                       std::span<DocumentSource const> sources,
                       std::string_view registry);
void load_inputs(DocumentState& state, std::filesystem::path const& path, std::string_view target);
void finish_load(DocumentState& state);
}
}
