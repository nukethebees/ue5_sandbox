#pragma once

#include <ioj/s7/detail/script_loader.h>
#include <ioj/s7/value.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ioj::s7::detail {
[[nodiscard]] auto prepare_loader_factory(Scheme* scheme) -> Value;

// The runtime and loader outlive this registration. Serialize all access to one
// interpreter, including destruction; the registry mutex only protects lookup
// across distinct interpreters, never evaluation or the returned context.
class ScriptBindings final {
  public:
    ScriptBindings(Scheme* scheme, ScriptLoader& loader, std::optional<std::string> root);
    ~ScriptBindings();
    ScriptBindings(ScriptBindings const&) = delete;
    auto operator=(ScriptBindings const&) -> ScriptBindings& = delete;

    void install(Value factory);
  private:
    static auto context(Scheme* scheme) -> ScriptBindings&;
    static auto resolve_callback(Scheme* scheme, Value arguments) -> Value;
    static auto complete_callback(Scheme* scheme, Value arguments) -> Value;
    static auto abort_callback(Scheme* scheme, Value arguments) -> Value;
    auto resolve(Value path) -> Value;

    inline static std::mutex instances_mutex_;
    inline static std::unordered_map<Scheme*, ScriptBindings*> instances_;
    Scheme* scheme_;
    ScriptLoader& loader_;
    std::optional<std::string> root_;
};
}
