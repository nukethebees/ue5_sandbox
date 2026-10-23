#pragma once

struct RestrictedLeaf {
#if defined(CODEGEN_BAD_DEFAULT)
    RestrictedLeaf() noexcept(false) {}
#else
    RestrictedLeaf() noexcept = default;
#endif
#if defined(CODEGEN_BAD_COPY)
    RestrictedLeaf(RestrictedLeaf const&) {}
#endif
#if defined(CODEGEN_BAD_DESTRUCTOR)
    ~RestrictedLeaf() {}
#endif
    int value{};
};
