#pragma once

#include <ioj/layout/frozen_registry.hpp>
#include <ioj/layout/source_id.hpp>

#include <lispb/indices.h>

#include <cstdint>
#include <map>
#include <vector>

namespace ioj::layout {
struct DocumentIoState {
    FrozenRegistry registry;
    lispb::StringId project_path;
    lispb::StringId project_root;
    lispb::StringId target_name;
    lispb::StringId registry_path;
    lispb::StringId project_text;
    std::uint32_t sources_begin{};
    std::uint32_t sources_end{};
    lispb::StringId source_comments;
    lispb::StringId saved_project_text;
    std::vector<lispb::StringId> saved_order;
    std::map<lispb::StringId, lispb::StringId> saved_sources;
    std::map<lispb::StringId, lispb::StringId> persisted_sources;
    std::map<SourceId, lispb::StringId> saved_paths;
};
}
