#pragma once

namespace ioj {
struct CpuSimdCapabilities {
    bool available{};
    bool sse{};
    bool sse2{};
    bool sse3{};
    bool ssse3{};
    bool sse4_1{};
    bool sse4_2{};
    bool sse4a{};
    bool avx{};
    bool avx2{};
    bool avx_vnni{};
    bool avx512_f{};
    bool avx512_cd{};
    bool avx512_bw{};
    bool avx512_dq{};
    bool avx512_vl{};
    bool amx_tile{};
    bool amx_bf16{};
    bool amx_int8{};
    bool amx_fp16{};
};

auto query_cpu_simd_capabilities() -> CpuSimdCapabilities;
}
