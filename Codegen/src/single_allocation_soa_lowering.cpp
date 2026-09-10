#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace codegen::detail {

static auto column_name(std::string_view const id) -> std::string {
    std::string result;
    result.reserve(id.size());
    bool capitalize{true};
    for (auto const character : id) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        result.push_back(
            capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                       : character);
        capitalize = false;
    }
    return result;
}

static auto layout_column_name(std::string_view const id,
                               std::set<std::string> const& type_identifiers) -> std::string {
    auto result{column_name(id)};
    if (type_identifiers.contains(result)) {
        result += "Column";
    }
    return result;
}

static auto vector_element(SoaSchema const& schema,
                           std::map<std::string, CppType> const& leaves,
                           std::string const& prefix) -> std::string {
    auto const dimensions{schema.members.size()};
    if (dimensions != 2 && dimensions != 3) {
        return {};
    }
    std::string type;
    for (std::size_t index{}; index < dimensions; ++index) {
        auto const& member{schema.members[index]};
        if (member.kind != SoaMemberKind::array ||
            member.name != std::string(1, "xyz"[index]) + "s") {
            return {};
        }
        auto const& element{leaves.at(prefix + "_" + member.name).spelling};
        if (index == 0) {
            type = element;
        } else if (element != type) {
            return {};
        }
    }
    static std::set<std::string> const scalars{"float",
                                               "double",
                                               "int8",
                                               "uint8",
                                               "int16",
                                               "uint16",
                                               "int32",
                                               "uint32",
                                               "int64",
                                               "uint64",
                                               "std::int8_t",
                                               "std::uint8_t",
                                               "std::int16_t",
                                               "std::uint16_t",
                                               "std::int32_t",
                                               "std::uint32_t",
                                               "std::int64_t",
                                               "std::uint64_t"};
    return scalars.contains(type) ? type : std::string{};
}

static auto compact_columns_expression(SoaSchema const& schema,
                                       std::map<std::string, SoaSchema const*> const& schemas,
                                       std::map<std::string, CppType> const& leaves,
                                       std::set<std::string> const& type_identifiers,
                                       std::vector<std::string> const& prefix,
                                       std::string const& layout,
                                       bool native,
                                       bool is_const) -> std::string {
    auto const mutable_view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<std::string> values;
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            values.push_back(compact_columns_expression(*schemas.at(*member.nested_schema),
                                                        schemas,
                                                        leaves,
                                                        type_identifiers,
                                                        path,
                                                        layout,
                                                        native,
                                                        is_const));
        } else {
            auto const id{join(path, "_")};
            values.push_back("{column_data_unchecked<" + leaves.at(id).spelling + ">(" + layout +
                             "::" + layout_column_name(id, type_identifiers) +
                             ".offset(blocks)), " +
                             (native ? "static_cast<std::size_t>(count_)" : "count_") + "}");
        }
    }
    return (is_const ? const_view : mutable_view) + "{" + join(values, ", ") + "}";
}

