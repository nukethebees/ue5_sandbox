#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const fixed_storage_dependency{
    "ml::TFixedStorage", "SandboxCore/fixed_storage.h", {}};
TypeDependency const move_temp{"MoveTemp", "Templates/UnrealTemplate.h", {}};

auto fixed_storage_type(FixedMemberLayout const& member) -> std::string {
    if (member.member.kind == SoaMemberKind::array) {
        return "ml::TFixedStorage<" + member.member.element_type.spelling + ", Capacity>";
    }
    return *member.nested_storage_name + "<Capacity>";
}

auto fixed_member_view(FixedMemberLayout const& member, bool is_const) -> std::string {
    if (member.member.kind == SoaMemberKind::array) {
        return std::string{is_const ? "TConstArrayView<" : "TArrayView<"} +
               member.member.element_type.spelling + ">{" + member.member.name +
               "_.data() + offset, count}";
    }
    return member.member.name +
           (is_const ? "_.get_const_view(offset, count)" : "_.get_view(offset, count)");
}

auto fixed_construct_lines(FixedLayout const& layout,
                           std::string const& operation,
                           std::vector<std::string> const& arguments = {})
    -> std::vector<std::string> {
    std::vector<std::string> result;
    std::size_t argument_index{};
    for (auto const& member : layout.members) {
        auto const name{member.member.name + "_"};
        auto const leaf_count{member.leaves.size()};
        std::vector<std::string> member_arguments;
        for (std::size_t index{0}; index < leaf_count && argument_index < arguments.size();
             ++index) {
            member_arguments.push_back(arguments[argument_index++]);
        }
        if (operation == "arguments") {
            result.push_back(name + ".construct_at(index, " + join(member_arguments, ", ") + ");");
        } else if (operation == "default") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index);"
                                 : name + ".default_construct_at(index);");
        } else if (operation == "copy") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index, other." + name + "[other_index]);"
                                 : name + ".copy_construct_at(index, other." + name +
                                       ", other_index);");
        } else if (operation == "move") {
            result.push_back(
                member.member.kind == SoaMemberKind::array
                    ? name + ".construct_at(index, MoveTemp(other." + name + "[other_index]));"
                    : name + ".move_construct_at(index, other." + name + ", other_index);");
        } else if (operation == "view") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index, source." + member.member.name +
                                       "[source_index]);"
                                 : name + ".construct_from_view_at(index, source." +
                                       member.member.name + ", source_index);");
        }
    }
    return result;
}

} // namespace

