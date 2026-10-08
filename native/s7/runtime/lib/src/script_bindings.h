#pragma once

#include "script_loader.h"

#include <s7.h>

#include <optional>
#include <string>

namespace ioj::s7::detail {
[[nodiscard]] auto prepare_loader_factory(s7_scheme* scheme) -> s7_pointer;

// The runtime and loader outlive the private Scheme closure that captures this instance.
// Serialize evaluation and destruction of each interpreter.
class ScriptBindings {
  public:
    ScriptBindings(s7_scheme* scheme, ScriptLoader& loader, std::optional<std::string> root);
    ScriptBindings(ScriptBindings const&) = delete;
    auto operator=(ScriptBindings const&) -> ScriptBindings& = delete;

    void install(s7_pointer factory);
  private:
    static auto context(s7_pointer arguments) -> ScriptBindings&;
    static auto resolve_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    static auto complete_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    static auto abort_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    auto resolve(s7_pointer path) -> s7_pointer;

    s7_scheme* scheme_;
    ScriptLoader& loader_;
    std::optional<std::string> root_;
};
}
