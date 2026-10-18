#pragma once
#include <codegen/loaded_module_source.h>
#include <codegen/loaded_type_registry.h>
#include <codegen/schema/manifest.h>
#include <codegen/sexpr/indices.h>

#include <filesystem>
#include <span>

namespace lispb {
struct CompilerStorage;
}

namespace lispb::codegen {

auto load_type_registry(CompilerStorage& storage, std::filesystem::path const& root)
    -> LoadedTypeRegistry;
// Parse an existing source without reading its root path; includes resolve beside that path.
auto parse_type_registry(CompilerStorage& storage, sexpr::SourceFileId root) -> LoadedTypeRegistry;
auto parse_module_source(CompilerStorage& storage, sexpr::SourceFileId file) -> LoadedModuleSource;

auto load_module_source(CompilerStorage& storage, std::filesystem::path const& path)
    -> LoadedModuleSource;
auto load_sources(CompilerStorage& storage,
                  LoadedTypeRegistry const& registry,
                  std::span<std::filesystem::path const> module_paths) -> Manifest;

auto load_sources(CompilerStorage& storage,
                  std::filesystem::path const& types_path,
                  std::span<std::filesystem::path const> module_paths) -> Manifest;

namespace detail {

using ModuleSourceReadHook = void (*)(std::filesystem::path const&);

auto set_module_source_read_hook_for_testing(ModuleSourceReadHook hook) noexcept
    -> ModuleSourceReadHook;

} // namespace detail

} // namespace lispb::codegen
