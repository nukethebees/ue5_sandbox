#include "planner_ui_properties_common.hpp"

#include <type_traits>

namespace ioj::layout_planner {

auto PlannerUi::draw_type_picker(std::string_view const module_name, TypeIdentity const& owner)
    -> std::optional<std::string> {
    if (ImGui::SmallButton("...")) {
        type_picker_filter_.fill('\0');
        ImGui::OpenPopup("semantic-type-picker");
    }
    ImGui::SetItemTooltip("Choose a semantic or physical type.");
    if (!ImGui::BeginPopup("semantic-type-picker")) {
        return std::nullopt;
    }

    ImGui::SetNextItemWidth(420.0F);
    ImGui::InputTextWithHint("##type-filter",
                             "Filter semantic or physical types",
                             type_picker_filter_.data(),
                             type_picker_filter_.size());
    auto lowercase{[](std::string text) {
        std::ranges::transform(text, text.begin(), [](unsigned char const character) {
            return static_cast<char>(std::tolower(character));
        });
        return text;
    }};
    auto const filter{lowercase(type_picker_filter_.data())};
    std::set<std::string, std::less<>> seen;
    std::optional<std::string> selected;
    auto draw_candidate = [&](std::string reference, std::string const& description) {
        if (!seen.insert(reference).second) {
            return;
        }
        auto const label{reference + "  [" + description + "]"};
        if (!filter.empty() && lowercase(label).find(filter) == std::string::npos) {
            return;
        }
        if (ImGui::Selectable(label.c_str())) {
            selected = std::move(reference);
            ImGui::CloseCurrentPopup();
        }
    };

    ImGui::BeginChild(
        "type-candidates", {420.0F, 260.0F}, true, ImGuiWindowFlags_HorizontalScrollbar);
    std::map<std::string, std::size_t, std::less<>> declaration_spelling_counts;
    for (auto const& declaration : document_->declarations()) {
        if (auto const type{analysis_session_.inputs.workspace.types().find(declaration.identity)};
            type.has_value()) {
            ++declaration_spelling_counts
                [analysis_session_.inputs.workspace.types().type(*type).cpp_spelling];
        }
    }

    if (detail::section("Declared semantic types")) {
        for (auto const& declaration : document_->declarations()) {
            if (declaration.identity == owner) {
                continue;
            }
            auto const type{analysis_session_.inputs.workspace.types().find(declaration.identity)};
            if (!type.has_value()) {
                continue;
            }
            auto const& node{analysis_session_.inputs.workspace.types().type(*type)};
            auto const same_module{declaration.identity.module_name == module_name};
            if (!same_module && declaration_spelling_counts[node.cpp_spelling] != 1) {
                auto const label{node.cpp_spelling + "  [ambiguous across declaration modules]"};
                if (filter.empty() || lowercase(label).find(filter) != std::string::npos) {
                    ImGui::TextDisabled("%s", label.c_str());
                }
                continue;
            }
            auto reference{same_module ? declaration.identity.name : node.cpp_spelling};
            draw_candidate(std::move(reference), "declared in " + declaration.identity.module_name);
        }
    }
    if (detail::section("Registered semantic types")) {
        auto const owner_type{analysis_session_.inputs.workspace.types().find(owner)};
        for (auto const& [name, cpp_type] : document_->manifest().types) {
            auto const type{analysis_session_.inputs.workspace.types().find_registered(name)};
            if (type.has_value() && type != owner_type) {
                draw_candidate("@" + name, cpp_type.spelling);
            }
        }
    }
    if (detail::section("Target physical types")) {
        for (auto const& [spelling, facts] : analysis_session_.primary_abi().types()) {
            static_cast<void>(facts);
            draw_candidate(spelling, "target ABI type");
        }
    }
    ImGui::EndChild();
    ImGui::EndPopup();
    return selected;
}

void PlannerUi::draw_properties_panel() {
    if (!properties_view_open_) {
        return;
    }
    auto const was_open{properties_view_open_};
    ImGui::Begin("Properties", &properties_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, properties_view_open_);
    if (!analysis_session_.inputs.selection.type.has_value()) {
        auto const declaration{analysis_session_.inputs.selection.declaration};
        auto const* info{document_.has_value() && declaration.has_value()
                             ? document_->declaration(*declaration)
                             : nullptr};
        if (info == nullptr) {
            ImGui::TextDisabled("No selection.");
            ImGui::End();
            return;
        }
        auto const& module{std::get<codegen::NormalModuleSchema>(
            document_->manifest().modules[info->module_index])};
        auto const& schema{module.declarations[info->declaration_index]};
        ImGui::Text("%s", info->identity.name.c_str());
        ImGui::TextDisabled("%s", info->identity.module_name.c_str());
        ImGui::TextDisabled("%s — view only",
                            std::string{codegen::declaration_head(schema)}.c_str());
        if (info->source.has_value() &&
            info->source->source_file_index < document_->source_files().size()) {
            auto const& path{document_->source_files()[info->source->source_file_index].path};
            ImGui::TextWrapped("Source: %s:%llu:%llu",
                               path.string().c_str(),
                               static_cast<unsigned long long>(info->source->line),
                               static_cast<unsigned long long>(info->source->column));
            if (ImGui::Button("View source")) {
                selected_source_view_path_ = path.lexically_normal();
                source_view_open_ = true;
                focus_source_view_ = true;
            }
        }
        std::visit(
            [&](auto const& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, codegen::HomogeneousLayoutSchema>) {
                    if (detail::section("Homogeneous layout declaration")) {
                        for (auto const& component : value.components) {
                            ImGui::BulletText("Component: %s", component.c_str());
                        }
                        for (auto const& input : value.input_members) {
                            ImGui::BulletText("Input member: %s", input.c_str());
                        }
                        for (auto const& type : value.value_types) {
                            ImGui::BulletText(
                                "Value type: %s%s", type.type.name.c_str(), type.suffix.c_str());
                        }
                    }
                } else if constexpr (std::is_same_v<Type, codegen::StaticTableSchema>) {
                    if (detail::section("Static table declaration")) {
                        for (auto const& row : value.rows) {
                            ImGui::BulletText("Row: %s", row.name.c_str());
                        }
                        for (auto const& column : value.columns) {
                            ImGui::BulletText(
                                "Column: %s (%s)", column.name.c_str(), column.type.name.c_str());
                        }
                        for (auto const& group : value.groups) {
                            ImGui::BulletText(
                                "Group: %s (%s)", group.name.c_str(), group.type.name.c_str());
                            for (auto const& column : group.columns) {
                                ImGui::TextDisabled("    %s", column.c_str());
                            }
                        }
                    }
                } else if constexpr (std::is_same_v<Type, codegen::FacadeSchema>) {
                    if (detail::section("Facade declaration")) {
                        ImGui::Text("Target type: %s", value.target_type.name.c_str());
                        ImGui::Text("Target member: %s", value.target_member_name.c_str());
                        for (auto const& method : value.methods) {
                            ImGui::BulletText("Method: %s → %s",
                                              method.name.c_str(),
                                              method.return_type.name.c_str());
                        }
                        for (auto const& dependency : value.validation_dependencies) {
                            ImGui::BulletText("Validation dependency: %s", dependency.c_str());
                        }
                    }
                }
            },
            schema);
        ImGui::TextDisabled("No semantic TypeGraph node or physical analysis is available.");
        ImGui::End();
        return;
    }

