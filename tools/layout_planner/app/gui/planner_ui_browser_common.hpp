#pragma once

#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <codegen/path_utils.h>
#include <imgui.h>
#include <ioj/layout/planner_type.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline auto lowercase(std::string_view const text) -> std::string {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

inline auto parse_integer_literal(std::string_view text)
    -> std::optional<codegen::PackedIntegerValue> {
    auto negative{false};
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t magnitude{};
    auto const [end,
                error]{std::from_chars(text.data(), text.data() + text.size(), magnitude, base)};
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return codegen::PackedIntegerValue::from_parts(negative, magnitude);
}

inline auto module_label(codegen::ModuleSchema const& module) -> std::string {
    return std::visit(
        [](auto const& value) {
            using Module = std::decay_t<decltype(value)>;
            auto const* kind{
                std::is_same_v<Module, codegen::EnumModuleSchema>             ? "enums"
                : std::is_same_v<Module, codegen::ScalarModuleSchema>         ? "integer scalars"
                : std::is_same_v<Module, codegen::RepresentationModuleSchema> ? "representations"
                : std::is_same_v<Module, codegen::PackedValueModuleSchema>    ? "packed values"
                : std::is_same_v<Module, codegen::RecordModuleSchema>         ? "records"
                : std::is_same_v<Module, codegen::UnionModuleSchema>          ? "unions"
                : std::is_same_v<Module, codegen::SoaModuleSchema>            ? "SoA"
                                                                              : "vectors"};
            return value.settings.name + "  [" + kind + "]";
        },
        module);
}

inline auto is_editable_module_destination(codegen::ModuleSchema const& module) -> bool {
    return std::holds_alternative<codegen::EnumModuleSchema>(module) ||
           std::holds_alternative<codegen::PackedValueModuleSchema>(module) ||
           std::holds_alternative<codegen::ScalarModuleSchema>(module) ||
           std::holds_alternative<codegen::RepresentationModuleSchema>(module) ||
           std::holds_alternative<codegen::RecordModuleSchema>(module) ||
           std::holds_alternative<codegen::UnionModuleSchema>(module) ||
           std::holds_alternative<codegen::SoaModuleSchema>(module);
}

inline auto matches_filter(TypeNode const& node, std::string_view const filter) -> bool {
    if (filter.empty()) {
        return true;
    }
    auto haystack{node.identity.module_name + " " + node.identity.namespace_name + " " +
                  node.identity.name + " " + node.cpp_spelling};
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->related_storage_name.has_value()) {
        haystack += " " + *soa->related_storage_name;
    }
    return lowercase(haystack).find(lowercase(filter)) != std::string::npos;
}

} // namespace

} // namespace ioj::layout_planner
