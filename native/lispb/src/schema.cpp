#include <codegen/schema.h>

#include "schema_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen {

auto canonical_module(ModuleSchema parsed) -> ModuleSchema {
    return std::visit(
        [](auto&& old) -> ModuleSchema {
            using T = std::decay_t<decltype(old)>;
            if constexpr (std::is_same_v<T, NormalModuleSchema> ||
                          std::is_same_v<T, SettingsModuleSchema> ||
                          std::is_same_v<T, UmbrellaModuleSchema>) {
                return std::move(old);
            } else {
                NormalModuleSchema module{.settings = std::move(old.settings)};
                auto append = [&](auto& values) {
                    for (auto& value : values) {
                        module.declarations.emplace_back(std::move(value));
                    }
                };
                if constexpr (std::is_same_v<T, EnumModuleSchema>) {
                    module.enum_helper_namespace = std::move(old.helper_namespace);
                    append(old.enums);
                } else if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    module.soa_backend = old.backend;
                    module.soa_array_allocators = std::move(old.array_allocators);
                    append(old.structs);
                } else if constexpr (std::is_same_v<T, ScalarModuleSchema>) {
                    append(old.scalars);
                } else if constexpr (std::is_same_v<T, RepresentationModuleSchema>) {
                    append(old.linear_quantized);
                    append(old.integer_varints);
                    append(old.fixed_points);
                    append(old.optional_sentinels);
                    append(old.optional_presence_bits);
                    append(old.mini_floats);
                } else if constexpr (std::is_same_v<T, PackedValueModuleSchema>) {
                    append(old.values);
                } else if constexpr (std::is_same_v<T, RecordModuleSchema>) {
                    append(old.records);
                } else if constexpr (std::is_same_v<T, UnionModuleSchema>) {
                    append(old.unions);
                    append(old.tagged_unions);
                } else if constexpr (std::is_same_v<T, StaticTableModuleSchema>) {
                    append(old.tables);
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    append(old.layouts);
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    module.declarations.emplace_back(std::move(old.facade));
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    module.soa_backend = old.backend;
                    module.declarations.emplace_back(VectorSoaSchema{
                        .name = std::move(old.storage_name),
                        .value_type = std::move(old.value_type),
                        .components = std::move(old.components),
                        .equivalent_members = std::move(old.equivalent_members),
                        .equivalent_constructor = std::move(old.equivalent_constructor),
                        .equivalent_type = std::move(old.equivalent_type),
                        .export_specifier = std::move(old.export_specifier),
                        .fixed = std::move(old.fixed)});
                }
                return module;
            }
        },
        std::move(parsed));
}

auto canonical_manifest(Manifest const& manifest) -> Manifest {
    auto result{manifest};
    for (auto& module : result.modules) {
        module = canonical_module(std::move(module));
    }
    return result;
}

auto resolve_type(TypeRef const& reference, std::map<std::string, CppType> const& types)
    -> CppType {
    CppType result;
    if (reference.name.starts_with('@')) {
        auto const key{reference.name.substr(1)};
        auto const found{types.find(key)};
        if (found == types.end()) {
            throw std::invalid_argument{"Unknown C++ type reference: " + reference.name};
        }
        result = found->second;
    } else {
        result = CppType{reference.name};
    }
    if (reference.nested.has_value()) {
        result.spelling += "::" + *reference.nested;
    }
    result.spelling += reference.suffix;
    return result;
}

auto native_spelling(std::string const& spelling) -> std::string {
    constexpr std::array integer_types{
        std::pair{"int8", "std::int8_t"},
        std::pair{"uint8", "std::uint8_t"},
        std::pair{"int16", "std::int16_t"},
        std::pair{"uint16", "std::uint16_t"},
        std::pair{"int32", "std::int32_t"},
        std::pair{"uint32", "std::uint32_t"},
        std::pair{"int64", "std::int64_t"},
        std::pair{"uint64", "std::uint64_t"},
    };
    for (auto const& [source, destination] : integer_types) {
        if (spelling == source) {
            return destination;
        }
    }
    return spelling;
}

auto all_storage_operations() -> std::vector<StorageOperation> {
    return {
        StorageOperation::reset,
        StorageOperation::reserve,
        StorageOperation::add_uninitialised,
        StorageOperation::add_defaulted,
        StorageOperation::remove_at_swap,
        StorageOperation::set_num,
        StorageOperation::copy_element,
        StorageOperation::append_from,
    };
}

} // namespace codegen

namespace codegen::detail {

auto output_path_key(std::filesystem::path const& path) -> std::string {
    auto result{path.lexically_normal().generic_string()};
#ifdef _WIN32
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return result;
}

} // namespace codegen::detail
