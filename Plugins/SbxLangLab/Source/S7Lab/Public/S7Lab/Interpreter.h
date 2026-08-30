#pragma once

#include <Containers/StringView.h>
#include <Containers/UnrealString.h>
#include <Templates/UniquePtr.h>

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
  private:
    class FImpl;
    TUniquePtr<FImpl> impl_;
};
}
