#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <array>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace codegen::detail {
namespace {
auto native_spelling(std::string const& spelling) -> std::string {
    constexpr std::array integer_types{
        std::pair{"int8", "std::int8_t"},
        std::pair{"uint8", "std::uint8_t"},
        std::pair{"int16", "std::int16_t"},
        std::pair{"uint16", "std::uint16_t"},
        std::pair{"int32", "std::int32_t"},
        std::pair{"uint32", "std::uint32_t"},
        std::pair{"int64", "std::int64_t"},
        std::pair{"uint64", "std::uint64_t"},
    };
    for (auto const& [source, destination] : integer_types) {
        if (spelling == source) {
            return destination;
        }
    }
    return spelling;
}

auto all_members_are_arrays(FixedLayout const& layout) -> bool {
    return std::ranges::all_of(layout.members, [](auto const& member) {
        return member.schema->kind == SoaMemberKind::array;
    });
}

void render_parameters(std::ostringstream& out, FixedLayout const& layout) {
    for (auto const& member : layout.members) {
        out << ", " << native_spelling(member.member.element_type.spelling) << " const new_"
            << member.schema->name;
    }
}

void render_arguments(std::ostringstream& out, FixedLayout const& layout) {
    for (auto const& member : layout.members) {
        out << ", new_" << member.schema->name;
    }
}
}

