#include <ioj/layout/fact_origin.hpp>

namespace ioj::layout {

auto fact_origin_name(FactOrigin const origin) -> std::string_view {
    switch (origin) {
        case FactOrigin::unspecified:
            return "unspecified";
        case FactOrigin::target_abi:
            return "target-abi";
        case FactOrigin::compiler_probe:
            return "compiler-probe";
        case FactOrigin::manual_assumption:
            return "manual-assumption";
        case FactOrigin::derived:
            return "derived";
    }
    return "unspecified";
}

} // namespace ioj::layout
