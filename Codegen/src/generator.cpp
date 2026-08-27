#include <codegen/generator.h>

#include "lowering.h"
#include "validation.h"

#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace codegen {
namespace {

auto lower_umbrella(UmbrellaModuleSchema const& module) -> Module {
    NodeListBuilder nodes;
    for (auto const& header : module.headers) {
        nodes.add(Include{header, false});
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
}

} // namespace

auto lower_modules(Manifest const& manifest) -> std::vector<Module> {
    detail::validate_manifest(manifest);
    std::vector<Module> result;
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    result.push_back(detail::lower_soa_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    result.push_back(detail::lower_facade_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    result.push_back(detail::lower_homogeneous_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    result.push_back(detail::lower_vector_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, UmbrellaModuleSchema>) {
                    result.push_back(lower_umbrella(module));
                }
            },
            schema);
    }
    return result;
}

auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile> {
    std::vector<GeneratedFile> result;
    std::set<std::filesystem::path> paths;
    for (auto const& module : modules) {
        for (auto const* file : {module.header ? &*module.header : nullptr,
                                module.source ? &*module.source : nullptr}) {
            if (file == nullptr) {
                continue;
            }
            auto const normalized{file->path.lexically_normal()};
            if (!paths.insert(normalized).second) {
                throw std::invalid_argument{"Duplicate generated output path: " +
                                            normalized.string()};
            }
            result.push_back(GeneratedFile{normalized, render(*file)});
        }
    }
    return result;
}

auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int {
    bool stale{false};
    for (auto const& file : files) {
        auto relative{file.path};
        if (relative.is_absolute()) {
            relative = std::filesystem::relative(relative, project_root);
        }
        auto const destination{output_root / relative};
        std::string current;
        if (std::ifstream input{destination, std::ios::binary}; input) {
            current.assign(std::istreambuf_iterator<char>{input},
                           std::istreambuf_iterator<char>{});
        }
        if (current == file.content) {
            std::cout << "Unchanged " << relative.generic_string() << '\n';
            continue;
        }
        if (check_only) {
            stale = true;
            std::cout << "Stale " << relative.generic_string() << '\n';
            continue;
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output{destination, std::ios::binary | std::ios::trunc};
        if (!output) {
            throw std::runtime_error{"Cannot write generated file: " + destination.string()};
        }
        output << file.content;
        std::cout << "Wrote " << relative.generic_string() << '\n';
    }
    if (stale) {
        std::cout << "Generated files are stale. Run the generate-code CMake workflow.\n";
        return 1;
    }
    return 0;
}

} // namespace codegen