static void emit_compact_views(std::ostringstream& out,
                               SoaSchema const& schema,
                               std::map<std::string, SoaSchema const*> const& schemas,
                               std::map<std::string, CppType> const& leaves,
                               std::set<std::string> const& type_identifiers,
                               std::string const& layout,
                               std::string const& runtime,
                               bool native) {
    auto const view{schema.name + "SingleView"};
    auto const const_view{schema.name + "SingleConstView"};
    for (bool const is_const : {true, false}) {
        auto const name{is_const ? const_view : view};
        auto const base{runtime + "CompactViewState<" + (is_const ? "true" : "false") + ">"};
        out << "struct " << name << " : " << base << " {\n"
            << "using Base = " << base << ";\nusing Base::Base;\n"
            << "using View = " << view << ";\nusing ConstView = " << const_view << ";\n"
            << name << "() = default;\n";
        if (is_const) {
            out << name << "(" << view << " const& other);\n";
        }
        out << "auto get_const_view() const -> ConstView { return *this; }\n"
            << "auto get_const_view(size_type offset, size_type count) const -> ConstView { return "
               "slice(offset, count); }\n";
        auto emit_columns = [&](SoaSchema const& target,
                                std::vector<std::string> const& prefix,
                                std::string const& function) {
            auto const type{is_const ? target.const_view_name.value_or(target.name + "ConstView")
                                     : target.view_name.value_or(target.name + "View")};
            out << "auto " << function << "() const -> " << type << " {\nvalidate();\n"
                << "if (!state_ || !state_->data_) { return {}; }\n"
                << "auto const blocks{capacity_blocks()};\nreturn "
                << compact_columns_expression(
                       target, schemas, leaves, type_identifiers, prefix, layout, native, is_const)
                << ";\n}\n";
        };
        for (auto const& member : schema.members) {
            if (member.kind == SoaMemberKind::nested) {
                auto const& nested{*schemas.at(*member.nested_schema)};
                auto const element{vector_element(nested, leaves, member.name)};
                if (element.empty()) {
                    emit_columns(nested, {member.name}, "view_" + member.name);
                    continue;
                }
                auto const vector_type{std::string{native ? "ml::native_soa::" : "ml::soa::"} +
                                       "Vector" + std::to_string(nested.members.size()) +
                                       (is_const ? "ConstView<" : "View<") + element + ">"};
                out << "auto view_" << member.name << "() const -> " << vector_type
                    << " {\nvalidate();\n"
                    << "if (!state_ || !state_->data_) { return {}; }\n"
                    << "auto const blocks{capacity_blocks()};\n"
                    << "auto const first{" << layout
                    << "::" << layout_column_name(member.name + "_xs", type_identifiers)
                    << ".offset(blocks)};\n"
                    << "auto const stride{" << layout
                    << "::" << layout_column_name(member.name + "_ys", type_identifiers)
                    << ".offset(blocks) - first};\n"
                    << "return {column_data_unchecked<" << element
                    << ">(first), stride, count_};\n}\n";
            } else {
                auto const type{leaves.at(member.name).spelling};
                out << "auto " << member.name << "() const -> "
                    << (native ? "std::span<" : "TArrayView<") << type << (is_const ? " const" : "")
                    << "> { return {column_data<" << type << ">(" << layout
                    << "::" << layout_column_name(member.name, type_identifiers)
                    << ".offset(capacity_blocks())), "
                    << (native ? "static_cast<std::size_t>(count_)" : "count_") << "}; }\n";
            }
        }
        emit_columns(schema, {}, "columns");
        out << (native ? "template <typename Func> void each_column(Func&& func) const { "
                         "columns().each_column(std::forward<Func>(func)); }\n"
                       : "template <typename Func> auto apply_arrays(Func&& func) const -> "
                         "decltype(auto) { auto arrays{columns()}; return "
                         "arrays.apply_arrays(std::forward<Func>(func)); }\n")
            << "};\nstatic_assert(sizeof(" << name << ") == 16);\n"
            << "static_assert(std::is_trivially_copyable_v<" << name << ">);\n";
    }
    out << "inline " << const_view << "::" << const_view << "(" << view
        << " const& other) : Base{other} {}\n";
}
auto lower_single_allocation_node(SoaSchema const& schema,
                                  std::map<std::string, SoaSchema const*> const& schemas,
                                  std::map<std::string, CppType> const& types,
                                  bool const native) -> Node {
    auto const* runtime{native ? "ml::native_soa::" : "ml::soa_storage::"};
    std::string free_data{native ? "ml::native_soa::free(data_, allocation_alignment)"
                                 : "ml::soa_storage::MimallocStorageAllocator::free(data_)"};
    auto const* copy{native ? "std::memcpy" : "FMemory::Memcpy"};
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto const& name{*schema.single_allocation};
    auto const storage_name{name + "Storage"};
    auto const layout_name{schema.name + "SingleLayout"};
    auto const compact_view{schema.name + "SingleView"};
    std::vector<TypeDependency> dependencies{
        {"single_allocation_storage",
         native ? "native_soa/storage.h" : "SandboxCore/single_allocation_storage.h",
         {}}};
    std::string allocate{std::string{runtime} +
                         (native ? "allocate" : "MimallocStorageAllocator::allocate")};
    if (schema.single_allocation_allocator) {
        auto const allocator{resolve_type(*schema.single_allocation_allocator, types)};
        dependencies.insert(
            dependencies.end(), allocator.dependencies.begin(), allocator.dependencies.end());
        allocate = allocator.spelling + "::allocate";
        free_data = allocator.spelling + "::free(data_)";
    }
    std::set<std::string> names;
    std::map<std::string, std::string> type_ids;
    std::vector<FixedLeaf const*> unique_types;
    std::set<std::string> type_identifiers;
    for (auto const& leaf : layout.leaves) {
        if (type_ids.emplace(leaf.type.spelling, fixed_leaf_argument(leaf)).second) {
            unique_types.push_back(&leaf);
        }
        for (std::size_t start{}; start < leaf.type.spelling.size();) {
            if (auto const character{static_cast<unsigned char>(leaf.type.spelling[start])};
                !std::isalpha(character) && character != '_') {
                ++start;
                continue;
            }
            auto end{start + 1};
            while (end < leaf.type.spelling.size()) {
                auto const character{static_cast<unsigned char>(leaf.type.spelling[end])};
                if (!std::isalnum(character) && character != '_') {
                    break;
                }
                ++end;
            }
            type_identifiers.emplace(leaf.type.spelling.substr(start, end - start));
            start = end;
        }
    }
    auto emit_copy_sizes = [&](std::ostringstream& output, std::string const& count) {
        for (auto const* leaf : unique_types) {
            output << "        auto const " << fixed_leaf_argument(*leaf) << "_bytes{" << count
                   << " * sizeof(" << leaf->type.spelling << ")};\n";
        }
    };
    std::ostringstream assertions;
    std::ostringstream out;
    std::ostringstream layout_output;
    out << "struct " << layout_name << " {\n"
        << "using size_type = " << (native ? "std::int32_t" : "int32")
        << ";\nusing byte_size_type = " << (native ? "std::size_t" : "SIZE_T") << ";\n\n"
        << "inline static constexpr byte_size_type "
           "max_allocation_size{std::numeric_limits<byte_size_type>::max()};\n"
        << "inline static constexpr size_type capacity_granularity{64};\n"
        << "inline static constexpr byte_size_type column_gap{192};\n\n"
        << "template <typename T>\nusing ColLayout = " << runtime << "ColumnLayout<T>;\n"
        << "inline static constexpr " << runtime
        << "ColumnLayoutStart LayoutStart{capacity_granularity, column_gap, 64};\n\n";

    std::set<std::string> column_names{"ColLayout", "LayoutStart", layout_name};
    std::vector<std::string> columns;
    std::string previous_column{"LayoutStart"};
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        if (id.find("__") != std::string::npos || id.back() == '_') {
            throw std::invalid_argument{
                "Single-allocation leaf would generate reserved identifiers: " + id};
        }
        if (!names.insert(id).second) {
            throw std::invalid_argument{"Single-allocation flattened leaf name collision: " + id};
        }
        auto const column{layout_column_name(id, type_identifiers)};
        if (!column_names.insert(column).second) {
            throw std::invalid_argument{"Single-allocation column name collision: " + column};
        }

        dependencies.insert(
            dependencies.end(), leaf.type.dependencies.begin(), leaf.type.dependencies.end());
        out << "inline static constexpr ColLayout<" << leaf.type.spelling << "> " << column << "{"
            << previous_column << "};\n";
        columns.push_back(column);
        previous_column = column;
    }
    out << "\ninline static constexpr byte_size_type allocation_alignment{" << runtime
        << "maximum_alignment(" << join(columns, ", ") << ")};\n\n";

    for (auto const* leaf : unique_types) {
        auto const& type{leaf->type.spelling};
        assertions
            << "static_assert(" << runtime << "supported_leaf<" << type
            << ">, \"Single-allocation leaf " << join(leaf->path, ".")
            << " requires a non-cv, trivially copyable/copy-constructible/destructible, nothrow "
               "default-constructible object type.\");\n";
    }
    assertions << "\nstatic_assert(allocation_alignment <= std::numeric_limits<"
               << (native ? "std::uint32_t" : "uint32")
               << ">::max(), \"Single-allocation alignment must fit the allocator's 32-bit "
                  "alignment argument.\");\n";
    for (std::size_t index{}; index < layout.leaves.size(); ++index) {
        auto const& type{layout.leaves[index].type.spelling};
        assertions << "static_assert(sizeof(" << type << ") <= (max_allocation_size - "
                   << columns[index] << ".block_offset) / capacity_granularity);\n";
    }
    assertions << "static_assert(" << (layout.leaves.size() - 1) << " <= (max_allocation_size - "
               << runtime << "layout_align(" << columns.back() << ".block_end"
               << ", allocation_alignment)) / (column_gap + allocation_alignment - 1));\n";
    out << "// Conservative per-block bound for checked capacity arithmetic; gaps do not scale "
           "with capacity.\n";
    out << "inline static constexpr byte_size_type "
           "capacity_block_bound{"
        << runtime << "layout_align(" << columns.back() << ".block_end, allocation_alignment) + "
        << (layout.leaves.size() - 1) << " * (column_gap + allocation_alignment - 1)};\n"
        << "inline static constexpr size_type "
           "max_capacity{"
        << runtime << "maximum_capacity(capacity_block_bound)};\n"
        << "static constexpr auto layout_bytes(byte_size_type blocks) noexcept -> byte_size_type "
           "{\n"
        << "return blocks == 0 ? 0 : " << columns.back() << ".data_end(blocks);\n}\n"
        << "\nprivate:\ninline static constexpr auto validate_layout = []() consteval -> bool {\n"
        << assertions.str()
        << "static_assert(max_capacity >= capacity_granularity);\nreturn true;\n};\n"
        << "static_assert(validate_layout());\n};\n\n";
    layout_output << out.str();
    out.str({});
    if (!schema.single_allocation_allocator) {
        out << "struct " << compact_view << ";\nstruct " << schema.name << "SingleConstView;\n"
            << layout_output.str();
    }
    out << "struct " << storage_name << " : " << layout_name << ", protected " << runtime
        << "StorageState, " << runtime << "StorageOperations {\n"
        << "using View = " << compact_view << ";\n"
        << "using ConstView = " << schema.name << "SingleConstView;\n"
        << "/* **************************************** */\n// Lifetime\n/* "
           "**************************************** */\n"
        << storage_name << "() noexcept = default;\n"
        << "~" << storage_name << "() { " << free_data << "; }\n"
        << storage_name << "(" << storage_name << " const&) = delete;\n"
        << "auto operator=(" << storage_name << " const&) -> " << storage_name << "& = delete;\n"
        << storage_name << "(" << storage_name << "&& other) noexcept\n"
        << "    : StorageState{std::exchange(other.data_, nullptr), std::exchange(other.num_, 0), "
           "std::exchange(other.capacity_, 0)} {}\n"
        << "auto operator=(" << storage_name << "&& other) noexcept -> " << storage_name << "& {\n"
        << "    if (this != &other) {\n        " << free_data
        << ";\n        data_ = "
           "std::exchange(other.data_, nullptr);\n        num_ = std::exchange(other.num_, 0);\n   "
           "     capacity_ = std::exchange(other.capacity_, 0);\n    }\n    return *this;\n}\n\n"
        << "protected:\n"
        << "template <typename Byte> struct DataPointers {\n"
        << "    template <typename T> using Element = std::conditional_t<std::is_const_v<Byte>, T "
           "const, T>;\n";
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "    Element<" << leaf.type.spelling << ">* " << id << "{};\n";
    }
    out << "    auto operator+(size_type const offset) const noexcept -> DataPointers {\n"
        << "        if (" << fixed_leaf_argument(layout.leaves.front())
        << " == nullptr) { return {}; }\n"
        << "        return {\n";
    for (auto const& leaf : layout.leaves) {
        out << "            " << fixed_leaf_argument(leaf) << " + offset,\n";
    }
    out << "        };\n    }\n};\n"
        << "template <typename Self> auto get_data(this Self& self) noexcept {\n"
        << "    using Byte = std::conditional_t<std::is_const_v<Self>, std::byte const, "
           "std::byte>;\n"
        << "    if (self.data_ == nullptr) { return DataPointers<Byte>{}; }\n"
        << "    return make_data_unchecked(static_cast<Byte*>(self.data_), "
           "self.capacity_blocks());\n}\n"
        << "template <typename Self> auto get_data(this Self& self, size_type const offset) "
           "noexcept {\n"
        << "    return self.get_data() + offset;\n}\n\n"
        << "private:\nfriend struct " << runtime << "StorageOperations;\n"
        << "/* **************************************** */\n// Column pointers\n/* "
           "**************************************** */\n"
        << "template <typename Byte> static auto make_data_unchecked(Byte* const data, "
           "byte_size_type const blocks) noexcept -> DataPointers<Byte> {\n"
        << "    auto const pointer_at = [data, blocks](auto const& column) noexcept {\n"
        << "        using Column = std::remove_cvref_t<decltype(column)>;\n"
        << "        using Pointer = std::conditional_t<std::is_const_v<Byte>,\n"
        << "                                           typename Column::const_pointer,\n"
        << "                                           typename Column::pointer>;\n"
        << "        return std::launder(\n"
        << "            reinterpret_cast<Pointer>(data + column.offset(blocks)));\n"
        << "    };\n"
        << "    return {\n";
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "        pointer_at(" << layout_column_name(id, type_identifiers) << "),\n";
    }
    out << "    };\n}\n"
        << "auto capacity_blocks() const noexcept -> byte_size_type { return "
           "static_cast<byte_size_type>(capacity_ / capacity_granularity); }\n\n"
        << "/* **************************************** */\n// Typed mutations and growth\n/* "
           "**************************************** */\n"
        << "void default_construct_columns(size_type const first, size_type const count) {\n"
        << "    auto const columns{make_data_unchecked(data_, capacity_blocks()) + first};\n";
    for (auto const& leaf : layout.leaves) {
        out << "    "
            << (native ? "std::uninitialized_value_construct_n<" : "DefaultConstructItems<")
            << leaf.type.spelling << (native ? "*>" : ">") << "(columns."
            << fixed_leaf_argument(leaf) << ", count);\n";
    }
    out << "}\n"
        << "void swap_remove_columns(size_type const index, size_type const source, size_type "
           "const move_count) {\n"
        << "    copy_columns(get_data(), index, source, move_count);\n}\n"
        << "static void copy_columns(DataPointers<std::byte> const& columns, size_type index, "
           "size_type source, size_type move_count) {\n"
        << "    auto const elements_to_move{static_cast<byte_size_type>(move_count)};\n";
    emit_copy_sizes(out, "elements_to_move");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "    " << copy << "(columns." << id << " + index, columns." << id << " + source, "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "}\n"
        << "void swap_remove_indices(std::span<size_type const> indices) {\n"
        << "auto const columns{get_data()};\n"
        << "ml::soa_storage_detail::for_each_removal_run(num_, indices, " << runtime
        << "require, [&](size_type index, size_type source, size_type count) { "
           "copy_columns(columns, index, source, count); });\n}\n"
        << "template <typename Columns> void append_columns(Columns const& source, size_type "
           "first, size_type count) {\n"
        << "auto const destination{get_data(first)};\nauto const "
           "elements_to_copy{static_cast<byte_size_type>(count)};\n";
    emit_copy_sizes(out, "elements_to_copy");
    for (auto const& leaf : layout.leaves) {
        out << copy << "(destination." << fixed_leaf_argument(leaf) << ", source."
            << join(leaf.path, ".") << (native ? ".data()" : ".GetData()") << ", "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "}\n"
        << "void reallocate(size_type const new_capacity) {\n"
        << "    auto* const "
           "new_data{"
        << allocate
        << "(layout_bytes(static_cast<byte_size_type>(new_capacity / capacity_granularity)), "
           "static_cast<"
        << (native ? "std::uint32_t" : "uint32") << ">(allocation_alignment))};\n"
        << "    if (num_ > 0) {\n"
        << "        auto const old_blocks{capacity_blocks()};\n"
        << "        auto const new_blocks{static_cast<byte_size_type>(new_capacity / "
           "capacity_granularity)};\n"
        << "        auto const source{make_data_unchecked(static_cast<std::byte const*>(data_), "
           "old_blocks)};\n"
        << "        auto const destination{make_data_unchecked(new_data, new_blocks)};\n"
        << "        auto const live_count{static_cast<byte_size_type>(num_)};\n";
    emit_copy_sizes(out, "live_count");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "        " << copy << "(destination." << id << ", source." << id << ", "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "    }\n    " << free_data
        << ";\n    data_ = new_data;\n    capacity_ = "
           "new_capacity;\n}\n"
        << "};\n\n";
    if (!schema.single_allocation_allocator) {
        std::map<std::string, CppType> leaf_types;
        for (auto const& leaf : layout.leaves) {
            leaf_types.emplace(fixed_leaf_argument(leaf), leaf.type);
        }
        emit_compact_views(
            out, schema, schemas, leaf_types, type_identifiers, layout_name, runtime, native);
    }
    out << "struct " << name << " : " << storage_name << " {\n"
        << name << "() noexcept = default;\n"
        << name << "(" << name << " const&) = delete;\n"
        << "auto operator=(" << name << " const&) -> " << name << "& = delete;\n"
        << name << "(" << name << "&&) noexcept = default;\n"
        << "auto operator=(" << name << "&&) noexcept -> " << name << "& = default;\n";
    for (bool const is_const : {false, true}) {
        auto const qualifier{is_const ? " const &" : " &"};
        auto const view{is_const ? "ConstView" : "View"};
        out << "auto get_view()" << qualifier << " -> " << view << " { return {this, 0, num()}; }\n"
            << "auto get_view(size_type offset, size_type count)" << qualifier << " -> " << view
            << " { return {this, offset, count}; }\n"
            << "auto slice(size_type offset, size_type count)" << qualifier << " -> " << view
            << " { return get_view(offset, count); }\n"
            << "auto left(size_type count)" << qualifier << " -> " << view
            << " { return get_view().left(count); }\n"
            << "auto right(size_type count)" << qualifier << " -> " << view
            << " { return get_view().right(count); }\n";
        auto const rvalue{is_const ? " const &&" : " &&"};
        out << "auto get_view()" << rvalue << " -> " << view << " = delete;\n"
            << "auto get_view(size_type, size_type)" << rvalue << " -> " << view << " = delete;\n"
            << "auto slice(size_type, size_type)" << rvalue << " -> " << view << " = delete;\n"
            << "auto left(size_type)" << rvalue << " -> " << view << " = delete;\n"
            << "auto right(size_type)" << rvalue << " -> " << view << " = delete;\n";
    }
    out << "auto get_const_view() const & -> ConstView { return get_view(); }\n"
        << "auto get_const_view(size_type offset, size_type count) const & -> ConstView { return "
           "get_view(offset, count); }\n"
        << "auto get_const_view() const && -> ConstView = delete;\n"
        << "auto get_const_view(size_type, size_type) const && -> ConstView = delete;\n};";
    return raw(out.str(), std::move(dependencies));
}

} // namespace codegen::detail
