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
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    CppDeclarationId() = default;
    explicit CppDeclarationId(NodeId node);
    explicit CppDeclarationId(EnvironmentId environment);
    explicit CppDeclarationId(FunctionId function);
    explicit CppDeclarationId(FunctionParameterId parameter);
    explicit CppDeclarationId(TemplateParameterId parameter);
    explicit CppDeclarationId(ExternalDeclarationId declaration);

    std::uint32_t value{std::numeric_limits<std::uint32_t>::max()};

    auto kind() const -> CppDeclarationKind {
        return static_cast<CppDeclarationKind>(value >> kind_shift);
    }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto is_null() const -> bool { return value == std::numeric_limits<std::uint32_t>::max(); }
    auto operator==(CppDeclarationId const&) const -> bool = default;
  private:
    CppDeclarationId(CppDeclarationKind kind, std::uint32_t index);
};
static_assert(sizeof(CppDeclarationId) == 4);
}
