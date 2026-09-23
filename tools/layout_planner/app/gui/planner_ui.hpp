#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace ioj::layout_planner {

class FileDialog;

struct WindowSize {
    int width{};
    int height{};
};

struct VarintDistributionRow {
    std::array<char, 32> value{};
    std::uint64_t weight{1};
};

class PlannerUi {
  public:
    explicit PlannerUi(layout::SchemaLoadResult loaded);

    void set_file_dialog(FileDialog* dialog);

    void register_settings_handler();
    void finish_startup(bool reopen_recent_project);
    auto saved_window_size() const -> std::optional<WindowSize>;
    void remember_window_size(WindowSize size);
    void request_close();
    auto take_close_confirmation() -> bool;
    auto draw() -> bool;
    [[nodiscard]] auto window_title() const -> std::string;
  private:
    struct PendingPackedEnumBinding {
        lispb::schema::DeclarationId packed_declaration;
        std::string field_name;
    };

    struct PendingPackedIntegerScalarBinding {
        lispb::schema::DeclarationId packed_declaration;
        std::string field_name;
    };

    struct PendingPackedFixedPointBinding {
        lispb::schema::DeclarationId packed_declaration;
        std::string field_name;
    };

    struct PendingPackedMiniFloatBinding {
        lispb::schema::DeclarationId packed_declaration;
        std::string field_name;
    };

    struct PendingSoaRecordBinding {
        lispb::schema::DeclarationId soa_declaration;
        std::string column_name;
    };

    enum class NewDeclarationDialog {
        enumeration,
        packed_value,
        integer_scalar,
        quantization,
        varint,
        fixed_point,
        mini_float,
        optional_sentinel,
        optional_presence_bit,
        record,
        union_value,
        tagged_union,
        soa,
    };

