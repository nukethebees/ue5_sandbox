#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::s7 {
using NodeIndex = std::uint32_t;
enum class NodeKind { List, Symbol, Keyword, String, Boolean, Integer, Ratio, Real };

struct NodeRange {
    std::uint32_t offset{};
    std::uint32_t count{};
};
struct Ratio {
    std::int64_t numerator{};
    std::int64_t denominator{1};
};
struct Node {
    NodeKind kind{NodeKind::List};
    union {
        NodeRange range{};
        bool boolean;
        std::int64_t integer;
        Ratio ratio;
        double real;
    };
};

// All ranges and indices belong to this object. Accessors require valid indices and kinds;
// their returned spans/views remain valid only until the owning storage is modified.
struct Ast {
    std::vector<Node> nodes{};
    std::vector<NodeIndex> child_indices{};
    std::string text_bytes{};
    NodeIndex root{};

    [[nodiscard]] auto node(NodeIndex index) const -> Node const&;
    [[nodiscard]] auto children(NodeIndex index) const -> std::span<NodeIndex const>;
    [[nodiscard]] auto text(NodeIndex index) const -> std::string_view;
};

enum class AstErrorCode {
    EvaluationFailed,
    UnsupportedValue,
    ImproperList,
    CyclicStructure,
    LimitExceeded
};
struct AstDiagnostic {
    AstErrorCode code{};
    std::string node_path{};
    std::string message{};
};
using AstDiagnostics = std::vector<AstDiagnostic>;
using AstResult = std::expected<Ast, AstDiagnostics>;
struct AstLimits {
    std::uint32_t max_depth{256};
    std::uint32_t max_nodes{1'000'000};
    std::uint32_t max_child_indices{2'000'000};
    std::uint32_t max_text_bytes{64 * 1024 * 1024};
};
}