auto lower_native_soa(SoaSchema const& schema,
                      std::map<std::string, SoaSchema const*> const& schemas,
                      std::map<std::string, CppType> const& types,
                      bool const allow_equivalent_type) -> LoweredSoa {
    if (schema.fixed || !schema.functions.empty() || !schema.mutable_view_functions.empty() ||
        !schema.using_declarations.empty() ||
        (schema.equivalent_type.has_value() && !allow_equivalent_type)) {
        throw std::invalid_argument{"Standard-library SoA does not support custom functions, "
                                    "fixed storage, using declarations or equivalent types"};
    }
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto const equivalent_type{schema.equivalent_type.has_value()
                                   ? std::optional{resolve_type(*schema.equivalent_type, types)}
                                   : std::nullopt};
    for (auto const& leaf : layout.leaves) {
        if (leaf.type.spelling == "bool") {
            throw std::invalid_argument{"Standard-library SoA requires contiguous columns; "
                                        "std::vector<bool> is unsupported"};
        }
    }

    auto const view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<TypeDependency> dependencies{
        {"native_storage", "native_soa/storage.h", {}},
        {"soa_permutation", "sandbox/core/soa_permutation.h", {}},
    };
    if (equivalent_type.has_value()) {
        dependencies.insert(dependencies.end(),
                            equivalent_type->dependencies.begin(),
                            equivalent_type->dependencies.end());
    }
    std::ostringstream out;

    out << "struct " << view << ";\n"
        << "struct " << const_view << ";\n";

    for (bool const immutable : {true, false}) {
        auto const view_type{immutable ? const_view : view};
        out << "struct " << view_type << " {\n"
            << "using View = " << view << ";\n"
            << "using ConstView = " << const_view << ";\n"
            << "using size_type = std::int32_t;\n";
        if (equivalent_type.has_value()) {
            out << "using equivalent_type = " << native_spelling(equivalent_type->spelling) << ";\n"
                << "auto operator[](size_type const index) const -> equivalent_type { return {";
            for (std::size_t index{}; index < layout.members.size(); ++index) {
                if (index > 0) {
                    out << ", ";
                }
                out << layout.members[index].schema->name << "[static_cast<std::size_t>(index)]";
            }
            out << "}; }\n";
        }
        for (auto const& member : layout.members) {
            auto const& resolved{member.member};
            if (member.schema->kind == SoaMemberKind::array) {
                out << "std::span<" << native_spelling(resolved.element_type.spelling)
                    << (immutable ? " const" : "") << "> " << member.schema->name << ";\n";
            } else {
                auto const& child{*schemas.at(*member.schema->nested_schema)};
                out << (immutable ? child.const_view_name.value_or(child.name + "ConstView")
                                  : child.view_name.value_or(child.name + "View"))
                    << " " << member.schema->name << ";\n";
            }
        }

        out << "auto num() const noexcept -> size_type { return static_cast<size_type>("
            << join(layout.leaves.front().path, ".") << ".size()); }\n"
            << "auto is_empty() const noexcept -> bool { return num() == 0; }\n"
            << "template <typename Fn> void each_column(Fn&& fn) const {\n";
        for (auto const& leaf : layout.leaves) {
            out << "fn(" << join(leaf.path, ".") << ");\n";
        }
        out << "}\n"
            << "void validate_array_sizes() const { auto const count{num()}; "
               "each_column([count](auto "
               "column) { ml::native_soa::require(column.size() == "
               "static_cast<std::size_t>(count)); }); }\n"
            << "auto slice(size_type const offset, size_type const count) const -> " << view_type
            << " { ml::native_soa::require(offset >= 0 && count >= 0 && offset <= num() && count "
               "<= num() - offset); return {\n";
        for (auto const& member : layout.members) {
            out << member.schema->name
                << (member.schema->kind == SoaMemberKind::array
                        ? ".subspan(static_cast<std::size_t>(offset), "
                          "static_cast<std::size_t>(count))"
                        : ".slice(offset, count)")
                << ",\n";
        }
        out << "}; }\n"
            << "auto get_view() const -> " << view_type << " { return *this; }\n"
            << "auto get_view(size_type const offset, size_type const count) const -> " << view_type
            << " { return slice(offset, count); }\n"
            << "auto get_const_view() const -> ConstView { return {\n";
        for (auto const& member : layout.members) {
            out << member.schema->name;
            if (member.schema->kind == SoaMemberKind::nested) {
                out << ".get_const_view()";
            }
            out << ",\n";
        }
        out << "}; }\n"
            << "auto get_const_view(size_type const offset, size_type const count) const -> "
               "ConstView { return get_const_view().slice(offset, count); }\n"
            << "auto left(size_type const count) const -> " << view_type
            << " { return slice(0, count); }\n"
            << "auto right(size_type const count) const -> " << view_type
            << " { return slice(num() - count, count); }\n";

        if (!immutable && all_members_are_arrays(layout)) {
            out << "void set(size_type const index";
            render_parameters(out, layout);
            out << ") const { ml::native_soa::require(index >= 0 && index < num());\n";
            for (auto const& member : layout.members) {
                out << member.schema->name << "[static_cast<std::size_t>(index)] = new_"
                    << member.schema->name << ";\n";
            }
            out << "}\n";
            if (equivalent_type.has_value()) {
                out << "void set(size_type const index, equivalent_type const value) const { "
                       "set(index";
                for (std::size_t index{}; index < layout.members.size(); ++index) {
                    out << ", value." << layout.members[index].schema->name.front();
                }
                out << "); }\n";
            }
        }
        out << "};\n";
    }

    out << "struct " << schema.name << " {\n"
        << "using View = " << view << ";\n"
        << "using ConstView = " << const_view << ";\n"
        << "using size_type = std::int32_t;\n";
    if (equivalent_type.has_value()) {
        out << "using equivalent_type = " << native_spelling(equivalent_type->spelling) << ";\n"
            << "auto operator[](size_type const index) const -> equivalent_type { return "
               "get_const_view()[index]; }\n";
    }
    for (auto const& member : layout.members) {
        auto const& type{member.member.element_type};
        dependencies.insert(dependencies.end(), type.dependencies.begin(), type.dependencies.end());
        out << (member.schema->kind == SoaMemberKind::array
                    ? "ml::native_soa::Vector<" + native_spelling(type.spelling) + ">"
                    : type.spelling)
            << " " << member.schema->name << ";\n";
    }

    out << "auto num() const noexcept -> size_type { return static_cast<size_type>("
        << join(layout.leaves.front().path, ".") << ".size()); }\n"
        << "auto is_empty() const noexcept -> bool { return num() == 0; }\n"
        << "template <typename Fn> void each_column(Fn&& fn) {\n";
    for (auto const& leaf : layout.leaves) {
        out << "fn(" << join(leaf.path, ".") << ");\n";
    }
    out << "}\n"
        << "template <typename Fn> void each_column(Fn&& fn) const {\n";
    for (auto const& leaf : layout.leaves) {
        out << "fn(" << join(leaf.path, ".") << ");\n";
    }
    out << "}\n"
        << "void validate_array_sizes() const { get_const_view().validate_array_sizes(); }\n"
        << "void reserve(size_type const count) { ml::native_soa::require(count >= 0);\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".reserve(static_cast<std::size_t>(count));\n";
    }
    out << "}\nvoid reset() noexcept {\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".clear();\n";
    }
    out << "}\n"
        << "void set_num(size_type const count) { ml::native_soa::require(count >= 0); auto const "
           "size{static_cast<std::size_t>(count)};\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".resize(size);\n";
    }
    out << "}\n"
        << "void add_uninitialised(size_type const count) { auto const old_num{num()}; "
           "ml::native_soa::require(count >= 0 && count <= "
           "std::numeric_limits<size_type>::max() - old_num); set_num(old_num + count); }\n"
        << "void add_defaulted(size_type const count) { add_uninitialised(count); }\n"
        << "void remove_at_swap(size_type const index, size_type const count) { auto const "
           "old_num{num()}; ml::native_soa::require(index >= 0 && index <= old_num && count >= 0 "
           "&& count <= old_num - index); auto const moved{std::min(count, old_num-index-count)}; "
           "auto const source{old_num-moved};\n";
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << "for (size_type i{}; i < moved; ++i) { " << column << "[index+i] = " << column
            << "[source+i]; }\n";
    }
    out << "set_num(old_num-count); }\n";

    if (all_members_are_arrays(layout)) {
        out << "void set(size_type const index";
        render_parameters(out, layout);
        out << ") { get_view().set(index";
        render_arguments(out, layout);
        out << "); }\n"
            << "auto add(";
        for (std::size_t index{}; index < layout.members.size(); ++index) {
            auto const& member{layout.members[index]};
            if (index > 0) {
                out << ", ";
            }
            out << native_spelling(member.member.element_type.spelling) << " const new_"
                << member.schema->name;
        }
        out << ") -> size_type { auto const index{num()}; add_defaulted(1); set(index";
        render_arguments(out, layout);
        out << "); return index; }\n";
        if (equivalent_type.has_value()) {
            out << "void set(size_type const index, equivalent_type const value) { "
                   "get_view().set(index, value); }\n"
                << "auto add(equivalent_type const value) -> size_type { auto const index{num()}; "
                   "add_defaulted(1); set(index, value); return index; }\n";
        }
    }

    out << "void append_from(ConstView source) { auto const count{source.num()};\n"
        << "ml::native_soa::require(count <= std::numeric_limits<size_type>::max() - num());\n"
        << "source.validate_array_sizes(); if (count == 0) { return; }\n";
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << "{ auto const address{reinterpret_cast<std::uintptr_t>(source." << column
            << ".data())}; auto const begin{reinterpret_cast<std::uintptr_t>(" << column
            << ".data())}; ml::native_soa::require(address < begin || address >= begin + " << column
            << ".size() * sizeof(" << native_spelling(leaf.type.spelling) << ")); }\n";
    }
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << column << ".insert(" << column << ".end(), source." << column << ".begin(), source."
            << column << ".end());\n";
    }
    out << "}\n";

    for (bool const immutable : {false, true}) {
        out << "auto get_view()" << (immutable ? " const" : "") << " -> "
            << (immutable ? "ConstView" : "View") << " { return {\n";
        for (auto const& member : layout.members) {
            out << member.schema->name;
            if (member.schema->kind == SoaMemberKind::nested) {
                out << ".get_view()";
            }
            out << ",\n";
        }
        out << "}; }\n";
    }
    out << "auto get_const_view() const -> ConstView { return get_view(); }\n"
        << "auto get_view(size_type const offset, size_type const count) -> View { return "
           "get_view().slice(offset, count); }\n"
        << "auto get_view(size_type const offset, size_type const count) const -> ConstView { "
           "return "
           "get_view().slice(offset, count); }\n"
        << "auto get_const_view(size_type const offset, size_type const count) const -> ConstView "
           "{ "
           "return get_const_view().slice(offset, count); }\n"
        << "auto slice(size_type const offset, size_type const count) -> View { return "
           "get_view(offset, count); }\n"
        << "auto slice(size_type const offset, size_type const count) const -> ConstView { return "
           "get_const_view(offset, count); }\n"
        << "auto left(size_type const count) -> View { return slice(0, count); }\n"
        << "auto right(size_type const count) -> View { return slice(num() - count, count); }\n"
        << "auto left(size_type const count) const -> ConstView { return slice(0, count); }\n"
        << "auto right(size_type const count) const -> ConstView { return slice(num() - count, "
           "count); }\n"
        << "template <typename Other> void copy_element(size_type const dst_index, Other const& "
           "other, size_type const src_index) {\n";
    for (auto const& member : layout.members) {
        if (member.schema->kind == SoaMemberKind::array) {
            out << member.schema->name << "[static_cast<std::size_t>(dst_index)] = other."
                << member.schema->name << "[static_cast<std::size_t>(src_index)];\n";
        } else {
            out << member.schema->name << ".copy_element(dst_index, other." << member.schema->name
                << ", src_index);\n";
        }
    }
    out << "}\n"
        << "template <typename Other> void copy_elements(size_type const dst_index, Other const& "
           "other, size_type const src_index, size_type const count) { for (size_type i{}; i < "
           "count; ++i) { copy_element(dst_index + i, other, src_index + i); } }\n"
        << "void apply_permutation(std::span<std::int32_t> const indices) { "
           "validate_array_sizes(); "
           "ml::native_soa::require(indices.size() == static_cast<std::size_t>(num())); "
           "each_column([indices](auto& column) { ml::apply_permutation(std::span{column}, "
           "indices); "
           "}); }\n"
        << "template <typename Compare> void sort(Compare&& compare, "
           "std::span<std::int32_t> const scratch_indices) { validate_array_sizes(); "
           "ml::native_soa::require(scratch_indices.size() == static_cast<std::size_t>(num())); "
           "for "
           "(size_type i{}; i < num(); ++i) { scratch_indices[static_cast<std::size_t>(i)] = i; } "
           "std::sort(scratch_indices.begin(), scratch_indices.end(), [this, &compare](size_type "
           "const lhs, size_type const rhs) { return compare(*this, lhs, rhs); }); "
           "apply_permutation(scratch_indices); }\n"
        << "};\n";

    return LoweredSoa{{raw(out.str(), std::move(dependencies))}, {}};
}
}
