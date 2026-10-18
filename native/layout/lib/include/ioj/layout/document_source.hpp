#pragma once

#include <string_view>

namespace ioj::layout {
// Source text is borrowed only for the duration of loading.
struct DocumentSource {
    std::string_view path;
    std::string_view text;
};
}
