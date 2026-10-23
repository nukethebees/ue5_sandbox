#include "renderer.h"

#include <ioj/is_any.h>

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <lispb/indices.h>
#include <lispb/strings.h>
#include <slate_codegen/alignment.h>
#include <slate_codegen/alignment_kind.h>
#include <slate_codegen/box_orientation.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/child_kind.h>
#include <slate_codegen/declaration_kind.h>
#include <slate_codegen/document.h>
#include <slate_codegen/indices.h>
#include <slate_codegen/value_id.h>
#include <slate_codegen/value_kind.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slate_codegen::detail {
namespace {
using lispb::StringId;
using lispb::codegen::Expr;
using lispb::codegen::sexpr::SourceSpan;

auto output_path(std::string_view owner) -> std::filesystem::path {
    std::filesystem::path result;
    while (true) {
        auto const separator{owner.find("::")};
        auto const component{owner.substr(0, separator)};
        if (separator == std::string_view::npos) {
            result /= std::string{component} + ".slate.generated.h";
            return result;
        }
        result /= component;
        owner.remove_prefix(separator + 2);
    }
}

class Renderer {
  public:
    Renderer(lispb::codegen::sexpr::Sources const& sources,
             Document const& document,
             lispb::codegen::CppIR& ir)
        : sources_{sources}
        , document_{document}
        , ir_{ir} {}

    auto lower(std::string_view const source_path, DeclarationId const declaration)
        -> lispb::codegen::CppFileId {
        auto const index{declaration.value};
        auto const owner{text(document_.declarations.names[index])};
        auto const separator{owner.rfind("::")};
        auto const builder{owner.substr(separator == std::string::npos ? 0 : separator + 2) +
                           "Builder"};
        auto const library{document_.declarations.kinds[index] == DeclarationKind::widget_library};
        lispb::codegen::Nodes children;
        if (!library) {
            children.push_back(ir_.using_declaration(ir_.strings.intern("ThisClass"),
                                                     ir_.types.intern("::" + owner)));
            auto const constructor{ir_.constructor(ir_.strings.intern(builder), true)};
            ir_.set_function_parameters(constructor, {ir_.parameter("ThisClass&", "self")});
            ir_.set_function_body(
                constructor,
                {},
                {ir_.member_initializer(ir_.strings.intern("self_"), ir_.store_text("self"))});
            ir_.function_flags(constructor).set_define_in_header(true);
            children.push_back(ir_.header_function(constructor));
        }
        bool has_parameters{};
        auto const functions{document_.declarations.functions[index]};
        auto const end{functions.begin + functions.count};
        for (auto function{functions.begin}; function < end; ++function) {
            has_parameters = has_parameters || document_.functions.parameters[function].count != 0;
            children.push_back(render_function(FunctionId{function}, library));
        }
        if (!library) {
            children.push_back(ir_.access(lispb::codegen::Access::private_));
            children.push_back(
                ir_.instance_member(ir_.types.intern("ThisClass&"), ir_.strings.intern("self_")));
            auto const node{ir_.record(ir_.strings.intern(builder),
                                       std::move(children),
                                       lispb::codegen::RecordKind::struct_)};
            children = {node};
        }
        auto const namespace_name{library ? "SlateGenerated::" + owner
                                  : separator == std::string::npos
                                      ? std::string{"SlateGenerated"}
                                      : "SlateGenerated::" + owner.substr(0, separator)};
        lispb::codegen::Nodes nodes;
        if (has_parameters) {
            nodes.push_back(
                ir_.include(ir_.store_text("utility"), lispb::codegen::IncludeStyle::system));
        }
        nodes.push_back(
            ir_.namespace_node(ir_.strings.intern(namespace_name), std::move(children)));
        auto const file{
            ir_.files.append(ir_.strings.intern(output_path(owner).generic_string()), nodes)};
        auto const file_index{ir_.files.index(file)};
        ir_.files.source[file_index] = ir_.strings.intern(source_path);
        ir_.files.flags[file_index].set_format_generated(true);
        ir_.modules.append(document_.declarations.names[index], file);
        return file;
    }
  private:
    auto text(StringId const id) const -> std::string { return std::string{ir_.strings.view(id)}; }

    auto source_line(SourceSpan const span) -> lispb::codegen::NodeId {
        auto const location{sources_.location(span)};
        return ir_.line_directive(location.line, location.path);
    }

