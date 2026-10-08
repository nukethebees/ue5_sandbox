#pragma once
#include <memory>
namespace ioj::s7::detail {
// Own an opaque OS handle; the platform implementation supplies its close operation.
struct FileHandleCloser {
    void operator()(void* handle) const;
};
using FileHandle = std::unique_ptr<void, FileHandleCloser>;
}
