#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <utility>

namespace codegen::detail {

auto lower_vector_module(VectorModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    auto const value_type{resolve_type(module.value_type, types)};
    std::vector<SoaMemberSchema> members;
    for (auto const& component : module.components) {
        members.push_back(SoaMemberSchema{component, SoaMemberKind::array, module.value_type});
    }
    std::vector<ParameterSchema> add_parameters;
    for (auto const& component : module.components) {
        add_parameters.push_back(ParameterSchema{
            TypeRef{.name = "value_type", .suffix = " const"},
            std::string(1, component.front()),
        });
    }
    std::vector<std::string> add_body{
        "auto const index{" + module.components.front() + ".Add(" +
            std::string(1, module.components.front().front()) + ")};"};
    for (std::size_t index{1}; index < module.components.size(); ++index) {
        add_body.push_back(module.components[index] + ".Add(" +
                           std::string(1, module.components[index].front()) + ");");
    }
    add_body.emplace_back("return index;");
    std::vector<std::string> equivalent_arguments;
    std::vector<std::string> data_pointers;
    static std::vector<std::string> const axes{"X", "Y", "Z"};
    for (std::size_t index{0}; index < module.components.size(); ++index) {
        equivalent_arguments.push_back("value." + axes[index]);
        data_pointers.push_back(module.components[index] + ".GetData()");
    }
    auto const joined_data_pointers{join(data_pointers, ", ")};
    std::vector<FunctionSchema> functions{
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return Data{" + joined_data_pointers + "};"},
                       .suffix = " -> Data",
                       .is_inline = true},
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return ConstData{" + joined_data_pointers + "};"},
                       .suffix = " const -> ConstData",
                       .is_inline = true},
        FunctionSchema{.name = "add",
                       .return_type = TypeRef{"auto"},
                       .parameters = std::move(add_parameters),
                       .body_lines = std::move(add_body),
                       .suffix = " -> size_type",
                       .is_inline = true},
        FunctionSchema{.name = "add",
                       .return_type = TypeRef{"auto"},
                       .parameters = {ParameterSchema{
                           TypeRef{.name = module.equivalent_type.name,
                                   .suffix = " const&"},
                           "value"}},
                       .body_lines = {"return add(" + join(equivalent_arguments, ", ") + ");"},
                       .suffix = " -> size_type",
                       .is_inline = true},
    };
    for (auto const& [name, method] : std::vector<std::pair<std::string, std::string>>{
             {"empty", "Empty()"},
             {"set_num_uninitialised", "SetNumUninitialized(count)"},
             {"add_zeroed", "AddZeroed(count)"}}) {
        std::vector<std::string> body;
        for (auto const& component : module.components) {
            body.push_back(component + "." + method + ";");
        }
        functions.push_back(FunctionSchema{
            .name = name,
            .return_type = TypeRef{"void"},
            .parameters = name == "empty"
                              ? std::vector<ParameterSchema>{}
                              : std::vector<ParameterSchema>{ParameterSchema{
                                    TypeRef{.name = "size_type", .suffix = " const"},
                                    "count"}},
            .body_lines = std::move(body),
            .is_inline = true,
        });
    }
    SoaSchema schema{
        .name = module.storage_name,
        .members = std::move(members),
        .operations = all_storage_operations(),
        .export_specifier = module.export_specifier,
        .functions = std::move(functions),
        .using_declarations = {"value_type = " + value_type.spelling,
                               "size_type = TArray<value_type>::SizeType"},
        .equivalent_type = module.equivalent_type,
        .copy_element_memberwise = true,
        .fixed = module.fixed,
    };
    auto pointer_struct = [&](std::string name, std::string const& pointer_suffix) {
        NodeListBuilder members;
        for (auto const& component : module.components) {
            members.add(Member{CppType{value_type.spelling + pointer_suffix}, component});
        }
        return Struct{.name = std::move(name), .children = members.build()};
    };
    NodeListBuilder storage_prelude;
    storage_prelude.add(pointer_struct("Data", "*"), 2)
        .add(pointer_struct("ConstData", " const*"), 2);
    auto lowered{lower_soa(schema, types, storage_prelude.build())};
    if (schema.fixed.has_value()) {
        std::map<std::string, SoaSchema const*> const schemas{{schema.name, &schema}};
        NodeListBuilder header;
        header.append(std::move(lowered.header))
            .new_lines(2)
            .append(lower_fixed_nodes(schema, schemas, types));
        lowered.header = header.build();
    }
    auto header_definitions{std::move(lowered.header)};
    auto source_definitions{std::move(lowered.source)};
    if (module.settings.namespace_name.has_value()) {
        header_definitions = {
            Namespace{*module.settings.namespace_name, std::move(header_definitions)}};
        source_definitions = {
            Namespace{*module.settings.namespace_name, std::move(source_definitions)}};
    }
    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.append(std::move(header_definitions));
    NodeListBuilder source_nodes;
    source_nodes.add(Include{source_include(module.settings), false}, 2)
        .add(IncludeDependencies{}, 2)
        .append(std::move(source_definitions));
    return Module{
        .name = module.settings.name,
        .header = CppFile{.path = module.settings.header,
                          .nodes = header_nodes.build(),
                          .clang_format_off = true,
                          .include_order = module.settings.include_order},
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{.path = *module.settings.source,
                                                       .nodes = source_nodes.build(),
                                                       .pragma_once = false,
                                                       .clang_format_off = true,
                                                       .include_order =
                                                           module.settings.include_order}}
                      : std::nullopt,
    };
}

} // namespace codegen::detail