    auto const revision_before_draw{analysis_session_.inputs.workspace.revision()};
    auto const scroll_before_draw{ImGui::GetScrollY()};
    auto const content_bottom_before_draw{ImGui::GetScrollMaxY() + ImGui::GetWindowHeight()};
    auto end_panel = [&] {
        if (analysis_session_.inputs.workspace.revision() != revision_before_draw &&
            scroll_before_draw > 0.0F) {
            // Keep the old content extent for a frame when a graph rebuild ends drawing early.
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), content_bottom_before_draw));
            ImGui::Dummy({0.0F, 1.0F});
            ImGui::SetScrollY(scroll_before_draw);
        }
        ImGui::End();
    };

    auto const selected{*analysis_session_.inputs.selection.type};
    auto const& node{analysis_session_.inputs.workspace.types().type(selected)};
    auto const capabilities{declaration_capabilities(node)};
    ImGui::Text("%s", node.identity.name.c_str());
    ImGui::TextDisabled("%s", node.identity.module_name.c_str());
    ImGui::TextDisabled("%s", node.cpp_spelling.c_str());
    auto const selected_declaration{document_.has_value()
                                        ? document_->find_declaration(node.identity)
                                        : std::optional<DeclarationId>{}};
    if (capabilities.editable && selected_declaration.has_value() &&
        ImGui::Button("Duplicate declaration")) {
        if (duplicate_selected_declaration(node)) {
            end_panel();
            return;
        }
    }
    if (selected_declaration.has_value()) {
        auto const* declaration_info{document_->declaration(*selected_declaration)};
        auto const editable{document_->enum_schema(*selected_declaration) != nullptr ||
                            document_->packed_value_schema(*selected_declaration) != nullptr ||
                            document_->integer_scalar_schema(*selected_declaration) != nullptr ||
                            document_->linear_quantized_schema(*selected_declaration) != nullptr ||
                            document_->integer_varint_schema(*selected_declaration) != nullptr ||
                            document_->fixed_point_schema(*selected_declaration) != nullptr ||
                            document_->optional_sentinel_schema(*selected_declaration) != nullptr ||
                            document_->optional_presence_bit_schema(*selected_declaration) !=
                                nullptr ||
                            document_->mini_float_schema(*selected_declaration) != nullptr ||
                            document_->record_schema(*selected_declaration) != nullptr ||
                            document_->union_schema(*selected_declaration) != nullptr ||
                            document_->tagged_union_schema(*selected_declaration) != nullptr ||
                            document_->soa_schema(*selected_declaration) != nullptr};
        if (declaration_info != nullptr && editable && capabilities.editable) {
            auto const& modules{document_->manifest().modules};
            auto const& source_module{modules[declaration_info->module_index]};
            auto const& source_settings{std::visit(
                [](auto const& module) -> codegen::ModuleSettings const& {
                    return module.settings;
                },
                source_module)};
            std::vector<std::size_t> destinations;
            for (std::size_t index{}; index < modules.size(); ++index) {
                if (index == declaration_info->module_index ||
                    modules[index].index() != source_module.index()) {
                    continue;
                }
                destinations.push_back(index);
            }
            if (!destinations.empty()) {
                std::optional<std::size_t> requested_destination;
                ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x));
                if (ImGui::BeginCombo("Module", source_settings.name.c_str())) {
                    for (auto const index : destinations) {
                        auto const& settings{std::visit(
                            [](auto const& module) -> codegen::ModuleSettings const& {
                                return module.settings;
                            },
                            modules[index])};
                        if (ImGui::Selectable(settings.name.c_str())) {
                            requested_destination = index;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (requested_destination.has_value()) {
                    auto const& destination_settings{std::visit(
                        [](auto const& module) -> codegen::ModuleSettings const& {
                            return module.settings;
                        },
                        modules[*requested_destination])};
                    auto selection{node.identity};
                    selection.module_name = destination_settings.name;
                    if (apply_document_edit(MoveDeclaration{.declaration = *selected_declaration,
                                                            .module_index = *requested_destination,
                                                            .insertion_index = std::nullopt},
                                            selection)) {
                        end_panel();
                        return;
                    }
                }
                ImGui::TextDisabled(
                    "Moving repairs semantic references when the namespace changes.");
            }
        }
    }
    auto const rename_supported{capabilities.editable && selected_declaration.has_value() &&
                                (!std::holds_alternative<SoaType>(node.definition) ||
                                 document_->soa_schema(*selected_declaration) != nullptr)};
    if (rename_supported) {
        if (rename_editor_declaration_ != selected_declaration) {
            rename_editor_declaration_ = selected_declaration;
            std::snprintf(declaration_name_.data(),
                          declaration_name_.size(),
                          "%s",
                          node.identity.name.c_str());
        }
        ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x - 90.0F));
        auto const submitted{ImGui::InputText("##declaration-name",
                                              declaration_name_.data(),
                                              declaration_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        ImGui::SameLine();
        auto const rename_clicked{ImGui::Button("Rename")};
        auto const user_count{analysis_session_.inputs.workspace.types().users_of(selected).size()};
        ImGui::TextDisabled("Renaming repairs %llu direct semantic user%s.",
                            static_cast<unsigned long long>(user_count),
                            user_count == 1 ? "" : "s");
        if (submitted || rename_clicked) {
            if (declaration_name_.front() == '\0') {
                schema_edit_message_ = "Declaration name cannot be empty.";
            } else if (node.identity.name != declaration_name_.data()) {
                auto selection{node.identity};
                selection.name = declaration_name_.data();
                if (apply_document_edit(RenameDeclaration{.declaration = *selected_declaration,
                                                          .new_name = declaration_name_.data()},
                                        selection)) {
                    rename_editor_declaration_.reset();
                    end_panel();
                    return;
                }
            }
        }
    }
    if (capabilities.editable && selected_declaration.has_value()) {
        auto const* declaration_info{document_->declaration(*selected_declaration)};
        if (declaration_info != nullptr) {
            auto const user_count{
                analysis_session_.inputs.workspace.types().users_of(selected).size()};
            ImGui::BeginDisabled(user_count != 0);
            if (ImGui::Button("Delete declaration")) {
                delete_declaration_ = *selected_declaration;
                delete_declaration_name_ = node.identity.name;
                ImGui::OpenPopup("Delete declaration?");
            }
            ImGui::EndDisabled();
            if (user_count != 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("Remove %llu direct user%s first.",
                                    static_cast<unsigned long long>(user_count),
                                    user_count == 1 ? "" : "s");
            }
        }
    }
    if (ImGui::BeginPopupModal("Delete declaration?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' from this schema?", delete_declaration_name_.c_str());
        ImGui::TextDisabled("This is a semantic edit; File > Undo restores the declaration.");
        if (ImGui::Button("Delete", {120.0F, 0.0F})) {
            if (delete_declaration_.has_value() && delete_declaration(*delete_declaration_)) {
                delete_declaration_.reset();
                delete_declaration_name_.clear();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                end_panel();
                return;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120.0F, 0.0F})) {
            delete_declaration_.reset();
            delete_declaration_name_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && !capabilities.physical_analysis_available) {
        if (detail::section("Semantic SoA")) {
            ImGui::Text("Backend: %s",
                        soa->backend == codegen::SoaBackend::unreal ? "Unreal"
                                                                    : "standard library");
            ImGui::Text("Source kind: %s",
                        soa->source_kind == SoaSourceKind::vector ? "vector-soa" : "struct");
            if (soa->equivalent_type.has_value()) {
                ImGui::Text("Equivalent type: %s", soa->equivalent_type->cpp_type.spelling.c_str());
            }
            if (soa->related_storage_name.has_value()) {
                ImGui::Text("Related storage: %s", soa->related_storage_name->c_str());
            }
            for (auto const& component : soa->vector_components) {
                ImGui::BulletText("Component: %s", component.c_str());
            }
            for (auto const& column : soa->columns) {
                auto const* kind{column.kind == codegen::SoaMemberKind::nested ? "nested SoA"
                                                                               : "array"};
                ImGui::BulletText("%s [%s]: %s",
                                  column.name.c_str(),
                                  kind,
                                  column.semantic_type.cpp_type.spelling.c_str());
                if (column.nested_type.has_value()) {
                    auto const& nested{
                        analysis_session_.inputs.workspace.types().type(*column.nested_type)};
                    ImGui::SameLine();
                    ImGui::PushID(column.name.c_str());
                    if (ImGui::SmallButton(nested.cpp_spelling.c_str())) {
                        select_type(*column.nested_type);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TextWrapped(
                "Physical layout analysis is not available for the Unreal SoA backend.");
        }
    } else if (auto const* enumeration{std::get_if<EnumType>(&node.definition)}) {
        if (detail::section("Enum")) {
            if (enumeration->underlying_type.has_value()) {
                auto const& underlying{analysis_session_.inputs.workspace.types().type(
                    enumeration->underlying_type->type)};
                ImGui::TextUnformatted("Explicit C++ backing");
                ImGui::SameLine();
                if (ImGui::SmallButton(underlying.cpp_spelling.c_str())) {
                    select_type(enumeration->underlying_type->type);
                    analysis_session_.inputs.selection.field.clear();
                    analysis_session_.inputs.selection.record_access_members.clear();
                    analysis_session_.inputs.selection.record_access_set_explicit = false;
                }
            } else if (analysis_session_.results().enum_domain.has_value()) {
                ImGui::Text("Derived C++ backing: %s",
                            analysis_session_.results().enum_domain->backing_type.c_str());
            } else {
                ImGui::TextDisabled("Derived C++ backing: Unknown");
            }
            if (enumeration->count.has_value()) {
                ImGui::Text("Count sentinel: %s", enumeration->count->c_str());
            }
            if (analysis_session_.results().enum_domain.has_value()) {
                auto const& domain{*analysis_session_.results().enum_domain};
                if (detail::section("Value domain")) {
                    ImGui::Text("Live symbols: %llu",
                                static_cast<unsigned long long>(domain.live_value_count));
                    ImGui::Text("Reserved/sentinel symbols: %llu",
                                static_cast<unsigned long long>(domain.reserved_value_count));
                    if (domain.minimum_value.has_value() && domain.maximum_value.has_value()) {
                        auto const minimum{lispb::schema::format_enum_code(*domain.minimum_value)};
                        auto const maximum{lispb::schema::format_enum_code(*domain.maximum_value)};
                        ImGui::Text("Known range: %s .. %s", minimum.c_str(), maximum.c_str());
                    } else {
                        ImGui::TextDisabled("Known range: Unknown");
                    }
                    if (domain.signed_domain.has_value()) {
                        ImGui::Text("Effective domain: %s",
                                    *domain.signed_domain ? "signed" : "unsigned");
                    } else {
                        ImGui::TextDisabled("Effective domain: Unknown");
                    }
                    if (domain.declared_signedness.has_value()) {
                        ImGui::Text("Declared signedness: %s",
                                    *domain.declared_signedness ? "signed" : "unsigned");
                    } else {
                        ImGui::TextUnformatted("Declared signedness: auto / inferred");
                    }
                    if (domain.minimum_required_bits.has_value()) {
                        ImGui::Text("Minimum semantic width: %u bits",
                                    *domain.minimum_required_bits);
                    } else {
                        ImGui::TextDisabled("Minimum semantic width: Unknown");
                    }
                    if (domain.declared_bit_width.has_value()) {
                        ImGui::Text("Declared semantic width: %u bits", *domain.declared_bit_width);
                    } else {
                        ImGui::TextUnformatted("Declared semantic width: auto");
                    }
                    if (domain.effective_bit_width.has_value()) {
                        ImGui::Text("Effective semantic width: %u bits",
                                    *domain.effective_bit_width);
                    } else {
                        ImGui::TextDisabled("Effective semantic width: Unknown");
                    }
                    if (domain.semantic_width_can_represent_domain.has_value()) {
                        ImGui::Text("Semantic width fit: %s",
                                    *domain.semantic_width_can_represent_domain ? "yes" : "NO");
                    } else {
                        ImGui::TextDisabled("Semantic width fit: Unknown");
                    }
                    if (domain.unused_semantic_codes.has_value()) {
                        ImGui::Text("Unused semantic codes: %llu",
                                    static_cast<unsigned long long>(*domain.unused_semantic_codes));
                    } else {
                        ImGui::TextDisabled("Unused semantic codes: Unknown");
                    }
                    if (domain.backing_bits.has_value()) {
                        ImGui::Text("Current physical backing: %s (%llu bits)",
                                    domain.backing_type.c_str(),
                                    static_cast<unsigned long long>(*domain.backing_bits));
                    } else {
                        ImGui::Text("Current physical backing: %s (Unknown size)",
                                    domain.backing_type.c_str());
                    }
                    if (domain.backing_can_represent_domain.has_value()) {
                        ImGui::Text("Backing fit: %s",
                                    *domain.backing_can_represent_domain ? "yes" : "NO");
                    } else {
                        ImGui::TextDisabled("Backing fit: Unknown");
                    }
                    if (domain.unused_backing_codes.has_value()) {
                        ImGui::Text("Unused backing codes: %llu",
                                    static_cast<unsigned long long>(*domain.unused_backing_codes));
                        ImGui::TextDisabled("Code-space inefficiency; not allocated byte waste.");
                    } else {
                        ImGui::TextDisabled("Unused backing codes: Unknown");
                    }
                    draw_diagnostics(domain.diagnostics);
                }
            }
            auto const revision_before_edit{analysis_session_.inputs.workspace.revision()};
            draw_enum_editor(node, *enumeration);
            if (analysis_session_.inputs.workspace.revision() != revision_before_edit) {
                end_panel();
                return;
            }
        }
    } else if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
        if (section_with_tooltip("Semantic integer domain",
                                 "The allowed values and named codes of this integer type, "
                                 "independent of storage.")) {
            if (analysis_session_.results().integer_scalar_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().integer_scalar_analysis};
                auto const minimum{codegen::format_packed_integer(analysis.minimum_value)};
                auto const maximum{codegen::format_packed_integer(analysis.maximum_value)};
                ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
                ImGui::Text("Live range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Live values: %s",
                            detail::format_number(analysis.live_value_count).c_str());
                ImGui::Text("Sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.sentinel_code_count));
                ImGui::Text("Required codes: %s",
                            detail::format_number(analysis.required_code_count).c_str());
                ImGui::Text("Minimum width: %u bits", analysis.minimum_required_bits);
                if (analysis.declared_bit_width.has_value()) {
                    ImGui::Text("Declared width: %u bits", *analysis.declared_bit_width);
                } else {
                    ImGui::TextUnformatted("Declared width: auto");
                }
                ImGui::Text("Effective width: %u bits", analysis.effective_bit_width);
                ImGui::Text("Unused codes: %s",
                            detail::format_number(analysis.unused_codes).c_str());
                ImGui::TextDisabled("Code-space inefficiency; this semantic declaration allocates "
                                    "no bytes itself.");
            }
            if (draw_integer_scalar_editor(node, *scalar)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* quantized{std::get_if<LinearQuantizedType>(&node.definition)}) {
        if (detail::section("Linear quantized representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types().type(quantized->source.type)};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
                select_type(quantized->source.type);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().linear_quantized_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().linear_quantized_analysis};
                auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
                ImGui::Text("Source range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Source span: %s", detail::format_number(analysis.source_span).c_str());
                ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
                ImGui::Text("Total codes: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Reserved codes: %llu",
                            static_cast<unsigned long long>(analysis.reserved_code_count));
                ImGui::Text("Usable codes: %s",
                            detail::format_code_count(analysis.usable_code_count).c_str());
                ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
                ImGui::Text("Maximum rounding error: %.12g",
                            static_cast<double>(analysis.maximum_rounding_error));
                ImGui::Text("Endpoint mapping: minimum %s, maximum %s",
                            analysis.minimum_endpoint_exact ? "exact" : "inexact",
                            analysis.maximum_endpoint_exact ? "exact" : "inexact");
                ImGui::Text("Out-of-range policy: %s",
                            codegen::quantization_clipping_name(analysis.clipping).data());
                ImGui::TextDisabled("Resolution/error are numeric encoding facts; no performance "
                                    "estimate is made.");
            }
            if (draw_linear_quantized_editor(node, *quantized)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* optional{std::get_if<OptionalSentinelType>(&node.definition)}) {
        if (detail::section("Sentinel-encoded optional representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types().type(optional->source.type)};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
                select_type(optional->source.type);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().optional_sentinel_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().optional_sentinel_analysis};
                auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
                auto const sentinel{codegen::format_packed_integer(analysis.sentinel_value)};
                ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Present values: %s",
                            detail::format_number(analysis.present_value_count).c_str());
                ImGui::Text(
                    "Absence sentinel: %s = %s", analysis.sentinel_name.c_str(), sentinel.c_str());
                ImGui::Text("Other named sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.other_sentinel_code_count));
                ImGui::Text("Unused codes: %s",
                            detail::format_number(analysis.unused_code_count).c_str());
                ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
                ImGui::Text("Total code space: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "Sentinel and unused codes are code-space roles, not allocated byte waste.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_optional_sentinel_editor(node, *optional)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* optional{std::get_if<OptionalPresenceBitType>(&node.definition)}) {
        if (detail::section("Presence-bit optional representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types().type(optional->source.type)};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
                select_type(optional->source.type);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().optional_presence_bit_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().optional_presence_bit_analysis};
                auto const minimum{codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{codegen::format_packed_integer(analysis.source_maximum)};
                ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Present values: %s",
                            detail::format_number(analysis.present_value_count).c_str());
                ImGui::Text("Bit allocation: %u presence + %u payload = %u bits/value",
                            analysis.presence_bits,
                            analysis.payload_bits,
                            analysis.encoded_storage_bits);
                ImGui::Text(
                    "Canonical absence states: %llu",
                    static_cast<unsigned long long>(analysis.canonical_absence_state_count));
                ImGui::Text("Source named sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.source_sentinel_code_count));
                ImGui::Text("Source unused payload codes: %s",
                            detail::format_number(analysis.source_unused_payload_codes).c_str());
                ImGui::Text("Ignored-payload absence patterns: %s noncanonical",
                            detail::format_number(analysis.noncanonical_absence_patterns).c_str());
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "The one canonical absence state is semantic; other absent payload patterns "
                    "are "
                    "redundant physical encodings, not extra states or byte padding.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_optional_presence_bit_editor(node, *optional)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* fixed{std::get_if<FixedPointType>(&node.definition)}) {
        if (detail::section("Fixed-point representation")) {
            if (analysis_session_.results().fixed_point_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().fixed_point_analysis};
                auto const minimum_raw{codegen::format_packed_integer(analysis.minimum_raw_value)};
                auto const maximum_raw{codegen::format_packed_integer(analysis.maximum_raw_value)};
                ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
                ImGui::Text("Bit allocation: %u whole + %u fractional%s",
                            analysis.whole_bits,
                            analysis.fractional_bits,
                            analysis.signedness ? " + 1 sign" : "");
                ImGui::Text("Total encoded width: %u bits", analysis.total_bits);
                ImGui::Text("Raw range: %s .. %s", minimum_raw.c_str(), maximum_raw.c_str());
                ImGui::Text("Scale: %.12g raw codes / unit", static_cast<double>(analysis.scale));
                ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
                ImGui::Text("Representable range: %.12g .. %.12g",
                            static_cast<double>(analysis.minimum_value),
                            static_cast<double>(analysis.maximum_value));
                ImGui::Text("Allowed range: %.12g .. %.12g",
                            static_cast<double>(analysis.minimum_allowed_value),
                            static_cast<double>(analysis.maximum_allowed_value));
                ImGui::Text("Rounding: %s",
                            codegen::fixed_point_rounding_name(analysis.rounding).data());
                ImGui::Text("Maximum rounding error: %.12g",
                            static_cast<double>(analysis.maximum_rounding_error));
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled("Payload bits are not an allocated byte size until "
                                    "placement/lowering is chosen.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_fixed_point_editor(node, *fixed)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* mini_float{std::get_if<MiniFloatType>(&node.definition)}) {
        if (detail::section("Mini-float representation")) {
            if (analysis_session_.results().mini_float_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().mini_float_analysis};
                ImGui::Text("Bit allocation: %u sign + %u exponent + %u significand = %u bits",
                            analysis.sign_bits,
                            analysis.exponent_bits,
                            analysis.significand_bits,
                            analysis.total_bits);
                ImGui::Text("Exponent bias: %d", analysis.exponent_bias);
                ImGui::Text("Normal exponent range: %d .. %d",
                            analysis.minimum_normal_exponent,
                            analysis.maximum_normal_exponent);
                ImGui::Text("Total code space: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Code roles: %llu zero, %llu infinity, %llu NaN, %llu subnormal",
                            static_cast<unsigned long long>(analysis.zero_code_count),
                            static_cast<unsigned long long>(analysis.infinity_code_count),
                            static_cast<unsigned long long>(analysis.nan_code_count),
                            static_cast<unsigned long long>(analysis.nonzero_subnormal_code_count));
                auto draw_numeric = [](char const* label, std::optional<long double> const value) {
                    if (value.has_value()) {
                        ImGui::Text("%s: %.12g", label, static_cast<double>(*value));
                    } else {
                        ImGui::TextDisabled("%s: Unknown", label);
                    }
                };
                draw_numeric("Minimum positive subnormal", analysis.minimum_positive_subnormal);
                draw_numeric("Minimum positive normal", analysis.minimum_positive_normal);
                draw_numeric("Minimum finite", analysis.minimum_finite);
                draw_numeric("Maximum finite", analysis.maximum_finite);
                draw_numeric("Resolution around 1.0", analysis.unit_interval_resolution);
                ImGui::Text("Maximum relative rounding error: %.12g",
                            static_cast<double>(analysis.maximum_relative_rounding_error));
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "IEEE-style code roles are encoding facts; arithmetic conformance, byte order, "
                    "ABI placement, and native compiler support are unspecified.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_mini_float_editor(node, *mini_float)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* varint{std::get_if<IntegerVarintType>(&node.definition)}) {
        auto const& source{analysis_session_.inputs.workspace.types().type(varint->source.type)};
        if (detail::section("Variable-length integer representation")) {
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(source.cpp_spelling.c_str())) {
                select_type(varint->source.type);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().integer_varint_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().integer_varint_analysis};
                ImGui::Text("Encoding: %s",
                            codegen::integer_varint_encoding_name(analysis.encoding).data());
                ImGui::Text("Encoded size: %u .. %u bytes/value",
                            analysis.minimum_encoded_bytes,
                            analysis.maximum_encoded_bytes);
                ImGui::Text("At %llu values: %s .. %s",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_bytes(analysis.minimum_total_bytes).c_str(),
                            detail::format_bytes(analysis.maximum_total_bytes).c_str());
                ImGui::TextDisabled(
                    "Expected encoded size is Unknown until a value distribution is supplied.");
                draw_diagnostics(analysis.diagnostics);
            }
        }
        if (detail::section("Session value distribution")) {
            auto& rows{varint_distributions_[source.identity]};
            auto remove_index{std::optional<std::size_t>{}};
            if (ImGui::BeginTable("varint-distribution",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Semantic value");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                for (std::size_t index{}; index < rows.size(); ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputText("##value", rows[index].value.data(), rows[index].value.size());
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputScalar("##weight",
                                       ImGuiDataType_U64,
                                       &rows[index].weight,
                                       nullptr,
                                       nullptr,
                                       "%llu");
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("Delete")) {
                        remove_index = index;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (remove_index.has_value()) {
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*remove_index));
            }

            ImGui::SetNextItemWidth(150.0F);
            ImGui::InputText("Value##new-varint-distribution",
                             new_varint_distribution_value_.data(),
                             new_varint_distribution_value_.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130.0F);
            ImGui::InputScalar("Weight##new-varint-distribution",
                               ImGuiDataType_U64,
                               &new_varint_distribution_weight_,
                               nullptr,
                               nullptr,
                               "%llu");
            ImGui::SameLine();
            auto const new_value{
                detail::parse_packed_integer(new_varint_distribution_value_.data())};
            ImGui::BeginDisabled(!new_value.has_value());
            if (ImGui::Button("Add##varint-distribution")) {
                VarintDistributionRow row{.value = {}, .weight = new_varint_distribution_weight_};
                std::snprintf(row.value.data(),
                              row.value.size(),
                              "%s",
                              new_varint_distribution_value_.data());
                rows.push_back(row);
            }
            ImGui::EndDisabled();

            auto invalid_literal_count{std::size_t{}};
            for (auto const& row : rows) {
                if (!detail::parse_packed_integer(row.value.data()).has_value()) {
                    ++invalid_literal_count;
                }
            }
            refresh_analysis();
            auto const& distribution{*analysis_session_.results().integer_varint_distribution};
            if (invalid_literal_count != 0) {
                ImGui::TextColored(detail::diagnostic_color(DiagnosticSeverity::error),
                                   "%llu distribution row%s contain an invalid integer literal.",
                                   static_cast<unsigned long long>(invalid_literal_count),
                                   invalid_literal_count == 1 ? "" : "s");
            }
            ImGui::Text("Exact sample weight: %s",
                        detail::format_number(distribution.total_weight).c_str());
            ImGui::Text("Exact sample bytes: %s",
                        detail::format_bytes(distribution.total_encoded_bytes).c_str());
            if (!distribution.entries.empty() &&
                ImGui::BeginTable("varint-distribution-breakdown",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("Bytes / value");
                ImGui::TableSetupColumn("Weighted bytes");
                ImGui::TableHeadersRow();
                for (auto const& entry : distribution.entries) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(codegen::format_packed_integer(entry.value).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", entry.encoded_bytes);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        detail::format_bytes(entry.weighted_encoded_bytes).c_str());
                }
                ImGui::EndTable();
            }
            if (distribution.expected_bytes_per_value.has_value()) {
                ImGui::Text("Expected bytes / value: %.8g",
                            static_cast<double>(*distribution.expected_bytes_per_value));
                ImGui::Text("Expected bytes at %llu values: %.8g",
                            static_cast<unsigned long long>(distribution.selected_element_count),
                            static_cast<double>(*distribution.expected_selected_bytes));
            } else {
                ImGui::TextDisabled("Expected bytes / value: Unknown");
            }
            ImGui::TextDisabled("Session workload only; weights are not written into the LispB "
                                "semantic declaration.");
            draw_diagnostics(distribution.diagnostics);
            if (draw_integer_varint_editor(node, *varint)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* record{std::get_if<RecordType>(&node.definition)}) {
        if (detail::section("LispB record declaration")) {
            if (draw_record_editor(node, *record)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* union_type{std::get_if<UnionType>(&node.definition)}) {
        if (detail::section("Raw union layout")) {
            auto const& analysis{*analysis_session_.results().union_analysis};
            ImGui::Text("Alternatives: %llu",
                        static_cast<unsigned long long>(union_type->alternatives.size()));
            ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
            ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
            ImGui::Text("Largest alternative: %s",
                        detail::format_bytes(analysis.largest_alternative_bytes).c_str());
            ImGui::Text("Tail padding: %s",
                        detail::format_bytes(analysis.tail_padding_bytes).c_str());
            if (selected_declaration.has_value()) {
                if (detail::section("Session alternative workload")) {
                    if (ImGui::BeginTable("raw-union-workload-weights",
                                          2,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Alternative");
                        ImGui::TableSetupColumn("Weight");
                        ImGui::TableHeadersRow();
                        for (auto const& alternative : union_type->alternatives) {
                            ImGui::PushID(alternative.name.c_str());
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(alternative.name.c_str());
                            ImGui::TableNextColumn();
                            auto weight{std::uint64_t{0}};
                            if (auto const* weights{
                                    analysis_session_.union_distribution(*selected_declaration)};
                                weights != nullptr) {
                                if (auto const found{weights->find(alternative.name)};
                                    found != weights->end()) {
                                    weight = found->second;
                                }
                            }
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                                analysis_session_.set_union_distribution_weight(
                                    *selected_declaration, alternative.name, weight);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (ImGui::SmallButton("Clear alternative workload")) {
                        analysis_session_.clear_union_distribution(*selected_declaration);
                    }
                    if (analysis_session_.results().union_distribution_analysis.has_value()) {
                        auto const& distribution{
                            *analysis_session_.results().union_distribution_analysis};
                        ImGui::Text("Sample weight: %s",
                                    detail::format_number(distribution.total_weight).c_str());
                        ImGui::Text("Sample active extent: %s",
                                    detail::format_bytes(distribution.total_extent_bytes).c_str());
                        ImGui::Text("Sample conditional slack: %s",
                                    detail::format_bytes(distribution.total_slack_bytes).c_str());
                        if (distribution.expected_extent_bytes_per_value.has_value()) {
                            ImGui::Text(
                                "Expected active extent / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_extent_bytes_per_value));
                            ImGui::Text(
                                "Expected conditional slack / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_slack_bytes_per_value));
                            ImGui::Text(
                                "Expected active extent at %llu values: %.8g bytes",
                                static_cast<unsigned long long>(
                                    distribution.selected_element_count),
                                static_cast<double>(*distribution.expected_selected_extent_bytes));
                            ImGui::Text(
                                "Expected conditional slack at %llu values: %.8g bytes",
                                static_cast<unsigned long long>(
                                    distribution.selected_element_count),
                                static_cast<double>(*distribution.expected_selected_slack_bytes));
                        } else {
                            ImGui::TextDisabled("Expected alternative usage: Unknown");
                        }
                        if (ImGui::BeginTable("raw-union-workload-analysis",
                                              6,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Alternative");
                            ImGui::TableSetupColumn("Weight");
                            ImGui::TableSetupColumn("Extent");
                            ImGui::TableSetupColumn("Slack");
                            ImGui::TableSetupColumn("Weighted extent");
                            ImGui::TableSetupColumn("Weighted slack");
                            ImGui::TableHeadersRow();
                            for (auto const& entry : distribution.entries) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(entry.alternative_name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.slack_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_slack_bytes).c_str());
                            }
                            ImGui::EndTable();
                        }
                        draw_diagnostics(distribution.diagnostics);
                    }
                    ImGui::TextDisabled("Session-only conditional workload; weights are not LispB "
                                        "semantics and do not "
                                        "imply a stored discriminant or a performance result.");
                }
            }
            ImGui::TextDisabled(
                "A raw union has no stored discriminant; tagged unions are separate.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (detail::section("LispB union declaration")) {
            if (draw_union_editor(node, *union_type)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&node.definition)}) {
        if (detail::section("Tagged union semantics")) {
            auto const& analysis{*analysis_session_.results().tagged_union_analysis};
            auto const& discriminant{
                analysis_session_.inputs.workspace.types().type(tagged->discriminant.type)};
            ImGui::Text("Discriminant: %s", discriminant.identity.name.c_str());
            ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
            ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
            ImGui::Text("Payload offset: %s",
                        detail::format_bytes(analysis.payload_offset_bytes).c_str());
            ImGui::Text("Padding: %s",
                        detail::format_bytes(analysis.internal_padding_bytes).c_str());
            ImGui::Text("Tail padding: %s",
                        detail::format_bytes(analysis.tail_padding_bytes).c_str());
            ImGui::Text("Tag coverage: %llu mapped, %llu unmapped, %llu named sentinel%s",
                        static_cast<unsigned long long>(analysis.mapped_live_tags.size()),
                        static_cast<unsigned long long>(analysis.unmapped_live_tags.size()),
                        static_cast<unsigned long long>(analysis.sentinel_tags.size()),
                        analysis.count_sentinel_tag.has_value() ? ", count sentinel present" : "");
            if (ImGui::SmallButton("Go to discriminant")) {
                select_type(tagged->discriminant.type);
                analysis_session_.inputs.selection.field.clear();
                end_panel();
                return;
            }
            if (ImGui::BeginTable("tagged-union-properties",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Tag");
                ImGui::TableSetupColumn("Alternative");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Count");
                ImGui::TableHeadersRow();
                for (auto const& alternative : tagged->alternatives) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alternative.tag.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alternative.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(analysis_session_.inputs.workspace.types()
                                               .type(alternative.semantic_type.type)
                                               .cpp_spelling.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu",
                                static_cast<unsigned long long>(alternative.count.value_or(1)));
                }
                ImGui::EndTable();
            }
            if (selected_declaration.has_value()) {
                if (detail::section("Session tag workload")) {
                    if (ImGui::BeginTable("tagged-union-workload-weights",
                                          3,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Tag");
                        ImGui::TableSetupColumn("Alternative");
                        ImGui::TableSetupColumn("Weight");
                        ImGui::TableHeadersRow();
                        for (auto const& alternative : tagged->alternatives) {
                            ImGui::PushID(alternative.tag.c_str());
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(alternative.tag.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(alternative.name.c_str());
                            ImGui::TableNextColumn();
                            auto weight{std::uint64_t{0}};
                            if (auto const* weights{analysis_session_.tagged_union_distribution(
                                    *selected_declaration)};
                                weights != nullptr) {
                                if (auto const found{weights->find(alternative.tag)};
                                    found != weights->end()) {
                                    weight = found->second;
                                }
                            }
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                                analysis_session_.set_tagged_union_distribution_weight(
                                    *selected_declaration, alternative.tag, weight);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (ImGui::SmallButton("Clear tag workload")) {
                        analysis_session_.clear_tagged_union_distribution(*selected_declaration);
                    }
                    if (analysis_session_.results()
                            .tagged_union_distribution_analysis.has_value()) {
                        auto const& distribution{
                            *analysis_session_.results().tagged_union_distribution_analysis};
                        ImGui::Text("Sample weight: %s",
                                    detail::format_number(distribution.total_weight).c_str());
                        ImGui::Text(
                            "Sample active payload extent: %s",
                            detail::format_bytes(distribution.total_payload_extent_bytes).c_str());
                        ImGui::Text(
                            "Sample payload slack: %s",
                            detail::format_bytes(distribution.total_payload_slack_bytes).c_str());
                        if (distribution.expected_payload_extent_bytes_per_value.has_value()) {
                            ImGui::Text("Expected active payload / value: %.8g bytes",
                                        static_cast<double>(
                                            *distribution.expected_payload_extent_bytes_per_value));
                            ImGui::Text("Expected payload slack / value: %.8g bytes",
                                        static_cast<double>(
                                            *distribution.expected_payload_slack_bytes_per_value));
                            ImGui::Text("Expected active payload at %llu values: %.8g bytes",
                                        static_cast<unsigned long long>(
                                            distribution.selected_element_count),
                                        static_cast<double>(
                                            *distribution.expected_selected_payload_extent_bytes));
                            ImGui::Text("Expected payload slack at %llu values: %.8g bytes",
                                        static_cast<unsigned long long>(
                                            distribution.selected_element_count),
                                        static_cast<double>(
                                            *distribution.expected_selected_payload_slack_bytes));
                        }
                        if (ImGui::BeginTable("tagged-union-workload-analysis",
                                              6,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Tag");
                            ImGui::TableSetupColumn("Weight");
                            ImGui::TableSetupColumn("Extent");
                            ImGui::TableSetupColumn("Slack");
                            ImGui::TableSetupColumn("Weighted extent");
                            ImGui::TableSetupColumn("Weighted slack");
                            ImGui::TableHeadersRow();
                            for (auto const& entry : distribution.entries) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(entry.tag.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.payload_extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.payload_slack_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_payload_extent_bytes)
                                        .c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_payload_slack_bytes)
                                        .c_str());
                            }
                            ImGui::EndTable();
                        }
                        draw_diagnostics(distribution.diagnostics);
                    } else {
                        ImGui::TextDisabled(
                            "Assign a positive weight to calculate workload expectations.");
                    }
                    ImGui::TextDisabled(
                        "Session workload only; weights are not written into LispB.");
                }
            }
            draw_diagnostics(analysis.diagnostics);
        }
        if (detail::section("LispB tagged-union declaration")) {
            if (draw_tagged_union_editor(node, *tagged)) {
                end_panel();
                return;
            }
        }
    } else if (auto const* external{std::get_if<ExternalType>(&node.definition)}) {
        if (detail::section("External type")) {
            ImGui::Text("C++ spelling: %s", external->cpp_type.spelling.c_str());
            if (!external->registered_names.empty()) {
                ImGui::TextUnformatted("Registered as");
                for (auto const& name : external->registered_names) {
                    ImGui::BulletText("@%s", name.c_str());
                }
            }
            ImGui::TextDisabled("Internal structure is not declared in LispB.");
        }
    } else {
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            if (detail::section("LispB packed declaration")) {
                if (draw_packed_editor(node, *packed)) {
                    end_panel();
                    return;
                }
            }
        } else if (auto const* soa{std::get_if<SoaType>(&node.definition)}) {
            if (detail::section("LispB SoA declaration")) {
                if (draw_soa_editor(node, *soa)) {
                    end_panel();
                    return;
                }
            }
        }

        auto const editable{analysis_session_.inputs.workspace.active_variant_id() !=
                            LayoutWorkspace::baseline_variant_id};
        if (!editable) {
            if (detail::section("Baseline")) {
                if (capabilities.supports_variant_overrides) {
                    ImGui::TextDisabled("Planning overrides are read only on the baseline.");
                    ImGui::TextWrapped(
                        "Edit the LispB declaration above, or create an experiment for "
                        "session-only physical overrides.");
                } else {
                    ImGui::TextDisabled("Loaded from LispB — read only.");
                    ImGui::TextWrapped(
                        "Experiments are session-only variants. The production schema "
                        "is never modified.");
                }
                ImGui::TextDisabled(
                    "Use + Add variant in the Layout panel to create an experiment.");
                if (std::holds_alternative<SoaType>(node.definition)) {
                    ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                        static_cast<unsigned long long>(
                                            analysis_session_.inputs.workspace.default_capacity()));
                }
            }
        } else {
            if (detail::section("Experiment")) {
                ImGui::Text("Editing %s",
                            analysis_session_.inputs.workspace.active_variant().name.c_str());
            }
        }

        ImGui::BeginDisabled(!editable);
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            auto const& analysis{*analysis_session_.results().active_packed};
            if (detail::section("Packed storage")) {
                ImGui::Text("Schema storage: %s",
                            analysis_session_.results().active_packed->schema_storage_type.c_str());
                if (ImGui::BeginCombo("Planning storage", analysis.storage_type.c_str())) {
                    if (ImGui::Selectable("Schema storage", !analysis.storage_overridden)) {
                        analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                   std::nullopt);
                    }
                    for (auto const& [type, facts] : analysis_session_.primary_abi().types()) {
                        if (facts.unsigned_value_bits.has_value() &&
                            ImGui::Selectable(type.c_str(), analysis.storage_type == type)) {
                            analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                       type);
                        }
                    }
                    ImGui::EndCombo();
                }
                draw_override_note(analysis.storage_overridden);
                if (analysis.storage_overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset storage")) {
                        analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                   std::nullopt);
                    }
                }

                if (analysis_session_.inputs.selection.field.empty() && !packed->segments.empty()) {
                    analysis_session_.inputs.selection.field = std::visit(
                        [](auto const& segment) -> std::string const& { return segment.name; },
                        packed->segments.front());
                }
            }
            if (detail::section("Fields")) {
                if (detail::begin_editable_table("packed-fields", 4, analysis.fields.size())) {
                    detail::editable_table_column("Field");
                    detail::editable_table_column("Semantic type");
                    detail::editable_table_column("Schema");
                    detail::editable_table_column("Planning");
                    ImGui::TableHeadersRow();
                    for (auto const& field : analysis.fields) {
                        ImGui::PushID(field.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(field.name.c_str(),
                                              analysis_session_.inputs.selection.field ==
                                                  field.name)) {
                            analysis_session_.inputs.selection.field = field.name;
                        }
                        ImGui::TableNextColumn();
                        if (field.semantic_type.has_value()) {
                            auto const& semantic_type{
                                analysis_session_.inputs.workspace.types().type(
                                    *field.semantic_type)};
                            if (ImGui::SmallButton(semantic_type.cpp_spelling.c_str())) {
                                select_type(*field.semantic_type);
                                analysis_session_.inputs.selection.field.clear();
                                analysis_session_.inputs.selection.record_access_members.clear();
                                analysis_session_.inputs.selection.record_access_set_explicit =
                                    false;
                            }
                        } else {
                            ImGui::TextDisabled("Reserved");
                        }
                        ImGui::TableNextColumn();
                        if (field.schema_bit_width_auto) {
                            ImGui::Text("auto => %u", field.schema_bit_width);
                        } else {
                            ImGui::Text("%u bits", field.schema_bit_width);
                        }
                        ImGui::TableNextColumn();
                        if (field.reserved) {
                            ImGui::TextDisabled("Durable");
                        } else {
                            auto width{field.bit_width};
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##planning-width", ImGuiDataType_U32, &width)) {
                                width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                                analysis_session_.inputs.workspace.set_packed_field_width(
                                    selected, field.name, width);
                            }
                            if (field.overridden && ImGui::SmallButton("Reset")) {
                                analysis_session_.inputs.workspace.set_packed_field_width(
                                    selected, field.name, std::nullopt);
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        } else if (auto const* soa{std::get_if<SoaType>(&node.definition)}) {
            auto const& analysis{*analysis_session_.results().active_soa};
            if (detail::section("Planner capacity")) {
                auto capacity{analysis.capacity};
                if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
                    analysis_session_.inputs.workspace.set_capacity(selected, capacity);
                }
                draw_override_note(analysis.capacity_overridden);
                if (analysis.capacity_overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset capacity")) {
                        analysis_session_.inputs.workspace.set_capacity(selected, std::nullopt);
                    }
                }

                if (analysis_session_.inputs.selection.field.empty() && !soa->columns.empty()) {
                    analysis_session_.inputs.selection.field = soa->columns.front().name;
                }
            }
            if (detail::section("Columns")) {
                if (detail::begin_editable_table(
                        "soa-columns-properties", 3, analysis.columns.size())) {
                    detail::editable_table_column("Column");
                    detail::editable_table_column("Semantic type");
                    detail::editable_table_column("Planning");
                    ImGui::TableHeadersRow();
                    for (auto const& column : analysis.columns) {
                        ImGui::PushID(column.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(column.name.c_str(),
                                              analysis_session_.inputs.selection.field ==
                                                  column.name)) {
                            analysis_session_.inputs.selection.field = column.name;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(column.schema_type.c_str());
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.0F);
                        if (ImGui::BeginCombo("##planning-type", column.physical_type.c_str())) {
                            if (ImGui::Selectable("Schema type", !column.overridden)) {
                                analysis_session_.inputs.workspace.set_soa_column_type(
                                    selected, column.name, std::nullopt);
                            }
                            for (auto const& [type, facts] :
                                 analysis_session_.primary_abi().types()) {
                                static_cast<void>(facts);
                                if (ImGui::Selectable(type.c_str(), column.physical_type == type)) {
                                    analysis_session_.inputs.workspace.set_soa_column_type(
                                        selected, column.name, type);
                                }
                            }
                            ImGui::EndCombo();
                        }
                        if (column.overridden && ImGui::SmallButton("Reset")) {
                            analysis_session_.inputs.workspace.set_soa_column_type(
                                selected, column.name, std::nullopt);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndDisabled();
    }

    auto draw_links = [&](char const* heading, std::span<TypeId const> const links) {
        if (detail::section(heading)) {
            if (links.empty()) {
                ImGui::TextDisabled("None");
            }
            for (auto const linked : links) {
                auto const& linked_node{analysis_session_.inputs.workspace.types().type(linked)};
                ImGui::PushID(static_cast<int>(linked.value));
                if (ImGui::SmallButton(linked_node.cpp_spelling.c_str())) {
                    select_type(linked);
                    analysis_session_.inputs.selection.field.clear();
                    analysis_session_.inputs.selection.record_access_members.clear();
                    analysis_session_.inputs.selection.record_access_set_explicit = false;
                }
                ImGui::PopID();
            }
        }
    };
    draw_links("Depends on", analysis_session_.inputs.workspace.types().dependencies_of(selected));
    draw_links("Used by", analysis_session_.inputs.workspace.types().users_of(selected));
    end_panel();
}

auto PlannerUi::duplicate_selected_declaration(TypeNode const& node) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* info{document_->declaration(*declaration)};
    if (info == nullptr) {
        return false;
    }

    auto name{node.identity.name + "_copy"};
    for (auto suffix{2U};
         analysis_session_.inputs.workspace.types().find_declared(node.identity.module_name, name);
         ++suffix) {
        name = node.identity.name + "_copy" + std::to_string(suffix);
    }
    auto selection{node.identity};
    selection.name = name;
    auto const id{document_->allocate_declaration_id()};

    if (auto const* schema{document_->enum_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateEnum{.declaration = id,
                                              .module_index = info->module_index,
                                              .schema = std::move(copy),
                                              .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->integer_scalar_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateIntegerScalar{.declaration = id,
                                                       .module_index = info->module_index,
                                                       .schema = std::move(copy),
                                                       .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->linear_quantized_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateLinearQuantized{.declaration = id,
                                                         .module_index = info->module_index,
                                                         .schema = std::move(copy),
                                                         .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->integer_varint_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateIntegerVarint{.declaration = id,
                                                       .module_index = info->module_index,
                                                       .schema = std::move(copy),
                                                       .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->fixed_point_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateFixedPoint{.declaration = id,
                                                    .module_index = info->module_index,
                                                    .schema = std::move(copy),
                                                    .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->mini_float_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateMiniFloat{.declaration = id,
                                                   .module_index = info->module_index,
                                                   .schema = std::move(copy),
                                                   .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->optional_sentinel_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateOptionalSentinel{.declaration = id,
                                                          .module_index = info->module_index,
                                                          .schema = std::move(copy),
                                                          .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->optional_presence_bit_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateOptionalPresenceBit{.declaration = id,
                                                             .module_index = info->module_index,
                                                             .schema = std::move(copy),
                                                             .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->packed_value_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreatePackedValue{.declaration = id,
                                                     .module_index = info->module_index,
                                                     .schema = std::move(copy),
                                                     .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->record_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateRecord{.declaration = id,
                                                .module_index = info->module_index,
                                                .schema = std::move(copy),
                                                .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->union_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateUnion{.declaration = id,
                                               .module_index = info->module_index,
                                               .schema = std::move(copy),
                                               .insertion_index = std::nullopt},
                                   selection);
    }
    if (auto const* schema{document_->tagged_union_schema(*declaration)}) {
        auto copy{*schema};
        copy.name = name;
        return apply_document_edit(CreateTaggedUnion{.declaration = id,
                                                     .module_index = info->module_index,
                                                     .schema = std::move(copy),
                                                     .insertion_index = std::nullopt},
                                   selection);
    }
    if (document_->soa_schema(*declaration) != nullptr) {
        auto copy{document_->prepare_soa_duplicate(*declaration)};
        if (!copy.has_value()) {
            schema_edit_message_ = copy.error().message;
            return false;
        }
        selection.name = copy->name;
        return apply_document_edit(CreateSoa{.declaration = id,
                                             .module_index = info->module_index,
                                             .schema = std::move(*copy),
                                             .insertion_index = std::nullopt},
                                   selection);
    }

    schema_edit_message_ = "This declaration kind is not editable and cannot be duplicated.";
    return false;
}

auto PlannerUi::delete_declaration(DeclarationId const declaration) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const* info{document_->declaration(declaration)};
    if (info == nullptr) {
        schema_edit_message_ = "The declaration no longer exists.";
        return false;
    }

    auto command{std::optional<SchemaEditCommand>{}};
    if (document_->enum_schema(declaration) != nullptr) {
        command = DeleteEnum{.declaration = declaration};
    } else if (document_->integer_scalar_schema(declaration) != nullptr) {
        command = DeleteIntegerScalar{.declaration = declaration};
    } else if (document_->linear_quantized_schema(declaration) != nullptr) {
        command = DeleteLinearQuantized{.declaration = declaration};
    } else if (document_->integer_varint_schema(declaration) != nullptr) {
        command = DeleteIntegerVarint{.declaration = declaration};
    } else if (document_->fixed_point_schema(declaration) != nullptr) {
        command = DeleteFixedPoint{.declaration = declaration};
    } else if (document_->mini_float_schema(declaration) != nullptr) {
        command = DeleteMiniFloat{.declaration = declaration};
    } else if (document_->optional_sentinel_schema(declaration) != nullptr) {
        command = DeleteOptionalSentinel{.declaration = declaration};
    } else if (document_->optional_presence_bit_schema(declaration) != nullptr) {
        command = DeleteOptionalPresenceBit{.declaration = declaration};
    } else if (document_->packed_value_schema(declaration) != nullptr) {
        command = DeletePackedValue{.declaration = declaration};
    } else if (document_->record_schema(declaration) != nullptr) {
        command = DeleteRecord{.declaration = declaration};
    } else if (document_->union_schema(declaration) != nullptr) {
        command = DeleteUnion{.declaration = declaration};
    } else if (document_->tagged_union_schema(declaration) != nullptr) {
        command = DeleteTaggedUnion{.declaration = declaration};
    } else if (document_->soa_schema(declaration) != nullptr) {
        command = DeleteSoa{.declaration = declaration};
    }
    if (!command.has_value()) {
        schema_edit_message_ = "This draft declaration kind cannot be deleted.";
        return false;
    }

    if (!apply_document_edit(std::move(*command))) {
        return false;
    }
    analysis_session_.inputs.selection.record_access_members.clear();
    analysis_session_.inputs.selection.record_access_set_explicit = false;
    return true;
}

void PlannerUi::draw_variants_panel() {
    if (!variants_view_open_) {
        return;
    }
    auto const was_open{variants_view_open_};
    ImGui::Begin("Variants", &variants_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, variants_view_open_);
    auto const selected_type{analysis_session_.inputs.selection.type};
    if (!selected_type.has_value() ||
        !declaration_capabilities(analysis_session_.inputs.workspace.types().type(*selected_type))
             .physical_analysis_available) {
        ImGui::TextWrapped("Physical variants are unavailable for this selection.");
        ImGui::End();
        return;
    }
    auto const baseline_before_actions{analysis_session_.inputs.workspace.active_variant_id() ==
                                       LayoutWorkspace::baseline_variant_id};
    if (ImGui::BeginTable("variant-actions",
                          2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (ImGui::Button("New experiment", {-1.0F, 0.0F})) {
            create_variant_for_selected_schema();
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Duplicate", {-1.0F, 0.0F})) {
            auto const name{analysis_session_.inputs.workspace.active_variant().name + " copy"};
            analysis_session_.inputs.workspace.duplicate_variant(
                analysis_session_.inputs.workspace.active_variant_id(), name);
            sync_variant_name();
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(baseline_before_actions);
        if (ImGui::Button("Reset all overrides", {-1.0F, 0.0F})) {
            analysis_session_.inputs.workspace.reset_variant(
                analysis_session_.inputs.workspace.active_variant_id());
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Delete", {-1.0F, 0.0F})) {
            analysis_session_.inputs.workspace.delete_variant(
                analysis_session_.inputs.workspace.active_variant_id());
            sync_variant_name();
            packed_dragged_divider_.reset();
            packed_dragged_variant_id_.reset();
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    auto const baseline{analysis_session_.inputs.workspace.active_variant_id() ==
                        LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
    if (variant_name_id_ != analysis_session_.inputs.workspace.active_variant_id()) {
        sync_variant_name();
    }
    if (ImGui::InputText("Name",
                         variant_name_.data(),
                         variant_name_.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        analysis_session_.inputs.workspace.rename_variant(
            analysis_session_.inputs.workspace.active_variant_id(), variant_name_.data());
        sync_variant_name();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::BeginChild(
            "variant-list", {0.0F, 0.0F}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (auto const& variant : analysis_session_.inputs.workspace.variants()) {
            auto const selected{analysis_session_.inputs.workspace.active_variant_id() ==
                                variant.id};
            auto const label{variant.id == LayoutWorkspace::baseline_variant_id
                                 ? "Baseline"
                                 : variant.name + " (" +
                                       std::to_string(detail::override_count(variant.overrides)) +
                                       " overrides)"};
            if (ImGui::Selectable(label.c_str(), selected)) {
                analysis_session_.inputs.workspace.select_variant(variant.id);
                sync_variant_name();
                packed_dragged_divider_.reset();
                packed_dragged_variant_id_.reset();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
