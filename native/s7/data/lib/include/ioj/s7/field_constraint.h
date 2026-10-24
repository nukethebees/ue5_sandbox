#pragma once

#include <ioj/constrained_value.h>
#include <ioj/s7/ast_result.h>

#include <string_view>

namespace ioj::s7 {
namespace constraints {
template <typename T, typename Predicate>
auto check(T const& value, Predicate predicate) -> bool {
    return predicate(value);
}
template <typename T, ConstrainedValuePolicy Policy, typename Predicate>
auto check(ConstrainedValue<T, Policy> const& value, Predicate predicate) -> bool {
    return predicate(value.get_ref());
}
template <typename T, typename Predicate>
auto check(std::optional<T> const& value, Predicate predicate) -> bool {
    return !value || check(*value, predicate);
}
}

template <typename Predicate>
struct FieldConstraint {
    template <typename T>
    auto operator()(T const& value) const -> bool {
        return constraints::check(value, predicate);
    }

    template <typename T>
    void validate(T const& value, std::string_view path, AstDiagnostics& errors) const {
        if (!(*this)(value)) {
            errors.push_back({code, std::string{path}, std::string{message}});
        }
    }

    Predicate predicate;
    std::string_view message;
    AstErrorCode code{AstErrorCode::InvalidNumber};
};
}
