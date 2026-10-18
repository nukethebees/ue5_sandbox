#include "fixed_soa_internal.h"
#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"
#include "soa_receiver.h"
#include "soa_representation.h"
#include <ioj/format_writer.h>

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

void render_parameters(LoweringContext& context,
                       std::string& out,
                       std::span<RowParameterId const> const parameters,
                       bool const leading_comma = true) {
    auto write{ioj::FormatWriter{std::back_inserter(out)}};
    auto const& parameters_store{context.ir.lowering.row_parameters};
    auto separator{leading_comma ? ", " : ""};
    for (auto const parameter : parameters) {
        write("{}{} const new_{}",
              separator,
              context.ir.types.spelling(parameters_store.type[parameters_store.index(parameter)]),
              context.text(parameters_store.name[parameters_store.index(parameter)]));
        separator = ", ";
    }
}

void render_arguments(LoweringContext& context,
                      std::string& out,
                      std::span<RowParameterId const> const parameters) {
    auto write{ioj::FormatWriter{std::back_inserter(out)}};
    auto const& parameters_store{context.ir.lowering.row_parameters};
    for (auto const parameter : parameters) {
        write(", new_{}", context.text(parameters_store.name[parameters_store.index(parameter)]));
    }
}

void render_vector_storage_operations(LoweringContext& context,
                                      std::string& out,
                                      DeclarationHandle schema) {
    auto write{ioj::FormatWriter{std::back_inserter(out)}};
    for (auto const operation : context.declarations.items(
             context.declarations.soas.operations[context.declarations.soas.index(schema)])) {
        std::string name;
        std::string parameters;
        std::string arguments;
        switch (operation) {
            case StorageOperation::reserve:
                name = "reserve";
                break;
            case StorageOperation::reset:
                name = "reset";
                break;
            case StorageOperation::set_num:
                name = "set_num";
                break;
            case StorageOperation::add_uninitialised:
                name = "add_uninitialised";
                break;
            case StorageOperation::add_defaulted:
                name = "add_defaulted";
                break;
            case StorageOperation::remove_at_swap:
                name = "remove_at_swap";
                break;
            default:
                continue;
        }
        if (operation == StorageOperation::remove_at_swap) {
            parameters = "size_type const index, size_type const count";
            arguments = ", index, count";
        } else if (operation != StorageOperation::reset) {
            parameters = "size_type const count";
            arguments = ", count";
        }
        write("void {0}({1}) {{ ioj::native_soa::vector_storage_ops::{0}(*this{2}); }}\n",
              name,
              parameters,
              arguments);
    }
    out += "void apply_permutation(std::span<std::int32_t> const indices) { "
           "ioj::native_soa::vector_storage_ops::apply_permutation(*this, indices); }\n";
}

}

