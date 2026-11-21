#pragma once

#include <codegen/ast/cpp_ir.h>

#include <cstddef>
#include <span>
#include <vector>

namespace lispb::codegen::detail {
// Members are sorted by their numeric enum values and contain no aliases.
auto enum_group_predicates(CppIR& ir,
                           Expr value,
                           std::span<std::size_t const> members,
                           auto member,
                           auto consecutive,
                           auto starts_at_minimum) -> std::vector<Expr> {
    std::vector<Expr> predicates;
    auto const count{members.size()};
    for (std::size_t first{}; first < count;) {
        auto last{first};
        while (last + 1 < count && consecutive(members[last], members[last + 1])) {
            ++last;
        }
        auto const lower{member(members[first])};
        if (first == last) {
            predicates.push_back(ir.binary(BinaryOperator::equal, value, lower));
        } else {
            auto const maximum{ir.binary(BinaryOperator::less_equal, value, member(members[last]))};
            predicates.push_back(
                starts_at_minimum(members[first])
                    ? maximum
                    : ir.binary(BinaryOperator::logical_and,
                                ir.binary(BinaryOperator::greater_equal, value, lower),
                                maximum));
        }
        first = last + 1;
    }
    return predicates;
}
}
