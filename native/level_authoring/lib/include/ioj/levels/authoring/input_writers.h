#pragma once

#include <ioj/levels/authoring/identifier_writer.h>
#include <ioj/levels/logic_definition.h>
#include <ioj/settings/settings_values_writer.h>

namespace ioj::s7 {
template <>
struct TypeWriter<levels::LogicDefinition> {
    static auto write(SExprIR& ir,
                      levels::LogicDefinition const& value,
                      levels::LogicDefinition const*) -> Expr;
    static auto omitted(levels::LogicDefinition const& value) -> bool {
        return value == levels::LogicDefinition{};
    }
    static auto equivalent(levels::LogicDefinition const& value,
                           levels::LogicDefinition const& base) -> bool;
};
}
