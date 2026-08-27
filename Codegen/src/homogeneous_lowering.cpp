#include "lowering.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const soa_permutation{
    "ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const fill_indices{"ml::fill_indices", "SandboxCore/array_utils.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto homogeneous_view_text(HomogeneousLayoutSchema const& layout) -> std::string {
    auto const view_name{"T" + layout.name + "View"};
    auto const components{join(layout.components, ", ")};
    std::vector<std::string> member_lines;
    for (auto const& component : layout.components) {
        member_lines.push_back("    TArrayView<T> " + component + ";");
    }
    auto slice_values = [&](std::string const& operation) {
        std::vector<std::string> values;
        for (auto const& component : layout.components) {
            values.push_back(component + "." + operation);
        }
        return join(values, ", ");
    };
    return "template <typename T>\n"
           "struct " + view_name + " {\n"
           "    using size_type = TArrayView<T>::SizeType;\n"
           "    using value_type = std::remove_const_t<T>;\n"
           "    using View = " + view_name + "<T>;\n"
           "    using ConstView = " + view_name + "<value_type const>;\n\n" +
           join_lines(member_lines) + "\n\n"
           "    auto get_view() -> View { return View{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) -> View {\n"
           "        return get_view().slice(offset, count);\n"
           "    }\n"
           "    auto get_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) const -> ConstView {\n"
           "        return get_view().slice(offset, count);\n"
           "    }\n"
           "    auto get_const_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_const_view(size_type const offset, size_type const count) const -> ConstView {\n"
           "        return get_const_view().slice(offset, count);\n"
           "    }\n"
           "    template <typename TFunc>\n"
           "    auto apply_arrays(TFunc&& func) -> decltype(auto) {\n"
           "        return std::forward<TFunc>(func)(" + components + ");\n"
           "    }\n"
           "    template <typename TFunc>\n"
           "    auto apply_arrays(TFunc&& func) const -> decltype(auto) {\n"
           "        return std::forward<TFunc>(func)(" + components + ");\n"
           "    }\n"
           "    auto num() const -> size_type { return " + layout.components.front() + ".Num(); }\n"
           "    auto is_empty() const -> bool { return num() == 0; }\n"
           "    auto slice(size_type const offset, size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Slice(offset, count)") + "};\n"
           "    }\n"
           "    auto left(size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Left(count)") + "};\n"
           "    }\n"
           "    auto right(size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Right(count)") + "};\n"
           "    }\n"
           "};";
}

