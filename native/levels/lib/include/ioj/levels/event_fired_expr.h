#pragma once
#include <ioj/levels/expr_type.h>

#include <string>
namespace ioj::levels {
struct EventFiredExpr {
    auto operator==(EventFiredExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::EventFired};
    std::string signal;
};
}
