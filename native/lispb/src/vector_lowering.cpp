#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <utility>

namespace codegen::detail {

auto lower_vector(VectorSoaSchema const& vector,
                  SoaBackend const backend,
                  std::map<std::string, CppType> const& types) -> DeclarationEmission {
    if (backend == SoaBackend::standard_library) {
        std::vector<SoaMemberSchema> members;
        for (auto const& component : vector.components) {
            members.push_back(SoaMemberSchema{component, SoaMemberKind::array, vector.value_type});
        }
        SoaSchema schema{
            .name = vector.name,
            .members = std::move(members),
            .operations = all_storage_operations(),
            .equivalent_type = vector.equivalent_type,
            .copy_element_memberwise = true,
        };
        std::map<std::string, SoaSchema const*> const schemas{{schema.name, &schema}};
        auto lowered{lower_native_soa(schema,
                                      schemas,
                                      types,
                                      true,
                                      vector.equivalent_members,
                                      vector.equivalent_constructor.value_or(""))};
        return {.header = std::move(lowered.header), .format_generated = true};
    }

    auto const value_type{resolve_type(vector.value_type, types)};
    std::vector<SoaMemberSchema> members;
    for (auto const& component : vector.components) {
        members.push_back(SoaMemberSchema{component, SoaMemberKind::array, vector.value_type});
    }
    std::vector<ParameterSchema> add_parameters;
    std::vector<ParameterSchema> view_set_parameters{ParameterSchema{TypeRef{"int32 const"}, "i"}};
    for (auto const& component : vector.components) {
        add_parameters.push_back(ParameterSchema{
            TypeRef{.name = "value_type", .suffix = " const"},
            std::string(1, component.front()),
        });
        auto component_type{vector.value_type};
        component_type.suffix += " const";
        view_set_parameters.push_back(
            ParameterSchema{std::move(component_type), std::string(1, component.front())});
    }
    std::vector<std::string> add_body{"auto const index{" + vector.components.front() + ".Add(" +
                                      std::string(1, vector.components.front().front()) + ")};"};
    for (std::size_t index{1}; index < vector.components.size(); ++index) {
        add_body.push_back(vector.components[index] + ".Add(" +
                           std::string(1, vector.components[index].front()) + ");");
    }
    add_body.emplace_back("return index;");
    std::vector<std::string> equivalent_arguments;
    std::vector<std::string> set_body;
    std::vector<std::string> data_pointers;
    static std::vector<std::string> const axes{"X", "Y", "Z"};
    auto const& equivalent_members{vector.equivalent_members.empty() ? axes
                                                                     : vector.equivalent_members};
    for (std::size_t index{0}; index < vector.components.size(); ++index) {
        equivalent_arguments.push_back("value." + equivalent_members[index]);
        set_body.push_back(vector.components[index] +
                           "[i] = " + std::string(1, vector.components[index].front()) + ";");
        data_pointers.push_back(vector.components[index] + ".GetData()");
    }
    auto const joined_data_pointers{join(data_pointers, ", ")};
    std::vector<FunctionSchema> functions{
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return Data{" + joined_data_pointers + "};"},
                       .trailing_return_type = TypeRef{"Data"},
                       .is_inline = true},
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return ConstData{" + joined_data_pointers + "};"},
                       .trailing_return_type = TypeRef{"ConstData"},
                       .is_const = true,
                       .is_inline = true},
        FunctionSchema{.name = "add",
                       .return_type = TypeRef{"auto"},
                       .parameters = add_parameters,
                       .body_lines = std::move(add_body),
                       .trailing_return_type = TypeRef{"size_type"},
                       .is_inline = true},
        FunctionSchema{
            .name = "add",
            .return_type = TypeRef{"auto"},
            .parameters = {ParameterSchema{
                TypeRef{.name = vector.equivalent_type.name, .suffix = " const&"}, "value"}},
            .body_lines = {"return add(" + join(equivalent_arguments, ", ") + ");"},
            .trailing_return_type = TypeRef{"size_type"},
            .is_inline = true},
        FunctionSchema{.name = "set",
                       .return_type = TypeRef{"void"},
                       .parameters =
                           [&] {
                               std::vector<ParameterSchema> parameters{
                                   ParameterSchema{TypeRef{"int32 const"}, "i"}};
                               parameters.insert(
                                   parameters.end(), add_parameters.begin(), add_parameters.end());
                               return parameters;
                           }(),
                       .body_lines = set_body,
                       .is_inline = true},
        FunctionSchema{.name = "set",
                       .return_type = TypeRef{"void"},
                       .parameters = {ParameterSchema{TypeRef{"int32 const"}, "i"},
                                      ParameterSchema{TypeRef{.name = vector.equivalent_type.name,
                                                              .suffix = " const"},
                                                      "value"}},
                       .body_lines = {"set(i, " + join(equivalent_arguments, ", ") + ");"},
                       .is_inline = true},
    };
    auto view_equivalent_type{vector.equivalent_type};
    view_equivalent_type.suffix += " const";
    std::vector<FunctionSchema> mutable_view_functions{
        FunctionSchema{
            .name = "set",
            .return_type = TypeRef{"void"},
            .parameters = std::move(view_set_parameters),
            .body_lines = std::move(set_body),
            .is_const = true,
            .is_inline = true,
        },
        FunctionSchema{
            .name = "set",
            .return_type = TypeRef{"void"},
            .parameters = {ParameterSchema{TypeRef{"int32 const"}, "i"},
                           ParameterSchema{std::move(view_equivalent_type), "value"}},
            .body_lines = {"set(i, " + join(equivalent_arguments, ", ") + ");"},
            .is_const = true,
            .is_inline = true,
        },
    };
    for (auto const& [name, method] : std::vector<std::pair<std::string, std::string>>{
             {"empty", "Empty()"},
             {"set_num_uninitialised", "SetNumUninitialized(count)"},
             {"add_zeroed", "AddZeroed(count)"}}) {
        std::vector<std::string> body;
        for (auto const& component : vector.components) {
            body.push_back(component + "." + method + ";");
        }
        functions.push_back(FunctionSchema{
            .name = name,
            .return_type = TypeRef{"void"},
            .parameters = name == "empty"
                            ? std::vector<ParameterSchema>{}
                            : std::vector<ParameterSchema>{ParameterSchema{
                                  TypeRef{.name = "size_type", .suffix = " const"}, "count"}},
            .body_lines = std::move(body),
            .is_inline = true,
        });
    }
    SoaSchema schema{
        .name = vector.name,
        .members = std::move(members),
        .operations = all_storage_operations(),
        .export_specifier = vector.export_specifier,
        .functions = std::move(functions),
        .mutable_view_functions = std::move(mutable_view_functions),
        .using_declarations = {"value_type = " + value_type.spelling,
                               "size_type = TArray<value_type>::SizeType"},
        .equivalent_type = vector.equivalent_type,
        .copy_element_memberwise = true,
        .fixed = vector.fixed,
    };
    auto pointer_struct = [&](std::string name, std::string const& pointer_suffix) {
        NodeListBuilder members;
        for (auto const& component : vector.components) {
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
    return {.header = std::move(lowered.header), .source = std::move(lowered.source)};
}

} // namespace codegen::detail