    static auto settings_read_open(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   char const* name) -> void*;
    static void settings_read_line(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   void* entry,
                                   char const* line);
    static void settings_write_all(ImGuiContext* context,
                                   ImGuiSettingsHandler* handler,
                                   ImGuiTextBuffer* output);
    void validate_comparison_variants();
    void setup_default_dock_layout(unsigned int dockspace_id);
    void persist_view_visibility(bool previous, bool current);
    auto draw_view_menu() -> bool;
    auto draw_file_menu() -> bool;
    void gate_new_declaration_dialogs();
    void draw_source_panel();
    void draw_diagnostics_panel();
    void draw_close_confirmation();
    void draw_project_path_dialogs();
    void draw_new_module_dialog();
    void draw_new_enum_dialog();
    void draw_new_packed_value_dialog();
    void draw_new_integer_scalar_dialog();
    void draw_new_linear_quantized_dialog();
    void draw_new_integer_varint_dialog();
    void draw_new_fixed_point_dialog();
    void draw_new_mini_float_dialog();
    void draw_new_optional_sentinel_dialog();
    void draw_new_optional_presence_bit_dialog();
    void draw_new_record_dialog();
    void draw_new_union_dialog();
    void draw_new_tagged_union_dialog();
    void draw_new_soa_dialog();
    void draw_enum_editor(lispb::schema::TypeNode const& node,
                          lispb::schema::EnumType const& enumeration);
    auto draw_packed_editor(lispb::schema::TypeNode const& node,
                            lispb::schema::PackedType const& packed) -> bool;
    auto draw_integer_scalar_editor(lispb::schema::TypeNode const& node,
                                    lispb::schema::IntegerScalarType const& scalar) -> bool;
    auto draw_linear_quantized_editor(lispb::schema::TypeNode const& node,
                                      lispb::schema::LinearQuantizedType const& quantized) -> bool;
    auto draw_integer_varint_editor(lispb::schema::TypeNode const& node,
                                    lispb::schema::IntegerVarintType const& varint) -> bool;
    auto draw_fixed_point_editor(lispb::schema::TypeNode const& node,
                                 lispb::schema::FixedPointType const& fixed) -> bool;
    auto draw_mini_float_editor(lispb::schema::TypeNode const& node,
                                lispb::schema::MiniFloatType const& mini_float) -> bool;
    auto draw_optional_sentinel_editor(lispb::schema::TypeNode const& node,
                                       lispb::schema::OptionalSentinelType const& optional) -> bool;
    auto draw_optional_presence_bit_editor(lispb::schema::TypeNode const& node,
                                           lispb::schema::OptionalPresenceBitType const& optional)
        -> bool;
    auto draw_record_editor(lispb::schema::TypeNode const& node,
                            lispb::schema::RecordType const& record) -> bool;
    auto draw_union_editor(lispb::schema::TypeNode const& node,
                           lispb::schema::UnionType const& union_type) -> bool;
    auto draw_tagged_union_editor(lispb::schema::TypeNode const& node,
                                  lispb::schema::TaggedUnionType const& tagged_union) -> bool;
    auto draw_soa_editor(lispb::schema::TypeNode const& node, lispb::schema::SoaType const& soa)
        -> bool;
    auto apply_document_edit(lispb::schema::SchemaEditCommand command,
                             std::optional<lispb::schema::TypeIdentity> selection = std::nullopt)
        -> bool;
    auto bind_new_enum_to_packed_field(lispb::schema::TypeIdentity const& enumeration) -> bool;
    auto bind_new_integer_scalar_to_packed_field(lispb::schema::TypeIdentity const& scalar) -> bool;
    auto bind_new_fixed_point_to_packed_field(lispb::schema::TypeIdentity const& fixed_point)
        -> bool;
    auto bind_new_mini_float_to_packed_field(lispb::schema::TypeIdentity const& mini_float) -> bool;
    auto bind_new_record_to_soa_column(lispb::schema::TypeIdentity const& record) -> bool;
    auto apply_project_edit(lispb::ProjectEditCommand command) -> bool;
    [[nodiscard]] auto project_history_active() const -> bool;
    [[nodiscard]] auto has_dirty_changes() const -> bool;
    auto save_changes() -> bool;
    auto export_cpp(std::filesystem::path const& build_root) -> bool;
    auto draw_type_picker(std::string_view module_name, lispb::schema::TypeIdentity const& owner)
        -> std::optional<std::string>;
    void sync_document_graph(std::optional<lispb::schema::TypeIdentity> selection);
    auto load_project(std::filesystem::path const& path,
                      bool allow_dirty = false,
                      bool use_recent_target = false,
                      std::optional<std::string> selected_target = std::nullopt) -> bool;
    void adopt_loaded_schema(layout::SchemaLoadResult loaded);
    void remember_recent_project(std::filesystem::path const& path);
    void refresh_analysis();
    void select_type(std::optional<lispb::schema::TypeId> type);
    void invalidate_type_editor_state();
    void draw_project_panel();
    void draw_layout_panel();
    void draw_target_profile_panel();
    auto draw_target_profile() -> bool;
    void draw_properties_panel();
    void draw_variants_panel();
    void draw_comparison_panel();
    auto draw_comparison_target_profile_picker() -> bool;
    void draw_enum_target_comparison();
    auto draw_soa_target_comparison() -> bool;
    void draw_union_target_comparison();
    void draw_tagged_union_target_comparison();
    void draw_graph_panel();
    auto draw_element_count() -> bool;
    auto draw_access_operation() -> bool;
    void draw_packed_layout(lispb::schema::PackedType const& packed,
                            layout::PackedAnalysis const& baseline);
    void draw_record_layout(layout::RecordAnalysis const& analysis);
    void draw_fixed_point_bit_layout(layout::FixedPointAnalysis const& analysis);
    void draw_soa_layout(lispb::schema::SoaType const& soa, layout::SoaAnalysis const& baseline);
    void draw_diagnostics(std::vector<layout::Diagnostic> const& diagnostics) const;
    auto duplicate_selected_declaration(lispb::schema::TypeNode const& node) -> bool;
    auto delete_declaration(lispb::schema::DeclarationId declaration) -> bool;
    void sync_variant_name();
    void sync_target_memory_fact_inputs();
    auto load_target_profile(std::filesystem::path const& path, bool persist) -> bool;
    void use_builtin_target_profile(bool clear_persisted);
    auto load_comparison_target_profile(std::filesystem::path const& path) -> bool;
    void use_builtin_comparison_target_profile();
    void create_variant_for_selected_schema();

