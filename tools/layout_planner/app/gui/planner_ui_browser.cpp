#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <codegen/path_utils.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

auto lowercase(std::string_view const text) -> std::string {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

auto parse_integer_literal(std::string_view text) -> std::optional<codegen::PackedIntegerValue> {
    auto negative{false};
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t magnitude{};
    auto const [end,
                error]{std::from_chars(text.data(), text.data() + text.size(), magnitude, base)};
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return codegen::PackedIntegerValue::from_parts(negative, magnitude);
}

auto visible_type(TypeNode const& node) -> bool {
    if (std::holds_alternative<EnumType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<IntegerScalarType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<LinearQuantizedType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<IntegerVarintType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<FixedPointType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<MiniFloatType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<OptionalSentinelType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<OptionalPresenceBitType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<PackedType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<RecordType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<UnionType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<TaggedUnionType>(node.definition)) {
        return true;
    }
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        return true;
    }
    return false;
}

auto type_kind(TypeNode const& node) -> char const* {
    if (std::holds_alternative<EnumType>(node.definition)) {
        return "enum";
    }
    if (std::holds_alternative<IntegerScalarType>(node.definition)) {
        return "integer scalar";
    }
    if (std::holds_alternative<LinearQuantizedType>(node.definition)) {
        return "linear quantized";
    }
    if (std::holds_alternative<IntegerVarintType>(node.definition)) {
        return "integer varint";
    }
    if (std::holds_alternative<FixedPointType>(node.definition)) {
        return "fixed point";
    }
    if (std::holds_alternative<MiniFloatType>(node.definition)) {
        return "mini float";
    }
    if (std::holds_alternative<OptionalSentinelType>(node.definition)) {
        return "optional sentinel";
    }
    if (std::holds_alternative<OptionalPresenceBitType>(node.definition)) {
        return "optional presence bit";
    }
    if (std::holds_alternative<PackedType>(node.definition)) {
        return "packed value";
    }
    if (std::holds_alternative<RecordType>(node.definition)) {
        return "record";
    }
    if (std::holds_alternative<UnionType>(node.definition)) {
        return "union";
    }
    if (std::holds_alternative<TaggedUnionType>(node.definition)) {
        return "tagged union";
    }
    return "SoA";
}

auto module_label(codegen::ModuleSchema const& module) -> std::string {
    return std::visit(
        [](auto const& value) {
            using Module = std::decay_t<decltype(value)>;
            auto const* kind{
                std::is_same_v<Module, codegen::EnumModuleSchema>             ? "enums"
                : std::is_same_v<Module, codegen::ScalarModuleSchema>         ? "integer scalars"
                : std::is_same_v<Module, codegen::RepresentationModuleSchema> ? "representations"
                : std::is_same_v<Module, codegen::PackedValueModuleSchema>    ? "packed values"
                : std::is_same_v<Module, codegen::RecordModuleSchema>         ? "records"
                : std::is_same_v<Module, codegen::UnionModuleSchema>          ? "unions"
                : std::is_same_v<Module, codegen::SoaModuleSchema>            ? "SoA"
                                                                              : "vectors"};
            return value.settings.name + "  [" + kind + "]";
        },
        module);
}

auto is_editable_module_destination(codegen::ModuleSchema const& module) -> bool {
    return std::holds_alternative<codegen::EnumModuleSchema>(module) ||
           std::holds_alternative<codegen::PackedValueModuleSchema>(module) ||
           std::holds_alternative<codegen::ScalarModuleSchema>(module) ||
           std::holds_alternative<codegen::RepresentationModuleSchema>(module) ||
           std::holds_alternative<codegen::RecordModuleSchema>(module) ||
           std::holds_alternative<codegen::UnionModuleSchema>(module) ||
           std::holds_alternative<codegen::SoaModuleSchema>(module);
}

auto matches_filter(TypeNode const& node, std::string_view const filter) -> bool {
    if (filter.empty()) {
        return true;
    }
    auto haystack{node.identity.module_name + " " + node.identity.namespace_name + " " +
                  node.identity.name + " " + node.cpp_spelling};
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->related_storage_name.has_value()) {
        haystack += " " + *soa->related_storage_name;
    }
    return lowercase(haystack).find(lowercase(filter)) != std::string::npos;
}

auto complete(TypeGraph const& types,
              TypeId const type,
              Variant const& baseline,
              AbiProfile const& abi,
              std::uint64_t const default_capacity) -> bool {
    auto const& definition{types.type(type).definition};
    if (std::holds_alternative<EnumType>(definition)) {
        return true;
    }
    if (std::holds_alternative<IntegerScalarType>(definition)) {
        return true;
    }
    if (std::holds_alternative<LinearQuantizedType>(definition)) {
        return true;
    }
    if (std::holds_alternative<IntegerVarintType>(definition)) {
        return true;
    }
    if (std::holds_alternative<FixedPointType>(definition)) {
        return true;
    }
    if (std::holds_alternative<MiniFloatType>(definition)) {
        return true;
    }
    if (std::holds_alternative<OptionalSentinelType>(definition)) {
        return true;
    }
    if (std::holds_alternative<OptionalPresenceBitType>(definition)) {
        return true;
    }
    if (std::holds_alternative<PackedType>(definition)) {
        return !detail::has_error(Analyzer::analyze_packed(types, type, baseline, abi).diagnostics);
    }
    if (std::holds_alternative<RecordType>(definition)) {
        auto const analysis{Analyzer::analyze_record(types, type, abi)};
        return analysis.size_bytes.has_value() && !detail::has_error(analysis.diagnostics);
    }
    if (std::holds_alternative<UnionType>(definition)) {
        auto const analysis{Analyzer::analyze_union(types, type, abi)};
        return analysis.size_bytes.has_value() && !detail::has_error(analysis.diagnostics);
    }
    if (std::holds_alternative<TaggedUnionType>(definition)) {
        return true;
    }
    auto const analysis{Analyzer::analyze_soa(types, type, baseline, abi, default_capacity)};
    return analysis.total_payload_bytes.has_value() && !detail::has_error(analysis.diagnostics);
}

} // namespace

