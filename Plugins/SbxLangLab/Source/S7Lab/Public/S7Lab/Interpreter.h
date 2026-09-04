#pragma once

#include <S7Lab/NativeApi.h>

#include <Containers/StringView.h>
#include <Containers/UnrealString.h>
#include <Templates/Function.h>
#include <Templates/UniquePtr.h>

struct s7_scheme;

namespace S7Lab {
struct FEvaluationResult {
    bool succeeded{};
    FString value;
    FString error;
};

class S7LAB_API FInterpreter final {
  public:
    FInterpreter();
    ~FInterpreter();

    FInterpreter(FInterpreter const&) = delete;
    auto operator=(FInterpreter const&) -> FInterpreter& = delete;

    [[nodiscard]] auto evaluate(FStringView expression) -> FEvaluationResult;
    [[nodiscard]] auto evaluate_value(FStringView expression,
                                      TFunctionRef<void(s7_scheme&, native::FValue)> consume_value)
        -> FEvaluationResult;
    [[nodiscard]] auto native_handle() noexcept -> s7_scheme*;
  private:
    class FImpl;
    TUniquePtr<FImpl> impl_;
};
}
