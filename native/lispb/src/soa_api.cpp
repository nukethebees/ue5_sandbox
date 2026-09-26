#include "soa_api.h"
#include "lowering_utils.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace codegen::detail {
auto uses_native_vector_view(SoaSchema const& schema, TypeRegistry const& types) -> bool {
    if (schema.equivalent_type) {
        auto const spelling{resolve_type(*schema.equivalent_type, types).spelling};
        if (spelling != "Vector3f" && spelling != "HMM_Vec3") {
            return false;
        }
    }
    if (schema.members.size() != 3 || !schema.const_view_functions.empty() ||
        !schema.mutable_view_functions.empty() || !schema.using_declarations.empty() ||
        schema.export_specifier) {
        return false;
    }
    constexpr std::array<std::string_view, 3> components{"xs", "ys", "zs"};
    for (std::size_t index{}; index < components.size(); ++index) {
        auto const& member{schema.members[index]};
        if (member.kind != SoaMemberKind::array || member.name != components[index] ||
            native_spelling(resolve_type(member.type, types).spelling) != "float") {
            return false;
        }
    }
    return true;
}
auto soa_function_spec(FunctionSchema const& schema, TypeRegistry const& types) -> FunctionSpec {
    std::vector<FunctionParameter> parameters;
    for (auto const& parameter : schema.parameters) {
        auto resolved{resolve_type(parameter.type, types)};
        if (parameter.default_value) {
            parameters.emplace_back(std::move(resolved), parameter.name, *parameter.default_value);
        } else {
            parameters.emplace_back(std::move(resolved), parameter.name);
        }
    }
    std::vector<TypeDependency> dependencies;
    for (auto const& key : schema.dependencies) {
        dependencies.push_back(dependency_for_key(key, types));
    }
    return FunctionSpec{
        .name = schema.name,
        .return_type = resolve_type(schema.return_type, types),
        .parameters = std::move(parameters),
        .body = {raw(join_lines(schema.body_lines), std::move(dependencies))},
        .qualifiers = {.trailing_return_type = schema.trailing_return_type
                                                 ? std::optional<CppType>{resolve_type(
                                                       *schema.trailing_return_type, types)}
                                                 : std::nullopt,
                       .is_const = schema.is_const,
                       .is_noexcept = schema.is_noexcept},
        .is_static = schema.is_static,
        .is_inline = schema.is_inline,
        .template_parameters = schema.template_parameters,
        .requires_clause = schema.requires_clause,
    };
}

auto logical_column_access(SoaSchema const& schema,
                           std::span<std::string const> const path,
                           SoaRepresentation const representation,
                           std::string receiver,
                           std::map<std::string, SoaSchema const*> const* schemas,
                           TypeRegistry const* types) -> std::string {
    if (path.empty()) {
        throw std::invalid_argument{"SOA column reference cannot be empty"};
    }
    auto const* current{&schema};
    for (std::size_t index{}; index < path.size(); ++index) {
        if (!receiver.empty() && !receiver.ends_with("->")) {
            receiver += ".";
        }
        auto const member{std::ranges::find(current->members, path[index], &SoaMemberSchema::name)};
        if (member == current->members.end()) {
            throw std::invalid_argument{"SOA '" + current->name + "' has no column '" +
                                        path[index] + "'"};
        }
        bool const nested{member->kind == SoaMemberKind::nested};
        receiver += representation == SoaRepresentation::compact && nested ? "view_" : "";
        receiver += path[index];
        if (representation == SoaRepresentation::compact ||
            (representation == SoaRepresentation::native_vector_view && types &&
             uses_native_vector_view(*current, *types))) {
            receiver += "()";
        }
        if (index + 1 < path.size()) {
            if (!nested || !member->nested_schema || !schemas) {
                throw std::invalid_argument{"SOA column path requires a resolved nested schema: " +
                                            path[index]};
            }
            current = schemas->at(*member->nested_schema);
        }
    }
    return receiver;
}

