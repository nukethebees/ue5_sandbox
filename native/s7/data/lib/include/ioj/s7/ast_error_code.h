#pragma once

namespace ioj::s7 {
enum class AstErrorCode {
    FileOpenFailed,
    FileReadFailed,
    EvaluationFailed,
    UnsupportedValue,
    ImproperList,
    CyclicStructure,
    LimitExceeded,
    ExpectedRecord,
    ExpectedKeyword,
    UnknownProperty,
    DuplicateProperty,
    MissingProperty,
    MissingValue,
    InvalidType,
    InvalidNumber,
    UnknownSymbol,
    InvalidSymbol
};
}
