#include <ioj/levels/diagnostics.h>

#include <ioj/files.h>

#include <format>

namespace ioj::levels {
auto format_diagnostics(Diagnostics const& diagnostics) -> std::string {
    std::string result;
    for (auto const& diagnostic : diagnostics) {
        if (!result.empty()) {
            result += '\n';
        }
        if (!diagnostic.source_path.empty()) {
            std::format_to(
                std::back_inserter(result), "{}: ", ioj::path_to_utf8(diagnostic.source_path));
        }
        std::format_to(
            std::back_inserter(result), "{}: {}", diagnostic.node_path, diagnostic.message);
    }
    return result;
}
}
