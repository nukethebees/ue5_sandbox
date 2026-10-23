#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/ir/range_storage.h>
#include <codegen/ast/template_kind.h>
#include <codegen/ast/template_parameter_kind.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lispb::codegen {

auto CppIR::type_template_parameter(IdentifierId name, CppTypeId default_type, bool pack)
    -> TemplateParameterId {
    auto const index{static_cast<std::uint32_t>(type_template_parameters_.name.size())};
    type_template_parameters_.name.push_back(name);
    type_template_parameters_.default_type.push_back(default_type);
    type_template_parameters_.pack.push_back(pack);
    return TemplateParameterId{(static_cast<std::uint32_t>(TemplateParameterKind::type) << 24) |
                               index};
}
auto CppIR::value_template_parameter(CppTypeId type,
                                     IdentifierId name,
                                     Expr default_value,
                                     bool pack) -> TemplateParameterId {
    auto const index{static_cast<std::uint32_t>(value_template_parameters_.name.size())};
    value_template_parameters_.type.push_back(type);
    value_template_parameters_.name.push_back(name);
    value_template_parameters_.default_value.push_back(default_value);
    value_template_parameters_.pack.push_back(pack);
    return TemplateParameterId{(static_cast<std::uint32_t>(TemplateParameterKind::value) << 24) |
                               index};
}
auto CppIR::template_declaration(std::span<TemplateParameterId const> parameters) -> TemplateId {
    assert(!parameters.empty());
    auto const index{static_cast<std::uint32_t>(templates_.parameters.size())};
    templates_.parameters.push_back(ir::save_range(template_parameter_references_, parameters));
    return TemplateId{(static_cast<std::uint32_t>(TemplateKind::parameters) << 24) | index};
}
auto CppIR::template_specialization() -> TemplateId {
    return TemplateId{static_cast<std::uint32_t>(TemplateKind::explicit_specialization) << 24};
}
auto CppIR::raw_template_declaration(StringId parameters) -> TemplateId {
    auto const index{static_cast<std::uint32_t>(templates_.authored_text.size())};
    templates_.authored_text.push_back(parameters);
    return TemplateId{(static_cast<std::uint32_t>(TemplateKind::authored_raw) << 24) | index};
}

void CppIR::append_template(std::string& output,
                            TemplateId declaration,
                            RenderContext const& context) const {
    assert(declaration);
    auto const row{declaration.index()};
    output += "template <";
    if (declaration.kind() == TemplateKind::authored_raw) {
        output += strings.view(templates_.authored_text[row]);
        context.state->environments.mark_unknown(context.state->current);
    } else if (declaration.kind() == TemplateKind::parameters) {
        auto const range{templates_.parameters[row]};
        auto const end{range.begin + range.count};
        for (auto position{range.begin}; position < end; ++position) {
            if (position != range.begin) {
                output += ", ";
            }
            auto const parameter{template_parameter_references_[position]};
            auto const index{parameter.index()};
            switch (parameter.kind()) {
                case TemplateParameterKind::type: {
                    output += "typename";
                    if (type_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{type_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += strings.view(name);
                    }
                    auto const default_type{type_template_parameters_.default_type[index]};
                    if (default_type.value != 0) {
                        output += " = ";
                        types.append(output, default_type, context);
                    }
                    break;
                }
                case TemplateParameterKind::value: {
                    types.append(output, value_template_parameters_.type[index], context);
                    if (value_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{value_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += strings.view(name);
                    }
                    if (auto const value{value_template_parameters_.default_value[index]}) {
                        output += " = ";
                        append_expr(output, value, context);
                    }
                    break;
                }
            }
            auto const name{parameter.kind() == TemplateParameterKind::type
                                ? type_template_parameters_.name[index]
                                : value_template_parameters_.name[index]};
            if (name.value != 0) {
                context.state->environments.declare(
                    context.state->current, name, CppDeclarationId::from(parameter));
            }
        }
    }
    output += '>';
}
void CppIR::collect_dependencies(TemplateId declaration,
                                 std::vector<TypeDependencyId>& result) const {
    if (!declaration) {
        return;
    }
    if (declaration.kind() != TemplateKind::parameters) {
        return;
    }
    auto append_type = [&](CppTypeId type) {
        if (type.value == 0) {
            return;
        }
        auto const dependencies{types.dependencies(type)};
        result.insert(result.end(), dependencies.begin(), dependencies.end());
    };
    auto const range{templates_.parameters[declaration.index()]};
    auto const end{range.begin + range.count};
    for (auto position{range.begin}; position < end; ++position) {
        auto const parameter{template_parameter_references_[position]};
        auto const index{parameter.index()};
        switch (parameter.kind()) {
            case TemplateParameterKind::type:
                append_type(type_template_parameters_.default_type[index]);
                break;
            case TemplateParameterKind::value:
                append_type(value_template_parameters_.type[index]);
                if (auto const value{value_template_parameters_.default_value[index]}) {
                    collect_dependencies(value, result);
                }
                break;
        }
    }
}

}
