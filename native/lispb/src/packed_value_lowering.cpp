#include "lowering.h"
#include "lowering_utils.h"
#include "packed_value_internal.h"

#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto hex_value(std::uint64_t const value) -> std::string {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

auto dependency_for_integer(CppType const& type) -> std::optional<TypeDependency> {
    if (!type.dependencies.empty()) {
        return std::nullopt;
    }
    if (type.spelling.starts_with("std::uint")) {
        return TypeDependency{type.spelling, "cstdint", {}};
    }
    if (type.spelling.starts_with("uint")) {
        return TypeDependency{type.spelling, "CoreTypes.h", {}};
    }
    return std::nullopt;
}

auto packed_value_text(PackedValueSchema const& schema,
                       std::map<std::string, CppType> const& types,
                       std::vector<ModuleSchema> const& modules) -> Raw {
    auto const storage{resolve_type(schema.storage_type, types)};
    auto const storage_bits{*packed_unsigned_width(storage.spelling)};
    auto const all_bits{storage_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                           : (std::uint64_t{1} << storage_bits) - 1};

    std::vector<TypeDependency> dependencies{
        TypeDependency{"assert", "cassert", {}},
        TypeDependency{"std::strong_ordering", "compare", {}},
        TypeDependency{"std::numeric_limits", "limits", {}},
        TypeDependency{"std::is_enum_v", "type_traits", {}},
    };
    dependencies.insert(
        dependencies.end(), storage.dependencies.begin(), storage.dependencies.end());
    if (auto dependency{dependency_for_integer(storage)}) {
        dependencies.push_back(std::move(*dependency));
    }

    auto output{std::string{"struct "} + schema.export_specifier.value_or("")};
    if (schema.export_specifier.has_value()) {
        output += " ";
    }
    output += schema.name + " {\n";
    output += "    using storage_type = " + storage.spelling + ";\n";
    output += "    static_assert(std::is_unsigned_v<storage_type>);\n";
    output += "    static_assert(std::numeric_limits<storage_type>::digits == " +
              std::to_string(storage_bits) + ");\n";

    int offset{};
    for (auto const& field : schema.fields) {
        auto const field_type{resolve_type(field.type, types)};
        dependencies.insert(
            dependencies.end(), field_type.dependencies.begin(), field_type.dependencies.end());
        if (auto dependency{dependency_for_integer(field_type)}) {
            dependencies.push_back(std::move(*dependency));
        }
        if (field.kind == PackedFieldKind::enumeration) {
            output += "    using " + field.name + "_underlying_type = std::underlying_type_t<" +
                      field_type.spelling + ">;\n";
            output += "    static_assert(std::is_enum_v<" + field_type.spelling + ">);\n";
            output +=
                "    static_assert(std::is_unsigned_v<" + field.name + "_underlying_type>);\n";
            output += "    static_assert(std::numeric_limits<" + field.name +
                      "_underlying_type>::digits >= " + std::to_string(field.bits) + ");\n";
        }

        auto const value_mask{field.bits == 64 ? all_bits : (std::uint64_t{1} << field.bits) - 1};
        auto const mask{static_cast<std::uint64_t>(value_mask << offset) & all_bits};
        output += "\n    inline static constexpr int " + field.name + "_offset{" +
                  std::to_string(offset) + "};\n";
        output += "    inline static constexpr int " + field.name + "_bits{" +
                  std::to_string(field.bits) + "};\n";
        output += "    inline static constexpr storage_type " + field.name +
                  "_value_mask{storage_type{" + hex_value(value_mask) + "}};\n";
        output += "    inline static constexpr storage_type " + field.name + "_mask{storage_type{" +
                  hex_value(mask) + "}};\n";

        if (field.kind == PackedFieldKind::enumeration) {
            if (auto const* enum_schema{find_packed_enum(field.type, types, modules)}) {
                for (auto const& enumerator : enum_schema->values) {
                    output += "    static_assert(static_cast<" + field.name + "_underlying_type>(" +
                              field_type.spelling + "::" + enumerator.name + ") <= static_cast<" +
                              field.name + "_underlying_type>(" + field.name + "_value_mask));\n";
                }
            }
        }
        offset += field.bits;
    }

    output += "\n    constexpr " + schema.name + "() noexcept = default;\n";
    output += "    explicit constexpr " + schema.name +
              "(storage_type const raw) noexcept : value_{raw} {}\n\n";
    output += "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {\n";
    output += "        return value_;\n    }\n\n";
    output += "    [[nodiscard]] constexpr auto operator<=>(" + schema.name +
              " const&) const noexcept = default;\n";

    for (auto const& field : schema.fields) {
        auto const field_type{resolve_type(field.type, types)};
        output += "\n    [[nodiscard]] constexpr auto " + field.name + "() const noexcept -> " +
                  field_type.spelling + " {\n";
        auto const extracted{"static_cast<storage_type>(value_ >> " + field.name + "_offset) & " +
                             field.name + "_value_mask"};
        if (field_type.spelling == "bool") {
            output += "        return (" + extracted + ") != 0;\n";
        } else if (field.kind == PackedFieldKind::enumeration) {
            output += "        auto const encoded{static_cast<" + field.name +
                      "_underlying_type>(" + extracted + ")};\n";
            output += "        return static_cast<" + field_type.spelling + ">(encoded);\n";
        } else {
            output +=
                "        return static_cast<" + field_type.spelling + ">(" + extracted + ");\n";
        }
        output += "    }\n";

        output += "\n    [[nodiscard]] constexpr auto try_set_" + field.name + "(" +
                  field_type.spelling + " const value) noexcept -> bool {\n";
        if (field_type.spelling == "bool") {
            output += "        auto const encoded{static_cast<storage_type>(value)};\n";
        } else if (field.kind == PackedFieldKind::enumeration) {
            output += "        auto const underlying{static_cast<" + field.name +
                      "_underlying_type>(value)};\n";
            output += "        if (underlying > static_cast<" + field.name + "_underlying_type>(" +
                      field.name + "_value_mask)) {\n";
            output += "            return false;\n        }\n";
            output += "        auto const encoded{static_cast<storage_type>(underlying)};\n";
        } else {
            output += "        if (value > static_cast<" + field_type.spelling + ">(" + field.name +
                      "_value_mask)) {\n";
            output += "            return false;\n        }\n";
            output += "        auto const encoded{static_cast<storage_type>(value)};\n";
        }
        output += "        auto const cleared{static_cast<storage_type>(\n";
        output += "            value_ & static_cast<storage_type>(~" + field.name + "_mask))};\n";
        output += "        auto const shifted{static_cast<storage_type>(\n";
        output += "            static_cast<storage_type>(encoded & " + field.name +
                  "_value_mask) << " + field.name + "_offset)};\n";
        output += "        value_ = static_cast<storage_type>(cleared | shifted);\n";
        output += "        return true;\n    }\n";

        output += "\n    constexpr void set_" + field.name + "(" + field_type.spelling +
                  " const value) noexcept {\n";
        output += "        if (!try_set_" + field.name + "(value)) {\n";
        output += "            assert(false && \"Packed field value does not fit.\");\n";
        output += "        }\n    }\n";
    }

    output += "  private:\n    storage_type value_{};\n};\n";
    output += "static_assert(sizeof(" + schema.name + ") == sizeof(" + schema.name +
              "::storage_type));\n";
    output += "static_assert(std::is_trivially_copyable_v<" + schema.name + ">);\n";
    output += "static_assert(std::is_standard_layout_v<" + schema.name + ">);";

    return Raw{std::move(output), std::move(dependencies)};
}

} // namespace

auto lower_packed_value_module(PackedValueModuleSchema const& module,
                               std::map<std::string, CppType> const& types,
                               std::vector<ModuleSchema> const& modules) -> Module {
    NodeListBuilder definitions;
    for (std::size_t index{}; index < module.values.size(); ++index) {
        definitions.add(packed_value_text(module.values[index], types, modules),
                        index + 1 < module.values.size() ? 2 : 1);
    }

    auto definition_nodes{definitions.build()};
    if (module.settings.namespace_name.has_value()) {
        definition_nodes = {
            Namespace{*module.settings.namespace_name, std::move(definition_nodes)}};
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.append(std::move(definition_nodes));

    return Module{
        .name = module.settings.name,
        .header =
            CppFile{
                .path = module.settings.header,
                .nodes = header_nodes.build(),
                .include_order = module.settings.include_order,
                .format_generated = true,
            },
    };
}

} // namespace codegen::detail
