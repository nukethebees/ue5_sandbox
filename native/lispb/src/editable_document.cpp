#include <lispb/schema/editable_document.h>

#include <codegen/manifest_error.h>
#include <codegen/sexpr/reader.h>
#include <codegen/source_loader.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

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

auto declaration_head(codegen::ModuleSchema const& module) -> std::string_view {
    return std::visit(
        [](auto const& value) -> std::string_view {
            using Module = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                return "enum";
            } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                return "packed-value";
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
                                               std::vector<SchemaSourceFile> source_files)
    : manifest_{std::move(manifest)}
    , types_{resolve_type_graph(manifest_)}
    , source_files_{std::move(source_files)} {}

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

    EditableSchemaDocument result{std::move(manifest), std::move(sources)};
    result.initialize_declarations(std::move(declaration_ranges));
    return result;
}

} // namespace lispb::schema
