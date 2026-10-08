#pragma once

namespace ioj::s7 {
enum class AstErrorCode {
    EvaluationFailed,
    UnsupportedValue,
    ImproperList,
    CyclicStructure,
    LimitExceeded
};
}
