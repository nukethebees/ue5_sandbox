#pragma once

#include <codegen/schema.h>

#include <filesystem>
#include <span>

namespace codegen {

struct RegistrySourceRange {
    std::size_t source_file_index{};
    std::size_t begin_offset{};
    std::size_t end_offset{};
    std::size_t line{1};
    std::size_t column{1};
};

struct RegistryInclude {
    RegistrySourceRange source_range;
    std::filesystem::path target;
};

struct RegistrySourceFile {
    std::filesystem::path path;
    std::string text;
    std::vector<RegistryInclude> includes;
};

struct LoadedTypeRegistry {
    TypeRegistry types;
    std::vector<RegistrySourceFile> sources;
    std::map<std::string, RegistrySourceRange, std::less<>> declarations;
};

struct LoadedModuleSource {
    // Editable source ownership must use the same bytes as semantic parsing.
    std::string text;
    std::vector<ModuleSchema> modules;
};

auto load_type_registry(std::filesystem::path const& root) -> LoadedTypeRegistry;
auto load_module_source(std::filesystem::path const& path) -> LoadedModuleSource;
auto load_sources(LoadedTypeRegistry const& registry,
                  std::span<std::filesystem::path const> module_paths) -> Manifest;

auto load_sources(std::filesystem::path const& types_path,
                  std::span<std::filesystem::path const> module_paths) -> Manifest;

namespace detail {

using ModuleSourceReadHook = void (*)(std::filesystem::path const&);

auto set_module_source_read_hook_for_testing(ModuleSourceReadHook hook) noexcept
    -> ModuleSourceReadHook;

} // namespace detail

} // namespace codegen