    std::filesystem::path project_path_;
    std::string target_name_;
    std::vector<std::filesystem::path> recent_projects_;
    std::map<std::string, std::string, std::less<>> recent_project_targets_;
    std::optional<lispb::EditableProjectDocument> project_document_;
    std::optional<lispb::schema::EditableSchemaDocument> document_;
    layout::PlannerAnalysisSession analysis_session_;
    layout::MemoryFacts target_memory_fact_defaults_;
    std::vector<layout::Diagnostic> load_diagnostics_;
    bool access_multiplicity_error_{};
    bool soa_region_map_pages_{};
    float soa_region_pixels_{64.0F};

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::array<char, 128> type_picker_filter_{};
    std::array<char, 1024> target_profile_path_{};
    std::array<char, 1024> comparison_target_profile_path_{};
    std::array<char, 32> target_cache_line_bytes_{};
    std::array<char, 32> target_page_bytes_{};
    std::array<char, 64> target_l1_data_cache_bytes_{};
    std::array<char, 64> target_l2_cache_bytes_{};
    std::array<char, 64> target_l3_cache_bytes_{};
    int target_l1_data_cache_unit_{1};
    int target_l2_cache_unit_{1};
    int target_l3_cache_unit_{1};
    std::string target_profile_load_error_;
    std::string comparison_target_profile_error_;
    std::string target_memory_fact_error_;
    std::array<char, 1024> open_project_path_{};
    std::array<char, 1024> save_as_project_path_{};
    std::array<char, 1024> export_build_root_path_{};
    std::array<char, 512> new_project_source_path_{};
    std::array<char, 1024> rename_project_source_path_{};
    std::optional<std::filesystem::path> rename_project_source_;
    bool focus_rename_project_source_{};
    std::array<char, 128> new_module_name_{};
    std::array<char, 256> new_module_header_{};
    std::array<char, 128> new_module_namespace_{};
    int new_module_backend_{};
    std::size_t new_module_source_file_index_{1};
    std::optional<NewDeclarationDialog> declaration_after_new_module_;
    bool module_initiated_dialog_{};
    bool confirm_unchecked_module_header_{};
    std::array<char, 128> new_enum_name_{};
    std::array<char, 128> new_enum_underlying_type_{"std::uint8_t"};
    bool new_enum_backing_auto_{true};
    bool new_enum_width_auto_{true};
    std::uint32_t new_enum_bit_width_{1};
    int new_enum_signedness_{};
    std::array<char, 128> new_packed_value_name_{};
    std::array<char, 128> new_packed_storage_type_{"std::uint32_t"};
    int new_packed_byte_order_{};
    int new_packed_bit_order_{};
    std::array<char, 128> new_integer_scalar_name_{};
    std::array<char, 32> new_integer_scalar_minimum_{"0"};
    std::array<char, 32> new_integer_scalar_maximum_{"255"};
    bool new_integer_scalar_signed_{};
    bool new_integer_scalar_width_auto_{true};
    std::uint32_t new_integer_scalar_bit_width_{8};
    std::array<char, 128> new_linear_quantized_name_{};
    std::array<char, 128> new_linear_quantized_source_{};
    std::uint32_t new_linear_quantized_bit_width_{8};
    std::uint64_t new_linear_quantized_reserved_codes_{};
    int new_linear_quantized_clipping_{};
    std::array<char, 128> new_integer_varint_name_{};
    std::array<char, 128> new_integer_varint_source_{};
    int new_integer_varint_encoding_{};
    std::array<char, 128> new_fixed_point_name_{};
    bool new_fixed_point_signed_{true};
    std::uint32_t new_fixed_point_total_bits_{16};
    std::uint32_t new_fixed_point_fractional_bits_{8};
    int new_fixed_point_rounding_{};
    std::array<char, 128> new_fixed_point_minimum_{};
    std::array<char, 128> new_fixed_point_maximum_{};
    std::array<char, 128> new_mini_float_name_{};
    std::uint32_t new_mini_float_sign_bits_{1};
    std::uint32_t new_mini_float_exponent_bits_{5};
    std::uint32_t new_mini_float_significand_bits_{10};
    std::int32_t new_mini_float_exponent_bias_{15};
    std::array<char, 128> new_optional_sentinel_name_{};
    std::array<char, 128> new_optional_sentinel_source_{};
    std::array<char, 128> new_optional_sentinel_code_{};
    std::array<char, 128> new_optional_presence_bit_name_{};
    std::array<char, 128> new_optional_presence_bit_source_{};
    std::array<char, 128> new_record_name_{};
    std::array<char, 128> new_record_member_type_{"std::uint32_t"};
    std::array<char, 128> new_union_name_{};
    std::array<char, 128> new_union_alternative_type_{"std::uint32_t"};
    std::array<char, 128> new_tagged_union_name_{};
    std::array<char, 128> new_tagged_union_discriminant_{};
    std::array<char, 128> new_tagged_union_alternative_type_{"std::uint32_t"};
    std::array<char, 128> new_tagged_union_tag_{};
    std::array<char, 128> new_soa_name_{};
    std::array<char, 128> new_soa_member_type_{"std::uint32_t"};
    std::array<char, 128> enum_value_name_{};
    std::array<char, 128> enum_value_initializer_{};
    std::array<char, 128> enum_value_display_name_{};
    std::array<char, 128> enum_value_serialized_name_{};
    std::array<char, 128> enum_export_specifier_{};
    std::array<char, 128> enum_projection_name_{};
    std::array<char, 260> enum_projection_header_{};
    std::array<char, 260> enum_projection_header_include_{};
    std::array<char, 260> enum_projection_conversion_header_{};
    std::array<char, 260> enum_projection_native_header_include_{};
    int enum_projection_reflection_{};
    std::array<char, 128> packed_storage_type_{};
    std::array<char, 64> packed_invalid_value_{};
    std::array<char, 128> packed_export_specifier_{};
    std::array<char, 128> packed_field_name_{};
    std::array<char, 128> packed_field_type_{};
    std::array<char, 32> packed_field_minimum_{};
    std::array<char, 32> packed_field_maximum_{};
    std::array<char, 128> packed_code_name_{};
    std::array<char, 32> packed_code_value_{};
    std::array<char, 128> packed_relationship_target_{};
    std::array<char, 128> integer_scalar_relationship_target_{};
    std::array<char, 32> integer_scalar_minimum_{};
    std::array<char, 32> integer_scalar_maximum_{};
    std::array<char, 128> integer_scalar_cpp_type_{};
    std::array<char, 128> integer_scalar_code_name_{};
    std::array<char, 32> integer_scalar_code_value_{};
    std::array<char, 128> record_member_name_{};
    std::array<char, 128> record_member_type_{};
    std::array<char, 128> record_relationship_target_{};
    std::array<char, 128> record_export_specifier_{};
    std::array<char, 128> union_alternative_name_{};
    std::array<char, 128> union_alternative_type_{};
    std::array<char, 128> union_export_specifier_{};
    std::array<char, 128> tagged_union_alternative_name_{};
    std::array<char, 128> tagged_union_alternative_type_{};
    std::array<char, 128> tagged_union_export_specifier_{};
    std::array<char, 128> soa_member_name_{};
    std::array<char, 128> soa_member_type_{};
    std::array<char, 128> soa_member_fixed_schema_{};
    std::array<char, 128> soa_member_nested_schema_{};
    std::array<char, 128> soa_relationship_target_{};
    std::array<char, 128> soa_equivalent_type_{};
    std::array<char, 128> soa_export_specifier_{};
    std::vector<std::array<char, 256>> soa_using_declarations_;
    std::optional<std::size_t> soa_using_declaration_index_;
    std::vector<std::array<char, 128>> soa_function_names_;
    std::vector<std::array<char, 128>> soa_function_return_types_;
    std::optional<std::size_t> soa_function_index_;
    std::vector<std::array<char, 128>> soa_parameter_names_;
    std::vector<std::array<char, 128>> soa_parameter_types_;
    std::vector<std::array<char, 128>> soa_parameter_defaults_;
    std::optional<std::size_t> soa_parameter_index_;
    std::vector<std::string> soa_function_body_lines_;
    std::optional<std::size_t> soa_function_body_index_;
    std::vector<std::string> soa_function_dependencies_;
    std::optional<std::size_t> soa_function_dependency_index_;
    std::string soa_new_function_dependency_;
    std::string soa_function_trailing_return_type_;
    std::string soa_function_template_parameters_;
    std::string soa_function_requires_clause_;
    std::array<char, 128> soa_view_name_{};
    std::array<char, 128> soa_const_view_name_{};
    std::array<char, 128> soa_single_allocation_name_{};
    std::array<char, 128> soa_new_single_allocation_allocator_{};
    std::vector<std::array<char, 128>> soa_single_allocation_variant_names_;
    std::vector<std::array<char, 128>> soa_single_allocation_variant_allocators_;
    std::optional<std::size_t> soa_single_allocation_variant_index_;
    std::array<char, 128> soa_fixed_storage_name_{};
    std::vector<std::array<char, 128>> soa_fixed_container_names_;
    std::optional<std::size_t> soa_fixed_container_index_;
    std::vector<std::array<char, 128>> soa_mask_dimension_names_;
    std::vector<std::array<char, 128>> soa_mask_dimension_extents_;
    std::optional<std::size_t> soa_mask_dimension_index_;
    std::string selected_enumerator_;
    std::map<lispb::schema::TypeIdentity, std::vector<VarintDistributionRow>> varint_distributions_;
    std::array<char, 32> new_varint_distribution_value_{"0"};
    std::uint64_t new_varint_distribution_weight_{1};
    std::optional<lispb::schema::DeclarationId> rename_editor_declaration_;
    std::array<char, 128> declaration_name_{};
    std::optional<lispb::schema::DeclarationId> inline_record_rename_;
    std::array<char, 128> inline_record_name_{};
    bool focus_inline_record_rename_{};
    std::optional<std::size_t> open_record_module_;
    std::optional<std::size_t> delete_module_index_;
    std::string delete_module_name_;
    std::optional<lispb::schema::DeclarationId> delete_declaration_;
    std::string delete_declaration_name_;
    std::optional<lispb::schema::DeclarationId> enum_editor_declaration_;
    std::string enum_editor_value_;
    std::array<char, 128> enum_underlying_type_{"std::uint8_t"};
    std::string schema_edit_message_;
    std::string schema_warning_message_;
    std::size_t new_enum_module_index_{};
    std::size_t new_packed_module_index_{};
    std::size_t new_integer_scalar_module_index_{};
    std::size_t new_linear_quantized_module_index_{};
    std::size_t new_integer_varint_module_index_{};
    std::size_t new_fixed_point_module_index_{};
    std::size_t new_mini_float_module_index_{};
    std::size_t new_optional_sentinel_module_index_{};
    std::size_t new_optional_presence_bit_module_index_{};
    std::size_t new_record_module_index_{};
    std::size_t new_union_module_index_{};
    std::size_t new_tagged_union_module_index_{};
    std::size_t new_soa_module_index_{};
    std::optional<lispb::schema::DeclarationId> packed_editor_declaration_;
    std::string packed_editor_field_;
    std::optional<lispb::schema::DeclarationId> packed_code_editor_declaration_;
    std::string packed_code_editor_field_;
    std::string packed_code_editor_name_;
    std::string selected_packed_code_;
    std::optional<lispb::schema::DeclarationId> integer_scalar_editor_declaration_;
    std::string integer_scalar_editor_code_;
    std::string selected_integer_scalar_code_;
    std::optional<lispb::schema::DeclarationId> linear_quantized_editor_declaration_;
    std::optional<lispb::schema::DeclarationId> integer_varint_editor_declaration_;
    std::optional<lispb::schema::DeclarationId> fixed_point_editor_declaration_;
    std::array<char, 128> fixed_point_minimum_{};
    std::array<char, 128> fixed_point_maximum_{};
    std::optional<lispb::schema::DeclarationId> optional_sentinel_editor_declaration_;
    std::optional<lispb::schema::DeclarationId> optional_presence_bit_editor_declaration_;
    std::optional<lispb::schema::DeclarationId> record_editor_declaration_;
    std::string record_editor_member_;
    std::optional<lispb::schema::DeclarationId> union_editor_declaration_;
    std::string union_editor_alternative_;
    std::optional<lispb::schema::DeclarationId> tagged_union_editor_declaration_;
    std::string tagged_union_editor_alternative_;
    std::optional<lispb::schema::DeclarationId> soa_editor_declaration_;
    std::string soa_editor_member_;
    int packed_field_bits_{1};
    int packed_relationship_kind_{};
    int integer_scalar_relationship_kind_{};
    int packed_relationship_unit_{};
    int integer_scalar_relationship_unit_{};
    int record_relationship_kind_{};
    int record_relationship_unit_{};
    int soa_relationship_kind_{};
    int soa_relationship_unit_{};
    bool packed_code_sentinel_{};
    bool integer_scalar_code_sentinel_{};
    std::uint64_t record_member_count_{1};
    bool record_member_is_array_{};
    std::uint64_t union_alternative_count_{1};
    bool union_alternative_is_array_{};
    std::uint64_t tagged_union_alternative_count_{1};
    bool tagged_union_alternative_is_array_{};
    std::optional<std::size_t> packed_dragged_divider_;
    std::optional<std::uint64_t> packed_dragged_variant_id_;
    std::optional<std::uint32_t> packed_dragged_left_width_;
    std::optional<std::uint32_t> packed_dragged_right_width_;
    std::uint64_t variant_name_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t next_variant_number_{1};
    float text_scale_{1.0F};
    float graph_pan_x_{32.0F};
    float graph_pan_y_{32.0F};
    float graph_zoom_{1.0F};
    std::map<lispb::schema::TypeIdentity, std::array<float, 2>> graph_node_positions_;
    std::map<std::string, std::map<lispb::schema::TypeIdentity, std::array<float, 2>>, std::less<>>
        persisted_graph_node_positions_;
    std::map<std::string, std::filesystem::path, std::less<>> persisted_target_profile_paths_;
    std::array<char, 128> graph_search_{};
    std::array<char, 1024> new_project_path_{};
    FileDialog* file_dialog_{};
    std::filesystem::path pending_project_path_;
    std::vector<std::string> pending_project_targets_;
    bool open_project_target_dialog_{};
    std::array<char, 128> new_project_target_{};
    std::optional<int> window_width_;
    std::optional<int> window_height_;
    bool dock_layout_initialized_{};
    bool reset_dock_layout_requested_{};
    bool open_new_module_dialog_{};
    bool open_new_enum_dialog_{};
    std::optional<PendingPackedEnumBinding> pending_packed_enum_binding_;
    bool open_new_packed_value_dialog_{};
    bool open_new_integer_scalar_dialog_{};
    std::optional<PendingPackedIntegerScalarBinding> pending_packed_integer_scalar_binding_;
    bool open_new_linear_quantized_dialog_{};
    bool open_new_integer_varint_dialog_{};
    bool open_new_fixed_point_dialog_{};
    std::optional<PendingPackedFixedPointBinding> pending_packed_fixed_point_binding_;
    bool open_new_mini_float_dialog_{};
    std::optional<PendingPackedMiniFloatBinding> pending_packed_mini_float_binding_;
    bool open_new_optional_sentinel_dialog_{};
    bool open_new_optional_presence_bit_dialog_{};
    bool open_new_record_dialog_{};
    std::optional<PendingSoaRecordBinding> pending_soa_record_binding_;
    bool open_new_union_dialog_{};
    bool open_new_tagged_union_dialog_{};
    bool open_new_soa_dialog_{};
    bool source_view_open_{};
    bool diagnostics_view_open_{true};
    bool focus_diagnostics_view_{};
    bool focus_source_view_{};
    bool open_project_dialog_{};
    bool open_new_project_dialog_{};
    bool open_save_as_dialog_{};
    bool open_export_build_root_dialog_{};
    bool save_shortcut_pending_{};
    bool project_changed_{};
    bool open_close_confirmation_{};
    bool close_confirmed_{};
    bool project_view_open_{true};
    bool layout_view_open_{true};
    bool target_profile_view_open_{true};
    bool focus_target_profile_view_{};
    bool properties_view_open_{true};
    bool variants_view_open_{true};
    bool comparison_view_open_{true};
    bool graph_view_open_{true};
    bool graph_focus_selected_{};
    bool graph_fit_all_{};
};

} // namespace ioj::layout_planner
