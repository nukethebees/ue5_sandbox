#include "packed_value_internal.h"

namespace codegen::detail {
namespace {

auto qualified_enum_name(EnumModuleSchema const& module, EnumSchema const& schema) -> std::string {
    return module.settings.namespace_name.has_value()
             ? *module.settings.namespace_name + "::" + schema.name
             : schema.name;
}

} // namespace

auto packed_unsigned_width(std::string_view const spelling) -> std::optional<int> {
    static std::map<std::string_view, int> const widths{
        {"uint8", 8},
        {"uint16", 16},
        {"uint32", 32},
        {"uint64", 64},
        {"std::uint8_t", 8},
        {"std::uint16_t", 16},
        {"std::uint32_t", 32},
        {"std::uint64_t", 64},
    };
    auto const found{widths.find(spelling)};
    return found == widths.end() ? std::nullopt : std::optional<int>{found->second};
}

auto find_packed_enum(TypeRef const& type,
                      std::map<std::string, CppType> const& types,
                      std::vector<ModuleSchema> const& modules) -> EnumSchema const* {
    auto const spelling{resolve_type(type, types).spelling};
    for (auto const& candidate : modules) {
        auto const* enum_module{std::get_if<EnumModuleSchema>(&candidate)};
        if (enum_module == nullptr) {
            continue;
        }
        for (auto const& schema : enum_module->enums) {
            if (qualified_enum_name(*enum_module, schema) == spelling) {
                return &schema;
            }
        }
    }
    return nullptr;
}

} // namespace codegen::detail
