#pragma once

#include <codegen/ast/indices.h>

#include <cassert>
#include <cstdint>

namespace lispb::codegen {

struct StandardTypes {
    struct Pmr {
        CppTypeId memory_resource;
    };

    // Select the smallest fixed-width integer that holds the requested bits.
    auto integer(std::uint32_t bits, bool is_signed = false) const -> CppTypeId {
        assert(bits > 0 && bits <= 64);
        if (bits <= 8) {
            return is_signed ? i8 : u8;
        }
        if (bits <= 16) {
            return is_signed ? i16 : u16;
        }
        if (bits <= 32) {
            return is_signed ? i32 : u32;
        }
        return is_signed ? i64 : u64;
    }
    auto is_signed_integer(CppTypeId type) const -> bool {
        return type == i8 || type == i16 || type == i32 || type == i64;
    }

    CppTypeId u8;
    CppTypeId u16;
    CppTypeId u32;
    CppTypeId u64;
    CppTypeId i8;
    CppTypeId i16;
    CppTypeId i32;
    CppTypeId i64;
    CppTypeId f32;
    CppTypeId f64;
    CppTypeId bool_;
    CppTypeId void_;
    CppTypeId char_;
    CppTypeId unsigned_char;
    CppTypeId int_;
    CppTypeId size_t;
    CppTypeId ptrdiff_t;
    CppTypeId uintptr_t;
    CppTypeId intptr_t;
    CppTypeId byte;
    CppTypeId string;
    CppTypeId string_view;
    CppTypeId array;
    CppTypeId span;
    CppTypeId vector;
    CppTypeId optional;
    CppTypeId unexpected;
    CppTypeId variant;
    CppTypeId tuple;
    CppTypeId numeric_limits;
    CppTypeId conditional_t;
    CppTypeId remove_cvref_t;
    CppTypeId remove_const_t;
    CppTypeId same_as;
    CppTypeId is_const_v;
    CppTypeId is_constructible_v;
    CppTypeId is_same_v;
    CppTypeId is_floating_point_v;
    CppTypeId is_trivially_copyable_v;

    Pmr pmr;
};

} // namespace lispb::codegen
