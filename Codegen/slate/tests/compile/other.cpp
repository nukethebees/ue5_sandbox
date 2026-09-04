#include "support.h"

#include "CompileSmoke/Widgets.slate.generated.h"
#include "CompileSmoke/Host.slate.generated.h"

#include <memory>

namespace CompileSmoke {

auto other_empty_function() -> EmptyFunction {
    return &SlateGenerated::CompileSmoke::Widgets::empty;
}

auto invoke_other_callback() -> int {
    auto widget{SlateGenerated::CompileSmoke::Widgets::callback(
        [value = std::make_unique<int>(29)] { return *value; })};
    return widget.invoke();
}

}
