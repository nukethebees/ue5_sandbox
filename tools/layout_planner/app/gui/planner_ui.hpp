#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/schema_loader.hpp>
#include <ioj/layout/workspace.hpp>

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

    void register_settings_handler();
    void finish_startup(bool reopen_recent_project);
    auto saved_window_size() const -> std::optional<WindowSize>;
    void remember_window_size(WindowSize size);
    void request_close();
    auto take_close_confirmation() -> bool;
    auto draw() -> bool;
  private:
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
    auto draw_view_menu() -> bool;
    auto draw_file_menu() -> bool;
    void draw_source_preview();
    void draw_close_confirmation();
    void draw_project_path_dialogs();
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
    auto draw_type_picker(std::string_view module_name, lispb::schema::TypeIdentity const& owner)
        -> std::optional<std::string>;
    void sync_document_graph(std::optional<lispb::schema::TypeIdentity> selection);
    auto load_project(std::filesystem::path const& path, bool allow_dirty = false) -> bool;
    void adopt_loaded_schema(layout::SchemaLoadResult loaded);
    void remember_recent_project(std::filesystem::path const& path);
    void refresh_analysis();
    void draw_project_panel();
    void draw_layout_panel();
    void draw_properties_panel();
    void draw_variants_panel();
    void draw_comparison_panel();
    void draw_graph_panel();
    auto draw_element_count() -> bool;
    void draw_packed_layout(lispb::schema::PackedType const& packed,
                            layout::PackedAnalysis const& baseline);
    void draw_record_layout(layout::RecordAnalysis const& analysis);
    void draw_soa_layout(lispb::schema::SoaType const& soa, layout::SoaAnalysis const& baseline);
    void draw_diagnostics(std::vector<layout::Diagnostic> const& diagnostics) const;
    auto duplicate_selected_declaration(lispb::schema::TypeNode const& node) -> bool;
    auto delete_declaration(lispb::schema::DeclarationId declaration) -> bool;
    void sync_variant_name();
    void create_variant_for_selected_schema();

    std::filesystem::path project_path_;
    std::string target_name_;
    std::vector<std::filesystem::path> recent_projects_;
    std::optional<lispb::schema::EditableSchemaDocument> document_;
    layout::LayoutWorkspace workspace_;
    layout::AbiProfile abi_{layout::AbiProfile::host_common()};
    std::vector<layout::Diagnostic> load_diagnostics_;
    std::optional<lispb::schema::TypeId> selected_type_;
    std::string selected_field_;

    std::uint64_t cached_revision_{};
    std::optional<lispb::schema::TypeId> cached_type_;
    std::string cached_selected_field_;
    std::set<std::string, std::less<>> cached_record_access_members_;
    bool cached_record_access_set_explicit_{};
    std::uint64_t cached_comparison_a_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_comparison_b_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::optional<lispb::schema::TypeIdentity> cached_quantized_comparison_type_;
    std::optional<lispb::schema::TypeIdentity> cached_varint_comparison_type_;
    std::optional<lispb::schema::TypeIdentity> cached_optional_comparison_type_;
    std::uint64_t cached_tagged_distribution_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::optional<layout::EnumDomainAnalysis> enum_domain_;
    std::optional<layout::IntegerScalarAnalysis> integer_scalar_analysis_;
    std::optional<layout::LinearQuantizedAnalysis> linear_quantized_analysis_;
    std::optional<layout::LinearQuantizedComparison> linear_quantized_comparison_;
    std::optional<layout::IntegerVarintAnalysis> integer_varint_analysis_;
    std::optional<layout::IntegerVarintComparison> integer_varint_comparison_;
    std::optional<layout::FixedPointAnalysis> fixed_point_analysis_;
    std::optional<layout::MiniFloatAnalysis> mini_float_analysis_;
    std::optional<layout::OptionalSentinelAnalysis> optional_sentinel_analysis_;
    std::optional<layout::OptionalPresenceBitAnalysis> optional_presence_bit_analysis_;
    std::optional<layout::OptionalEncodingComparison> optional_encoding_comparison_;
    std::optional<layout::PackedAnalysis> baseline_packed_;
    std::optional<layout::PackedAnalysis> active_packed_;
    std::vector<std::pair<std::uint64_t, layout::PackedAnalysis>> packed_variants_;
    std::optional<layout::SoaAnalysis> baseline_soa_;
    std::optional<layout::SoaAnalysis> active_soa_;
    std::vector<std::pair<std::uint64_t, layout::SoaAnalysis>> soa_variants_;
    std::optional<layout::PackedAnalysis> comparison_a_packed_;
    std::optional<layout::PackedAnalysis> comparison_b_packed_;
    std::optional<layout::RecordAnalysis> record_analysis_;
    std::optional<layout::RecordAccessAnalysis> record_access_analysis_;
    std::optional<layout::UnionAnalysis> union_analysis_;
    std::optional<layout::TaggedUnionAnalysis> tagged_union_analysis_;
    std::optional<layout::TaggedUnionDistributionAnalysis> tagged_union_distribution_analysis_;
    std::optional<layout::SoaAnalysis> comparison_a_soa_;
    std::optional<layout::SoaAnalysis> comparison_b_soa_;

    std::array<char, 128> variant_name_{};
    std::array<char, 128> schema_filter_{};
    std::array<char, 128> type_picker_filter_{};
    std::array<char, 1024> open_project_path_{};
    std::array<char, 1024> save_as_project_path_{};
    std::array<char, 128> new_enum_name_{};
    std::array<char, 128> new_enum_underlying_type_{"std::uint8_t"};
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
    std::array<char, 128> packed_storage_type_{};
    std::array<char, 64> packed_invalid_value_{};
    std::array<char, 128> packed_field_name_{};
    std::array<char, 128> packed_field_type_{};
    std::array<char, 32> packed_field_minimum_{};
    std::array<char, 32> packed_field_maximum_{};
    std::array<char, 128> packed_code_name_{};
    std::array<char, 32> packed_code_value_{};
    std::array<char, 128> packed_relationship_target_{};
    std::array<char, 32> integer_scalar_minimum_{};
    std::array<char, 32> integer_scalar_maximum_{};
    std::array<char, 128> integer_scalar_code_name_{};
    std::array<char, 32> integer_scalar_code_value_{};
    std::array<char, 128> record_member_name_{};
    std::array<char, 128> record_member_type_{};
    std::array<char, 128> union_alternative_name_{};
    std::array<char, 128> union_alternative_type_{};
    std::array<char, 128> tagged_union_alternative_name_{};
    std::array<char, 128> tagged_union_alternative_type_{};
    std::array<char, 128> soa_member_name_{};
    std::array<char, 128> soa_member_type_{};
    std::array<char, 128> soa_member_fixed_schema_{};
    std::array<char, 128> soa_member_nested_schema_{};
    std::vector<std::array<char, 128>> soa_mask_dimension_names_;
    std::vector<std::array<char, 128>> soa_mask_dimension_extents_;
    std::optional<std::size_t> soa_mask_dimension_index_;
    std::string selected_enumerator_;
    std::set<std::string, std::less<>> record_access_members_;
    std::map<lispb::schema::TypeIdentity, std::vector<VarintDistributionRow>> varint_distributions_;
    std::map<lispb::schema::DeclarationId, std::map<std::string, std::uint64_t>>
        tagged_union_distributions_;
    std::uint64_t tagged_distribution_revision_{};
    std::array<char, 32> new_varint_distribution_value_{"0"};
    std::uint64_t new_varint_distribution_weight_{1};
    bool record_access_set_explicit_{};
    std::optional<lispb::schema::DeclarationId> rename_editor_declaration_;
    std::array<char, 128> declaration_name_{};
    std::optional<lispb::schema::DeclarationId> delete_declaration_;
    std::string delete_declaration_name_;
    std::optional<lispb::schema::DeclarationId> enum_editor_declaration_;
    std::string enum_editor_value_;
    std::string schema_edit_message_;
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
    std::uint64_t comparison_a_variant_id_{layout::LayoutWorkspace::baseline_variant_id};
    std::uint64_t comparison_b_variant_id_{layout::LayoutWorkspace::baseline_variant_id};
    std::optional<lispb::schema::TypeIdentity> quantized_comparison_type_;
    std::optional<lispb::schema::TypeIdentity> varint_comparison_type_;
    std::optional<lispb::schema::TypeIdentity> optional_comparison_type_;
    float text_scale_{1.0F};
    float graph_pan_x_{32.0F};
    float graph_pan_y_{32.0F};
    float graph_zoom_{1.0F};
    std::optional<int> window_width_;
    std::optional<int> window_height_;
    bool dock_layout_initialized_{};
    bool reset_dock_layout_requested_{};
    bool comparison_b_follows_active_{true};
    bool open_new_enum_dialog_{};
    bool open_new_packed_value_dialog_{};
    bool open_new_integer_scalar_dialog_{};
    bool open_new_linear_quantized_dialog_{};
    bool open_new_integer_varint_dialog_{};
    bool open_new_fixed_point_dialog_{};
    bool open_new_mini_float_dialog_{};
    bool open_new_optional_sentinel_dialog_{};
    bool open_new_optional_presence_bit_dialog_{};
    bool open_new_record_dialog_{};
    bool open_new_union_dialog_{};
    bool open_new_tagged_union_dialog_{};
    bool open_new_soa_dialog_{};
    bool open_source_preview_{};
    bool open_project_dialog_{};
    bool open_save_as_dialog_{};
    bool project_changed_{};
    bool open_close_confirmation_{};
    bool close_confirmed_{};
    bool graph_view_open_{true};
    bool graph_focus_selected_{};
};

} // namespace ioj::layout_planner