auto lower_native_soa(LoweringContext& context,
                      DeclarationHandle schema,
                      std::map<IdentifierId, DeclarationHandle> const& schemas,
                      bool const allow_equivalent_type,
                      std::span<std::string const> const equivalent_members,
                      StringId const equivalent_constructor,
                      bool const equivalent_accessors) -> LoweredSoa {
    auto& ir{context.ir};
    auto const layout{build_soa_layout(context, schema, schemas, false)};
    auto& storage{ir.lowering};
    auto& fixed_members{storage.fixed_member_layouts};
    auto& leaves{storage.fixed_leaves};
    auto& resolved_members{storage.resolved_members};
    auto& parameters{storage.row_parameters};
    auto const layout_members{
        storage.items(storage.fixed_layouts.members[storage.fixed_layouts.index(layout)])};
    auto const layout_leaves{
        storage.items(storage.fixed_layouts.leaves[storage.fixed_layouts.index(layout)])};
    auto const equivalent_type{
        context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]
                .has_value()
            ? std::optional{context.resolve(
                  *context.declarations.soas
                       .equivalent_type[context.declarations.soas.index(schema)])}
            : std::nullopt};
    if (allow_equivalent_type && equivalent_type.has_value() &&
        equivalent_members.size() != layout_members.size()) {
        throw std::invalid_argument{
            "Standard-library SoA equivalent members must match its columns"};
    }

    auto const view{
        context
            .optional_text(
                context.declarations.soas.view_name[context.declarations.soas.index(schema)])
            .value_or(context.text(
                          context.declarations.soas.name[context.declarations.soas.index(schema)]) +
                      "View")};
    auto const const_view{
        context
            .optional_text(
                context.declarations.soas.const_view_name[context.declarations.soas.index(schema)])
            .value_or(context.text(
                          context.declarations.soas.name[context.declarations.soas.index(schema)]) +
                      "ConstView")};
    auto const vector3f_schema{uses_native_vector_view(context, schema)};
    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("native_storage", "ioj/native_soa/storage.h"),
        ir.types.dependency("native_vector_storage_ops", "ioj/native_soa/vector_storage_ops.h"),
        ir.types.dependency("std::forward", "utility"),
        ir.types.dependency("assert", "cassert"),
    };
    if (vector3f_schema) {
        dependencies.push_back(ir.types.dependency("vector_soa_view", "ioj/vector_soa_view.h"));
    }
    std::vector<RowParameterId> row_parameters;
    for (auto const member : layout_members) {
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                fixed_members.source[fixed_members.index(member)])] == SoaMemberKind::nested) {
            auto const& child{
                schemas.at(*context.declarations.soa_members
                                .nested_schema[context.declarations.soa_members.index(
                                    fixed_members.source[fixed_members.index(member)])])};
            if (context.declarations.soas.equivalent_type[context.declarations.soas.index(child)]
                    .has_value()) {
                auto const type{
                    context.resolve(*context.declarations.soas
                                         .equivalent_type[context.declarations.soas.index(child)])};
                dependencies.insert(dependencies.end(),
                                    ir.types.dependencies(type).begin(),
                                    ir.types.dependencies(type).end());
                auto const parameter{parameters.append()};
                auto const parameter_row{parameters.index(parameter)};
                parameters.type[parameter_row] =
                    ir.types.derive(type, native_spelling(std::string{ir.types.spelling(type)}));
                parameters.name[parameter_row] =
                    context.declarations.soa_members.name[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])];
                parameters.column[parameter_row] = storage.paths.append(
                    {},
                    context.declarations.soa_members.name[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])]);
                parameters.nested[parameter_row] = true;
                row_parameters.push_back(parameter);
                continue;
            }
        }
        for (auto const leaf : layout_leaves) {
            if (storage.paths.items(leaves.path[leaves.index(leaf)]).front() ==
                context.declarations.soa_members.name[context.declarations.soa_members.index(
                    fixed_members.source[fixed_members.index(member)])]) {
                auto const parameter{parameters.append()};
                auto const parameter_row{parameters.index(parameter)};
                parameters.type[parameter_row] =
                    ir.types.derive(leaves.type[leaves.index(leaf)],
                                    native_spelling(std::string{
                                        ir.types.spelling(leaves.type[leaves.index(leaf)])}));
                parameters.name[parameter_row] =
                    ir.strings.intern(context.path_text(leaves.path[leaves.index(leaf)], "_"));
                parameters.column[parameter_row] = leaves.path[leaves.index(leaf)];
                parameters.nested[parameter_row] = false;
                row_parameters.push_back(parameter);
            }
        }
    }
    if (equivalent_type.has_value()) {
        dependencies.insert(dependencies.end(),
                            ir.types.dependencies(*equivalent_type).begin(),
                            ir.types.dependencies(*equivalent_type).end());
    }
    std::string out{};
    auto write{ioj::FormatWriter{std::back_inserter(out)}};
    NodeListBuilder header{ir};
    NodeListBuilder source{ir};
    Nodes record_members;
    auto building_record{false};
    auto flush = [&] {
        auto const content{out};
        if (!content.empty()) {
            auto const node{ir.raw(content, dependencies)};
            if (building_record) {
                record_members.push_back(node);
            } else {
                header.add(node);
            }
            out.clear();
        }
    };
    auto begin_record = [&] {
        flush();
        building_record = true;
    };
    auto end_record = [&](IdentifierId const name) {
        flush();
        auto const record{ir.record(name, record_members)};
        ir.set_record_export_specifier(
            record,
            context.declarations.soas.export_specifier[context.declarations.soas.index(schema)]);
        ir.set_record_dependencies(record, dependencies);
        header.add(record).new_lines();
        record_members.clear();
        building_record = false;
    };
    auto api = [&](SoaReceiver const receiver, std::string const& name) {
        flush();
        auto lowered{lower_soa_api(context,
                                   schema,
                                   receiver == SoaReceiver::owner
                                       ? SoaRepresentation::vector
                                       : SoaRepresentation::native_vector_view,
                                   receiver,
                                   name,
                                   &schemas,
                                   SoaBackend::standard_library)};
        record_members.insert(record_members.end(), lowered.header.begin(), lowered.header.end());
        source.append(std::move(lowered.source)).new_lines(2);
    };
    auto const fn_template{
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Fn"))})};
    auto const fn_parameter_type{ir.types.intern("Fn&&")};
    auto const void_type{ir.types.intern("void")};
    auto each_column = [&](Nodes const& body, bool const immutable) {
        flush();
        auto const function{ir.function(ir.strings.intern("each_column"), void_type)};
        ir.set_function_template(function, fn_template);
        ir.set_function_parameters(function,
                                   {ir.parameter(fn_parameter_type, ir.strings.intern("fn"))});
        ir.set_function_body(function, body);
        ir.set_function_receiver(
            function, immutable ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        ir.function_flags(function).set_define_in_header(true);
        ir.function_formatting(function).template_placement = FunctionTemplatePlacement::same_line;
        record_members.push_back(ir.header_function(function));
        record_members.push_back(ir.lines(1));
    };

    if (vector3f_schema) {
        write(R"(using {} = ioj::Vector3fSoAView;
using {} = ioj::Vector3fSoAConstView;
)",
              view,
              const_view);
    } else {
        header.add(ir.forward_declaration(ir.strings.intern(view))).new_lines();
        header.add(ir.forward_declaration(ir.strings.intern(const_view))).new_lines();
    }

    auto const nested_vector3f = [&](IdentifierId member_name) {
        auto const member = std::ranges::find_if(layout_members, [&](auto const& value) {
            return context.declarations.soa_members.name[context.declarations.soa_members.index(
                       fixed_members.source[fixed_members.index(value)])] == member_name;
        });
        return member != layout_members.end() &&
               context.declarations.soa_members.kind[context.declarations.soa_members.index(
                   fixed_members.source[fixed_members.index(*member)])] == SoaMemberKind::nested &&
               uses_native_vector_view(
                   context,
                   schemas.at(*context.declarations.soa_members
                                   .nested_schema[context.declarations.soa_members.index(
                                       fixed_members.source[fixed_members.index(*member)])]));
    };
    auto const view_leaf = [&](auto const leaf, std::string_view const prefix = {}) {
        auto expression{std::string{prefix} +
                        context.path_text(leaves.path[leaves.index(leaf)], ".")};
        if (vector3f_schema ||
            nested_vector3f(storage.paths.items(leaves.path[leaves.index(leaf)]).front())) {
            expression += "()";
        }
        return expression;
    };
    auto const view_leaf_data = [&](auto const leaf, std::string_view const prefix = {}) {
        return view_leaf(leaf, prefix) + ".data()";
    };

    auto const column_expression = [&](auto const leaf, bool const view_receiver) {
        auto const path{storage.paths.items(leaves.path[leaves.index(leaf)])};
        auto expression{ir.named(path.front())};
        for (auto const member : path.subspan(1)) {
            expression = ir.member_access(expression, member);
        }
        if (view_receiver && (vector3f_schema || nested_vector3f(path.front()))) {
            expression = ir.call(expression);
        }
        return expression;
    };

    if (!vector3f_schema) {
        for (bool const immutable : {true, false}) {
            auto const view_type{immutable ? const_view : view};
            begin_record();
            write(R"(using View = {};
using ConstView = {};
using size_type = std::uint32_t;
)",
                  view,
                  const_view);
            if (equivalent_type.has_value()) {
                write("using equivalent_type = {};\nauto operator[](size_type const index) const "
                      "-> equivalent_type {{ return ",
                      native_spelling(std::string{ir.types.spelling(*equivalent_type)}));
                if (equivalent_constructor.value == 0) {
                    out += "{";
                } else {
                    write("{}(", ir.strings.view(equivalent_constructor));
                }
                for (std::size_t index{}; index < layout_members.size(); ++index) {
                    if (index > 0) {
                        out += ", ";
                    }
                    write("{}[static_cast<std::size_t>(index)]",
                          context.text(context.declarations.soa_members
                                           .name[context.declarations.soa_members.index(
                                               fixed_members.source[fixed_members.index(
                                                   layout_members[index])])]));
                }
                out += (equivalent_constructor.value == 0 ? "}; }\n" : "); }\n");
            }
            for (auto const member : layout_members) {
                auto const resolved{fixed_members.member[fixed_members.index(member)]};
                if (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])] ==
                    SoaMemberKind::array) {
                    write(
                        "std::span<{}{}> {};\n",
                        native_spelling(std::string{ir.types.spelling(
                            resolved_members.element_type[resolved_members.index(resolved)])}),
                        (immutable ? " const" : ""),
                        context.text(context.declarations.soa_members
                                         .name[context.declarations.soa_members.index(
                                             fixed_members.source[fixed_members.index(member)])]));
                } else {
                    auto const& child{
                        schemas.at(*context.declarations.soa_members
                                        .nested_schema[context.declarations.soa_members.index(
                                            fixed_members.source[fixed_members.index(member)])])};
                    write(
                        "{} {};\n",
                        (immutable
                             ? context
                                   .optional_text(
                                       context.declarations.soas
                                           .const_view_name[context.declarations.soas.index(child)])
                                   .value_or(
                                       context.text(
                                           context.declarations.soas
                                               .name[context.declarations.soas.index(child)]) +
                                       "ConstView")
                             : context
                                   .optional_text(
                                       context.declarations.soas
                                           .view_name[context.declarations.soas.index(child)])
                                   .value_or(
                                       context.text(
                                           context.declarations.soas
                                               .name[context.declarations.soas.index(child)]) +
                                       "View")),
                        context.text(context.declarations.soa_members
                                         .name[context.declarations.soa_members.index(
                                             fixed_members.source[fixed_members.index(member)])]));
                }
            }

            auto const first_leaf{layout_leaves.front()};
            auto const first_component{
                storage.paths.items(leaves.path[leaves.index(first_leaf)]).front()};
            auto const first_count{
                nested_vector3f(first_component)
                    ? context.text(first_component) + ".num()"
                    : context.path_text(leaves.path[leaves.index(first_leaf)], ".") + ".size()"};
            write(R"(auto num() const noexcept -> size_type {{ return static_cast<size_type>({}); }}
auto is_empty() const noexcept -> bool {{ return num() == 0; }}
)",
                  first_count);
            Nodes column_calls;
            for (auto const leaf : layout_leaves) {
                column_calls.push_back(ir.expression_statement(
                    ir.call(ir.named("fn"), {column_expression(leaf, true)})));
            }
            each_column(column_calls, true);
            write(
                R"(void validate_array_sizes() const {{ ioj::native_soa::vector_storage_ops::validate_array_sizes(*this); }}
auto slice(size_type const offset, size_type const count) const -> {} {{ assert(offset <= num() && count <= num() - offset); return {{
)",
                view_type);
            for (auto const member : layout_members) {
                write(
                    "{}{},\n",
                    context.text(context.declarations.soa_members
                                     .name[context.declarations.soa_members.index(
                                         fixed_members.source[fixed_members.index(member)])]),
                    (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                         fixed_members.source[fixed_members.index(member)])] == SoaMemberKind::array
                         ? ".subspan(static_cast<std::size_t>(offset), "
                           "static_cast<std::size_t>(count))"
                         : ".slice(offset, count)"));
            }
            write(R"(}}; }}
auto get_view() const -> {0} {{ return *this; }}
auto get_view(size_type const offset, size_type const count) const -> {0} {{ return slice(offset, count); }}
auto get_const_view() const -> ConstView {{ return {{
)",
                  view_type);
            for (auto const member : layout_members) {
                out += context.text(
                    context.declarations.soa_members.name[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])]);
                if (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])] ==
                    SoaMemberKind::nested) {
                    out += ".get_const_view()";
                }
                out += ",\n";
            }
            write(R"(}}; }}
auto get_const_view(size_type const offset, size_type const count) const -> ConstView {{ return get_const_view().slice(offset, count); }}
auto left(size_type const count) const -> {0} {{ return slice(0, count); }}
auto right(size_type const count) const -> {0} {{ assert(count <= num()); return slice(num() - count, count); }}
)",
                  view_type);

            if (!immutable) {
                out += "void set(size_type const index";
                render_parameters(context, out, row_parameters);
                out += ") const { assert(index < num());\n";
                for (auto const parameter : row_parameters) {
                    if (parameters.nested[parameters.index(parameter)]) {
                        write(
                            "{}.set(index, new_{});\n",
                            context.path_text(parameters.column[parameters.index(parameter)], "."),
                            context.text(parameters.name[parameters.index(parameter)]));
                    } else {
                        auto column{
                            context.path_text(parameters.column[parameters.index(parameter)], ".")};
                        if (nested_vector3f(
                                storage.paths.items(parameters.column[parameters.index(parameter)])
                                    .front())) {
                            column += "()";
                        }
                        write("{}[static_cast<std::size_t>(index)] = new_{};\n",
                              column,
                              context.text(parameters.name[parameters.index(parameter)]));
                    }
                }
                out += "}\n";
                if (equivalent_type.has_value() && allow_equivalent_type) {
                    out += "void set(size_type const index, equivalent_type const value) const { "
                           "set(index";
                    for (std::size_t index{}; index < layout_members.size(); ++index) {
                        write(", value.{}{}",
                              equivalent_members[index],
                              (equivalent_accessors ? "()" : ""));
                    }
                    out += "); }\n";
                }
            }
            api(immutable ? SoaReceiver::const_view : SoaReceiver::mutable_view, view_type);
            end_record(ir.strings.intern(view_type));
        }
    }

    begin_record();
    write(R"(using View = {};
using ConstView = {};
using size_type = std::uint32_t;
)",
          view,
          const_view);
    if (equivalent_type.has_value()) {
        write(R"(using equivalent_type = {};
auto operator[](size_type const index) const -> equivalent_type {{ return get_const_view()[index]; }}
)",
              native_spelling(std::string{ir.types.spelling(*equivalent_type)}));
    }
    for (auto const member : layout_members) {
        auto const type{resolved_members.element_type[resolved_members.index(
            fixed_members.member[fixed_members.index(member)])]};
        dependencies.insert(dependencies.end(),
                            ir.types.dependencies(type).begin(),
                            ir.types.dependencies(type).end());
        write("{} {};\n",
              (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                   fixed_members.source[fixed_members.index(member)])] == SoaMemberKind::array
                   ? "ioj::native_soa::Vector<" +
                         native_spelling(std::string{ir.types.spelling(type)}) + ">"
                   : std::string{ir.types.spelling(type)}),
              context.text(
                  context.declarations.soa_members.name[context.declarations.soa_members.index(
                      fixed_members.source[fixed_members.index(member)])]));
    }

    write(R"(auto num() const noexcept -> size_type {{ return static_cast<size_type>({}.size()); }}
auto is_empty() const noexcept -> bool {{ return num() == 0; }}
)",
          context.path_text(leaves.path[leaves.index(layout_leaves.front())], "."));
    Nodes column_calls;
    for (auto const leaf : layout_leaves) {
        column_calls.push_back(
            ir.expression_statement(ir.call(ir.named("fn"), {column_expression(leaf, false)})));
    }
    each_column(column_calls, false);
    each_column(column_calls, true);
    out += "void validate_array_sizes() const { get_const_view().validate_array_sizes(); }\n";

    render_vector_storage_operations(context, out, schema);
    flush();
    auto const compare_type{ir.types.intern("Compare")};
    auto const sort{ir.function(ir.strings.intern("sort"), void_type)};
    ir.set_function_template(
        sort, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Compare"))}));
    ir.set_function_parameters(
        sort,
        {ir.parameter(ir.types.derive(compare_type, "Compare&&"), ir.strings.intern("compare")),
         ir.parameter(ir.types.intern("std::span<std::int32_t> const"),
                      ir.strings.intern("scratch_indices"))});
    auto const forward_compare{
        ir.call(ir.template_id(ir.named("std::forward"), {ir.type_reference(compare_type)}),
                {ir.named("compare")})};
    ir.set_function_body(
        sort,
        {ir.expression_statement(ir.call(ir.named("ioj::native_soa::vector_storage_ops::sort"),
                                         {ir.unary(UnaryOperator::dereference, ir.named("this")),
                                          forward_compare,
                                          ir.named("scratch_indices")}))});
    ir.function_flags(sort).set_define_in_header(true);
    ir.function_formatting(sort).template_placement = FunctionTemplatePlacement::same_line;
    ir.function_formatting(sort).body_layout = FunctionBodyLayout::compact;
    record_members.push_back(ir.header_function(sort));
    record_members.push_back(ir.lines(1));

    {
        out += "void set(size_type const index";
        render_parameters(context, out, row_parameters);
        out += ") { get_view().set(index";
        render_arguments(context, out, row_parameters);
        out += "); }\nauto add(";
        render_parameters(context, out, row_parameters, false);
        out += ") -> size_type { return ioj::native_soa::vector_storage_ops::append_rows(*this, 1, "
               "[&] {\n";
        for (auto const parameter : row_parameters) {
            if (parameters.nested[parameters.index(parameter)]) {
                write("{}.add(new_{});\n",
                      context.path_text(parameters.column[parameters.index(parameter)], "."),
                      context.text(parameters.name[parameters.index(parameter)]));
            } else {
                write("{}.emplace_back(new_{});\n",
                      context.path_text(parameters.column[parameters.index(parameter)], "."),
                      context.text(parameters.name[parameters.index(parameter)]));
            }
        }
        out += "}); }\n";
        if (equivalent_type.has_value() && allow_equivalent_type) {
            out += "void set(size_type const index, equivalent_type const value) { "
                   "get_view().set(index, value); }\nauto add(equivalent_type const value) -> "
                   "size_type { return add(";
            for (std::size_t index{}; index < layout_members.size(); ++index) {
                if (index > 0) {
                    out += ", ";
                }
                write("value.{}{}", equivalent_members[index], (equivalent_accessors ? "()" : ""));
            }
            out += "); }\n";
        }
    }

    if (std::ranges::contains(
            context.declarations.items(
                context.declarations.soas.operations[context.declarations.soas.index(schema)]),
            StorageOperation::append_from)) {
        out += R"(void append_from(ConstView source) { auto const count{source.num()};
assert(count <= std::numeric_limits<size_type>::max() - num());
source.validate_array_sizes(); if (count == 0) { return; }
)";
        for (auto const leaf : layout_leaves) {
            auto const column{context.path_text(leaves.path[leaves.index(leaf)], ".")};
            write("assert(ioj::native_soa::is_external_source({}, {}));\n",
                  column,
                  view_leaf(leaf, "source."));
        }
        out += "ioj::native_soa::vector_storage_ops::append_rows(*this, count, [&] {\n";
        for (auto const leaf : layout_leaves) {
            auto const column{context.path_text(leaves.path[leaves.index(leaf)], ".")};
            auto const source_data{view_leaf_data(leaf, "source.")};
            write("{0}.insert({0}.end(), {1}, {1} + count);\n", column, source_data);
        }
        out += R"(});
}
)";
    }

    for (bool const immutable : {false, true}) {
        write("auto get_view(){} -> {} {{ return {{\n",
              (immutable ? " const" : ""),
              (immutable ? "ConstView" : "View"));
        if (vector3f_schema) {
            for (auto const member : layout_members) {
                write("{}.data(),\n",
                      context.text(context.declarations.soa_members
                                       .name[context.declarations.soa_members.index(
                                           fixed_members.source[fixed_members.index(member)])]));
            }
            out += "num(),\n";
        } else {
            for (auto const member : layout_members) {
                out += context.text(
                    context.declarations.soa_members.name[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])]);
                if (context.declarations.soa_members.kind[context.declarations.soa_members.index(
                        fixed_members.source[fixed_members.index(member)])] ==
                    SoaMemberKind::nested) {
                    out += ".get_view()";
                }
                out += ",\n";
            }
        }
        out += "}; }\n";
    }
    out += R"(auto get_const_view() const -> ConstView { return get_view(); }
