#pragma once

#include <ioj/ascii.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/objective_name.h>
#include <ioj/s7/writer.h>

namespace ioj::s7 {
template <typename Id>
    requires (levels::Identifier<Id> || std::same_as<Id, levels::ObjectiveName>)
struct TypeWriter<Id> {
    static auto write(SExprIR& ir, Id const& value, Id const*) -> Expr {
        auto const& symbol{value.value};
        if (!symbol.empty() && symbol.front() >= 'a' && symbol.front() <= 'z' &&
            std::ranges::all_of(symbol, is_lowercase_symbol_character)) {
            return ir.quote(symbol);
        }
        return ir.form("string->symbol", {ir.string(symbol)}).build();
    }
    static auto omitted(Id const& value) -> bool { return value.value.empty(); }
};
}
