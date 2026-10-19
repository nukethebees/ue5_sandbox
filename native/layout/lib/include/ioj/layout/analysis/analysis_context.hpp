#pragma once
#include <ioj/layout/diagnostic.hpp>
namespace lispb::schema {
class TypeGraph;
}
namespace ioj::layout {
class Document;
struct AnalysisStorage;
struct AnalysisContext {
    AnalysisContext(Document const& document, AnalysisStorage& output);
    void report(Diagnostic const& diagnostic) const;
    Document const& document;
    lispb::schema::TypeGraph const& types;
    AnalysisStorage& storage;
};
}
