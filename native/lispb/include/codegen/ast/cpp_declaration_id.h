#pragma once

#include <codegen/ast/cpp_declaration_kind.h>
#include <codegen/ast/environment_id.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/template_parameter_id.h>

#include <cstdint>
#include <limits>

namespace lispb::codegen {

// Pack an 8-bit declaration kind and a 24-bit row index. Default construction denotes null.
struct CppDeclarationId {
    CppDeclarationId() = default;
    explicit CppDeclarationId(NodeId node);
    explicit CppDeclarationId(EnvironmentId environment);
    explicit CppDeclarationId(FunctionId function);
    explicit CppDeclarationId(FunctionParameterId parameter);
    explicit CppDeclarationId(TemplateParameterId parameter);

    std::uint32_t value{std::numeric_limits<std::uint32_t>::max()};

    auto kind() const -> CppDeclarationKind { return static_cast<CppDeclarationKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto is_null() const -> bool { return value == std::numeric_limits<std::uint32_t>::max(); }
    auto operator==(CppDeclarationId const&) const -> bool = default;
  private:
    CppDeclarationId(CppDeclarationKind kind, std::uint32_t index);
};
static_assert(sizeof(CppDeclarationId) == 4);
}