namespace {
auto expand_columns(std::string body,
                    SoaSchema const& schema,
                    SoaRepresentation const representation,
                    SoaReceiver const receiver,
                    std::map<std::string, SoaSchema const*> const* schemas,
                    TypeRegistry const& types) -> std::string {
    constexpr std::string_view marker{"$column("};
    std::size_t begin{};
    while ((begin = body.find(marker, begin)) != std::string::npos) {
        auto const end{body.find(')', begin + marker.size())};
        if (end == std::string::npos) {
            throw std::invalid_argument{"Unterminated $column reference in SOA '" + schema.name +
                                        "'"};
        }
        std::vector<std::string> path;
        auto part{begin + marker.size()};
        while (part <= end) {
            auto const separator{std::min(body.find('.', part), end)};
            auto name{body.substr(part, separator - part)};
            if (name.empty() ||
                name.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") !=
                    std::string::npos) {
                throw std::invalid_argument{"Invalid $column path in SOA '" + schema.name + "'"};
            }
            path.push_back(std::move(name));
            part = separator + 1;
        }
        auto const prefix{representation == SoaRepresentation::compact &&
                                  receiver == SoaReceiver::owner
                              ? "get_view()"
                              : "this->"};
        auto const replacement{
            logical_column_access(schema, path, representation, prefix, schemas, &types)};
        body.replace(begin, end + 1 - begin, replacement);
        begin += replacement.size();
    }
    return body;
}
}

auto lower_soa_api(SoaSchema const& schema,
                   TypeRegistry const& types,
                   SoaRepresentation const representation,
                   SoaReceiver const receiver,
                   std::string const& type_name,
                   std::map<std::string, SoaSchema const*> const* schemas,
                   SoaBackend const backend,
                   std::string_view const equivalent_constructor) -> LoweredSoa {
    NodeListBuilder header;
    NodeListBuilder source;
    for (auto const& alias : schema.using_declarations) {
        header.add(raw("using " + alias + ";")).new_lines(1);
    }
    auto append = [&](std::vector<FunctionSchema> const& functions) {
        for (auto function : functions) {
            for (auto& line : function.body_lines) {
                line = expand_columns(
                    std::move(line), schema, representation, receiver, schemas, types);
            }
            auto spec{soa_function_spec(function, types)};
            if (backend == SoaBackend::standard_library) {
                auto normalize = [](CppType& type) {
                    auto const physical{classify_physical_type_use(type.spelling)};
                    auto const native{native_spelling(physical.object_spelling)};
                    if (native != physical.object_spelling) {
                        auto const begin{type.spelling.find(physical.object_spelling)};
                        type.spelling.replace(begin, physical.object_spelling.size(), native);
                        type.dependencies.push_back({native, "cstdint", {}});
                    }
                };
                normalize(spec.return_type);
                if (spec.qualifiers.trailing_return_type) {
                    normalize(*spec.qualifiers.trailing_return_type);
                }
                for (auto& parameter : spec.parameters) {
                    normalize(parameter.type);
                }
            }
            if (function.definition_in_source) {
                header.add(declaration(spec)).new_lines(1);
                if (!spec.qualifiers.trailing_return_type && spec.return_type.spelling != "auto" &&
                    spec.return_type.spelling != "decltype(auto)") {
                    spec.qualifiers.trailing_return_type = spec.return_type;
                    spec.return_type = CppType{"auto"};
                }
                source.add(definition(spec, type_name)).new_lines(2);
            } else {
                spec.is_inline = spec.is_inline || !function.body_lines.empty();
                header.add(header_function(spec)).new_lines(1);
            }
        }
    };
    if (receiver == SoaReceiver::owner) {
        append(schema.functions);
    } else {
        append(schema.const_view_functions);
        if (receiver == SoaReceiver::mutable_view) {
            append(schema.mutable_view_functions);
        }
    }
    if (representation == SoaRepresentation::compact && schema.equivalent_type) {
        auto const equivalent{resolve_type(*schema.equivalent_type, types)};
        header.add(UsingDeclaration{"equivalent_type", equivalent}).new_lines(1);
        std::string expression;
        if (receiver == SoaReceiver::owner) {
            expression = "get_const_view()[index]";
        } else {
            expression = equivalent_constructor.empty() ? "equivalent_type{"
                                                        : std::string{equivalent_constructor} + "(";
            bool first{true};
            for (auto const& member : schema.members) {
                if (!first) {
                    expression += ", ";
                }
                first = false;
                std::vector<std::string> const path{member.name};
                expression += logical_column_access(schema, path, representation) + "[index]";
            }
            expression += equivalent_constructor.empty() ? "}" : ")";
        }
        header
            .add(header_function(FunctionSpec{
                .name = "operator[]",
                .return_type = "auto",
                .parameters = {FunctionParameter{"std::int32_t", "index"}},
                .body = {raw("return " + expression + ";")},
                .qualifiers = {.trailing_return_type = CppType{"equivalent_type"},
                               .is_const = true},
                .is_inline = true,
            }))
            .new_lines(1);
    }
    return {header.build(), source.build()};
}
}