auto get_view(size_type const offset, size_type const count) -> View { return get_view().slice(offset, count); }
auto get_view(size_type const offset, size_type const count) const -> ConstView { return get_view().slice(offset, count); }
auto get_const_view(size_type const offset, size_type const count) const -> ConstView { return get_const_view().slice(offset, count); }
auto slice(size_type const offset, size_type const count) -> View { return get_view(offset, count); }
auto slice(size_type const offset, size_type const count) const -> ConstView { return get_const_view(offset, count); }
auto left(size_type const count) -> View { return slice(0, count); }
auto right(size_type const count) -> View { assert(count <= num()); return slice(num() - count, count); }
auto left(size_type const count) const -> ConstView { return slice(0, count); }
auto right(size_type const count) const -> ConstView { assert(count <= num()); return slice(num() - count, count); }
)";
    if (std::ranges::contains(
            context.declarations.items(
                context.declarations.soas.operations[context.declarations.soas.index(schema)]),
            StorageOperation::copy_element)) {
        flush();
        auto const other_template{
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))})};
        auto const index_type{ir.types.intern("size_type const")};
        auto const destination_index{ir.parameter(index_type, ir.strings.intern("dst_index"))};
        auto const other{ir.parameter(ir.types.intern("Other const&"), ir.strings.intern("other"))};
        auto const source_index{ir.parameter(index_type, ir.strings.intern("src_index"))};
        auto const copy_element{ir.function(ir.strings.intern("copy_element"), void_type)};
        ir.set_function_template(copy_element, other_template);
        ir.set_function_parameters(copy_element, {destination_index, other, source_index});
        if (context.declarations.soas
                .copy_element_memberwise[context.declarations.soas.index(schema)] ||
            std::ranges::any_of(layout_members, [&](auto const member) {
                return context.declarations.soa_members.kind[context.declarations.soa_members.index(
                           fixed_members.source[fixed_members.index(member)])] ==
                       SoaMemberKind::array;
            })) {
            out += R"(auto const destination_index{static_cast<std::size_t>(dst_index)};
auto const source_index{static_cast<std::size_t>(src_index)};
)";
        }
        for (auto const member : layout_members) {
            if (context.declarations.soas
                    .copy_element_memberwise[context.declarations.soas.index(schema)] ||
                context.declarations.soa_members.kind[context.declarations.soa_members.index(
                    fixed_members.source[fixed_members.index(member)])] == SoaMemberKind::array) {
                write("{0}[destination_index] = other.{0}[source_index];\n",
                      context.text(context.declarations.soa_members
                                       .name[context.declarations.soa_members.index(
                                           fixed_members.source[fixed_members.index(member)])]));
            } else {
                write("{0}.copy_element(dst_index, other.{0}, src_index);\n",
                      context.text(context.declarations.soa_members
                                       .name[context.declarations.soa_members.index(
                                           fixed_members.source[fixed_members.index(member)])]));
            }
        }
        auto copy_body{out};
        if (!copy_body.empty()) {
            copy_body.pop_back();
        }
        ir.set_function_body(copy_element, {ir.raw(copy_body)});
        ir.function_flags(copy_element).set_define_in_header(true);
        out.clear();
        ir.function_formatting(copy_element).template_placement =
            FunctionTemplatePlacement::same_line;
        record_members.push_back(ir.header_function(copy_element));
        record_members.push_back(ir.lines(1));

        auto const copy_elements{ir.function(ir.strings.intern("copy_elements"), void_type)};
        ir.set_function_template(copy_elements, other_template);
        ir.set_function_parameters(copy_elements,
                                   {destination_index,
                                    other,
                                    source_index,
                                    ir.parameter(index_type, ir.strings.intern("count"))});
        ir.set_function_body(copy_elements,
                             {ir.raw("for (size_type i{}; i < count; ++i) { "
                                     "copy_element(dst_index + i, other, src_index + i); }")});
        ir.function_flags(copy_elements).set_define_in_header(true);
        ir.function_formatting(copy_elements).template_placement =
            FunctionTemplatePlacement::same_line;
        ir.function_formatting(copy_elements).body_layout = FunctionBodyLayout::compact;
        record_members.push_back(ir.header_function(copy_elements));
        record_members.push_back(ir.lines(1));
    }
    api(SoaReceiver::owner,
        context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]));
    end_record(context.declarations.soas.name[context.declarations.soas.index(schema)]);
    return LoweredSoa{header.build(), source.build()};
}
}
