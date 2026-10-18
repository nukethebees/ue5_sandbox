#pragma once

#include "planner_ui.hpp"
#include "planner_ui_support.hpp"
#include <ioj/layout/planner_type.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/path_utils.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>

#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline auto parse_integer_literal(std::string_view text)
    -> std::optional<lispb::codegen::PackedIntegerValue> {
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
    return lispb::codegen::PackedIntegerValue::from_parts(negative, magnitude);
}

inline auto module_label(lispb::codegen::Declarations const& store,
                         lispb::codegen::ModuleHandle module) -> std::string {
    auto const* kind{module.kind() == lispb::codegen::ModuleKind::normal     ? "module"
                     : module.kind() == lispb::codegen::ModuleKind::settings ? "settings"
                                                                             : "umbrella"};
    return std::string{store.strings().view(
               store.module_settings_store
                   .name[store.module_settings_store.index(store.module_settings(module))])} +
           "  [" + kind + "]";
}

inline auto is_editable_module_destination(lispb::codegen::ModuleHandle module) -> bool {
    return module.kind() == lispb::codegen::ModuleKind::normal;
}

inline auto matches_filter(TypeGraph const& types, TypeId const node, std::string_view const filter)
    -> bool {
    if (filter.empty()) {
        return true;
    }
    auto const& strings{types.strings()};
    auto haystack{std::string{strings.view(types.module_name(node))} + " " +
                  std::string{strings.view(types.namespace_name(node))} + " " +
                  std::string{strings.view(types.name(node))} + " " +
                  std::string{strings.view(types.cpp_spelling(node))}};
    if (auto const soa{((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::soa
                            ? std::optional{types.definition(node)}
                            : std::nullopt)};
        static_cast<bool>(soa) &&
        types.definitions()
            .soas.related_storage_name[types.definitions().soas.index(*soa)]
            .has_value()) {
        haystack +=
            " " + std::string{types.cpp_types().strings().view(
                      *types.definitions()
                           .soas.related_storage_name[types.definitions().soas.index(*soa)])};
    }
    return detail::lowercase(haystack).find(detail::lowercase(filter)) != std::string::npos;
}

} // namespace

} // namespace ioj::layout_planner
