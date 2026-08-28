#pragma once

#include <codegen/generator.h>

#include <string_view>

namespace codegen::detail {

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string;
auto join_lines(std::vector<std::string> const& values) -> std::string;
auto source_include(ModuleSettings const& settings) -> std::string;
auto dependency_for_key(std::string const& key,
                        std::map<std::string, CppType> const& types) -> TypeDependency;
auto qualify(CppType type, std::string const& suffix) -> CppType;
auto output_path_key(std::filesystem::path const& path) -> std::string;

} // namespace codegen::detail
