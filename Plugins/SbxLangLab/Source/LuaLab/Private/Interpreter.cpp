#include "LuaLab/Interpreter.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "Math/Vector.h"

namespace {
void open_library(lua_State& state, char const* const name, lua_CFunction const open_function) {
    luaL_requiref(&state, name, open_function, 1);
    lua_pop(&state, 1);
}

int vector_length_squared(lua_State* const state) {
    auto const argument_count{lua_gettop(state)};
    if (argument_count != 3) {
        return luaL_error(
            state, "sbx_vector_length_squared expects 3 arguments, received %d", argument_count);
    }

    auto const vector{FVector{
        luaL_checknumber(state, 1), luaL_checknumber(state, 2), luaL_checknumber(state, 3)}};
    lua_pushnumber(state, vector.SizeSquared());
    return 1;
}

int uppercase(lua_State* const state) {
    auto const argument_count{lua_gettop(state)};
    if (argument_count != 1) {
        return luaL_error(state, "sbx_uppercase expects 1 argument, received %d", argument_count);
    }

    size_t length{};
    auto const* const input{luaL_checklstring(state, 1, &length)};
    auto const unreal_string{FString{UTF8_TO_TCHAR(input)}};
    auto const uppercase_string{unreal_string.ToUpper()};
    auto const utf8_string{StringCast<UTF8CHAR>(*uppercase_string)};
    lua_pushlstring(state,
                    reinterpret_cast<char const*>(utf8_string.Get()),
                    static_cast<size_t>(utf8_string.Length()));
    return 1;
}

auto read_error(lua_State& state) -> FString {
    auto const* const error{lua_tostring(&state, -1)};
    return error != nullptr ? FString{UTF8_TO_TCHAR(error)} : TEXT("Unknown Lua error.");
}

auto read_results(lua_State& state) -> FString {
    TArray<FString> values;
    auto const result_count{lua_gettop(&state)};
    values.Reserve(result_count);
    for (int32 result_index{1}; result_index <= result_count; ++result_index) {
        size_t length{};
        auto const* const value{luaL_tolstring(&state, result_index, &length)};
        values.Emplace(UTF8_TO_TCHAR(value));
        lua_pop(&state, 1);
    }

    return FString::Join(values, TEXT("\t"));
}
}

namespace LuaLab {
class FInterpreter::FImpl final {
  public:
    FImpl()
        : state_{luaL_newstate()} {
        if (state_ == nullptr) {
            return;
        }

        open_library(*state_, LUA_GNAME, luaopen_base);
        open_library(*state_, LUA_COLIBNAME, luaopen_coroutine);
        open_library(*state_, LUA_TABLIBNAME, luaopen_table);
        open_library(*state_, LUA_STRLIBNAME, luaopen_string);
        open_library(*state_, LUA_MATHLIBNAME, luaopen_math);
        open_library(*state_, LUA_UTF8LIBNAME, luaopen_utf8);

        lua_register(state_, "sbx_vector_length_squared", vector_length_squared);
        lua_register(state_, "sbx_uppercase", uppercase);
    }

    ~FImpl() {
        if (state_ != nullptr) {
            lua_close(state_);
        }
    }

    [[nodiscard]] auto evaluate(FStringView const script) -> FEvaluationResult {
        if (state_ == nullptr) {
            return {.error = TEXT("Lua interpreter initialization failed.")};
        }

        lua_settop(state_, 0);
        auto const script_string{FString{script}};
        auto const utf8_script{StringCast<UTF8CHAR>(*script_string)};
        auto const* const bytes{reinterpret_cast<char const*>(utf8_script.Get())};
        auto const byte_count{static_cast<size_t>(utf8_script.Length())};

        auto status{luaL_loadbuffer(state_, bytes, byte_count, "SbxLangLab")};
        if (status == LUA_OK) {
            status = lua_pcall(state_, 0, LUA_MULTRET, 0);
        }

        if (status != LUA_OK) {
            auto error{read_error(*state_)};
            lua_settop(state_, 0);
            return {.error = MoveTemp(error)};
        }

        auto value{read_results(*state_)};
        lua_settop(state_, 0);
        return {.succeeded = true, .value = MoveTemp(value)};
    }
  private:
    lua_State* state_{};
};

FInterpreter::FInterpreter()
    : impl_{MakeUnique<FImpl>()} {}

FInterpreter::~FInterpreter() = default;

auto FInterpreter::evaluate(FStringView const script) -> FEvaluationResult {
    return impl_->evaluate(script);
}
}
