#include <codegen/ast/cpp_declaration_id.h>

#include <cassert>

namespace lispb::codegen {
CppDeclarationId::CppDeclarationId(CppDeclarationKind kind, std::uint32_t index) {
    assert(index <= index_mask);
    value = (static_cast<std::uint32_t>(kind) << kind_shift) | index;
}
CppDeclarationId::CppDeclarationId(NodeId node)
    : CppDeclarationId{static_cast<CppDeclarationKind>(
                           static_cast<std::uint8_t>(CppDeclarationKind::node_begin) +
                           static_cast<std::uint8_t>(node.kind())),
                       node.index()} {
    assert(node.kind() != NodeKind::invalid);
}
CppDeclarationId::CppDeclarationId(EnvironmentId environment)
    : CppDeclarationId{CppDeclarationKind::environment, environment.value} {
    assert(!environment.is_null());
}
CppDeclarationId::CppDeclarationId(FunctionId function)
    : CppDeclarationId{CppDeclarationKind::function, function.value} {}
CppDeclarationId::CppDeclarationId(FunctionParameterId parameter)
    : CppDeclarationId{CppDeclarationKind::parameter, parameter.value} {}
CppDeclarationId::CppDeclarationId(TemplateParameterId parameter)
    : CppDeclarationId{parameter.kind() == TemplateParameterKind::type
                           ? CppDeclarationKind::type_parameter
                           : CppDeclarationKind::value_parameter,
                       parameter.index()} {}
CppDeclarationId::CppDeclarationId(ExternalDeclarationId declaration)
    : CppDeclarationId{CppDeclarationKind::external, declaration.value} {}
}
