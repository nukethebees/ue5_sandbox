#include <lispb/project.h>

#include <codegen/sexpr/handle_range.h>
#include <lispb/indices.h>
#include <lispb/kernel_profile.h>
#include <lispb/rooted_path.h>
#include <lispb/strings.h>
#include <lispb/target_columns.h>
#include <lispb/target_id.h>
#include <lispb/target_kind.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb {

auto Project::columns(TargetId const target) const -> TargetColumns const& {
    switch (target.kind()) {
        case TargetKind::cpp_schema:
            return cpp_schema_;
        case TargetKind::slate:
            return slate_;
        case TargetKind::kernel:
            return kernel_;
        case TargetKind::material:
            return material_;
        default:
            assert(false);
            std::unreachable();
    }
}
auto Project::columns(TargetId const target) -> TargetColumns& {
    return const_cast<TargetColumns&>(std::as_const(*this).columns(target));
}
auto Project::store_paths(std::span<std::filesystem::path const> const paths)
    -> codegen::sexpr::HandleRange {
    codegen::sexpr::HandleRange const result{static_cast<std::uint32_t>(paths_.size()),
                                             static_cast<std::uint32_t>(paths.size())};
    for (auto const& path : paths) {
        paths_.push_back(string_id(strings_->intern(path.generic_string())));
    }
    return result;
}
auto Project::paths(codegen::sexpr::HandleRange const range) const
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> result;
    result.reserve(range.count);
    auto const end{range.offset + range.count};
    for (auto index{range.offset}; index < end; ++index) {
        result.emplace_back(strings_->view(paths_[index]));
    }
    return result;
}
auto Project::find_target(std::string_view const name) const -> std::optional<TargetId> {
    auto const found{names_.find(strings_->find(name))};
    if (found == names_.end() || found->second.kind() == TargetKind::group) {
        return {};
    }
    return found->second;
}
auto Project::target_name(TargetId const target) const -> std::string_view {
    return strings_->view(target.kind() == TargetKind::group
                              ? groups_.names[target.index()]
                              : columns(target).names[target.index()]);
}
auto Project::sources(TargetId const target) const -> std::vector<std::filesystem::path> {
    return paths(columns(target).sources[target.index()]);
}
auto Project::source_range(TargetId const target) const -> lispb::codegen::sexpr::HandleRange {
    return columns(target).sources[target.index()];
}
auto Project::sources(lispb::codegen::sexpr::HandleRange const range) const
    -> std::vector<std::filesystem::path> {
    return paths(range);
}
auto Project::source_files(TargetId const target) const -> std::vector<std::filesystem::path> {
    auto result{sources(target)};
    if (target.kind() == TargetKind::cpp_schema) {
        result.push_back(types(target));
    }
    return result;
}
auto Project::types(TargetId const target) const -> std::filesystem::path {
    assert(target.kind() == TargetKind::cpp_schema);
    return std::filesystem::path{strings_->view(cpp_types_[target.index()])};
}
auto Project::include_directories(TargetId const target) const
    -> std::vector<std::filesystem::path> {
    assert(target.kind() == TargetKind::slate);
    return paths(slate_includes_[target.index()]);
}
auto Project::profile(TargetId const target) const -> KernelProfile {
    assert(target.kind() == TargetKind::kernel);
    return kernel_profiles_[target.index()];
}
auto Project::output_path(TargetId const target) const -> RootedPath {
    auto const& data{columns(target)};
    return {data.output_bases[target.index()],
            std::filesystem::path{strings_->view(data.output_paths[target.index()])}};
}
auto Project::append(TargetKind const kind, IdentifierId const name) -> TargetId {
    if (names_.contains(name)) {
        throw std::invalid_argument{"duplicate Lispb target or group '" +
                                    std::string{strings_->view(name)} + "'"};
    }
    TargetId id{static_cast<std::uint32_t>(kind) << 24};
    auto& data{columns(id)};
    assert(data.names.size() < 0x01000000u);
    id.value |= static_cast<std::uint32_t>(data.names.size());
    data.names.push_back(name);
    data.sources.emplace_back();
    data.output_paths.emplace_back();
    data.output_bases.push_back(PathBase::project);
    if (kind == TargetKind::cpp_schema) {
        cpp_types_.emplace_back();
    } else if (kind == TargetKind::slate) {
        slate_includes_.emplace_back();
    } else if (kind == TargetKind::kernel) {
        kernel_profiles_.push_back(KernelProfile::unreal);
    }
    names_.emplace(name, id);
    order_.push_back(id);
    return id;
}
auto Project::append_paths(std::span<StringId const> const paths) -> codegen::sexpr::HandleRange {
    codegen::sexpr::HandleRange const range{static_cast<std::uint32_t>(paths_.size()),
                                            static_cast<std::uint32_t>(paths.size())};
    paths_.insert(paths_.end(), paths.begin(), paths.end());
    return range;
}
void Project::set_sources(TargetId const target, codegen::sexpr::HandleRange const sources) {
    columns(target).sources[target.index()] = sources;
}
void Project::set_types(TargetId const target, StringId const types) {
    assert(target.kind() == TargetKind::cpp_schema);
    cpp_types_[target.index()] = types;
}
void Project::set_includes(TargetId const target, codegen::sexpr::HandleRange const includes) {
    assert(target.kind() == TargetKind::slate);
    slate_includes_[target.index()] = includes;
}
void Project::set_profile(TargetId const target, KernelProfile const profile) {
    assert(target.kind() == TargetKind::kernel);
    kernel_profiles_[target.index()] = profile;
}
void Project::set_output(TargetId const target, PathBase const base, StringId const path) {
    auto& data{columns(target)};
    data.output_bases[target.index()] = base;
    data.output_paths[target.index()] = path;
}
void Project::replace_sources(TargetId const target,
                              std::span<std::filesystem::path const> const sources) {
    assert(target.kind() == TargetKind::cpp_schema);
    columns(target).sources[target.index()] = store_paths(sources);
}
void Project::add_group(IdentifierId const name, std::span<IdentifierId const> const members) {
    auto const identifier{name};
    if (names_.contains(identifier)) {
        throw std::invalid_argument{"duplicate Lispb target or group '" +
                                    std::string{strings_->view(name)} + "'"};
    }
    assert(groups_.names.size() < 0x01000000u);
    TargetId const id{static_cast<std::uint32_t>(TargetKind::group) << 24 |
                      static_cast<std::uint32_t>(groups_.names.size())};
    groups_.names.push_back(identifier);
    groups_.members.push_back({static_cast<std::uint32_t>(unresolved_group_members_.size()),
                               static_cast<std::uint32_t>(members.size())});
    for (auto const& member : members) {
        unresolved_group_members_.push_back(member);
    }
    names_.emplace(identifier, id);
}
void Project::resolve_groups() {
    groups_.targets.reserve(unresolved_group_members_.size());
    for (auto const member : unresolved_group_members_) {
        auto const found{names_.find(member)};
        if (found == names_.end()) {
            throw std::invalid_argument{"Unknown Lispb target or group: " +
                                        std::string{strings_->view(member)}};
        }
        groups_.targets.push_back(found->second);
    }
    unresolved_group_members_.clear();
    auto const count{groups_.names.size()};
    for (std::size_t index{}; index < count; ++index) {
        std::vector<TargetId> active;
        std::vector<TargetId> result;
        expand(TargetId{static_cast<std::uint32_t>(TargetKind::group) << 24 |
                        static_cast<std::uint32_t>(index)},
               active,
               result);
    }
}
void Project::expand(TargetId const target,
                     std::vector<TargetId>& active,
                     std::vector<TargetId>& result) const {
    if (target.kind() != TargetKind::group) {
        if (!std::ranges::contains(result, target)) {
            result.push_back(target);
        }
        return;
    }
    if (std::ranges::contains(active, target)) {
        throw std::invalid_argument{"Lispb target group cycle at: " +
                                    std::string{target_name(target)}};
    }
    active.push_back(target);
    auto const range{groups_.members[target.index()]};
    auto const end{range.offset + range.count};
    for (auto index{range.offset}; index < end; ++index) {
        expand(groups_.targets[index], active, result);
    }
    active.pop_back();
}
auto Project::expand(std::string_view const name) const -> std::vector<TargetId> {
    auto const found{names_.find(strings_->find(name))};
    if (found == names_.end()) {
        throw std::invalid_argument{"Unknown Lispb target or group: " + std::string{name}};
    }
    std::vector<TargetId> active;
    std::vector<TargetId> result;
    expand(found->second, active, result);
    return result;
}

}
