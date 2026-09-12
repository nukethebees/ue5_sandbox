#include <codegen/generator.h>

#include "lowering.h"
#include "lowering_utils.h"
#include "validation.h"

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
    if (!module.settings.prelude_lines.empty()) {
        nodes.new_lines(2).add(raw(detail::join_lines(module.settings.prelude_lines)));
    }
    return Module{
        .name = module.settings.name,
        .header =
            CppFile{
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
                if constexpr (std::is_same_v<T, EnumModuleSchema>) {
                    result.push_back(detail::lower_enum_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    result.push_back(detail::lower_soa_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, StaticTableModuleSchema>) {
                    result.push_back(detail::lower_static_table_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    result.push_back(detail::lower_facade_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, SettingsModuleSchema>) {
                    result.push_back(detail::lower_settings_module(module, manifest.types));
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
    std::set<std::string> paths;
    for (auto const& module : modules) {
        for (auto const* file : {module.header ? &*module.header : nullptr,
                                 module.source ? &*module.source : nullptr}) {
            if (file == nullptr) {
                continue;
            }
            auto const normalized{file->path.lexically_normal()};
            if (!paths.insert(detail::output_path_key(normalized)).second) {
                throw std::invalid_argument{"Duplicate generated output path: " +
                                            normalized.string()};
            }
            result.push_back(GeneratedFile{normalized, render(*file), file->format_generated});
        }
    }
    return result;
}

} // namespace codegen