auto fixed_storage_node(FixedLayout const& layout) -> Node {
    auto const& schema{*layout.schema};
    auto const& storage_name{schema.fixed->storage_name};
    std::vector<std::string> template_parameters;
    std::vector<FunctionParameter> function_parameters;
    std::vector<std::string> forwarded;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{fixed_leaf_argument(layout.leaves[index])};
        template_parameters.push_back("typename TArg" + std::to_string(index));
        function_parameters.emplace_back("TArg" + std::to_string(index) + "&&", "new_" + argument);
        forwarded.push_back("std::forward<TArg" + std::to_string(index) + ">(new_" + argument +
                            ")");
    }
    std::vector<std::string> mutable_views;
    std::vector<std::string> const_views;
    std::vector<std::string> copy_assign;
    std::vector<std::string> move_assign;
    for (auto const& member : layout.members) {
        mutable_views.push_back(fixed_member_view(member, false));
        const_views.push_back(fixed_member_view(member, true));
        auto const name{member.member.name + "_"};
        copy_assign.push_back(member.member.kind == SoaMemberKind::array
                                  ? name + "[dst_index] = source." + member.member.name +
                                        "[source_index];"
                                  : name + ".copy_assign_from_view_at(dst_index, source." +
                                        member.member.name + ", source_index);");
        move_assign.push_back(
            member.member.kind == SoaMemberKind::array
                ? name + "[dst_index] = MoveTemp(other." + name + "[source_index]);"
                : name + ".move_assign_at(dst_index, other." + name + ", source_index);");
    }
    std::vector<std::string> destroy;
    for (auto iterator{layout.members.rbegin()}; iterator != layout.members.rend(); ++iterator) {
        destroy.push_back(iterator->member.name + "_.destroy_at(index);");
    }
    auto function = [](std::string name,
                       CppType return_type,
                       std::vector<FunctionParameter> parameters,
                       std::string body,
                       FunctionQualifiers qualifiers = {},
                       std::vector<TypeDependency> dependencies = {},
                       std::optional<std::string> function_template = std::nullopt) {
        return header_function(FunctionSpec{
            .name = std::move(name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body), std::move(dependencies))},
            .qualifiers = std::move(qualifiers),
            .is_inline = true,
            .template_parameters = std::move(function_template),
        });
    };

    NodeListBuilder children;
    children
        .add(UsingDeclaration{"View", CppType{schema.view_name.value_or(schema.name + "View")}}, 1)
        .add(UsingDeclaration{"ConstView",
                              CppType{schema.const_view_name.value_or(schema.name + "ConstView")}},
             2)
        .add(function("get_view",
                      "auto",
                      {FunctionParameter{"int32 const", "offset"},
                       FunctionParameter{"int32 const", "count"}},
                      "return View{" + join(mutable_views, ", ") + "};",
                      {.trailing_return_type = CppType{"View"}},
                      {tarray_view}),
             1)
        .add(function("get_const_view",
                      "auto",
                      {FunctionParameter{"int32 const", "offset"},
                       FunctionParameter{"int32 const", "count"}},
                      "return ConstView{" + join(const_views, ", ") + "};",
                      {.trailing_return_type = CppType{"ConstView"}, .is_const = true},
                      {tarray_view}),
             2)
        .add(function(
                 "construct_at",
                 "void",
                 [&] {
                     auto parameters{function_parameters};
                     parameters.insert(parameters.begin(),
                                       FunctionParameter{"int32 const", "index"});
                     return parameters;
                 }(),
                 join_lines(fixed_construct_lines(layout, "arguments", forwarded)),
                 {},
                 {std_forward},
                 join(template_parameters, ", ")),
             2)
        .add(function("default_construct_at",
                      "void",
                      {FunctionParameter{"int32 const", "index"}},
                      join_lines(fixed_construct_lines(layout, "default"))),
             2)
        .add(function("copy_construct_at",
                      "void",
                      {FunctionParameter{"int32 const", "index"},
                       FunctionParameter{storage_name + " const&", "other"},
                       FunctionParameter{"int32 const", "other_index"}},
                      join_lines(fixed_construct_lines(layout, "copy"))),
             2)
        .add(function("move_construct_at",
                      "void",
                      {FunctionParameter{"int32 const", "index"},
                       FunctionParameter{storage_name + "&", "other"},
                       FunctionParameter{"int32 const", "other_index"}},
                      join_lines(fixed_construct_lines(layout, "move")),
                      {},
                      {move_temp}),
             2)
        .add(function("construct_from_view_at",
                      "void",
                      {FunctionParameter{"int32 const", "index"},
                       FunctionParameter{"SourceView const&", "source"},
                       FunctionParameter{"int32 const", "source_index"}},
                      join_lines(fixed_construct_lines(layout, "view")),
                      {},
                      {},
                      "typename SourceView"),
             2)
        .add(function("copy_assign_from_view_at",
                      "void",
                      {FunctionParameter{"int32 const", "dst_index"},
                       FunctionParameter{"SourceView const&", "source"},
                       FunctionParameter{"int32 const", "source_index"}},
                      join_lines(copy_assign),
                      {},
                      {},
                      "typename SourceView"),
             2)
        .add(function("move_assign_at",
                      "void",
                      {FunctionParameter{"int32 const", "dst_index"},
                       FunctionParameter{storage_name + "&", "other"},
                       FunctionParameter{"int32 const", "source_index"}},
                      join_lines(move_assign),
                      {},
                      {move_temp}),
             2)
        .add(function("destroy_at",
                      "void",
                      {FunctionParameter{"int32 const", "index"}},
                      join_lines(destroy),
                      {.is_noexcept = true}),
             2);
    for (std::size_t index{0}; index < layout.members.size(); ++index) {
        auto const& member{layout.members[index]};
        auto dependencies{std::vector<TypeDependency>{fixed_storage_dependency}};
        dependencies.insert(dependencies.end(),
                            member.member.element_type.dependencies.begin(),
                            member.member.element_type.dependencies.end());
        children.add(Member{
            CppType{fixed_storage_type(member), std::move(dependencies)},
            member.member.name + "_",
        });
        if (index + 1 < layout.members.size()) {
            children.new_lines();
        }
    }

    return Struct{
        .name = storage_name,
        .children = children.build(),
        .template_parameters = "int32 Capacity",
        .requires_clause = "(Capacity >= 0)",
    };
}

} // namespace codegen::detail