void PlannerUi::draw_project_panel() {
    if (!project_view_open_) {
        return;
    }
    auto const was_open{project_view_open_};
    ImGui::Begin("Project / Schema", &project_view_open_);
    persist_view_visibility(was_open, project_view_open_);
    if (!project_path_.empty()) {
        ImGui::TextDisabled("%s", project_path_.string().c_str());
    }
    if (project_document_.has_value()) {
        ImGui::SeparatorText("Project sources");
        ImGui::TextDisabled("Target: %s", target_name_.c_str());
        std::optional<std::filesystem::path> unregister_source;
        auto const target_found{project_document_->project().targets.find(target_name_)};
        auto const* project_target{
            target_found == project_document_->project().targets.end()
                ? nullptr
                : std::get_if<lispb::CppSchemaTarget>(&target_found->second)};
        if (project_target != nullptr) {
            for (auto const& source : project_target->sources) {
                auto const source_label{source.generic_string()};
                auto const pending{project_document_->source_is_pending(source)};
                ImGui::PushID(source_label.c_str());
                ImGui::BulletText("%s%s", source_label.c_str(), pending ? " (pending new)" : "");
                ImGui::SameLine();
                ImGui::BeginDisabled(pending || (document_.has_value() && document_->dirty()));
                if (ImGui::SmallButton("Unregister")) {
                    unregister_source = source;
                }
                ImGui::EndDisabled();
                if (pending && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Undo the pending creation to remove this unpublished file.");
                }
                ImGui::PopID();
            }
        }
        if (unregister_source.has_value() &&
            apply_project_edit(lispb::RemoveCppSchemaSource{.target_name = target_name_,
                                                            .source = *unregister_source})) {
            schema_edit_message_ =
                "Staged source unregistration. The source file will remain on disk after Save.";
        }
        ImGui::SetNextItemWidth(360.0F);
        ImGui::InputTextWithHint("##new-project-source",
                                 "relative/path/to/new-source.lispb",
                                 new_project_source_path_.data(),
                                 new_project_source_path_.size());
        ImGui::SameLine();
        auto const schema_dirty{document_.has_value() && document_->dirty()};
        ImGui::BeginDisabled(new_project_source_path_.front() == '\0' || schema_dirty);
        if (ImGui::Button("Register existing")) {
            if (apply_project_edit(lispb::AddCppSchemaSource{
                    .target_name = target_name_, .source = new_project_source_path_.data()})) {
                new_project_source_path_.fill('\0');
                source_view_open_ = true;
                focus_source_view_ = true;
                schema_edit_message_ =
                    "Staged an existing LispB source registration. Preview it, then Save to "
                    "reload.";
            }
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(
                "The existing source is validated with the complete target before the draft is "
                "accepted.");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(new_project_source_path_.front() == '\0' || schema_dirty);
        if (ImGui::Button("Create empty")) {
            if (apply_project_edit(
                    lispb::CreateCppSchemaSource{.target_name = target_name_,
                                                 .source = new_project_source_path_.data(),
                                                 .contents = {}})) {
                new_project_source_path_.fill('\0');
                source_view_open_ = true;
                focus_source_view_ = true;
                schema_edit_message_ =
                    "Staged a new empty LispB source. Preview it, then Save to publish and reload.";
            }
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(
                "The destination stays absent until explicit Save. The first module is authored "
                "through + New module after reload.");
        }
        if (project_history_active()) {
            ImGui::TextDisabled(
                project_document_->dirty()
                    ? "Project source-list draft active; schema editing resumes after Save or "
                      "discard."
                    : "Project source-list draft is fully undone; Redo it or discard its history.");
            if (ImGui::SmallButton("Discard project source draft")) {
                auto loaded{load_lispb_schema(project_path_, target_name_)};
                if (loaded.loaded) {
                    adopt_loaded_schema(std::move(loaded));
                    schema_edit_message_ = "Discarded the project source-list draft.";
                } else {
                    schema_edit_message_ = loaded.diagnostics.empty()
                                             ? "Could not reload the LispB project."
                                             : loaded.diagnostics.front().message;
                }
            }
        }
    }

    ImGui::SeparatorText("Schema declarations");
    ImGui::BeginDisabled(!document_.has_value() || project_history_active());
    if (ImGui::Button("+ New module")) {
        declaration_after_new_module_.reset();
        open_new_module_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New enum")) {
        pending_packed_enum_binding_.reset();
        open_new_enum_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New packed value")) {
        open_new_packed_value_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New integer scalar")) {
        open_new_integer_scalar_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New quantization")) {
        open_new_linear_quantized_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New varint")) {
        open_new_integer_varint_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New fixed point")) {
        open_new_fixed_point_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New mini float")) {
        open_new_mini_float_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New optional")) {
        open_new_optional_sentinel_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New presence optional")) {
        open_new_optional_presence_bit_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New record")) {
        pending_soa_record_binding_.reset();
        open_new_record_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New union")) {
        open_new_union_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New tagged union")) {
        open_new_tagged_union_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New SoA")) {
        open_new_soa_dialog_ = true;
    }
    ImGui::EndDisabled();
    if (!schema_edit_message_.empty()) {
        ImGui::SameLine();
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint(
        "##schema-filter", "Filter semantic types", schema_filter_.data(), schema_filter_.size());

    auto const types{workspace_.types().types()};
    if (types.empty() || !document_.has_value()) {
        ImGui::TextDisabled("No semantic types loaded.");
    }

    auto const filter{std::string_view{schema_filter_.data()}};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const modules{document_.has_value() ? std::span{document_->manifest().modules}
                                             : std::span<codegen::ModuleSchema const>{}};
    for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
        std::vector<DeclarationInfo const*> declarations;
        for (auto const& declaration : document_->declarations()) {
            if (declaration.module_index != module_index) {
                continue;
            }
            auto const type{workspace_.types().find(declaration.identity)};
            if (!type.has_value()) {
                continue;
            }
            auto const& node{workspace_.types().type(*type)};
            if (visible_type(node) && matches_filter(node, filter)) {
                declarations.push_back(&declaration);
            }
        }
        if (declarations.empty() &&
            (!filter.empty() || !is_editable_module_destination(modules[module_index]))) {
            continue;
        }
        std::ranges::sort(declarations, {}, &DeclarationInfo::declaration_index);

        auto const label{module_label(modules[module_index])};
        ImGui::PushID(static_cast<int>(module_index));
        auto const open{ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)};
        if (ImGui::IsItemHovered() && !declarations.empty() &&
            declarations.front()->source.has_value()) {
            auto const source_index{declarations.front()->source->source_file_index};
            if (source_index < document_->source_files().size()) {
                ImGui::SetTooltip("%s",
                                  document_->source_files()[source_index].path.string().c_str());
            }
        }
        if (open) {
            if (declarations.empty()) {
                ImGui::TextDisabled("Empty module; choose it from a New declaration dialog.");
            }
            for (auto const* declaration : declarations) {
                auto const type{*workspace_.types().find(declaration->identity)};
                auto const& node{workspace_.types().type(type)};
                auto const item_label{node.identity.name + "  [" + type_kind(node) + "]"};
                auto const selected{selected_type_.has_value() && *selected_type_ == type};
                ImGui::PushID(static_cast<int>(declaration->id.value));
                if (ImGui::Selectable(item_label.c_str(), selected)) {
                    selected_type_ = type;
                    selected_field_.clear();
                    packed_access_fields_.clear();
                    packed_access_set_explicit_ = false;
                    record_access_members_.clear();
                    record_access_set_explicit_ = false;
                    packed_dragged_divider_.reset();
                    packed_dragged_variant_id_.reset();
                }
                ImGui::SameLine();
                auto const* status{
                    complete(
                        workspace_.types(), type, baseline, abi_, workspace_.default_capacity())
                        ? "facts available"
                        : "contains unknowns"};
                ImGui::TextDisabled("%s", status);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (!load_diagnostics_.empty()) {
        ImGui::SeparatorText("Load diagnostics");
        draw_diagnostics(load_diagnostics_);
    }
    ImGui::End();
}

void PlannerUi::draw_new_module_dialog() {
    if (open_new_module_dialog_) {
        ImGui::OpenPopup("New editable module");
        open_new_module_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New editable module", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value() || document_->source_files().size() < 2) {
        ImGui::TextDisabled("No loaded LispB module source can receive a new module.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    constexpr std::array kinds{
        "Enum", "Packed value", "Integer scalar", "Representation", "Record", "Union", "SoA"};
    ImGui::BeginDisabled(declaration_after_new_module_.has_value());
    ImGui::Combo("Kind", &new_module_kind_, kinds.data(), static_cast<int>(kinds.size()));
    ImGui::EndDisabled();
    if (ImGui::InputText("Module name", new_module_name_.data(), new_module_name_.size()) &&
        new_module_header_.front() == '\0') {
        confirm_unchecked_module_header_ = false;
    }
    auto const module_name{std::string{new_module_name_.data()}};
    auto const default_header{module_name + ".h"};
    if (ImGui::InputTextWithHint("Generated header",
                                 default_header.c_str(),
                                 new_module_header_.data(),
                                 new_module_header_.size())) {
        confirm_unchecked_module_header_ = false;
    }
    ImGui::InputText(
        "Namespace (optional)", new_module_namespace_.data(), new_module_namespace_.size());

    auto const sources{document_->source_files()};
    if (new_module_source_file_index_ == 0 || new_module_source_file_index_ >= sources.size()) {
        new_module_source_file_index_ = 1;
    }
    auto const source_label{sources[new_module_source_file_index_].path.filename().string()};
    if (ImGui::BeginCombo("LispB source", source_label.c_str())) {
        for (std::size_t index{1}; index < sources.size(); ++index) {
            auto const label{sources[index].path.string()};
            if (ImGui::Selectable(label.c_str(), index == new_module_source_file_index_)) {
                new_module_source_file_index_ = index;
            }
        }
        ImGui::EndCombo();
    }
    if (declaration_after_new_module_.has_value()) {
        ImGui::TextDisabled("Creates this module, then opens the requested declaration form.");
    } else {
        ImGui::TextDisabled(
            "Creates a valid empty destination; add declarations with the normal New controls.");
    }

    auto const header{std::filesystem::path{new_module_header_.front() == '\0'
                                                ? default_header
                                                : std::string{new_module_header_.data()}}};
    std::string conflict;
    if (module_name.empty()) {
        conflict = "Enter a module name.";
    } else if (new_module_header_.front() == '\0' &&
               !std::ranges::all_of(module_name, [](unsigned char const character) {
                   return std::isalnum(character) != 0 || character == '_' || character == '-';
               })) {
        conflict = "Enter an explicit header for this module name.";
    } else if (header.is_absolute() || header.has_root_path() ||
               std::ranges::find(header, std::filesystem::path{".."}) != header.end() ||
               header.filename().empty() || header.filename() == ".") {
        conflict = "Generated header must be a relative file path within the output root.";
    }

    if (conflict.empty()) {
        auto const header_key{codegen::output_path_key(header)};
        for (auto const& candidate : document_->manifest().modules) {
            std::visit(
                [&](auto const& module) {
                    if (module.settings.name == module_name) {
                        conflict = "A module with this name already exists.";
                    } else if (codegen::output_path_key(module.settings.header) == header_key ||
                               (module.settings.source.has_value() &&
                                codegen::output_path_key(*module.settings.source) == header_key)) {
                        conflict = "Another module already uses this generated output path.";
                    }
                },
                candidate);
            if (!conflict.empty()) {
                break;
            }
        }
    }

    auto output_location_known{false};
    if (conflict.empty() && project_document_.has_value()) {
        auto const& project{project_document_->project()};
        auto const target{project.targets.find(target_name_)};
        if (target != project.targets.end()) {
            if (auto const* schema{std::get_if<lispb::CppSchemaTarget>(&target->second)}) {
                if (schema->output_root.base == lispb::PathBase::project) {
                    output_location_known = true;
                    std::error_code error;
                    auto const output_path{project.root / schema->output_root.path / header};
                    if (std::filesystem::exists(output_path, error)) {
                        conflict = "A file already exists at " + output_path.string() +
                                   ". Choose another header.";
                    } else if (error) {
                        conflict =
                            "Cannot inspect generated header destination: " + error.message();
                    }
                }
            }
        }
    }
    if (conflict.empty() && !output_location_known) {
        ImGui::TextColored(ImVec4{1.0F, 0.85F, 0.2F, 1.0F},
                           "Output location cannot be checked; check for an existing file first.");
        ImGui::Checkbox("I checked the output path", &confirm_unchecked_module_header_);
    }
    if (!conflict.empty()) {
        ImGui::TextColored(ImVec4{1.0F, 0.85F, 0.2F, 1.0F}, "%s", conflict.c_str());
    }

    auto const ready{conflict.empty() &&
                     (output_location_known || confirm_unchecked_module_header_) &&
                     new_module_kind_ >= 0 && new_module_kind_ < static_cast<int>(kinds.size())};
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button("Create")) {
        auto const settings{codegen::ModuleSettings{
            .name = new_module_name_.data(),
            .header = header,
            .source = std::nullopt,
            .header_include = std::nullopt,
            .namespace_name = new_module_namespace_.front() == '\0'
                                ? std::nullopt
                                : std::optional<std::string>{new_module_namespace_.data()},
            .include_order = {},
            .prelude_lines = {}}};
        auto module = [&]() -> codegen::ModuleSchema {
            switch (new_module_kind_) {
                case 0:
                    return codegen::EnumModuleSchema{
                        .settings = settings, .helper_namespace = std::nullopt, .enums = {}};
                case 1:
                    return codegen::PackedValueModuleSchema{.settings = settings, .values = {}};
                case 2:
                    return codegen::ScalarModuleSchema{.settings = settings, .scalars = {}};
                case 3:
                    return codegen::RepresentationModuleSchema{.settings = settings,
                                                               .linear_quantized = {},
                                                               .integer_varints = {},
                                                               .fixed_points = {},
                                                               .optional_sentinels = {},
                                                               .optional_presence_bits = {},
                                                               .mini_floats = {}};
                case 4:
                    return codegen::RecordModuleSchema{.settings = settings, .records = {}};
                case 5:
                    return codegen::UnionModuleSchema{
                        .settings = settings, .unions = {}, .tagged_unions = {}};
                default:
                    return codegen::SoaModuleSchema{.settings = settings,
                                                    .structs = {},
                                                    .backend =
                                                        codegen::SoaBackend::standard_library,
                                                    .array_allocators = {}};
            }
        }();
        if (apply_document_edit(CreateModule{.source_file_index = new_module_source_file_index_,
                                             .schema = std::move(module)})) {
            schema_warning_message_.clear();
            new_module_name_.fill('\0');
            new_module_header_.fill('\0');
            new_module_namespace_.fill('\0');
            confirm_unchecked_module_header_ = false;
            ImGui::CloseCurrentPopup();
            if (declaration_after_new_module_.has_value()) {
                switch (*declaration_after_new_module_) {
                    case NewDeclarationDialog::enumeration:
                        open_new_enum_dialog_ = true;
                        break;
                    case NewDeclarationDialog::packed_value:
                        open_new_packed_value_dialog_ = true;
                        break;
                    case NewDeclarationDialog::integer_scalar:
                        open_new_integer_scalar_dialog_ = true;
                        break;
                    case NewDeclarationDialog::quantization:
                        open_new_linear_quantized_dialog_ = true;
                        break;
                    case NewDeclarationDialog::varint:
                        open_new_integer_varint_dialog_ = true;
                        break;
                    case NewDeclarationDialog::fixed_point:
                        open_new_fixed_point_dialog_ = true;
                        break;
                    case NewDeclarationDialog::mini_float:
                        open_new_mini_float_dialog_ = true;
                        break;
                    case NewDeclarationDialog::optional_sentinel:
                        open_new_optional_sentinel_dialog_ = true;
                        break;
                    case NewDeclarationDialog::optional_presence_bit:
                        open_new_optional_presence_bit_dialog_ = true;
                        break;
                    case NewDeclarationDialog::record:
                        open_new_record_dialog_ = true;
                        break;
                    case NewDeclarationDialog::union_value:
                        open_new_union_dialog_ = true;
                        break;
                    case NewDeclarationDialog::tagged_union:
                        open_new_tagged_union_dialog_ = true;
                        break;
                    case NewDeclarationDialog::soa:
                        open_new_soa_dialog_ = true;
                        break;
                }
                declaration_after_new_module_.reset();
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        declaration_after_new_module_.reset();
        new_module_name_.fill('\0');
        new_module_header_.fill('\0');
        new_module_namespace_.fill('\0');
        confirm_unchecked_module_header_ = false;
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_enum_dialog() {
    if (open_new_enum_dialog_) {
        ImGui::OpenPopup("New enum");
        open_new_enum_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New enum", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_enum_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared enum declaration and bind packed field '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            pending_packed_enum_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_enum_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::EnumModuleSchema>(modules[index])) {
            first_enum_module = index;
            break;
        }
    }
    if (!first_enum_module.has_value()) {
        ImGui::TextDisabled("The target has no enum module to receive a new declaration.");
    } else {
        if (new_enum_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::EnumModuleSchema>(modules[new_enum_module_index_])) {
            new_enum_module_index_ = *first_enum_module;
        }
        auto const& selected_module{
            std::get<codegen::EnumModuleSchema>(modules[new_enum_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::EnumModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_enum_module_index_)) {
                    new_enum_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_enum_name_.data(), new_enum_name_.size());
        ImGui::Checkbox("Auto C++ backing", &new_enum_backing_auto_);
        if (!new_enum_backing_auto_) {
            ImGui::InputText("C++ backing type",
                             new_enum_underlying_type_.data(),
                             new_enum_underlying_type_.size());
            ImGui::TextDisabled("Use a registered name such as @native_uint8 or a C++ spelling.");
        } else {
            ImGui::TextDisabled(
                "A legal fixed-width C++ backing is derived from the semantic domain.");
        }
        ImGui::Checkbox("Auto semantic width", &new_enum_width_auto_);
        if (!new_enum_width_auto_) {
            ImGui::InputScalar(
                "Semantic width", ImGuiDataType_U32, &new_enum_bit_width_, nullptr, nullptr, "%u");
        }
        ImGui::TextDisabled(
            "Semantic width is independent of the C++ type used by the current lowering target.");
        constexpr std::array signedness_labels{"Auto / inferred", "Unsigned", "Signed"};
        ImGui::Combo("Semantic signedness",
                     &new_enum_signedness_,
                     signedness_labels.data(),
                     static_cast<int>(signedness_labels.size()));

        auto const ready{
            new_enum_name_.front() != '\0' &&
            (new_enum_backing_auto_ || new_enum_underlying_type_.front() != '\0') &&
            (new_enum_width_auto_ || (new_enum_bit_width_ >= 1 && new_enum_bit_width_ <= 64))};
        ImGui::BeginDisabled(!ready);
        auto const create_label{pending_packed_enum_binding_.has_value() ? "Create and use"
                                                                         : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{
                std::get<codegen::EnumModuleSchema>(modules[new_enum_module_index_])};
            auto const name{std::string{new_enum_name_.data()}};
            auto const bit_width{new_enum_width_auto_ ? std::optional<std::uint32_t>{}
                                                      : std::optional{new_enum_bit_width_}};
            auto const signedness{new_enum_signedness_ == 0   ? std::optional<bool>{}
                                  : new_enum_signedness_ == 2 ? std::optional<bool>{true}
                                                              : std::optional<bool>{false}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_enum_binding_.has_value()) {
                if (auto const* packed_info{
                        document_->declaration(pending_packed_enum_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateEnum{
                        .declaration = id,
                        .module_index = new_enum_module_index_,
                        .schema =
                            codegen::EnumSchema{
                                .name = name,
                                .underlying_type = new_enum_backing_auto_
                                                     ? std::optional<codegen::TypeRef>{}
                                                     : std::optional{codegen::TypeRef{
                                                           .name = new_enum_underlying_type_.data(),
                                                           .suffix = {},
                                                           .nested = std::nullopt}},
                                .bit_width = bit_width,
                                .signedness = signedness,
                                .reflection = codegen::EnumReflection::none,
                                .values = {{.name = "Value0",
                                            .initializer = "0",
                                            .display_name = std::nullopt,
                                            .hidden = false,
                                            .serialized_name = std::nullopt}},
                                .enum_array = false,
                                .count = std::nullopt,
                                .conversions = {},
                                .export_specifier = std::nullopt,
                                .native_api = false,
                                .unreal_projection = std::nullopt},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_enum_to_packed_field(identity)) {
                new_enum_name_.fill('\0');
                std::snprintf(new_enum_underlying_type_.data(),
                              new_enum_underlying_type_.size(),
                              "%s",
                              "std::uint8_t");
                new_enum_width_auto_ = true;
                new_enum_bit_width_ = 1;
                new_enum_signedness_ = 0;
                new_enum_backing_auto_ = true;
                selected_enumerator_ = "Value0";
                pending_packed_enum_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_enum_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::bind_new_enum_to_packed_field(TypeIdentity const& enumeration) -> bool {
    if (!pending_packed_enum_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_enum_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new enum was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) + " The new enum could not be rolled back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new enum could not be rolled back because history did "
                                   "not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = enumeration.namespace_name.empty()
                                             ? enumeration.name
                                             : enumeration.namespace_name + "::" + enumeration.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::enumeration;
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The enum was valid, but binding the packed field failed: " +
                                 schema_edit_message_);
    }

    selected_field_ = binding.field_name;
    return true;
}

auto PlannerUi::bind_new_integer_scalar_to_packed_field(TypeIdentity const& scalar) -> bool {
    if (!pending_packed_integer_scalar_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_integer_scalar_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new integer scalar was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ =
                std::move(message) +
                " The new integer scalar could not be rolled back: " + rollback.error().message;
        } else {
            schema_edit_message_ =
                std::move(message) +
                " The new integer scalar could not be rolled back because history did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto const scalar_declaration{document_->find_declaration(scalar)};
    auto const* scalar_schema{scalar_declaration.has_value()
                                  ? document_->integer_scalar_schema(*scalar_declaration)
                                  : nullptr};
    if (scalar_schema == nullptr) {
        return rollback_creation("The new integer scalar is no longer available.");
    }

    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = scalar.namespace_name.empty()
                                             ? scalar.name
                                             : scalar.namespace_name + "::" + scalar.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = scalar_schema->signedness ? codegen::PackedFieldKind::signed_integer
                                            : codegen::PackedFieldKind::unsigned_integer;
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The integer scalar was valid, but binding the packed field "
                                 "failed: " +
                                 schema_edit_message_);
    }

    selected_field_ = binding.field_name;
    return true;
}

void PlannerUi::draw_new_packed_value_dialog() {
    if (open_new_packed_value_dialog_) {
        ImGui::OpenPopup("New packed value");
        open_new_packed_value_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New packed value", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_packed_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::PackedValueModuleSchema>(modules[index])) {
            first_packed_module = index;
            break;
        }
    }
    if (!first_packed_module.has_value()) {
        ImGui::TextDisabled("The target has no packed-value module to receive a new declaration.");
    } else {
        if (new_packed_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::PackedValueModuleSchema>(
                modules[new_packed_module_index_])) {
            new_packed_module_index_ = *first_packed_module;
        }
        auto const& selected_module{
            std::get<codegen::PackedValueModuleSchema>(modules[new_packed_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::PackedValueModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_packed_module_index_)) {
                    new_packed_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_packed_value_name_.data(), new_packed_value_name_.size());
        ImGui::InputText(
            "Storage type", new_packed_storage_type_.data(), new_packed_storage_type_.size());
        constexpr std::array byte_order_labels{"Unspecified", "Little endian", "Big endian"};
        ImGui::Combo("Serialized byte order",
                     &new_packed_byte_order_,
                     byte_order_labels.data(),
                     static_cast<int>(byte_order_labels.size()));
        constexpr std::array bit_order_labels{
            "Default (LSB-first)", "Explicit LSB-first", "MSB-first"};
        ImGui::Combo("Segment bit order",
                     &new_packed_bit_order_,
                     bit_order_labels.data(),
                     static_cast<int>(bit_order_labels.size()));
        ImGui::TextDisabled("The declaration starts with one editable 1-bit uint8 field.");
        ImGui::TextDisabled(
            "Byte order describes serialized bytes; it does not change the host integer ABI.");

        auto const ready{new_packed_value_name_.front() != '\0' &&
                         new_packed_storage_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{
                std::get<codegen::PackedValueModuleSchema>(modules[new_packed_module_index_])};
            auto const name{std::string{new_packed_value_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreatePackedValue{
                        .declaration = id,
                        .module_index = new_packed_module_index_,
                        .schema =
                            codegen::PackedValueSchema{
                                .name = name,
                                .storage_type =
                                    codegen::TypeRef{.name = new_packed_storage_type_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .segments = {codegen::PackedFieldSchema{
                                    .name = "value",
                                    .type = codegen::TypeRef{.name = "std::uint8_t",
                                                             .suffix = {},
                                                             .nested = std::nullopt},
                                    .bits = 1,
                                    .kind = codegen::PackedFieldKind::unsigned_integer,
                                    .range_helper = false,
                                    .minimum_value = std::nullopt,
                                    .maximum_value = std::nullopt,
                                    .named_codes = {},
                                    .relationship = std::nullopt}},
                                .invalid_value = std::nullopt,
                                .export_specifier = std::nullopt,
                                .byte_order =
                                    new_packed_byte_order_ == 0
                                        ? std::nullopt
                                        : std::optional{new_packed_byte_order_ == 1
                                                            ? codegen::PackedByteOrder::
                                                                  little_endian
                                                            : codegen::PackedByteOrder::big_endian},
                                .bit_order =
                                    new_packed_bit_order_ == 0
                                        ? std::nullopt
                                        : std::
                                              optional{new_packed_bit_order_ == 1
                                                           ? codegen::PackedBitOrder::
                                                                 least_significant_first
                                                           : codegen::PackedBitOrder::
                                                                 most_significant_first}},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_packed_value_name_.fill('\0');
                std::snprintf(new_packed_storage_type_.data(),
                              new_packed_storage_type_.size(),
                              "%s",
                              "std::uint32_t");
                new_packed_byte_order_ = 0;
                new_packed_bit_order_ = 0;
                selected_field_ = "value";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_integer_scalar_dialog() {
    if (open_new_integer_scalar_dialog_) {
        ImGui::OpenPopup("New integer scalar");
        open_new_integer_scalar_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New integer scalar", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_integer_scalar_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared integer-scalar domain and bind packed field '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            pending_packed_integer_scalar_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_scalar_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::ScalarModuleSchema>(modules[index])) {
            first_scalar_module = index;
            break;
        }
    }
    if (!first_scalar_module.has_value()) {
        ImGui::TextDisabled("The target has no scalar module to receive a new declaration.");
    } else {
        if (new_integer_scalar_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::ScalarModuleSchema>(
                modules[new_integer_scalar_module_index_])) {
            new_integer_scalar_module_index_ = *first_scalar_module;
        }
        auto const& selected_module{
            std::get<codegen::ScalarModuleSchema>(modules[new_integer_scalar_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::ScalarModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_integer_scalar_module_index_)) {
                    new_integer_scalar_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_integer_scalar_name_.data(), new_integer_scalar_name_.size());
        ImGui::Checkbox("Signed domain", &new_integer_scalar_signed_);
        ImGui::InputText(
            "Minimum", new_integer_scalar_minimum_.data(), new_integer_scalar_minimum_.size());
        ImGui::InputText(
            "Maximum", new_integer_scalar_maximum_.data(), new_integer_scalar_maximum_.size());
        ImGui::Checkbox("Auto bit width", &new_integer_scalar_width_auto_);
        ImGui::BeginDisabled(new_integer_scalar_width_auto_);
        ImGui::InputScalar(
            "Bit width", ImGuiDataType_U32, &new_integer_scalar_bit_width_, nullptr, nullptr);
        ImGui::EndDisabled();
        ImGui::TextDisabled(
            "This declares semantic meaning only; it does not choose a physical C++ type.");

        auto const minimum{parse_integer_literal(new_integer_scalar_minimum_.data())};
        auto const maximum{parse_integer_literal(new_integer_scalar_maximum_.data())};
        auto const ready{new_integer_scalar_name_.front() != '\0' && minimum.has_value() &&
                         maximum.has_value() &&
                         (new_integer_scalar_width_auto_ || (new_integer_scalar_bit_width_ >= 1 &&
                                                             new_integer_scalar_bit_width_ <= 64))};
        ImGui::BeginDisabled(!ready);
        auto const create_label{
            pending_packed_integer_scalar_binding_.has_value() ? "Create and use" : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{
                std::get<codegen::ScalarModuleSchema>(modules[new_integer_scalar_module_index_])};
            auto const name{std::string{new_integer_scalar_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            auto named_codes{std::vector<codegen::PackedNamedCodeSchema>{}};
            if (pending_packed_integer_scalar_binding_.has_value()) {
                auto const& binding{*pending_packed_integer_scalar_binding_};
                if (auto const* packed_info{document_->declaration(binding.packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
                if (auto const* packed_schema{
                        document_->packed_value_schema(binding.packed_declaration)};
                    packed_schema != nullptr) {
                    auto const segment{
                        std::ranges::find_if(packed_schema->segments, [&](auto const& candidate) {
                            return codegen::packed_segment_name(candidate) == binding.field_name;
                        })};
                    if (segment != packed_schema->segments.end()) {
                        if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&*segment)}) {
                            named_codes = field->named_codes;
                        }
                    }
                }
            }
            if (apply_document_edit(
                    CreateIntegerScalar{
                        .declaration = id,
                        .module_index = new_integer_scalar_module_index_,
                        .schema =
                            codegen::IntegerScalarSchema{
                                .name = name,
                                .signedness = new_integer_scalar_signed_,
                                .minimum_value = *minimum,
                                .maximum_value = *maximum,
                                .bit_width = new_integer_scalar_width_auto_
                                               ? std::nullopt
                                               : std::optional{new_integer_scalar_bit_width_},
                                .named_codes = std::move(named_codes),
                                .relationship = std::nullopt,
                                .cpp_emission = codegen::IntegerScalarCppEmission::none,
                                .cpp_type = std::nullopt},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_integer_scalar_to_packed_field(identity)) {
                new_integer_scalar_name_.fill('\0');
                std::snprintf(new_integer_scalar_minimum_.data(),
                              new_integer_scalar_minimum_.size(),
                              "%s",
                              "0");
                std::snprintf(new_integer_scalar_maximum_.data(),
                              new_integer_scalar_maximum_.size(),
                              "%s",
                              "255");
                new_integer_scalar_signed_ = false;
                new_integer_scalar_width_auto_ = true;
                new_integer_scalar_bit_width_ = 8;
                selected_integer_scalar_code_.clear();
                pending_packed_integer_scalar_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (!minimum.has_value() || !maximum.has_value()) {
            ImGui::TextDisabled(
                "Minimum and maximum must be signed decimal or hexadecimal values.");
        }
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_integer_scalar_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_linear_quantized_dialog() {
    if (open_new_linear_quantized_dialog_) {
        ImGui::OpenPopup("New linear quantization");
        open_new_linear_quantized_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New linear quantization", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else {
        if (new_linear_quantized_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_linear_quantized_module_index_])) {
            new_linear_quantized_module_index_ = *first_module;
        }
        auto const& selected_module{std::get<codegen::RepresentationModuleSchema>(
            modules[new_linear_quantized_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_linear_quantized_module_index_)) {
                    new_linear_quantized_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_linear_quantized_name_.data(), new_linear_quantized_name_.size());
        if (new_linear_quantized_source_.front() == '\0') {
            for (auto const& node : document_->types().types()) {
                if (std::holds_alternative<IntegerScalarType>(node.definition)) {
                    std::snprintf(new_linear_quantized_source_.data(),
                                  new_linear_quantized_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    break;
                }
            }
        }
        auto const source_label{new_linear_quantized_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_linear_quantized_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            for (auto const& node : document_->types().types()) {
                if (!std::holds_alternative<IntegerScalarType>(node.definition)) {
                    continue;
                }
                auto const selected{node.cpp_spelling == new_linear_quantized_source_.data()};
                if (ImGui::Selectable(node.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_linear_quantized_source_.data(),
                                  new_linear_quantized_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputScalar("Encoded bit width",
                           ImGuiDataType_U32,
                           &new_linear_quantized_bit_width_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Reserved codes",
                           ImGuiDataType_U64,
                           &new_linear_quantized_reserved_codes_,
                           nullptr,
                           nullptr,
                           "%llu");
        constexpr std::array clipping_labels{"Reject", "Clamp"};
        ImGui::Combo("Out-of-range values",
                     &new_linear_quantized_clipping_,
                     clipping_labels.data(),
                     static_cast<int>(clipping_labels.size()));
        ImGui::TextDisabled(
            "Linear endpoint mapping; encoded width is not a standalone ABI sizeof.");

        auto maximum_reserved{std::optional<std::uint64_t>{}};
        if (new_linear_quantized_bit_width_ >= 1 && new_linear_quantized_bit_width_ <= 64) {
            maximum_reserved = new_linear_quantized_bit_width_ == 64
                                 ? (std::numeric_limits<std::uint64_t>::max)() - 1
                                 : (std::uint64_t{1} << new_linear_quantized_bit_width_) - 2;
        }
        auto const ready{new_linear_quantized_name_.front() != '\0' &&
                         new_linear_quantized_source_.front() != '\0' &&
                         maximum_reserved.has_value() &&
                         new_linear_quantized_reserved_codes_ <= *maximum_reserved};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_linear_quantized_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateLinearQuantized{
                        .declaration = id,
                        .module_index = new_linear_quantized_module_index_,
                        .schema =
                            codegen::LinearQuantizedSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_linear_quantized_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .bit_width = new_linear_quantized_bit_width_,
                                .reserved_codes = new_linear_quantized_reserved_codes_,
                                .clipping = new_linear_quantized_clipping_ == 0
                                              ? codegen::QuantizationClipping::reject
                                              : codegen::QuantizationClipping::clamp},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_linear_quantized_name_.fill('\0');
                new_linear_quantized_bit_width_ = 8;
                new_linear_quantized_reserved_codes_ = 0;
                new_linear_quantized_clipping_ = 0;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (maximum_reserved.has_value() &&
            new_linear_quantized_reserved_codes_ > *maximum_reserved) {
            ImGui::TextDisabled("At least two usable codes are required.");
        }
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_integer_varint_dialog() {
    if (open_new_integer_varint_dialog_) {
        ImGui::OpenPopup("New integer varint");
        open_new_integer_varint_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New integer varint", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else {
        if (new_integer_varint_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_integer_varint_module_index_])) {
            new_integer_varint_module_index_ = *first_module;
        }
        auto const& selected_module{std::get<codegen::RepresentationModuleSchema>(
            modules[new_integer_varint_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_integer_varint_module_index_)) {
                    new_integer_varint_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_integer_varint_name_.data(), new_integer_varint_name_.size());
        if (new_integer_varint_source_.front() == '\0') {
            for (auto const& node : document_->types().types()) {
                if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
                    std::snprintf(new_integer_varint_source_.data(),
                                  new_integer_varint_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    new_integer_varint_encoding_ = scalar->signedness ? 2 : 0;
                    break;
                }
            }
        }
        auto source_signed{std::optional<bool>{}};
        for (auto const& node : document_->types().types()) {
            if (node.cpp_spelling == new_integer_varint_source_.data()) {
                if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
                    source_signed = scalar->signedness;
                }
                break;
            }
        }
        auto const source_label{new_integer_varint_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_integer_varint_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            for (auto const& node : document_->types().types()) {
                auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)};
                if (scalar == nullptr) {
                    continue;
                }
                auto const selected{node.cpp_spelling == new_integer_varint_source_.data()};
                if (ImGui::Selectable(node.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_integer_varint_source_.data(),
                                  new_integer_varint_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    source_signed = scalar->signedness;
                    new_integer_varint_encoding_ = scalar->signedness ? 2 : 0;
                }
            }
            ImGui::EndCombo();
        }
        constexpr std::array signed_encoding_labels{"Signed varint", "ZigZag varint"};
        if (source_signed.value_or(false)) {
            auto signed_encoding{new_integer_varint_encoding_ == 1 ? 0 : 1};
            ImGui::Combo("Encoding",
                         &signed_encoding,
                         signed_encoding_labels.data(),
                         static_cast<int>(signed_encoding_labels.size()));
            new_integer_varint_encoding_ = signed_encoding == 0 ? 1 : 2;
        } else {
            new_integer_varint_encoding_ = 0;
            ImGui::TextUnformatted("Encoding: unsigned varint");
        }
        ImGui::TextDisabled(
            "Encoded size is variable; no fixed ABI sizeof or expected size is inferred.");

        auto const ready{new_integer_varint_name_.front() != '\0' &&
                         new_integer_varint_source_.front() != '\0' && source_signed.has_value()};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_integer_varint_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const encoding{new_integer_varint_encoding_ == 0
                                    ? codegen::IntegerVarintEncoding::unsigned_varint
                                : new_integer_varint_encoding_ == 1
                                    ? codegen::IntegerVarintEncoding::signed_varint
                                    : codegen::IntegerVarintEncoding::zigzag_varint};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateIntegerVarint{
                        .declaration = id,
                        .module_index = new_integer_varint_module_index_,
                        .schema =
                            codegen::IntegerVarintSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_integer_varint_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .encoding = encoding},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_integer_varint_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::bind_new_fixed_point_to_packed_field(TypeIdentity const& fixed_point) -> bool {
    if (!pending_packed_fixed_point_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_fixed_point_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new fixed point was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) +
                                   " The new fixed point could not be rolled "
                                   "back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new fixed point could not be rolled back because history "
                                   "did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = fixed_point.namespace_name.empty()
                                             ? fixed_point.name
                                             : fixed_point.namespace_name + "::" + fixed_point.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::fixed_point;
    field->bits.reset();
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    field->relationship.reset();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation(
            "The fixed point was valid, but binding the packed field failed: " +
            schema_edit_message_);
    }

    selected_field_ = binding.field_name;
    return true;
}

void PlannerUi::draw_new_fixed_point_dialog() {
    if (open_new_fixed_point_dialog_) {
        ImGui::OpenPopup("New fixed point");
        open_new_fixed_point_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New fixed point", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            pending_packed_fixed_point_binding_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_fixed_point_binding_.has_value()) {
        ImGui::TextWrapped("Create a shared fixed-point representation and bind packed field '%s' "
                           "to it. Creation and binding are separate undoable history steps.",
                           pending_packed_fixed_point_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else {
        if (new_fixed_point_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_fixed_point_module_index_])) {
            new_fixed_point_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::RepresentationModuleSchema>(modules[new_fixed_point_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_fixed_point_module_index_)) {
                    new_fixed_point_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_fixed_point_name_.data(), new_fixed_point_name_.size());
        ImGui::Checkbox("Signed", &new_fixed_point_signed_);
        ImGui::InputScalar(
            "Total width", ImGuiDataType_U32, &new_fixed_point_total_bits_, nullptr, nullptr, "%u");
        ImGui::InputScalar("Fractional width",
                           ImGuiDataType_U32,
                           &new_fixed_point_fractional_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        constexpr std::array rounding_labels{"Nearest even", "Toward zero"};
        ImGui::Combo("Rounding",
                     &new_fixed_point_rounding_,
                     rounding_labels.data(),
                     static_cast<int>(rounding_labels.size()));

        auto const total_valid{new_fixed_point_total_bits_ >= 1 &&
                               new_fixed_point_total_bits_ <= 64};
        auto const maximum_fractional{
            total_valid ? new_fixed_point_total_bits_ - (new_fixed_point_signed_ ? 1U : 0U) : 0U};
        auto const widths_valid{total_valid &&
                                new_fixed_point_fractional_bits_ <= maximum_fractional};
        if (!widths_valid) {
            ImGui::TextDisabled(new_fixed_point_signed_
                                    ? "Fractional width must leave one sign bit."
                                    : "Fractional width cannot exceed total width.");
        }
        ImGui::TextDisabled(
            "Encoded bits are representation facts; no standalone ABI sizeof is inferred.");

        auto const ready{new_fixed_point_name_.front() != '\0' && widths_valid};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button(pending_packed_fixed_point_binding_.has_value() ? "Create and use"
                                                                          : "Create")) {
            auto const name{std::string{new_fixed_point_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_fixed_point_binding_.has_value()) {
                if (auto const* packed_info{document_->declaration(
                        pending_packed_fixed_point_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateFixedPoint{
                        .declaration = id,
                        .module_index = new_fixed_point_module_index_,
                        .schema =
                            codegen::FixedPointSchema{
                                .name = name,
                                .signedness = new_fixed_point_signed_,
                                .total_bits = new_fixed_point_total_bits_,
                                .fractional_bits = new_fixed_point_fractional_bits_,
                                .rounding = new_fixed_point_rounding_ == 0
                                              ? codegen::FixedPointRounding::nearest_even
                                              : codegen::FixedPointRounding::toward_zero},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_fixed_point_to_packed_field(identity)) {
                new_fixed_point_name_.fill('\0');
                new_fixed_point_signed_ = true;
                new_fixed_point_total_bits_ = 16;
                new_fixed_point_fractional_bits_ = 8;
                new_fixed_point_rounding_ = 0;
                pending_packed_fixed_point_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_fixed_point_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::bind_new_mini_float_to_packed_field(TypeIdentity const& mini_float) -> bool {
    if (!pending_packed_mini_float_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_mini_float_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new mini float was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) +
                                   " The new mini float could not be rolled "
                                   "back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new mini float could not be rolled back because history "
                                   "did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = mini_float.namespace_name.empty()
                                             ? mini_float.name
                                             : mini_float.namespace_name + "::" + mini_float.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::mini_float;
    field->bits.reset();
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    field->relationship.reset();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The mini float was valid, but binding the packed field failed: " +
                                 schema_edit_message_);
    }

    selected_field_ = binding.field_name;
    return true;
}

void PlannerUi::draw_new_mini_float_dialog() {
    if (open_new_mini_float_dialog_) {
        ImGui::OpenPopup("New mini float");
        open_new_mini_float_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New mini float", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            pending_packed_mini_float_binding_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_mini_float_binding_.has_value()) {
        ImGui::TextWrapped("Create a shared mini-float representation and bind packed field '%s' "
                           "to it. Creation and binding are separate undoable history steps.",
                           pending_packed_mini_float_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else {
        if (new_mini_float_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_mini_float_module_index_])) {
            new_mini_float_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::RepresentationModuleSchema>(modules[new_mini_float_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module != nullptr && ImGui::Selectable(module->settings.name.c_str(),
                                                           index == new_mini_float_module_index_)) {
                    new_mini_float_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_mini_float_name_.data(), new_mini_float_name_.size());
        ImGui::InputScalar(
            "Sign bits", ImGuiDataType_U32, &new_mini_float_sign_bits_, nullptr, nullptr, "%u");
        ImGui::InputScalar("Exponent bits",
                           ImGuiDataType_U32,
                           &new_mini_float_exponent_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Significand bits",
                           ImGuiDataType_U32,
                           &new_mini_float_significand_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Exponent bias",
                           ImGuiDataType_S32,
                           &new_mini_float_exponent_bias_,
                           nullptr,
                           nullptr,
                           "%d");

        auto const widths_valid{
            new_mini_float_sign_bits_ <= 1 && new_mini_float_exponent_bits_ >= 2 &&
            new_mini_float_exponent_bits_ <= 15 && new_mini_float_significand_bits_ <= 62 &&
            static_cast<std::uint64_t>(new_mini_float_sign_bits_) + new_mini_float_exponent_bits_ +
                    new_mini_float_significand_bits_ <=
                64};
        auto const bias_valid{new_mini_float_exponent_bias_ >= -32'768 &&
                              new_mini_float_exponent_bias_ <= 32'767};
        if (!widths_valid) {
            ImGui::TextDisabled(
                "Sign must be 0..1, exponent 2..15, significand 0..62, and total at most 64.");
        }
        if (!bias_valid) {
            ImGui::TextDisabled("Exponent bias must be in the range -32768..32767.");
        }
        ImGui::TextDisabled(
            "All-zero/all-one exponents encode zero/subnormal and infinity/NaN roles.");
        ImGui::TextDisabled(
            "Encoded bits do not imply native arithmetic, byte order, or an ABI sizeof.");

        auto const ready{new_mini_float_name_.front() != '\0' && widths_valid && bias_valid};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button(pending_packed_mini_float_binding_.has_value() ? "Create and use"
                                                                         : "Create")) {
            auto const name{std::string{new_mini_float_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_mini_float_binding_.has_value()) {
                if (auto const* packed_info{document_->declaration(
                        pending_packed_mini_float_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateMiniFloat{.declaration = id,
                                    .module_index = new_mini_float_module_index_,
                                    .schema =
                                        codegen::MiniFloatSchema{
                                            .name = name,
                                            .sign_bits = new_mini_float_sign_bits_,
                                            .exponent_bits = new_mini_float_exponent_bits_,
                                            .significand_bits = new_mini_float_significand_bits_,
                                            .exponent_bias = new_mini_float_exponent_bias_},
                                    .insertion_index = std::nullopt},
                    selection) &&
                bind_new_mini_float_to_packed_field(identity)) {
                new_mini_float_name_.fill('\0');
                new_mini_float_sign_bits_ = 1;
                new_mini_float_exponent_bits_ = 5;
                new_mini_float_significand_bits_ = 10;
                new_mini_float_exponent_bias_ = 15;
                pending_packed_mini_float_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_mini_float_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_optional_sentinel_dialog() {
    if (open_new_optional_sentinel_dialog_) {
        ImGui::OpenPopup("New sentinel-encoded optional");
        open_new_optional_sentinel_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New sentinel-encoded optional", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }

    std::vector<TypeId> sources;
    auto const types{workspace_.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        auto const* scalar{std::get_if<IntegerScalarType>(&types[index].definition)};
        if (scalar != nullptr &&
            std::ranges::any_of(scalar->named_codes, &PackedNamedCode::sentinel)) {
            sources.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else if (sources.empty()) {
        ImGui::TextDisabled(
            "No integer scalar has a named sentinel code. Add one before creating this encoding.");
    } else {
        if (new_optional_sentinel_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_optional_sentinel_module_index_])) {
            new_optional_sentinel_module_index_ = *first_module;
        }
        auto const& selected_module{std::get<codegen::RepresentationModuleSchema>(
            modules[new_optional_sentinel_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_optional_sentinel_module_index_)) {
                    new_optional_sentinel_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_sentinel_name_.data(), new_optional_sentinel_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return workspace_.types().type(type).cpp_spelling ==
                   new_optional_sentinel_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{workspace_.types().type(*selected_source)};
            std::snprintf(new_optional_sentinel_source_.data(),
                          new_optional_sentinel_source_.size(),
                          "%s",
                          node.cpp_spelling.c_str());
            auto const& scalar{std::get<IntegerScalarType>(node.definition)};
            auto const sentinel{std::ranges::find_if(
                scalar.named_codes, [](auto const& code) { return code.sentinel; })};
            std::snprintf(new_optional_sentinel_code_.data(),
                          new_optional_sentinel_code_.size(),
                          "%s",
                          sentinel->name.c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              workspace_.types().type(selected_source_type).cpp_spelling.c_str())) {
            for (auto const type : sources) {
                auto const& candidate{workspace_.types().type(type)};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_sentinel_source_.data(),
                                  new_optional_sentinel_source_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                    auto const& scalar{std::get<IntegerScalarType>(candidate.definition)};
                    auto const sentinel{std::ranges::find_if(
                        scalar.named_codes, [](auto const& code) { return code.sentinel; })};
                    std::snprintf(new_optional_sentinel_code_.data(),
                                  new_optional_sentinel_code_.size(),
                                  "%s",
                                  sentinel->name.c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const& source_node{workspace_.types().type(selected_source_type)};
        auto const& source_scalar{std::get<IntegerScalarType>(source_node.definition)};
        auto selected_code{std::ranges::find_if(source_scalar.named_codes, [&](auto const& code) {
            return code.sentinel && code.name == new_optional_sentinel_code_.data();
        })};
        if (selected_code == source_scalar.named_codes.end()) {
            selected_code = std::ranges::find_if(source_scalar.named_codes,
                                                 [](auto const& code) { return code.sentinel; });
            std::snprintf(new_optional_sentinel_code_.data(),
                          new_optional_sentinel_code_.size(),
                          "%s",
                          selected_code->name.c_str());
        }
        if (ImGui::BeginCombo("Absence sentinel", selected_code->name.c_str())) {
            for (auto const& code : source_scalar.named_codes) {
                if (!code.sentinel) {
                    continue;
                }
                auto const selected{code.name == selected_code->name};
                if (ImGui::Selectable(code.name.c_str(), selected)) {
                    std::snprintf(new_optional_sentinel_code_.data(),
                                  new_optional_sentinel_code_.size(),
                                  "%s",
                                  code.name.c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "The source scalar owns the live domain and code values; this declaration selects one "
            "named sentinel as the absence state.");

        auto const ready{new_optional_sentinel_name_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_optional_sentinel_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateOptionalSentinel{
                        .declaration = id,
                        .module_index = new_optional_sentinel_module_index_,
                        .schema =
                            codegen::OptionalSentinelSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_optional_sentinel_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .sentinel = new_optional_sentinel_code_.data()},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_optional_sentinel_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_optional_presence_bit_dialog() {
    if (open_new_optional_presence_bit_dialog_) {
        ImGui::OpenPopup("New presence-bit optional");
        open_new_optional_presence_bit_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New presence-bit optional", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RepresentationModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }

    std::vector<TypeId> sources;
    auto const types{workspace_.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        if (std::holds_alternative<IntegerScalarType>(types[index].definition)) {
            sources.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled(
            "The target has no representation module to receive a new declaration.");
    } else if (sources.empty()) {
        ImGui::TextDisabled(
            "No integer scalar is available. Add a semantic scalar before creating this encoding.");
    } else {
        if (new_optional_presence_bit_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RepresentationModuleSchema>(
                modules[new_optional_presence_bit_module_index_])) {
            new_optional_presence_bit_module_index_ = *first_module;
        }
        auto const& selected_module{std::get<codegen::RepresentationModuleSchema>(
            modules[new_optional_presence_bit_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{
                    std::get_if<codegen::RepresentationModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_optional_presence_bit_module_index_)) {
                    new_optional_presence_bit_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_presence_bit_name_.data(), new_optional_presence_bit_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return workspace_.types().type(type).cpp_spelling ==
                   new_optional_presence_bit_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{workspace_.types().type(*selected_source)};
            std::snprintf(new_optional_presence_bit_source_.data(),
                          new_optional_presence_bit_source_.size(),
                          "%s",
                          node.cpp_spelling.c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              workspace_.types().type(selected_source_type).cpp_spelling.c_str())) {
            for (auto const type : sources) {
                auto const& candidate{workspace_.types().type(type)};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_presence_bit_source_.data(),
                                  new_optional_presence_bit_source_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const& source_scalar{
            std::get<IntegerScalarType>(workspace_.types().type(selected_source_type).definition)};
        ImGui::Text("Encoding: 1 presence bit + %u payload bits = %u bits/value",
                    source_scalar.bit_width,
                    source_scalar.bit_width + 1);
        ImGui::TextDisabled(
            "Concrete byte packing and presence-bit ordering remain unspecified until placement.");

        auto const ready{new_optional_presence_bit_name_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_optional_presence_bit_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateOptionalPresenceBit{
                        .declaration = id,
                        .module_index = new_optional_presence_bit_module_index_,
                        .schema =
                            codegen::OptionalPresenceBitSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name =
                                                         new_optional_presence_bit_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt}},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_optional_presence_bit_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_record_dialog() {
    if (open_new_record_dialog_) {
        ImGui::OpenPopup("New record");
        open_new_record_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New record", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_soa_record_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared record declaration and bind SoA column '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            pending_soa_record_binding_->column_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_record_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::RecordModuleSchema>(modules[index])) {
            first_record_module = index;
            break;
        }
    }
    if (!first_record_module.has_value()) {
        ImGui::TextDisabled("The target has no record module to receive a new declaration.");
    } else {
        if (new_record_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::RecordModuleSchema>(
                modules[new_record_module_index_])) {
            new_record_module_index_ = *first_record_module;
        }
        auto const& selected_module{
            std::get<codegen::RecordModuleSchema>(modules[new_record_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::RecordModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_record_module_index_)) {
                    new_record_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_record_name_.data(), new_record_name_.size());
        ImGui::InputText(
            "First member type", new_record_member_type_.data(), new_record_member_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable scalar member.");

        auto const ready{new_record_name_.front() != '\0' &&
                         new_record_member_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        auto const create_label{pending_soa_record_binding_.has_value() ? "Create and use"
                                                                        : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{
                std::get<codegen::RecordModuleSchema>(modules[new_record_module_index_])};
            auto const name{std::string{new_record_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto schema{codegen::RecordSchema{
                .name = name,
                .members = {{.name = "value",
                             .type = codegen::TypeRef{.name = new_record_member_type_.data(),
                                                      .suffix = {},
                                                      .nested = std::nullopt},
                             .count = std::nullopt,
                             .relationship = std::nullopt}},
                .export_specifier = std::nullopt}};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_soa_record_binding_.has_value()) {
                if (auto const* soa_info{
                        document_->declaration(pending_soa_record_binding_->soa_declaration)};
                    soa_info != nullptr) {
                    selection = soa_info->identity;
                }
            }
            if (apply_document_edit(CreateRecord{.declaration = id,
                                                 .module_index = new_record_module_index_,
                                                 .schema = std::move(schema),
                                                 .insertion_index = std::nullopt},
                                    selection) &&
                bind_new_record_to_soa_column(identity)) {
                new_record_name_.fill('\0');
                std::snprintf(new_record_member_type_.data(),
                              new_record_member_type_.size(),
                              "%s",
                              "std::uint32_t");
                if (!pending_soa_record_binding_.has_value()) {
                    selected_field_ = "value";
                }
                pending_soa_record_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        pending_soa_record_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::bind_new_record_to_soa_column(TypeIdentity const& record) -> bool {
    if (!pending_soa_record_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_soa_record_binding_};
    auto const* soa_info{document_->declaration(binding.soa_declaration)};
    auto const* soa_schema{document_->soa_schema(binding.soa_declaration)};
    auto const soa_identity{soa_info == nullptr ? std::optional<TypeIdentity>{}
                                                : std::optional{soa_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(soa_identity);
            schema_edit_message_ = std::move(message) + " The new record was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ =
                std::move(message) +
                " The new record could not be rolled back: " + rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new record could not be rolled back because history did "
                                   "not change.";
        }
        return false;
    };

    if (soa_info == nullptr || soa_schema == nullptr) {
        return rollback_creation("The SoA declaration is no longer available.");
    }
    auto replacement{*soa_schema};
    auto const column{std::ranges::find(
        replacement.members, binding.column_name, &codegen::SoaMemberSchema::name)};
    if (column == replacement.members.end()) {
        return rollback_creation("The selected SoA column is no longer available.");
    }

    column->type = codegen::TypeRef{.name = record.namespace_name.empty()
                                              ? record.name
                                              : record.namespace_name + "::" + record.name,
                                    .suffix = {},
                                    .nested = std::nullopt};
    if (!apply_document_edit(
            ReplaceSoa{.declaration = binding.soa_declaration, .schema = std::move(replacement)},
            soa_info->identity)) {
        return rollback_creation("The record was valid, but binding the SoA column failed: " +
                                 schema_edit_message_);
    }

    selected_field_ = binding.column_name;
    return true;
}

void PlannerUi::draw_new_union_dialog() {
    if (open_new_union_dialog_) {
        ImGui::OpenPopup("New union");
        open_new_union_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New union", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::UnionModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no union module to receive a new declaration.");
    } else {
        if (new_union_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::UnionModuleSchema>(modules[new_union_module_index_])) {
            new_union_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::UnionModuleSchema>(modules[new_union_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::UnionModuleSchema>(&modules[index])};
                if (module != nullptr && ImGui::Selectable(module->settings.name.c_str(),
                                                           index == new_union_module_index_)) {
                    new_union_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_union_name_.data(), new_union_name_.size());
        ImGui::InputText("First alternative type",
                         new_union_alternative_type_.data(),
                         new_union_alternative_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable scalar alternative.");
        auto const ready{new_union_name_.front() != '\0' &&
                         new_union_alternative_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{
                std::get<codegen::UnionModuleSchema>(modules[new_union_module_index_])};
            auto const name{std::string{new_union_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto schema{codegen::UnionSchema{
                .name = name,
                .alternatives = {{.name = "value",
                                  .type =
                                      codegen::TypeRef{.name = new_union_alternative_type_.data(),
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                                  .count = std::nullopt}},
                .export_specifier = std::nullopt}};
            if (apply_document_edit(CreateUnion{.declaration = id,
                                                .module_index = new_union_module_index_,
                                                .schema = std::move(schema),
                                                .insertion_index = std::nullopt},
                                    identity)) {
                new_union_name_.fill('\0');
                std::snprintf(new_union_alternative_type_.data(),
                              new_union_alternative_type_.size(),
                              "%s",
                              "std::uint32_t");
                selected_field_ = "value";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_tagged_union_dialog() {
    if (open_new_tagged_union_dialog_) {
        ImGui::OpenPopup("New tagged union");
        open_new_tagged_union_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New tagged union", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::UnionModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    auto const enum_types{workspace_.types().types()};
    auto const first_enum{std::ranges::find_if(enum_types, [](auto const& node) {
        return std::holds_alternative<EnumType>(node.definition);
    })};
    if (new_tagged_union_discriminant_.front() == '\0' && first_enum != enum_types.end()) {
        std::snprintf(new_tagged_union_discriminant_.data(),
                      new_tagged_union_discriminant_.size(),
                      "%s",
                      first_enum->cpp_spelling.c_str());
    }
    auto const selected_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
        return std::holds_alternative<EnumType>(node.definition) &&
               node.cpp_spelling == new_tagged_union_discriminant_.data();
    })};
    if (new_tagged_union_tag_.front() == '\0' && selected_enum != enum_types.end()) {
        auto const& enumeration{std::get<EnumType>(selected_enum->definition)};
        auto const first_tag{std::ranges::find_if(enumeration.enumerators, [](auto const& value) {
            return !value.sentinel && !value.count_sentinel;
        })};
        if (first_tag != enumeration.enumerators.end()) {
            std::snprintf(new_tagged_union_tag_.data(),
                          new_tagged_union_tag_.size(),
                          "%s",
                          first_tag->name.c_str());
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no union module to receive a new declaration.");
    } else if (first_enum == enum_types.end()) {
        ImGui::TextDisabled("The target has no enum to use as a discriminant.");
    } else {
        if (new_tagged_union_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::UnionModuleSchema>(
                modules[new_tagged_union_module_index_])) {
            new_tagged_union_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::UnionModuleSchema>(modules[new_tagged_union_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::UnionModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_tagged_union_module_index_)) {
                    new_tagged_union_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_tagged_union_name_.data(), new_tagged_union_name_.size());
        if (ImGui::BeginCombo("Discriminant", new_tagged_union_discriminant_.data())) {
            for (auto const& candidate : enum_types) {
                if (!std::holds_alternative<EnumType>(candidate.definition)) {
                    continue;
                }
                auto const selected{candidate.cpp_spelling ==
                                    new_tagged_union_discriminant_.data()};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_tagged_union_discriminant_.data(),
                                  new_tagged_union_discriminant_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                    new_tagged_union_tag_.fill('\0');
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("First alternative type",
                         new_tagged_union_alternative_type_.data(),
                         new_tagged_union_alternative_type_.size());
        auto const current_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
            return std::holds_alternative<EnumType>(node.definition) &&
                   node.cpp_spelling == new_tagged_union_discriminant_.data();
        })};
        if (current_enum != enum_types.end()) {
            auto const& enumeration{std::get<EnumType>(current_enum->definition)};
            if (ImGui::BeginCombo("First alternative tag", new_tagged_union_tag_.data())) {
                for (auto const& value : enumeration.enumerators) {
                    if (value.sentinel || value.count_sentinel) {
                        continue;
                    }
                    if (ImGui::Selectable(value.name.c_str(),
                                          value.name == new_tagged_union_tag_.data())) {
                        std::snprintf(new_tagged_union_tag_.data(),
                                      new_tagged_union_tag_.size(),
                                      "%s",
                                      value.name.c_str());
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("The declaration starts with one editable payload alternative.");
        auto const ready{new_tagged_union_name_.front() != '\0' &&
                         new_tagged_union_discriminant_.front() != '\0' &&
                         new_tagged_union_alternative_type_.front() != '\0' &&
                         new_tagged_union_tag_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{
                std::get<codegen::UnionModuleSchema>(modules[new_tagged_union_module_index_])};
            auto const name{std::string{new_tagged_union_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            codegen::TaggedUnionSchema schema{};
            schema.name = name;
            schema.discriminant.name = new_tagged_union_discriminant_.data();
            codegen::TaggedUnionAlternativeSchema alternative{};
            alternative.name = "value";
            alternative.type.name = new_tagged_union_alternative_type_.data();
            alternative.tag = new_tagged_union_tag_.data();
            schema.alternatives.push_back(std::move(alternative));
            if (apply_document_edit(
                    CreateTaggedUnion{.declaration = id,
                                      .module_index = new_tagged_union_module_index_,
                                      .schema = std::move(schema),
                                      .insertion_index = std::nullopt},
                    identity)) {
                new_tagged_union_name_.fill('\0');
                std::snprintf(new_tagged_union_alternative_type_.data(),
                              new_tagged_union_alternative_type_.size(),
                              "%s",
                              "std::uint32_t");
                selected_field_ = "value";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_soa_dialog() {
    if (open_new_soa_dialog_) {
        ImGui::OpenPopup("New SoA");
        open_new_soa_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New SoA", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_soa_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        auto const* module{std::get_if<codegen::SoaModuleSchema>(&modules[index])};
        if (module != nullptr && module->backend == codegen::SoaBackend::standard_library) {
            first_soa_module = index;
            break;
        }
    }
    if (!first_soa_module.has_value()) {
        ImGui::TextDisabled("The target has no standard-library SoA module for a new declaration.");
    } else {
        auto const selected_valid{[&] {
            if (new_soa_module_index_ >= modules.size()) {
                return false;
            }
            auto const* module{
                std::get_if<codegen::SoaModuleSchema>(&modules[new_soa_module_index_])};
            return module != nullptr && module->backend == codegen::SoaBackend::standard_library;
        }()};
        if (!selected_valid) {
            new_soa_module_index_ = *first_soa_module;
        }
        auto const& selected_module{
            std::get<codegen::SoaModuleSchema>(modules[new_soa_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::SoaModuleSchema>(&modules[index])};
                if (module == nullptr || module->backend != codegen::SoaBackend::standard_library) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_soa_module_index_)) {
                    new_soa_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_soa_name_.data(), new_soa_name_.size());
        ImGui::InputText(
            "First column type", new_soa_member_type_.data(), new_soa_member_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable array column.");

        auto const ready{new_soa_name_.front() != '\0' && new_soa_member_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{std::get<codegen::SoaModuleSchema>(modules[new_soa_module_index_])};
            auto const name{std::string{new_soa_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            codegen::SoaSchema schema{};
            schema.name = name;
            schema.members = {codegen::SoaMemberSchema{
                .name = "values",
                .kind = codegen::SoaMemberKind::array,
                .type = codegen::TypeRef{.name = new_soa_member_type_.data(),
                                         .suffix = {},
                                         .nested = std::nullopt},
                .fixed_schema = std::nullopt,
                .nested_schema = std::nullopt,
                .mask_field = false,
                .mask_dimensions = {},
                .relationship = std::nullopt}};
            if (apply_document_edit(CreateSoa{.declaration = id,
                                              .module_index = new_soa_module_index_,
                                              .schema = std::move(schema),
                                              .insertion_index = std::nullopt},
                                    identity)) {
                new_soa_name_.fill('\0');
                std::snprintf(new_soa_member_type_.data(),
                              new_soa_member_type_.size(),
                              "%s",
                              "std::uint32_t");
                selected_field_ = "values";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

} // namespace ioj::layout_planner