    auto render_function(FunctionId const function, bool const library) -> lispb::codegen::NodeId {
        auto const index{function.value};
        auto const spec{ir_.function(text(document_.functions.names[index]), "auto")};
        std::vector<lispb::codegen::FunctionParameterId> cpp_parameters;
        auto const parameters{document_.functions.parameters[index]};
        auto const parameter_end{parameters.begin + parameters.count};
        auto const parameter_type{ir_.types.intern("auto&&")};
        for (auto parameter{parameters.begin}; parameter < parameter_end; ++parameter) {
            cpp_parameters.push_back(
                ir_.parameter(parameter_type, document_.parameters.names[parameter]));
        }
        lispb::codegen::Nodes body;
        auto const bindings{document_.functions.bindings[index]};
        auto const binding_end{bindings.begin + bindings.count};
        for (auto binding{bindings.begin}; binding < binding_end; ++binding) {
            body.push_back(source_line(document_.bindings.spans[binding]));
            body.push_back(ir_.variable(ir_.types.intern("auto const"),
                                        document_.bindings.names[binding],
                                        render_value(document_.bindings.initializers[binding])));
        }
        if (bindings.count != 0) {
            body.push_back(ir_.lines(2));
        }
        auto const root{document_.functions.roots[index]};
        body.push_back(source_line(document_.child_span(root)));
        body.push_back(ir_.return_stmt(render_child(root)));
        ir_.set_function_parameters(spec, cpp_parameters);
        ir_.set_function_body(spec, body);
        ir_.function_flags(spec).set_is_inline(library);
        ir_.function_flags(spec).set_define_in_header(true);
        return ir_.header_function(spec);
    }

    auto forward_parameter(std::string const& parameter) -> Expr {
        auto const type{ir_.types.intern("decltype(" + parameter + ")")};
        return ir_.call(ir_.template_id(ir_.named("std::forward"), {ir_.type_reference(type)}),
                        {ir_.named(parameter)});
    }

    auto render_child(ChildId const child) -> Expr {
        switch (child.kind()) {
            case ChildKind::widget:
                return render_widget(child.index());
            case ChildKind::box:
                return render_box(child.index());
            case ChildKind::existing: {
                auto const parameter{text(document_.existing.parameters[child.index()])};
                return forward_parameter(parameter);
            }
            case ChildKind::call: {
                auto const index{child.index()};
                std::vector<Expr> cpp_arguments;
                auto const arguments{document_.calls.arguments[index]};
                auto const end{arguments.begin + arguments.count};
                for (auto argument{arguments.begin}; argument < end; ++argument) {
                    cpp_arguments.push_back(render_value(document_.value_lists[argument]));
                }
                return ir_.call(ir_.named(document_.calls.parameters[index]), cpp_arguments);
            }
            case ChildKind::none:
                break;
        }
        assert(false);
        return {};
    }

    auto render_widget(std::uint32_t const index) -> Expr {
        auto const assigned{document_.widgets.assigned_members[index]};
        auto const type{ir_.type_reference(ir_.types.intern(text(document_.widgets.types[index])))};
        auto result{assigned.value != 0
                        ? ir_.call(ir_.named("SAssignNew"),
                                   {ir_.member_access(ir_.named("self_"), assigned), type})
                        : ir_.call(ir_.named("SNew"), {type})};
        auto const arguments{document_.widgets.arguments[index]};
        auto const argument_end{arguments.begin + arguments.count};
        for (auto argument{arguments.begin}; argument < argument_end; ++argument) {
            result = render_argument(result, argument);
        }
        auto const content{document_.widgets.contents[index]};
        if (content) {
            result = ir_.subscript(result, render_child(content));
        }
        auto const slots{document_.widgets.named_slots[index]};
        auto const slot_end{slots.begin + slots.count};
        for (auto slot{slots.begin}; slot < slot_end; ++slot) {
            result = ir_.subscript(
                ir_.call(ir_.member_access(result, document_.named_slots.names[slot])),
                render_child(document_.named_slots.children[slot]));
        }
        return result;
    }

    auto render_box(std::uint32_t const index) -> Expr {
        auto const vertical{document_.boxes.orientations[index] == BoxOrientation::vertical};
        auto result{ir_.call(
            ir_.named("SNew"),
            {ir_.type_reference(ir_.types.intern(vertical ? "SVerticalBox" : "SHorizontalBox"))})};
        auto const slots{document_.boxes.slots[index]};
        auto const end{slots.begin + slots.count};
        for (auto slot{slots.begin}; slot < end; ++slot) {
            auto slot_expression{render_slot(slot, vertical)};
            auto const horizontal{document_.box_slots.horizontal[slot]};
            auto const vertical_alignment{document_.box_slots.vertical[slot]};
            if (horizontal.kind != AlignmentKind::none) {
                slot_expression = ir_.call(ir_.member_access(slot_expression, "HAlign"),
                                           {alignment(horizontal, true)});
            }
            if (vertical_alignment.kind != AlignmentKind::none) {
                slot_expression = ir_.call(ir_.member_access(slot_expression, "VAlign"),
                                           {alignment(vertical_alignment, false)});
            }
            result = ir_.binary(
                lispb::codegen::BinaryOperator::add,
                result,
                ir_.subscript(slot_expression, render_child(document_.box_slots.children[slot])));
        }
        return result;
    }

