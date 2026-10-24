#include <ioj/s7/reader.h>

namespace ioj::s7 {
auto Reader::list(NodeIndex const node, std::string_view const path) const
    -> ReadResult<std::span<NodeIndex const>> {
    if (ast_.node(node).kind != NodeKind::List) {
        return read_error(AstErrorCode::InvalidType, path, "Expected a list");
    }
    return ast_.children(node);
}
auto Reader::symbol(NodeIndex const node, std::string_view const path) const
    -> ReadResult<std::string> {
    if (ast_.node(node).kind != NodeKind::Symbol) {
        return read_error(AstErrorCode::InvalidType, path, "Expected a symbol");
    }
    std::string result{ast_.text(node)};
    std::ranges::transform(result, result.begin(), ioj::tolower);
    return result;
}
auto Reader::number(NodeIndex const node, std::string_view const path) const -> ReadResult<double> {
    switch (ast_.node(node).kind) {
        case NodeKind::Integer:
            return static_cast<double>(ast_.integer(node));
        case NodeKind::Real:
            return ast_.real(node);
        case NodeKind::Ratio: {
            auto const ratio{ast_.ratio(node)};
            if (ratio.denominator > 0) {
                return static_cast<double>(ratio.numerator) /
                       static_cast<double>(ratio.denominator);
            }
            return read_error(
                AstErrorCode::InvalidNumber, path, "Expected a positive ratio denominator");
        }
        default:
            return read_error(AstErrorCode::InvalidType, path, "Expected a real number");
    }
}
}
