#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/s7/ast_result.h>

namespace ioj::levels::authoring::detail {
inline auto diagnostic_code(s7::AstErrorCode const code) -> DiagnosticCode {
    switch (code) {
        case s7::AstErrorCode::EvaluationFailed:
            return DiagnosticCode::ScriptEvaluationFailed;
        case s7::AstErrorCode::UnsupportedValue:
            return DiagnosticCode::UnsupportedValue;
        case s7::AstErrorCode::ImproperList:
            return DiagnosticCode::ImproperList;
        case s7::AstErrorCode::CyclicStructure:
            return DiagnosticCode::CyclicStructure;
        case s7::AstErrorCode::LimitExceeded:
            return DiagnosticCode::LimitExceeded;
        case s7::AstErrorCode::ExpectedRecord:
            return DiagnosticCode::ExpectedRecord;
        case s7::AstErrorCode::ExpectedKeyword:
            return DiagnosticCode::ExpectedKeyword;
        case s7::AstErrorCode::UnknownProperty:
            return DiagnosticCode::UnknownProperty;
        case s7::AstErrorCode::DuplicateProperty:
            return DiagnosticCode::DuplicateProperty;
        case s7::AstErrorCode::MissingProperty:
            return DiagnosticCode::MissingProperty;
        case s7::AstErrorCode::MissingValue:
            return DiagnosticCode::MissingValue;
        case s7::AstErrorCode::InvalidType:
            return DiagnosticCode::InvalidType;
        case s7::AstErrorCode::InvalidNumber:
            return DiagnosticCode::InvalidNumber;
        case s7::AstErrorCode::UnknownSymbol:
            return DiagnosticCode::UnknownSymbol;
        case s7::AstErrorCode::InvalidSymbol:
            return DiagnosticCode::InvalidSymbol;
    }
    std::unreachable();
}
inline auto diagnostics(s7::AstDiagnostics errors) -> Diagnostics {
    Diagnostics result;
    result.reserve(errors.size());
    for (auto& error : errors) {
        result.push_back(
            {diagnostic_code(error.code), std::move(error.node_path), std::move(error.message)});
    }
    return result;
}
}
