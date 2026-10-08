#pragma once

#include <ioj/s7/node.h>
#include <ioj/s7/node_index.h>
#include <ioj/s7/ratio.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::s7 {
// All ranges and indices belong to this object. Accessors require valid indices and kinds;
// their returned spans/views remain valid only until the owning storage is modified.
struct Ast {
    std::vector<Node> nodes{};
    std::vector<NodeIndex> child_indices{};
    std::string text_bytes{};
    std::vector<std::uint8_t> booleans{};
    std::vector<std::int64_t> integers{};
    std::vector<Ratio> ratios{};
    std::vector<double> reals{};
    NodeIndex root{};

    [[nodiscard]] auto node(NodeIndex index) const -> Node const&;
    [[nodiscard]] auto children(NodeIndex index) const -> std::span<NodeIndex const>;
    [[nodiscard]] auto text(NodeIndex index) const -> std::string_view;
    [[nodiscard]] auto boolean(NodeIndex index) const -> bool;
    [[nodiscard]] auto integer(NodeIndex index) const -> std::int64_t;
    [[nodiscard]] auto ratio(NodeIndex index) const -> Ratio;
    [[nodiscard]] auto real(NodeIndex index) const -> double;
};

}
