#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <stdexcept>
#include <utility>

namespace codegen::detail {

auto lower_vector_module(VectorModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    auto const equivalent{resolve_type(module.equivalent_type, types)};
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
    static std::vector<std::string> const axes{"X", "Y", "Z"};
    for (std::size_t index{0}; index < module.components.size(); ++index) {
        equivalent_arguments.push_back("value." + axes[index]);
    }
    std::vector<FunctionSchema> functions{
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return Data{" + [&] {
                           std::vector<std::string> values;
                           for (auto const& component : module.components) {
                               values.push_back(component + ".GetData()");
                           }
                           return join(values, ", ");
                       }() + "};"},
                       .suffix = " -> Data",
                       .is_inline = true},
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return ConstData{" + [&] {
                           std::vector<std::string> values;
                           for (auto const& component : module.components) {
                               values.push_back(component + ".GetData()");
                           }
                           return join(values, ", ");
                       }() + "};"},
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
        .using_declarations = {"value_type = " + resolve_type(module.value_type, types).spelling,
                               "size_type = TArray<value_type>::SizeType"},
        .equivalent_type = module.equivalent_type,
        .copy_element_memberwise = true,
        .fixed = module.fixed,
    };
    auto lowered{lower_soa(schema, types)};
    auto* storage{lowered.header.back().get_if<Struct>()};
    if (storage == nullptr) {
        throw std::logic_error{"Vector SOA lowering did not produce a storage struct"};
    }
    Nodes data_nodes{
        Struct{.name = "Data",
               .children = [&] {
                   Nodes values;
                   auto const pointer_type{resolve_type(module.value_type, types).spelling + "*"};
                   for (auto const& component : module.components) {
                       values.push_back(Member{CppType{pointer_type}, component});
                   }
                   return values;
               }()},
        lines(2),
        Struct{.name = "ConstData",
               .children = [&] {
                   Nodes values;
                   auto const pointer_type{resolve_type(module.value_type, types).spelling +
                                           " const*"};
                   for (auto const& component : module.components) {
                       values.push_back(Member{CppType{pointer_type}, component});
                   }
                   return values;
               }()},
        lines(2),
    };
    storage->children.insert(storage->children.begin() + 4,
                             data_nodes.begin(),
                             data_nodes.end());
    if (schema.fixed.has_value()) {
        std::map<std::string, SoaSchema const*> const schemas{{schema.name, &schema}};
        lowered.header.push_back(lines(2));
        auto fixed{lower_fixed_nodes(schema, schemas, types)};
        lowered.header.insert(lowered.header.end(), fixed.begin(), fixed.end());
    }
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    header_nodes.insert(header_nodes.end(), lowered.header.begin(), lowered.header.end());
    Nodes source_nodes{Include{source_include(module.settings), false},
                       lines(2),
                       IncludeDependencies{},
                       lines(2)};
    source_nodes.insert(source_nodes.end(), lowered.source.begin(), lowered.source.end());
    return Module{
        .name = module.settings.name,
        .header = CppFile{.path = module.settings.header,
                          .nodes = std::move(header_nodes),
                          .clang_format_off = true,
                          .include_order = module.settings.include_order},
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{.path = *module.settings.source,
                                                       .nodes = std::move(source_nodes),
                                                       .pragma_once = false,
                                                       .clang_format_off = true,
                                                       .include_order =
                                                           module.settings.include_order}}
                      : std::nullopt,
    };
}

} // namespace codegen::detail
