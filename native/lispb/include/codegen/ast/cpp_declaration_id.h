#pragma once

#include <codegen/ast/environment_id.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/template_parameter_id.h>
#include <lispb/indices.h>

#include <cassert>
#include <cstdint>
#include <limits>

namespace lispb::codegen {

enum class CppDeclarationKind : std::uint8_t {
    environment,
    function,
    parameter,
    type_parameter,
    value_parameter,
    external,
    node_begin = 32,
};

// Identify declarations, rather than individual uses of their types.
struct CppDeclarationId {
    std::uint32_t value{std::numeric_limits<std::uint32_t>::max()};

    static auto make(CppDeclarationKind kind, std::uint32_t index) -> CppDeclarationId {
        assert(index <= 0x00ffffffU);
        return {(static_cast<std::uint32_t>(kind) << 24) | index};
    }
    static auto from(NodeId node) -> CppDeclarationId {
        return make(static_cast<CppDeclarationKind>(
                        static_cast<std::uint8_t>(CppDeclarationKind::node_begin) +
                        static_cast<std::uint8_t>(node.kind())),
                    node.index());
    }
    static auto from(EnvironmentId environment) -> CppDeclarationId {
        assert(!environment.is_null());
        return make(CppDeclarationKind::environment, environment.value);
    }
    static auto from(FunctionId function) -> CppDeclarationId {
        return make(CppDeclarationKind::function, function.value);
    }
    static auto from(FunctionParameterId parameter) -> CppDeclarationId {
        return make(CppDeclarationKind::parameter, parameter.value);
    }
    static auto from(TemplateParameterId parameter) -> CppDeclarationId {
        return make(parameter.kind() == TemplateParameterKind::type
                        ? CppDeclarationKind::type_parameter
                        : CppDeclarationKind::value_parameter,
                    parameter.index());
    }
    auto kind() const -> CppDeclarationKind { return static_cast<CppDeclarationKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto is_null() const -> bool { return value == std::numeric_limits<std::uint32_t>::max(); }
    auto operator==(CppDeclarationId const&) const -> bool = default;
};

static_assert(sizeof(CppDeclarationId) == 4);

}