auto homogeneous_storage_text(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> std::string {
    auto const value_type{resolve_type(value.type, types)};
    auto const view_name{"T" + layout.name + "View"};
    auto const storage_name{"F" + layout.name + value.suffix};
    auto const export_prefix{layout.export_specifier.has_value()
                                 ? *layout.export_specifier + " "
                                 : std::string{}};
    std::vector<std::string> data_members;
    std::vector<std::string> const_data_members;
    std::vector<std::string> arrays;
    std::vector<std::string> pointers;
    std::vector<std::string> calls;
    for (auto const& component : layout.components) {
        data_members.push_back("        value_type* " + component + ";");
        const_data_members.push_back("        value_type const* " + component + ";");
        arrays.push_back("    TArray<value_type> " + component + ";");
        pointers.push_back(component + ".GetData()");
    }
    auto each = [&](std::string const& expression) {
        std::vector<std::string> lines;
        for (auto const& component : layout.components) {
            auto line{expression};
            auto marker{line.find("{}")};
            while (marker != std::string::npos) {
                line.replace(marker, 2, component);
                marker = line.find("{}", marker + component.size());
            }
            lines.push_back("        " + line);
        }
        return join_lines(lines);
    };
    auto const components{join(layout.components, ", ")};
    std::vector<std::string> validation;
    for (std::size_t index{1}; index < layout.components.size(); ++index) {
        validation.push_back("        check(" + layout.components[index] + ".Num() == " +
                             layout.components.front() + ".Num());");
    }
    static_cast<void>(calls);
    return "struct " + export_prefix + storage_name + " {\n"
           "    using value_type = " + value_type.spelling + ";\n"
           "    using size_type = TArray<value_type>::SizeType;\n"
           "    using View = " + view_name + "<value_type>;\n"
           "    using ConstView = " + view_name + "<value_type const>;\n\n"
           "    struct Data {\n" + join_lines(data_members) + "\n    };\n\n"
           "    struct ConstData {\n" + join_lines(const_data_members) + "\n    };\n\n" +
           join_lines(arrays) + "\n\n"
           "    auto get_data() -> Data { return Data{" + join(pointers, ", ") + "}; }\n"
           "    auto get_data() const -> ConstData { return ConstData{" + join(pointers, ", ") + "}; }\n"
           "    auto get_view() -> View { return View{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) -> View { return get_view().slice(offset, count); }\n"
           "    auto get_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) const -> ConstView { return get_view().slice(offset, count); }\n"
           "    auto get_const_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_const_view(size_type const offset, size_type const count) const -> ConstView { return get_const_view().slice(offset, count); }\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) -> decltype(auto) { return std::forward<TFunc>(func)(" + components + "); }\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) const -> decltype(auto) { return std::forward<TFunc>(func)(" + components + "); }\n"
           "    auto num() const -> size_type { return " + layout.components.front() + ".Num(); }\n"
           "    void validate_array_sizes() const {\n" + join_lines(validation) + "\n    }\n"
           "    auto is_empty() const -> bool { return num() == 0; }\n"
           "    template <typename Other> auto copy_element(size_type const dst_i, Other const& src, size_type const src_i) -> void {\n" +
           each("{}[dst_i] = src.{}[src_i];") + "\n    }\n"
           "    template <typename Other> auto copy_elements(size_type const dst_i, Other const& src, size_type const src_i, size_type const count) -> void {\n"
           "        for (auto i{0}; i < count; ++i) {\n" +
           each("    {}[dst_i + i] = src.{}[src_i + i];") + "\n        }\n    }\n"
           "    template <typename Other> auto copy_to_tail(Other const& src) -> void {\n"
           "        auto const count{src.num()};\n        check(num() >= count);\n"
           "        copy_elements(num() - count, src, 0, count);\n    }\n"
           "    template <typename Other> void append_from(Other const& other) {\n" +
           each("{}.Append(other.{});") + "\n    }\n"
           "    void apply_permutation(TArrayView<int32> indices);\n"
           "    template <typename Compare> void sort(Compare&& compare, TArrayView<int32> scratch_indices) {\n"
           "        validate_array_sizes();\n        auto const n{num()};\n"
           "        check(scratch_indices.Num() == n);\n        ml::fill_indices(scratch_indices);\n"
           "        scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) { return compare(*this, lhs, rhs); });\n"
           "        apply_permutation(scratch_indices);\n    }\n"
           "    template <auto Compare> void sort(TArrayView<int32> scratch_indices) {\n"
           "        validate_array_sizes();\n        auto const n{num()};\n"
           "        check(scratch_indices.Num() == n);\n        ml::fill_indices(scratch_indices);\n"
           "        scratch_indices.Sort([this](int32 const lhs, int32 const rhs) { return Compare(*this, lhs, rhs); });\n"
           "        apply_permutation(scratch_indices);\n    }\n"
           "    auto reset() -> void {\n" + each("{}.Reset();") + "\n    }\n"
           "    auto empty() -> void {\n" + each("{}.Empty();") + "\n    }\n"
           "    auto reserve(size_type const count) -> void {\n" + each("{}.Reserve(count);") + "\n    }\n"
           "    auto set_num(size_type const count, EAllowShrinking const allow_shrinking) -> void {\n" + each("{}.SetNum(count, allow_shrinking);") + "\n    }\n"
           "    auto set_num_uninitialised(size_type const count) -> void {\n" + each("{}.SetNumUninitialized(count);") + "\n    }\n"
           "    auto add_uninitialised(size_type const count) -> void {\n" + each("{}.AddUninitialized(count);") + "\n    }\n"
           "    auto remove_at_swap(size_type const index, size_type const count, EAllowShrinking const allow_shrinking) -> void {\n" + each("{}.RemoveAtSwap(index, count, allow_shrinking);") + "\n    }\n"
           "    auto add_zeroed(size_type const count) -> void {\n" + each("{}.AddZeroed(count);") + "\n    }\n"
           "    auto add_defaulted(size_type const count) -> void {\n" + each("{}.AddDefaulted(count);") + "\n    }\n"
           "};";
}

auto lower_homogeneous_module_impl(HomogeneousModuleSchema const& module,
                                   std::map<std::string, CppType> const& types) -> Module {
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    Nodes source_nodes{Include{source_include(module.settings), false},
                       lines(2),
                       IncludeDependencies{},
                       lines(2)};
    for (std::size_t layout_index{0}; layout_index < module.layouts.size(); ++layout_index) {
        auto const& layout{module.layouts[layout_index]};
        header_nodes.push_back(raw(homogeneous_view_text(layout),
                                   {tarray_view, std_remove_const, std_forward}));
        header_nodes.push_back(lines(2));
        for (std::size_t value_index{0}; value_index < layout.value_types.size(); ++value_index) {
            auto const& value{layout.value_types[value_index]};
            auto value_type{resolve_type(value.type, types)};
            std::vector<TypeDependency> dependencies{
                tarray, tarray_view, allow_shrinking, check_dependency, fill_indices, std_forward};
            dependencies.insert(dependencies.end(),
                                value_type.dependencies.begin(),
                                value_type.dependencies.end());
            header_nodes.push_back(raw(homogeneous_storage_text(layout, value, types),
                                       std::move(dependencies)));
            if (value_index + 1 < layout.value_types.size()) {
                header_nodes.push_back(lines(2));
            }

            auto const storage_name{"F" + layout.name + value.suffix};
            std::vector<std::string> body{
                "validate_array_sizes();", "check(indices.Num() == num());"};
            for (auto const& component : layout.components) {
                body.push_back("ml::apply_permutation(" + component + ", indices);");
            }
            source_nodes.push_back(raw(
                "void " + storage_name + "::apply_permutation(TArrayView<int32> indices) {\n" +
                    join_lines([&] {
                        std::vector<std::string> indented;
                        for (auto const& line : body) {
                            indented.push_back("    " + line);
                        }
                        return indented;
                    }()) +
                    "\n}",
                {tarray_view, check_dependency, soa_permutation}));
            if (layout_index + 1 < module.layouts.size() ||
                value_index + 1 < layout.value_types.size()) {
                source_nodes.push_back(lines(2));
            }
        }
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{
                            .path = *module.settings.source,
                            .nodes = std::move(source_nodes),
                            .pragma_once = false,
                            .clang_format_off = true,
                            .include_order = module.settings.include_order,
                        }}
                      : std::nullopt,
    };
}

} // namespace

auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module {
    return lower_homogeneous_module_impl(module, types);
}

} // namespace codegen::detail
