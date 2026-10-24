#pragma once
#include "reader_diagnostics.h"
#include <ioj/constrained_strings.h>
#include <ioj/levels/authoring/grammar.h>
#include <ioj/levels/authoring/identifier_reader.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/entity_archetype.h>
#include <ioj/levels/team.h>

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

    template <typename T>
    [[nodiscard]] auto read(s7::NodeIndex node, std::string_view path) -> T {
        return take(s7::read<T>(s7::Reader{ast_}, node, path));
    }
    [[nodiscard]] auto record(s7::NodeIndex node, RecordKind kind, std::string_view const node_path)
        -> std::vector<Field>;
    [[nodiscard]] auto list(s7::NodeIndex node, std::string_view const node_path)
        -> std::span<s7::NodeIndex const>;
    [[nodiscard]] auto symbol(s7::NodeIndex node, std::string_view const node_path) -> std::string;
    [[nodiscard]] auto nonempty_symbol(s7::NodeIndex node, std::string_view node_path)
        -> std::optional<NonEmptyString>;
    [[nodiscard]] auto string(s7::NodeIndex node, std::string_view const node_path) -> std::string;
    [[nodiscard]] auto number(s7::NodeIndex node, std::string_view const node_path) -> double;
    [[nodiscard]] auto integer(s7::NodeIndex node, std::string_view const node_path)
        -> std::int32_t;
    [[nodiscard]] auto float_number(s7::NodeIndex node, std::string_view const node_path) -> float;
    [[nodiscard]] auto team(s7::NodeIndex node, std::string_view node_path) -> Team;
    [[nodiscard]] auto archetype(s7::NodeIndex node, std::string_view node_path) -> EntityArchetype;
    void
        require(std::span<Field const> fields, Property property, std::string_view const node_path);
    void unknown(Field const& field);
    void error(DiagnosticCode code, std::string_view const node_path, std::string message);
    [[nodiscard]] auto errors() const -> Diagnostics const& { return errors_; }
    [[nodiscard]] auto take_errors() -> Diagnostics { return std::move(errors_); }

    template <typename Id>
    [[nodiscard]] auto identifier(s7::NodeIndex const node, std::string_view const node_path)
        -> Id {
        auto const value{nonempty_symbol(node, node_path)};
        return value ? Id{value->get()} : Id{};
    }

    template <typename Id>
    [[nodiscard]] auto ids(s7::NodeIndex const node, std::string_view const node_path)
        -> std::vector<Id> {
        return read<std::vector<Id>>(node, node_path);
    }
  private:
    template <typename T>
    auto take(s7::ReadResult<T> result) -> T {
        if (result) {
            return std::move(*result);
        }
        for (auto& diagnostic : result.error()) {
            error(diagnostic_code(diagnostic.code),
                  diagnostic.node_path,
                  std::move(diagnostic.message));
        }
        return {};
    }

    s7::Ast const& ast_;
    Diagnostics errors_{};
};
}