    auto render_value(ValueId const value) -> Expr {
        auto const index{value.index()};
        switch (value.kind()) {
            case ValueKind::text:
                return ir_.call(ir_.named("FText::FromString"),
                                {ir_.call(ir_.named("TEXT"),
                                          {ir_.string_literal(document_.values.text[index])})});
            case ValueKind::localized_text:
                return ir_.call(ir_.named("NSLOCTEXT"),
                                {ir_.string_literal(document_.localized.contexts[index]),
                                 ir_.string_literal(document_.localized.keys[index]),
                                 ir_.string_literal(document_.localized.text[index])});
            case ValueKind::margin: {
                auto const binding{document_.margins.bindings[index]};
                if (binding.value != 0) {
                    return ir_.init_list({ir_.named(binding)}, ir_.types.intern("FMargin"));
                }
                std::vector<Expr> elements;
                auto const values{document_.margins.values[index]};
                auto const end{values.begin + values.count};
                for (auto item{values.begin}; item < end; ++item) {
                    elements.push_back(
                        ir_.literal(float_literal(text(document_.margin_values[item]))));
                }
                return ir_.init_list(elements, ir_.types.intern("FMargin"));
            }
            case ValueKind::number:
            case ValueKind::boolean:
                return ir_.literal(document_.values.text[index]);
            default:
                return ir_.named(text(document_.values.text[index]));
        }
    }

    auto render_argument(Expr const object, std::uint32_t const argument) -> Expr {
        auto const name{text(document_.arguments.names[argument])};
        auto const value{document_.arguments.values[argument]};
        if (value.kind() == ValueKind::callback) {
            auto const parameter{text(document_.values.text[value.index()])};
            return ir_.call(ir_.member_access(object, name + "_Lambda"),
                            {forward_parameter(parameter)});
        }
        if (ioj::is_any(value.kind(), {ValueKind::method, ValueKind::uobject})) {
            auto const member{name + (value.kind() == ValueKind::uobject ? "_UObject" : "")};
            return ir_.call(
                ir_.member_access(object, member),
                {ir_.unary(lispb::codegen::UnaryOperator::address_of, ir_.named("self_")),
                 ir_.unary(lispb::codegen::UnaryOperator::address_of,
                           ir_.named("ThisClass::" + text(document_.values.text[value.index()])))});
        }
        return ir_.call(ir_.member_access(object, name), {render_value(value)});
    }

    auto render_slot(std::uint32_t const slot, bool const vertical) -> Expr {
        std::string const prefix{vertical ? "SandboxUI::Slate::vbox_" : "SandboxUI::Slate::hbox_"};
        auto const margin{document_.box_slots.padding[slot]};
        std::vector<Expr> arguments;
        auto const fill{document_.box_slots.fill[slot] != 0};
        auto const weight{document_.box_slots.weights[slot]};
        if (fill && weight) {
            arguments.push_back(
                weight.kind() == ValueKind::number
                    ? ir_.literal(float_literal(text(document_.values.text[weight.index()])))
                    : render_value(weight));
        } else if (fill && margin) {
            arguments.push_back(ir_.literal("1.0f"));
        }
        if (margin) {
            arguments.push_back(render_value(margin));
        }
        return ir_.call(ir_.named(prefix + (fill ? "fill_slot" : "auto_slot")), arguments);
    }

    static auto float_literal(std::string value) -> std::string {
        if (!value.contains('.')) {
            value += ".0";
        }
        value += 'f';
        return value;
    }
    auto alignment(Alignment const alignment, bool const horizontal) -> Expr {
        if (alignment.kind == AlignmentKind::parameter) {
            return ir_.named(alignment.parameter);
        }
        std::string result{horizontal ? "HAlign_" : "VAlign_"};
        switch (alignment.kind) {
            case AlignmentKind::start:
                return ir_.named(result + (horizontal ? "Left" : "Top"));
            case AlignmentKind::end:
                return ir_.named(result + (horizontal ? "Right" : "Bottom"));
            case AlignmentKind::center:
                return ir_.named(result + "Center");
            case AlignmentKind::fill:
                return ir_.named(result + "Fill");
            default:
                assert(false);
                return {};
        }
    }

    lispb::codegen::sexpr::Sources const& sources_;
    Document const& document_;
    lispb::codegen::CppIR& ir_;
};

}

auto lower(std::string_view const source_path,
           lispb::codegen::sexpr::Sources const& sources,
           Document const& document,
           DeclarationId const declaration,
           lispb::codegen::CppIR& ir) -> lispb::codegen::CppFileId {
    return Renderer{sources, document, ir}.lower(source_path, declaration);
}

}
