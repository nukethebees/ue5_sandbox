#pragma once

#include <ioj/layout/frozen_registry.hpp>
#include <ioj/layout/source_id.hpp>

#include <lispb/indices.h>

#include <cstdint>
#include <map>
#include <vector>

namespace ioj::layout {
struct DocumentIoState {
    using StringId = lispb::StringId;

    FrozenRegistry registry;
    StringId project_path;
    StringId project_root;
    StringId target_name;
    StringId registry_path;
    StringId project_text;
    std::uint32_t sources_begin{};
    std::uint32_t sources_end{};
    StringId source_comments;
    StringId saved_project_text;
    std::vector<StringId> saved_order;
    std::map<StringId, StringId> saved_sources;
    std::map<StringId, StringId> persisted_sources;
    std::map<SourceId, StringId> saved_paths;
};
}
