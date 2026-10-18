#pragma once

#include <string_view>

namespace ioj::layout {
// Loading borrows these views for the duration of the call. Views returned by sources()
// expire when any operation grows the owning EditorSession's string store.
struct DocumentSource {
    std::string_view path;
    std::string_view text;
};
}
