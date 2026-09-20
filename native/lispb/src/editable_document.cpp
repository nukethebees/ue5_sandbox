#include <lispb/schema/editable_document.h>

#include <codegen/manifest_error.h>
#include <codegen/sexpr/reader.h>
#include <codegen/source_loader.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace lispb::schema {
namespace {

using codegen::sexpr::Form;

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw codegen::ManifestError{"Cannot open manifest file: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto form_range(Form const& form, std::size_t const source_file_index) -> SourceRange {
    auto const end_offset{form.is_list() ? form.closing.span.offset + 1
                                         : form.token.span.offset + form.token.text.size()};
    return {.source_file_index = source_file_index,
            .begin_offset = form.token.span.offset,
            .end_offset = end_offset,
            .line = form.token.span.line,
            .column = form.token.span.column};
}

auto quote(std::string_view const value) -> std::string {
    std::string result{"\""};
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    result += '"';
    return result;
}

auto render_type_ref(codegen::TypeRef const& type) -> std::string {
    if (type.suffix.empty() && !type.nested.has_value()) {
        return type.name;
    }
    auto result{"(type-ref " + type.name};
    if (!type.suffix.empty()) {
        result += " :suffix " + quote(type.suffix);
    }
    if (type.nested.has_value()) {
        result += " :nested " + quote(*type.nested);
    }
    return result + ')';
}

auto reflection_name(codegen::EnumReflection const reflection) -> std::string_view {
    switch (reflection) {
        case codegen::EnumReflection::none:
            return "none";
        case codegen::EnumReflection::uenum:
            return "uenum";
        case codegen::EnumReflection::blueprint:
            return "blueprint";
    }
    return "none";
}

auto conversion_name(codegen::EnumConversion const conversion) -> std::string_view {
    switch (conversion) {
        case codegen::EnumConversion::lex_to_string:
            return "lex-to-string";
        case codegen::EnumConversion::string_view:
            return "string-view";
        case codegen::EnumConversion::string:
            return "string";
        case codegen::EnumConversion::lex_to_display_string:
            return "lex-to-display-string";
        case codegen::EnumConversion::display_string_view:
            return "display-string-view";
        case codegen::EnumConversion::display_string:
            return "display-string";
        case codegen::EnumConversion::lex_to_serialized_string:
            return "lex-to-serialized-string";
        case codegen::EnumConversion::try_parse_serialized:
            return "try-parse-serialized";
    }
    return "lex-to-string";
}

auto render_enum(codegen::EnumSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(enum " << schema.name << ' ' << render_type_ref(schema.underlying_type);
    if (schema.reflection != codegen::EnumReflection::none) {
        output << "\n    :reflection " << reflection_name(schema.reflection);
    }
    if (schema.enum_array) {
        output << "\n    :enum-array true";
    }
    if (schema.count.has_value()) {
        output << "\n    :count " << *schema.count;
    }
    if (!schema.conversions.empty()) {
        output << "\n    :conversions (";
        for (std::size_t index{}; index < schema.conversions.size(); ++index) {
            output << (index == 0 ? "" : " ") << conversion_name(schema.conversions[index]);
        }
        output << ')';
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (schema.native_api) {
        output << "\n    :native-api true";
    }
    for (auto const& value : schema.values) {
        output << "\n    (value " << value.name;
        if (value.initializer.has_value()) {
            output << "\n      :value " << quote(*value.initializer);
        }
        if (value.display_name.has_value()) {
            output << "\n      :display-name " << quote(*value.display_name);
        }
        if (value.hidden) {
            output << "\n      :hidden true";
        }
        if (value.serialized_name.has_value()) {
            output << "\n      :serialized-name " << quote(*value.serialized_name);
        }
        output << ')';
    }
    if (schema.unreal_projection.has_value()) {
        auto const& projection{*schema.unreal_projection};
        output << "\n    (unreal-projection " << projection.name << "\n      :header "
               << quote(projection.header.string()) << "\n      :header-include "
               << quote(projection.header_include) << "\n      :conversion-header "
               << quote(projection.conversion_header.string()) << "\n      :native-header-include "
               << quote(projection.native_header_include);
        if (projection.reflection != codegen::EnumReflection::uenum) {
            output << "\n      :reflection " << reflection_name(projection.reflection);
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto packed_field_kind_name(codegen::PackedFieldKind const kind) -> std::string_view {
    switch (kind) {
        case codegen::PackedFieldKind::unsigned_integer:
            return "unsigned";
        case codegen::PackedFieldKind::enumeration:
            return "enum";
    }
    return "unsigned";
}

auto soa_member_kind_name(codegen::SoaMemberKind const kind) -> std::string_view {
    switch (kind) {
        case codegen::SoaMemberKind::array:
            return "array";
        case codegen::SoaMemberKind::nested:
            return "nested";
    }
    return "array";
}

auto storage_operation_name(codegen::StorageOperation const operation) -> std::string_view {
    switch (operation) {
        case codegen::StorageOperation::reset:
            return "reset";
        case codegen::StorageOperation::reserve:
            return "reserve";
        case codegen::StorageOperation::add_uninitialised:
            return "add-uninitialised";
        case codegen::StorageOperation::add_defaulted:
            return "add-defaulted";
        case codegen::StorageOperation::remove_at_swap:
            return "remove-at-swap";
        case codegen::StorageOperation::set_num:
            return "set-num";
        case codegen::StorageOperation::copy_element:
            return "copy-element";
        case codegen::StorageOperation::append_from:
            return "append-from";
    }
    return "reset";
}

void render_quoted_list(std::ostringstream& output, std::vector<std::string> const& values) {
    output << '(';
    for (std::size_t index{}; index < values.size(); ++index) {
        output << (index == 0 ? "" : " ") << quote(values[index]);
    }
    output << ')';
}

void render_function(std::ostringstream& output,
                     codegen::FunctionSchema const& function,
                     std::string_view const indent) {
    output << indent << "(function " << function.name << ' '
           << render_type_ref(function.return_type);
    if (!function.body_lines.empty()) {
        output << "\n" << indent << "  :body ";
        render_quoted_list(output, function.body_lines);
    }
    if (!function.dependencies.empty()) {
        output << "\n" << indent << "  :dependencies ";
        render_quoted_list(output, function.dependencies);
    }
    if (function.trailing_return_type.has_value()) {
        output << "\n"
               << indent << "  :trailing-return-type "
               << render_type_ref(*function.trailing_return_type);
    }
    if (function.is_const) {
        output << "\n" << indent << "  :const true";
    }
    if (function.is_noexcept) {
        output << "\n" << indent << "  :noexcept true";
    }
    if (function.is_static) {
        output << "\n" << indent << "  :static true";
    }
    if (function.is_inline) {
        output << "\n" << indent << "  :inline true";
    }
    if (function.definition_in_source) {
        output << "\n" << indent << "  :definition-in-source true";
    }
    if (function.template_parameters.has_value()) {
        output << "\n"
               << indent << "  :template-parameters " << quote(*function.template_parameters);
    }
    if (function.requires_clause.has_value()) {
        output << "\n" << indent << "  :requires " << quote(*function.requires_clause);
    }
    for (auto const& parameter : function.parameters) {
        output << "\n"
               << indent << "  (parameter " << parameter.name << ' '
               << render_type_ref(parameter.type);
        if (parameter.default_value.has_value()) {
            output << " :default " << quote(*parameter.default_value);
        }
        output << ')';
    }
    output << ')';
}

auto render_packed_value(codegen::PackedValueSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(packed-value " << schema.name << "\n    :storage "
           << render_type_ref(schema.storage_type);
    if (schema.invalid_value.has_value()) {
        output << "\n    :invalid-value " << *schema.invalid_value;
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    for (auto const& field : schema.fields) {
        output << "\n    (field " << field.name << ' ' << render_type_ref(field.type) << " :bits "
               << field.bits;
        if (field.kind != codegen::PackedFieldKind::unsigned_integer) {
            output << " :kind " << packed_field_kind_name(field.kind);
        }
        if (field.range_helper) {
            output << " :range-helper true";
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

auto render_soa(codegen::SoaSchema const& schema) -> std::string {
    std::ostringstream output;
    output << "(struct " << schema.name;
    if (schema.view_name.has_value()) {
        output << "\n    :view-name " << *schema.view_name;
    }
    if (schema.const_view_name.has_value()) {
        output << "\n    :const-view-name " << *schema.const_view_name;
    }
    if (!schema.operations.empty()) {
        output << "\n    :operations (";
        for (std::size_t index{}; index < schema.operations.size(); ++index) {
            output << (index == 0 ? "" : " ") << storage_operation_name(schema.operations[index]);
        }
        output << ')';
    }
    if (schema.export_specifier.has_value()) {
        output << "\n    :export-specifier " << *schema.export_specifier;
    }
    if (!schema.using_declarations.empty()) {
        output << "\n    :using-declarations ";
        render_quoted_list(output, schema.using_declarations);
    }
    if (schema.equivalent_type.has_value()) {
        output << "\n    :equivalent-type " << render_type_ref(*schema.equivalent_type);
    }
    if (schema.copy_element_memberwise) {
        output << "\n    :copy-element-memberwise true";
    }
    if (schema.layout_only) {
        output << "\n    :layout-only true";
    }
    if (schema.field_mask_name.has_value()) {
        output << "\n    :field-mask-name " << *schema.field_mask_name;
    }
    if (schema.field_enum_name.has_value()) {
        output << "\n    :field-enum-name " << *schema.field_enum_name;
    }
    for (auto const& member : schema.members) {
        output << "\n    (member " << member.name << ' ' << soa_member_kind_name(member.kind) << ' '
               << render_type_ref(member.type);
        if (member.fixed_schema.has_value()) {
            output << "\n      :fixed-schema " << *member.fixed_schema;
        }
        if (member.nested_schema.has_value()) {
            output << "\n      :nested-schema " << *member.nested_schema;
        }
        if (member.mask_field) {
            output << "\n      :mask-field true";
        }
        if (!member.mask_dimensions.empty()) {
            output << "\n      :mask-dimensions (";
            for (std::size_t index{}; index < member.mask_dimensions.size(); ++index) {
                auto const& dimension{member.mask_dimensions[index]};
                output << (index == 0 ? "" : " ") << '(' << dimension.index_name << ' '
                       << quote(dimension.extent) << ')';
            }
            output << ')';
        }
        output << ')';
    }
    for (auto const& function : schema.functions) {
        output << '\n';
        render_function(output, function, "    ");
    }
    if (schema.fixed.has_value()) {
        output << "\n    (fixed " << schema.fixed->storage_name;
        if (!schema.fixed->containers.empty()) {
            output << " :containers (";
            for (std::size_t index{}; index < schema.fixed->containers.size(); ++index) {
                output << (index == 0 ? "" : " ") << schema.fixed->containers[index];
            }
            output << ')';
        }
        output << ')';
    }
    if (schema.single_allocation.has_value()) {
        output << "\n    (single-allocation " << *schema.single_allocation;
        for (auto const& variant : schema.single_allocation_variants) {
            output << "\n      (variant " << variant.name << ' '
                   << render_type_ref(variant.allocator) << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}

void replace_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(),
                     destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error{
            "Cannot replace LispB source file",
            source,
            destination,
            std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

auto declaration_head(codegen::ModuleSchema const& module) -> std::string_view {
    return std::visit(
        [](auto const& value) -> std::string_view {
            using Module = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                return "enum";
            } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                return "packed-value";
            } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                return "record";
            } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                return "struct";
            }
            return {};
        },
        module);
}

auto declaration_count(codegen::ModuleSchema const& module) -> std::size_t {
    return std::visit(
        [](auto const& value) -> std::size_t {
            using Module = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                return value.enums.size();
            } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                return value.values.size();
            } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                return value.records.size();
            } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                return value.structs.size();
            } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                return 1;
            }
            return 0;
        },
        module);
}

template <typename Function>
void for_each_declaration(codegen::Manifest const& manifest, Function&& function) {
    for (std::size_t module_index{}; module_index < manifest.modules.size(); ++module_index) {
        auto const& module{manifest.modules[module_index]};
        std::visit(
            [&](auto const& value) {
                using Module = std::decay_t<decltype(value)>;
                auto const add{[&](std::size_t const declaration_index, std::string const& name) {
                    function(module_index, declaration_index, value.settings, name);
                }};
                if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                    for (std::size_t index{}; index < value.enums.size(); ++index) {
                        add(index, value.enums[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                    for (std::size_t index{}; index < value.values.size(); ++index) {
                        add(index, value.values[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                    for (std::size_t index{}; index < value.records.size(); ++index) {
                        add(index, value.records[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                    for (std::size_t index{}; index < value.structs.size(); ++index) {
                        add(index, value.structs[index].name);
                    }
                } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                    add(0, value.storage_name);
                }
            },
            module);
    }
}

} // namespace

EditableSchemaDocument::EditableSchemaDocument(codegen::Manifest manifest,
                                               std::vector<SchemaSourceFile> source_files,
                                               std::filesystem::path types_path,
                                               std::vector<std::filesystem::path> module_paths)
    : manifest_{std::move(manifest)}
    , types_{resolve_type_graph(manifest_)}
    , source_files_{std::move(source_files)}
    , types_path_{std::move(types_path)}
    , module_paths_{std::move(module_paths)} {}

auto EditableSchemaDocument::from_manifest(codegen::Manifest manifest) -> EditableSchemaDocument {
    EditableSchemaDocument result{std::move(manifest), {}};
    result.initialize_declarations({});
    return result;
}

auto EditableSchemaDocument::manifest() const -> codegen::Manifest const& {
    return manifest_;
}

auto EditableSchemaDocument::types() const -> TypeGraph const& {
    return types_;
}

auto EditableSchemaDocument::source_files() const -> std::span<SchemaSourceFile const> {
    return source_files_;
}

auto EditableSchemaDocument::declarations() const -> std::span<DeclarationInfo const> {
    return declarations_;
}

auto EditableSchemaDocument::declaration(DeclarationId const id) const -> DeclarationInfo const* {
    auto const found{std::ranges::find(declarations_, id, &DeclarationInfo::id)};
    return found == declarations_.end() ? nullptr : &*found;
}

auto EditableSchemaDocument::find_declaration(TypeIdentity const& identity) const
    -> std::optional<DeclarationId> {
    auto const found{std::ranges::find(declarations_, identity, &DeclarationInfo::identity)};
    return found == declarations_.end() ? std::nullopt : std::optional{found->id};
}

auto EditableSchemaDocument::enum_schema(DeclarationId const declaration_id) const
    -> codegen::EnumSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->enums.size()
             ? nullptr
             : &module->enums[info->declaration_index];
}

auto EditableSchemaDocument::packed_value_schema(DeclarationId const declaration_id) const
    -> codegen::PackedValueSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::PackedValueModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->values.size()
             ? nullptr
             : &module->values[info->declaration_index];
}

auto EditableSchemaDocument::soa_schema(DeclarationId const declaration_id) const
    -> codegen::SoaSchema const* {
    auto const* info{declaration(declaration_id)};
    if (info == nullptr) {
        return nullptr;
    }
    auto const* module{
        std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info->module_index])};
    return module == nullptr || info->declaration_index >= module->structs.size()
             ? nullptr
             : &module->structs[info->declaration_index];
}

auto EditableSchemaDocument::allocate_declaration_id() -> DeclarationId {
    return DeclarationId{next_declaration_id_++};
}

auto EditableSchemaDocument::apply(SchemaEditCommand command)
    -> std::expected<bool, SchemaEditError> {
    auto inverse{execute(command)};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    if (!inverse->has_value()) {
        return false;
    }

    if (saved_history_position_.has_value() && *saved_history_position_ > history_position_) {
        saved_history_position_.reset();
    }
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_position_),
                   history_.end());
    history_.push_back(
        HistoryEntry{.forward = std::move(command), .inverse = std::move(**inverse)});
    ++history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::undo() -> std::expected<bool, SchemaEditError> {
    if (!can_undo()) {
        return false;
    }
    auto const& entry{history_[history_position_ - 1]};
    auto result{execute(entry.inverse)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Undo command did not change the schema draft"}};
    }
    --history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::redo() -> std::expected<bool, SchemaEditError> {
    if (!can_redo()) {
        return false;
    }
    auto const& entry{history_[history_position_]};
    auto result{execute(entry.forward)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Redo command did not change the schema draft"}};
    }
    ++history_position_;
    ++revision_;
    return true;
}

auto EditableSchemaDocument::can_undo() const -> bool {
    return history_position_ != 0;
}

auto EditableSchemaDocument::can_redo() const -> bool {
    return history_position_ != history_.size();
}

auto EditableSchemaDocument::dirty() const -> bool {
    return !saved_history_position_.has_value() || history_position_ != *saved_history_position_;
}

auto EditableSchemaDocument::revision() const -> std::uint64_t {
    return revision_;
}

void EditableSchemaDocument::mark_saved() {
    saved_history_position_ = history_position_;
}

auto EditableSchemaDocument::preview_source_updates() const
    -> std::expected<std::vector<SchemaSourceUpdate>, SchemaEditError> {
    if (!dirty()) {
        return std::vector<SchemaSourceUpdate>{};
    }
    if (source_files_.empty() || module_source_ranges_.size() != manifest_.modules.size()) {
        return std::unexpected{SchemaEditError{"Schema draft has no source ownership information"}};
    }

    std::set<DeclarationId> touched;
    for (std::size_t index{}; index < history_position_; ++index) {
        std::visit(
            [&](auto const& edit) {
                if constexpr (requires { edit.enum_declaration; }) {
                    touched.insert(edit.enum_declaration);
                } else {
                    touched.insert(edit.declaration);
                }
            },
            history_[index].forward);
    }

    struct Replacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };
    std::vector<std::vector<Replacement>> replacements(source_files_.size());
    std::map<std::size_t, std::set<DeclarationId>> insertions;
    auto render_declaration = [&](DeclarationId const id) -> std::optional<std::string> {
        if (auto const* schema{enum_schema(id)}) {
            return render_enum(*schema);
        }
        if (auto const* schema{packed_value_schema(id)}) {
            return render_packed_value(*schema);
        }
        if (auto const* schema{soa_schema(id)}) {
            return render_soa(*schema);
        }
        return std::nullopt;
    };
    for (auto const id : touched) {
        auto const* info{declaration(id)};
        auto const rendered{render_declaration(id)};
        if (info == nullptr || !rendered.has_value()) {
            continue;
        }
        if (info->source.has_value()) {
            replacements[info->source->source_file_index].push_back(
                {.begin = info->source->begin_offset,
                 .end = info->source->end_offset,
                 .text = *rendered});
        } else {
            insertions[info->module_index].insert(id);
        }
    }
    for (auto const& [module_index, ids] : insertions) {
        if (module_index >= module_source_ranges_.size() ||
            !module_source_ranges_[module_index].has_value()) {
            return std::unexpected{SchemaEditError{"New declaration's module has no source range"}};
        }
        auto const& module_range{*module_source_ranges_[module_index]};
        std::string insertion;
        std::vector<DeclarationInfo const*> ordered;
        for (auto const& declaration_info : declarations_) {
            if (declaration_info.module_index == module_index &&
                ids.contains(declaration_info.id)) {
                ordered.push_back(&declaration_info);
            }
        }
        std::ranges::sort(ordered, {}, &DeclarationInfo::declaration_index);
        for (auto const* declaration_info : ordered) {
            auto const rendered{render_declaration(declaration_info->id)};
            if (!rendered.has_value()) {
                return std::unexpected{
                    SchemaEditError{"New declaration kind cannot be serialized"}};
            }
            insertion += "\n  " + *rendered;
        }
        replacements[module_range.source_file_index].push_back(
            {.begin = module_range.end_offset - 1,
             .end = module_range.end_offset - 1,
             .text = std::move(insertion)});
    }

    std::vector<SchemaSourceUpdate> updates;
    for (std::size_t source_index{}; source_index < replacements.size(); ++source_index) {
        auto& source_replacements{replacements[source_index]};
        if (source_replacements.empty()) {
            continue;
        }
        std::ranges::sort(source_replacements, std::greater{}, &Replacement::begin);
        auto updated{source_files_[source_index].text};
        for (auto const& replacement : source_replacements) {
            if (replacement.begin > replacement.end || replacement.end > updated.size()) {
                return std::unexpected{SchemaEditError{"Invalid source replacement range"}};
            }
            updated.replace(
                replacement.begin, replacement.end - replacement.begin, replacement.text);
        }
        updates.push_back({.path = source_files_[source_index].path,
                           .original = source_files_[source_index].text,
                           .updated = std::move(updated)});
    }
    return updates;
}

auto EditableSchemaDocument::save()
    -> std::expected<std::vector<std::filesystem::path>, SchemaEditError> {
    auto updates{preview_source_updates()};
    if (!updates.has_value()) {
        return std::unexpected{std::move(updates.error())};
    }
    if (updates->empty()) {
        return std::vector<std::filesystem::path>{};
    }

    std::map<std::filesystem::path, std::filesystem::path> temporary_paths;
    auto cleanup{[&] {
        std::error_code ignored;
        for (auto const& [path, temporary] : temporary_paths) {
            static_cast<void>(path);
            std::filesystem::remove(temporary, ignored);
        }
    }};
    try {
        for (auto const& update : *updates) {
            auto temporary{update.path};
            temporary += ".layout-planner.tmp";
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
            output.write(update.updated.data(),
                         static_cast<std::streamsize>(update.updated.size()));
            output.close();
            if (!output) {
                throw std::runtime_error{"Cannot write temporary LispB source: " +
                                         temporary.string()};
            }
            temporary_paths.emplace(update.path, std::move(temporary));
        }

        std::vector<std::filesystem::path> validation_modules;
        validation_modules.reserve(module_paths_.size());
        for (auto const& path : module_paths_) {
            auto const temporary{temporary_paths.find(path)};
            validation_modules.push_back(temporary == temporary_paths.end() ? path
                                                                            : temporary->second);
        }
        auto const validated{codegen::load_sources(types_path_, validation_modules)};
        static_cast<void>(resolve_type_graph(validated));

        std::vector<std::filesystem::path> saved;
        saved.reserve(updates->size());
        for (auto const& update : *updates) {
            replace_file(temporary_paths.at(update.path), update.path);
            saved.push_back(update.path);
        }
        auto reloaded{load_editable_schema_document(types_path_, module_paths_)};
        *this = std::move(reloaded);
        return saved;
    } catch (std::exception const& error) {
        cleanup();
        return std::unexpected{SchemaEditError{error.what()}};
    }
}

void EditableSchemaDocument::initialize_declarations(
    std::vector<std::optional<SourceRange>> source_ranges) {
    declarations_.clear();
    auto source_index{std::size_t{}};
    auto next_id{std::uint64_t{1}};
    for_each_declaration(manifest_,
                         [&](std::size_t const module_index,
                             std::size_t const declaration_index,
                             codegen::ModuleSettings const& settings,
                             std::string const& name) {
                             auto const type{types_.find_declared(settings.name, name)};
                             if (!type.has_value()) {
                                 throw std::logic_error{"Resolved graph omitted declaration '" +
                                                        settings.name + ":" + name + "'"};
                             }
                             auto source{source_index < source_ranges.size()
                                             ? source_ranges[source_index]
                                             : std::nullopt};
                             declarations_.push_back({.id = DeclarationId{next_id++},
                                                      .identity = types_.type(*type).identity,
                                                      .module_index = module_index,
                                                      .declaration_index = declaration_index,
                                                      .source = std::move(source)});
                             ++source_index;
                         });
    if (!source_ranges.empty() && source_index != source_ranges.size()) {
        throw std::logic_error{"Source declaration count does not match resolved schema"};
    }
    next_declaration_id_ = next_id;
}

auto EditableSchemaDocument::execute(SchemaEditCommand const& command)
    -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError> {
    return std::visit(
        [&](auto const& edit) -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError> {
            using Edit = std::decay_t<decltype(edit)>;
            if constexpr (std::is_same_v<Edit, SetEnumeratorDisplayName>) {
                auto const* info{declaration(edit.enum_declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration id"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                if (module == nullptr || info->declaration_index >= module->enums.size()) {
                    return std::unexpected{
                        SchemaEditError{"Display names can only be edited on enum declarations"}};
                }
                auto& schema{module->enums[info->declaration_index]};
                auto value{std::ranges::find(
                    schema.values, edit.enumerator_name, &codegen::EnumeratorSchema::name)};
                if (value == schema.values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema.name +
                                                           "' has no enumerator named '" +
                                                           edit.enumerator_name + "'"}};
                }
                if (value->display_name == edit.display_name) {
                    return std::nullopt;
                }

                auto const previous{value->display_name};
                value->display_name = edit.display_name;
                try {
                    auto resolved{resolve_type_graph(manifest_)};
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    value->display_name = previous;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    SetEnumeratorDisplayName{.enum_declaration = edit.enum_declaration,
                                             .enumerator_name = edit.enumerator_name,
                                             .display_name = previous}};
            } else if constexpr (std::is_same_v<Edit, SetEnumeratorName>) {
                auto const* info{declaration(edit.enum_declaration)};
                if (info == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown declaration id"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                if (module == nullptr || info->declaration_index >= module->enums.size()) {
                    return std::unexpected{
                        SchemaEditError{"Enumerator names can only be edited on enums"}};
                }
                auto& schema{module->enums[info->declaration_index]};
                auto value{std::ranges::find(
                    schema.values, edit.current_name, &codegen::EnumeratorSchema::name)};
                if (value == schema.values.end()) {
                    return std::unexpected{SchemaEditError{"Enum '" + schema.name +
                                                           "' has no enumerator named '" +
                                                           edit.current_name + "'"}};
                }
                if (edit.current_name == edit.new_name) {
                    return std::nullopt;
                }

                auto const previous_count{schema.count};
                value->name = edit.new_name;
                if (schema.count == edit.current_name) {
                    schema.count = edit.new_name;
                }
                try {
                    auto resolved{resolve_type_graph(manifest_)};
                    types_ = std::move(resolved);
                } catch (std::exception const& error) {
                    value->name = edit.current_name;
                    schema.count = previous_count;
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    SetEnumeratorName{.enum_declaration = edit.enum_declaration,
                                      .current_name = edit.new_name,
                                      .new_name = edit.current_name}};
            } else if constexpr (std::is_same_v<Edit, CreateEnum>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New enum requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown enum module index"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New enums can only be added to enum modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->enums.size())};
                if (insertion_index > module->enums.size()) {
                    return std::unexpected{SchemaEditError{"Invalid enum insertion index"}};
                }

                module->enums.insert(module->enums.begin() +
                                         static_cast<std::ptrdiff_t>(insertion_index),
                                     edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = std::nullopt});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->enums.erase(module->enums.begin() +
                                        static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteEnum{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceEnum>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{enum_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown enum declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceEnum cannot rename a declaration; use a rename command"}};
                }
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info->module_index])};
                auto previous{module->enums[info->declaration_index]};
                module->enums[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->enums[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceEnum{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteEnum>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{enum_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown enum declaration"}};
                }
                if (found->source.has_value()) {
                    return std::unexpected{SchemaEditError{
                        "Deleting source declarations is not enabled in this authoring slice"}};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::EnumModuleSchema>(&manifest_.modules[info.module_index])};
                module->enums.erase(module->enums.begin() +
                                    static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->enums.insert(module->enums.begin() +
                                             static_cast<std::ptrdiff_t>(info.declaration_index),
                                         schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{CreateEnum{.declaration = edit.declaration,
                                                    .module_index = info.module_index,
                                                    .schema = std::move(schema),
                                                    .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreatePackedValue>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New packed value requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value module index"}};
                }
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{SchemaEditError{
                        "New packed values can only be added to packed-value modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->values.size())};
                if (insertion_index > module->values.size()) {
                    return std::unexpected{SchemaEditError{"Invalid packed-value insertion index"}};
                }

                module->values.insert(module->values.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index),
                                      edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = std::nullopt});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->values.erase(module->values.begin() +
                                         static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeletePackedValue{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplacePackedValue>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{packed_value_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplacePackedValue cannot rename a declaration; use a rename command"}};
                }
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[info->module_index])};
                auto previous{module->values[info->declaration_index]};
                module->values[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->values[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{ReplacePackedValue{.declaration = edit.declaration,
                                                            .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeletePackedValue>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{packed_value_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown packed-value declaration"}};
                }
                if (found->source.has_value()) {
                    return std::unexpected{SchemaEditError{
                        "Deleting source declarations is not enabled in this authoring slice"}};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{std::get_if<codegen::PackedValueModuleSchema>(
                    &manifest_.modules[info.module_index])};
                module->values.erase(module->values.begin() +
                                     static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->values.insert(module->values.begin() +
                                              static_cast<std::ptrdiff_t>(info.declaration_index),
                                          schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    CreatePackedValue{.declaration = edit.declaration,
                                      .module_index = info.module_index,
                                      .schema = std::move(schema),
                                      .insertion_index = info.declaration_index}};
            } else if constexpr (std::is_same_v<Edit, CreateSoa>) {
                if (!edit.declaration.valid() || declaration(edit.declaration) != nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New SoA requires a unique declaration id"}};
                }
                if (edit.module_index >= manifest_.modules.size()) {
                    return std::unexpected{SchemaEditError{"Unknown SoA module index"}};
                }
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[edit.module_index])};
                if (module == nullptr) {
                    return std::unexpected{
                        SchemaEditError{"New SoAs can only be added to SoA modules"}};
                }
                auto const insertion_index{edit.insertion_index.value_or(module->structs.size())};
                if (insertion_index > module->structs.size()) {
                    return std::unexpected{SchemaEditError{"Invalid SoA insertion index"}};
                }

                module->structs.insert(module->structs.begin() +
                                           static_cast<std::ptrdiff_t>(insertion_index),
                                       edit.schema);
                for (auto& existing : declarations_) {
                    if (existing.module_index == edit.module_index &&
                        existing.declaration_index >= insertion_index) {
                        ++existing.declaration_index;
                    }
                }
                auto const namespace_name{module->settings.namespace_name.value_or("")};
                declarations_.push_back(
                    {.id = edit.declaration,
                     .identity = TypeIdentity{.origin = TypeOrigin::declaration,
                                              .module_name = module->settings.name,
                                              .namespace_name = namespace_name,
                                              .name = edit.schema.name},
                     .module_index = edit.module_index,
                     .declaration_index = insertion_index,
                     .source = std::nullopt});
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    declarations_.pop_back();
                    for (auto& existing : declarations_) {
                        if (existing.module_index == edit.module_index &&
                            existing.declaration_index > insertion_index) {
                            --existing.declaration_index;
                        }
                    }
                    module->structs.erase(module->structs.begin() +
                                          static_cast<std::ptrdiff_t>(insertion_index));
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{DeleteSoa{.declaration = edit.declaration}};
            } else if constexpr (std::is_same_v<Edit, ReplaceSoa>) {
                auto const* info{declaration(edit.declaration)};
                auto const* current{soa_schema(edit.declaration)};
                if (info == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
                }
                if (edit.schema.name != current->name) {
                    return std::unexpected{SchemaEditError{
                        "ReplaceSoa cannot rename a declaration; use a rename command"}};
                }
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info->module_index])};
                auto previous{module->structs[info->declaration_index]};
                module->structs[info->declaration_index] = edit.schema;
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->structs[info->declaration_index] = std::move(previous);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{
                    ReplaceSoa{.declaration = edit.declaration, .schema = std::move(previous)}};
            } else if constexpr (std::is_same_v<Edit, DeleteSoa>) {
                auto const* found{declaration(edit.declaration)};
                auto const* current{soa_schema(edit.declaration)};
                if (found == nullptr || current == nullptr) {
                    return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
                }
                if (found->source.has_value()) {
                    return std::unexpected{SchemaEditError{
                        "Deleting source declarations is not enabled in this authoring slice"}};
                }
                auto const info{*found};
                auto schema{*current};
                auto* module{
                    std::get_if<codegen::SoaModuleSchema>(&manifest_.modules[info.module_index])};
                module->structs.erase(module->structs.begin() +
                                      static_cast<std::ptrdiff_t>(info.declaration_index));
                declarations_.erase(
                    std::ranges::find(declarations_, edit.declaration, &DeclarationInfo::id));
                for (auto& existing : declarations_) {
                    if (existing.module_index == info.module_index &&
                        existing.declaration_index > info.declaration_index) {
                        --existing.declaration_index;
                    }
                }
                try {
                    types_ = resolve_type_graph(manifest_);
                } catch (std::exception const& error) {
                    module->structs.insert(module->structs.begin() +
                                               static_cast<std::ptrdiff_t>(info.declaration_index),
                                           schema);
                    for (auto& existing : declarations_) {
                        if (existing.module_index == info.module_index &&
                            existing.declaration_index >= info.declaration_index) {
                            ++existing.declaration_index;
                        }
                    }
                    declarations_.push_back(info);
                    return std::unexpected{SchemaEditError{error.what()}};
                }
                return SchemaEditCommand{CreateSoa{.declaration = edit.declaration,
                                                   .module_index = info.module_index,
                                                   .schema = std::move(schema),
                                                   .insertion_index = info.declaration_index}};
            }
        },
        command);
}

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> const module_paths)
    -> EditableSchemaDocument {
    auto manifest{codegen::load_sources(types_path, module_paths)};
    std::vector<SchemaSourceFile> sources;
    sources.push_back({.path = types_path, .text = read_file(types_path)});

    std::vector<std::optional<SourceRange>> declaration_ranges;
    std::vector<std::optional<SourceRange>> module_ranges;
    auto module_index{std::size_t{}};
    for (auto const& path : module_paths) {
        auto source{read_file(path)};
        auto const source_file_index{sources.size()};
        auto const forms{codegen::sexpr::read_forms(path.string(), source)};
        sources.push_back({.path = path, .text = std::move(source)});
        for (auto const& form : forms) {
            if (module_index >= manifest.modules.size()) {
                throw std::logic_error{"Source contains more modules than the loaded manifest"};
            }
            auto const& module{manifest.modules[module_index++]};
            module_ranges.push_back(form_range(form, source_file_index));
            auto const expected_count{declaration_count(module)};
            if (expected_count == 0) {
                continue;
            }
            if (std::holds_alternative<codegen::VectorModuleSchema>(module)) {
                declaration_ranges.push_back(form_range(form, source_file_index));
                continue;
            }
            auto const head{declaration_head(module)};
            auto found_count{std::size_t{}};
            for (auto const& child : form.children) {
                if (child.head() == head) {
                    declaration_ranges.push_back(form_range(child, source_file_index));
                    ++found_count;
                }
            }
            if (found_count != expected_count) {
                throw std::logic_error{"Source declaration count does not match loaded module"};
            }
        }
    }
    if (module_index != manifest.modules.size()) {
        throw std::logic_error{"Source contains fewer modules than the loaded manifest"};
    }

    EditableSchemaDocument result{std::move(manifest),
                                  std::move(sources),
                                  types_path,
                                  {module_paths.begin(), module_paths.end()}};
    result.module_source_ranges_ = std::move(module_ranges);
    result.initialize_declarations(std::move(declaration_ranges));
    return result;
}

} // namespace lispb::schema
