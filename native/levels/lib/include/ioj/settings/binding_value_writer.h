#pragma once

#include <ioj/input/logical/flight_bindings.h>
#include <ioj/s7/writer.h>

#include <string>

namespace ioj::s7 {
template <>
struct TypeWriter<FlightBindings> {
    static auto write(SExprIR& ir, FlightBindings const& bindings, FlightBindings const*) -> Expr {
        auto list{ir.form("list")};
        auto const defaults{default_flight_bindings()};
        for (auto mapping : EnumTraits<FlightMapping>::values) {
            if (bindings[mapping] != defaults[mapping]) {
                auto pair{ir.form("list")};
                pair.add(ir.string(std::string{to_serialized_string(mapping)}));
                pair.add(ir.string(std::string{to_serialized_string(bindings[mapping])}));
                list.add(pair.build());
            }
        }
        return list.build();
    }
};
}
