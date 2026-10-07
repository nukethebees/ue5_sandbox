#pragma once
#include <ioj/ascii.h>
#include <ioj/levels/authoring/grammar.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/s7/ast.h>

#include <algorithm>
#include <format>
#include <span>

namespace ioj::levels::authoring::detail {
struct Field {
    Property property{};
    s7::NodeIndex value{};
    std::string node_path{};
};
// Share scalar checks and record mechanics; domain dispatch stays in each parser.
class AstParser {
  public:
    explicit AstParser(s7::Ast const& ast)
        : ast_{ast} {}
    [[nodiscard]] auto record(s7::NodeIndex node, RecordKind kind, std::string const& path)
        -> std::vector<Field>;
    [[nodiscard]] auto list(s7::NodeIndex node, std::string const& path)
        -> std::span<s7::NodeIndex const>;
    [[nodiscard]] auto symbol(s7::NodeIndex node, std::string const& path) -> std::string;
    [[nodiscard]] auto string(s7::NodeIndex node, std::string const& path) -> std::string;
    [[nodiscard]] auto number(s7::NodeIndex node, std::string const& path) -> double;
    [[nodiscard]] auto integer(s7::NodeIndex node, std::string const& path) -> std::int32_t;
    [[nodiscard]] auto float_number(s7::NodeIndex node, std::string const& path) -> float;
    void require(std::span<Field const> fields, Property property, std::string const& path);
    void unknown(Field const& field);
    void error(DiagnosticCode code, std::string path, std::string message);
    [[nodiscard]] auto errors() const -> Diagnostics const& { return errors_; }
    [[nodiscard]] auto take_errors() -> Diagnostics { return std::move(errors_); }

    template <typename Id>
    [[nodiscard]] auto ids(s7::NodeIndex const node, std::string const& path) -> std::vector<Id> {
        std::vector<Id> result;
        auto const values{list(node, path)};
        auto const count{values.size()};
        for (std::size_t index{}; index < count; ++index) {
            result.emplace_back(symbol(values[index], std::format("{}[{}]", path, index)));
        }
        return result;
    }
  private:
    s7::Ast const& ast_;
    Diagnostics errors_{};
};
}
