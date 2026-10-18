#pragma once

#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/variant_id.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout {
class EditorSession;

// Borrow the editor for the controller's lifetime. Draw from the published document and
// analysis, enqueue actions, then drain once after drawing.
class PlannerController {
  public:
    explicit PlannerController(EditorSession& editor);
    PlannerController(EditorSession& editor, Document document);
    PlannerController(PlannerController const&) = delete;
    PlannerController(PlannerController&&) = delete;
    auto operator=(PlannerController const&) -> PlannerController& = delete;
    auto operator=(PlannerController&&) -> PlannerController& = delete;

    auto document() -> Document*;
    auto document() const -> Document const*;
    auto analysis() const -> AnalysisStorage const&;
    auto analysis_count() const -> std::uint64_t;
    auto last_edit_succeeded() const -> std::optional<bool>;
    auto selected_type() const -> std::optional<lispb::schema::TypeId>;
    auto selected_module() const -> std::string_view;
    auto selected_declaration() const -> std::string_view;
    auto active_variant() const -> VariantId;
    auto comparison_variant(bool first) const -> VariantId;

    void submit(Transaction transaction);
    void select_declaration(std::string module, std::string name);
    void select_created_declaration(std::string module);
    void select_created_variant();
    void select_variant(VariantId variant);
    void select_comparison_variant(bool first, VariantId variant);
    void undo();
    void redo();
    void save();
    void save_as(std::filesystem::path path, std::string target);
    void open(std::filesystem::path path, std::string target);
    void new_document();
    auto drain() -> bool;
  private:
    void select_now(std::string module, std::string name);
    void adopt(Document document);
    void reconcile();
    void analyze();

    EditorSession& editor_;
    std::optional<Document> document_;
    AnalysisStorage analysis_;
    std::vector<std::move_only_function<void()>> pending_;
    std::string module_;
    std::string declaration_;
    std::optional<lispb::schema::DeclarationId> declaration_id_;
    VariantId active_variant_{};
    VariantId comparison_a_{};
    VariantId comparison_b_{};
    bool comparison_b_follows_active_{true};
    bool analysis_needed_{};
    std::optional<bool> last_edit_succeeded_;
    std::uint64_t analysis_count_{};
};
}
