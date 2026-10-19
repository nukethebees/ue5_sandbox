#pragma once

#include <codegen/sexpr/handle_range.h>
#include <lispb/indices.h>
#include <lispb/kernel_profile.h>
#include <lispb/rooted_path.h>
#include <lispb/target_columns.h>
#include <lispb/target_groups.h>
#include <lispb/target_id.h>

#include <cassert>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace lispb {
class Strings;

class Project {
  public:
    explicit Project(Strings& strings)
        : strings_{&strings} {}
    Project(Project const&) = delete;
    auto operator=(Project const&) -> Project& = delete;
    Project(Project&&) noexcept = default;
    auto operator=(Project&&) noexcept -> Project& = default;

    std::filesystem::path root;
    [[nodiscard]] auto strings() const -> Strings& { return *strings_; }
    [[nodiscard]] auto find_target(std::string_view name) const -> std::optional<TargetId>;
    [[nodiscard]] auto target_ids() const -> std::span<TargetId const> { return order_; }
    [[nodiscard]] auto target_name(TargetId target) const -> std::string_view;
    [[nodiscard]] auto sources(TargetId target) const -> std::vector<std::filesystem::path>;
    [[nodiscard]] auto source_range(TargetId target) const -> codegen::sexpr::HandleRange;
    [[nodiscard]] auto sources(codegen::sexpr::HandleRange range) const
        -> std::vector<std::filesystem::path>;
    [[nodiscard]] auto source_files(TargetId target) const -> std::vector<std::filesystem::path>;
    [[nodiscard]] auto types(TargetId target) const -> std::filesystem::path;
    [[nodiscard]] auto include_directories(TargetId target) const
        -> std::vector<std::filesystem::path>;
    [[nodiscard]] auto profile(TargetId target) const -> KernelProfile;
    [[nodiscard]] auto output_path(TargetId target) const -> RootedPath;
    [[nodiscard]] auto expand(std::string_view name) const -> std::vector<TargetId>;
    auto append(TargetKind kind, IdentifierId name) -> TargetId;
    auto append_paths(std::span<StringId const> paths) -> codegen::sexpr::HandleRange;
    void set_sources(TargetId target, codegen::sexpr::HandleRange sources);
    void set_types(TargetId target, StringId types);
    void set_includes(TargetId target, codegen::sexpr::HandleRange includes);
    void set_profile(TargetId target, KernelProfile profile);
    void set_output(TargetId target, PathBase base, StringId path);
    void add_group(IdentifierId name, std::span<IdentifierId const> members);
    void resolve_groups();
    void replace_sources(TargetId target, std::span<std::filesystem::path const> sources);
  private:
    TargetColumns cpp_schema_, slate_, kernel_, material_;
    TargetGroups groups_;
    [[nodiscard]] auto columns(this auto& self, TargetId target) -> decltype(auto) {
        switch (target.kind()) {
            case TargetKind::cpp_schema:
                return (self.cpp_schema_);
            case TargetKind::slate:
                return (self.slate_);
            case TargetKind::kernel:
                return (self.kernel_);
            case TargetKind::material:
                return (self.material_);
            default:
                assert(false);
                std::unreachable();
        }
    }
    [[nodiscard]] auto store_paths(std::span<std::filesystem::path const> paths)
        -> codegen::sexpr::HandleRange;
    [[nodiscard]] auto paths(codegen::sexpr::HandleRange range) const
        -> std::vector<std::filesystem::path>;
    void
        expand(TargetId target, std::vector<TargetId>& active, std::vector<TargetId>& result) const;
    Strings* strings_;
    std::map<IdentifierId, TargetId> names_;
    std::vector<TargetId> order_;
    std::vector<StringId> paths_;
    std::vector<StringId> cpp_types_;
    std::vector<codegen::sexpr::HandleRange> slate_includes_;
    std::vector<KernelProfile> kernel_profiles_;
    std::vector<IdentifierId> unresolved_group_members_;
};

}
