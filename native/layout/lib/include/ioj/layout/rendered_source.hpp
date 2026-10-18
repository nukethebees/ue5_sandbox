#pragma once

#include <ioj/layout/source_id.hpp>

#include <string>

namespace ioj::layout {
struct RenderedSource {
    SourceId id;
    std::string path;
    std::string text;
};
}
