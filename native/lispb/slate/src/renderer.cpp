#include "renderer.h"

#include <codegen/ast/access.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/cpp_string.h>
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

namespace slate_codegen::detail {
namespace {
using lispb::IdentifierId;
using lispb::StringId;
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

// Slate's builder expressions have their own layout; declarations and files use CppIR.
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
            children.push_back(
                ir_.raw("explicit " + builder + "(ThisClass& self) : self_{self} {}"));
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
            ir_.files.append(ir_.strings.add(output_path(owner).generic_string()), nodes)};
        auto const file_index{ir_.files.index(file)};
        ir_.files.source[file_index] = ir_.strings.add(source_path);
        ir_.files.flags[file_index].set_format_generated(true);
        ir_.modules.append(document_.declarations.names[index], file);
        return file;
    }
  private:
    auto text(IdentifierId const id) const -> std::string {
        return std::string{ir_.strings.view(id)};
    }
    auto text(StringId const id) const -> std::string { return std::string{ir_.strings.view(id)}; }

    void line(std::size_t const indent, std::string_view const text) {
        output_.append(indent, ' ');
        output_ += text;
        output_ += '\n';
    }
    void source_line(SourceSpan const span) {
        auto const location{sources_.location(span)};
        line(0,
             "#line " + std::to_string(location.line) + " \"" +
                 lispb::codegen::escape_cpp_string(location.path) + "\"");
    }

    auto render_function(FunctionId const function, bool const library) -> lispb::codegen::NodeId {
        auto const index{function.value};
        auto const spec{ir_.function(ir_.strings.intern(text(document_.functions.names[index])),
                                     ir_.types.intern("auto"))};
        std::vector<lispb::codegen::FunctionParameterId> cpp_parameters;
        auto const parameters{document_.functions.parameters[index]};
        auto const parameter_end{parameters.begin + parameters.count};
        auto const parameter_type{ir_.types.intern("auto&&")};
        for (auto parameter{parameters.begin}; parameter < parameter_end; ++parameter) {
            cpp_parameters.push_back(
                ir_.parameter(parameter_type, document_.parameters.names[parameter]));
        }
        output_.clear();
        auto const bindings{document_.functions.bindings[index]};
        auto const binding_end{bindings.begin + bindings.count};
        for (auto binding{bindings.begin}; binding < binding_end; ++binding) {
            source_line(document_.bindings.spans[binding]);
            line(0,
                 "auto const " + text(document_.bindings.names[binding]) + "{" +
                     render_value(document_.bindings.initializers[binding]) + "};");
        }
        if (bindings.count != 0) {
            line(0, {});
        }
        line(0, "return");
        auto const root{document_.functions.roots[index]};
        source_line(document_.child_span(root));
        render_child(root, 4);
        output_.pop_back();
        output_ += ";";
        ir_.set_function_parameters(spec, cpp_parameters);
        ir_.set_function_body(spec, {ir_.raw(output_)});
        ir_.function_flags(spec).set_is_inline(library);
        ir_.function_flags(spec).set_define_in_header(true);
        return ir_.header_function(spec);
    }

    void render_child(ChildId const child, std::size_t const indent) {
        switch (child.kind()) {
            case ChildKind::widget:
                render_widget(child.index(), indent);
                return;
            case ChildKind::box:
                render_box(child.index(), indent);
                return;
            case ChildKind::existing: {
                auto const parameter{text(document_.existing.parameters[child.index()])};
                line(indent, "std::forward<decltype(" + parameter + ")>(" + parameter + ")");
                return;
            }
            case ChildKind::call: {
                auto const index{child.index()};
                auto invocation{text(document_.calls.parameters[index]) + "("};
                auto const arguments{document_.calls.arguments[index]};
                auto const end{arguments.begin + arguments.count};
                for (auto argument{arguments.begin}; argument < end; ++argument) {
                    if (argument != arguments.begin) {
                        invocation += ", ";
                    }
                    invocation += render_value(document_.value_lists[argument]);
                }
                invocation += ')';
                line(indent, invocation);
                return;
            }
            case ChildKind::none:
                break;
        }
        assert(false);
    }

    void render_widget(std::uint32_t const index, std::size_t const indent) {
        auto const assigned{document_.widgets.assigned_members[index]};
        auto const type{text(document_.widgets.types[index])};
        line(indent,
             assigned.value != 0 ? "SAssignNew(self_." + text(assigned) + ", " + type + ")"
                                 : "SNew(" + type + ")");
        auto const arguments{document_.widgets.arguments[index]};
        auto const argument_end{arguments.begin + arguments.count};
        for (auto argument{arguments.begin}; argument < argument_end; ++argument) {
            auto const invocation{render_argument(argument)};
            if (indent + 4 + invocation.size() <= 100) {
                line(indent + 4, invocation);
            } else {
                auto const opening{invocation.find('(')};
                line(indent + 4, invocation.substr(0, opening + 1));
                line(indent + 8, invocation.substr(opening + 1));
            }
        }
        auto const content{document_.widgets.contents[index]};
        if (content) {
            line(indent + 4, "[");
            render_child(content, indent + 8);
            line(indent + 4, "]");
        }
        auto const slots{document_.widgets.named_slots[index]};
        auto const slot_end{slots.begin + slots.count};
        for (auto slot{slots.begin}; slot < slot_end; ++slot) {
            line(indent + 4, "." + text(document_.named_slots.names[slot]) + "()");
            line(indent + 4, "[");
            render_child(document_.named_slots.children[slot], indent + 8);
            line(indent + 4, "]");
        }
    }

    void render_box(std::uint32_t const index, std::size_t const indent) {
        auto const vertical{document_.boxes.orientations[index] == BoxOrientation::vertical};
        line(indent, vertical ? "SNew(SVerticalBox)" : "SNew(SHorizontalBox)");
        auto const slots{document_.boxes.slots[index]};
        auto const end{slots.begin + slots.count};
        for (auto slot{slots.begin}; slot < end; ++slot) {
            line(indent + 4, "+ " + render_slot(slot, vertical));
            auto const horizontal{document_.box_slots.horizontal[slot]};
            auto const vertical_alignment{document_.box_slots.vertical[slot]};
            if (horizontal.kind != AlignmentKind::none) {
                line(indent + 8, ".HAlign(" + alignment(horizontal, true) + ")");
            }
            if (vertical_alignment.kind != AlignmentKind::none) {
                line(indent + 8, ".VAlign(" + alignment(vertical_alignment, false) + ")");
            }
            line(indent + 8, "[");
            render_child(document_.box_slots.children[slot], indent + 12);
            line(indent + 8, "]");
        }
    }

    auto render_value(ValueId const value) const -> std::string {
        auto const index{value.index()};
        switch (value.kind()) {
            case ValueKind::text:
                return "FText::FromString(TEXT(\"" +
                       lispb::codegen::escape_cpp_string(
                           ir_.strings.view(document_.values.text[index])) +
                       "\"))";
            case ValueKind::localized_text:
                return "NSLOCTEXT(\"" +
                       lispb::codegen::escape_cpp_string(
                           ir_.strings.view(document_.localized.contexts[index])) +
                       "\", \"" +
                       lispb::codegen::escape_cpp_string(
                           ir_.strings.view(document_.localized.keys[index])) +
                       "\", \"" +
                       lispb::codegen::escape_cpp_string(
                           ir_.strings.view(document_.localized.text[index])) +
                       "\")";
            case ValueKind::margin: {
                auto const binding{document_.margins.bindings[index]};
                if (binding.value != 0) {
                    return "FMargin{" + text(binding) + "}";
                }
                std::string result{"FMargin{"};
                auto const values{document_.margins.values[index]};
                auto const end{values.begin + values.count};
                for (auto item{values.begin}; item < end; ++item) {
                    if (item != values.begin) {
                        result += ", ";
                    }
                    result += float_literal(text(document_.margin_values[item]));
                }
                result += '}';
                return result;
            }
            default:
                return text(document_.values.text[index]);
        }
    }

    auto render_argument(std::uint32_t const argument) const -> std::string {
        auto const name{text(document_.arguments.names[argument])};
        auto const value{document_.arguments.values[argument]};
        if (value.kind() == ValueKind::callback) {
            auto const parameter{text(document_.values.text[value.index()])};
            return "." + name + "_Lambda(std::forward<decltype(" + parameter + ")>(" + parameter +
                   "))";
        }
        if (value.kind() == ValueKind::method) {
            return "." + name +
                   "(&self_, &ThisClass::" + text(document_.values.text[value.index()]) + ")";
        }
        if (value.kind() == ValueKind::uobject) {
            return "." + name +
                   "_UObject(&self_, &ThisClass::" + text(document_.values.text[value.index()]) +
                   ")";
        }
        return "." + name + "(" + render_value(value) + ")";
    }

    auto render_slot(std::uint32_t const slot, bool const vertical) const -> std::string {
        std::string const prefix{vertical ? "SandboxUI::Slate::vbox_" : "SandboxUI::Slate::hbox_"};
        auto const margin{document_.box_slots.padding[slot]};
        auto const padding{margin ? render_value(margin) : std::string{}};
        if (document_.box_slots.fill[slot] == 0) {
            return prefix + "auto_slot(" + padding + ")";
        }
        auto const weight{document_.box_slots.weights[slot]};
        if (weight) {
            auto const spelling{weight.kind() == ValueKind::number
                                    ? float_literal(text(document_.values.text[weight.index()]))
                                    : render_value(weight)};
            return prefix + "fill_slot(" + spelling + (padding.empty() ? "" : ", " + padding) + ")";
        }
        return prefix + "fill_slot(" + (padding.empty() ? "" : "1.0f, " + padding) + ")";
    }

    static auto float_literal(std::string value) -> std::string {
        if (!value.contains('.')) {
            value += ".0";
        }
        value += 'f';
        return value;
    }
    auto alignment(Alignment const alignment, bool const horizontal) const -> std::string {
        if (alignment.kind == AlignmentKind::parameter) {
            return text(alignment.parameter);
        }
        std::string result{horizontal ? "HAlign_" : "VAlign_"};
        switch (alignment.kind) {
            case AlignmentKind::start:
                return result + (horizontal ? "Left" : "Top");
            case AlignmentKind::end:
                return result + (horizontal ? "Right" : "Bottom");
            case AlignmentKind::center:
                return result + "Center";
            case AlignmentKind::fill:
                return result + "Fill";
            default:
                assert(false);
                return {};
        }
    }

    lispb::codegen::sexpr::Sources const& sources_;
    Document const& document_;
    lispb::codegen::CppIR& ir_;
    std::string output_;
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
