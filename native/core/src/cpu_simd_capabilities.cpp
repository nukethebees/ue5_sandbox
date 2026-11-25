#include <ioj/game_capabilities.h>

#include <cpu_features_macros.h>
#ifdef CPU_FEATURES_ARCH_X86
#include <cpuinfo_x86.h>
#endif

namespace ioj {
void query_cpu_capabilities(GameCapabilities& result) {
#ifdef CPU_FEATURES_ARCH_X86
    auto const cpu{cpu_features::GetX86Info()};
    result.cpu_vendor = cpu.vendor;
    result.cpu_brand = cpu.brand_string;
    auto const& features{cpu.features};
    result.cpu_simd = CpuSimdCapabilities{.available = true,
                                          .sse = features.sse != 0,
                                          .sse2 = features.sse2 != 0,
                                          .sse3 = features.sse3 != 0,
                                          .ssse3 = features.ssse3 != 0,
                                          .sse4_1 = features.sse4_1 != 0,
                                          .sse4_2 = features.sse4_2 != 0,
                                          .sse4a = features.sse4a != 0,
                                          .avx = features.avx != 0,
                                          .avx2 = features.avx2 != 0,
                                          .avx_vnni = features.avx_vnni != 0,
                                          .avx512_f = features.avx512f != 0,
                                          .avx512_cd = features.avx512cd != 0,
                                          .avx512_bw = features.avx512bw != 0,
                                          .avx512_dq = features.avx512dq != 0,
                                          .avx512_vl = features.avx512vl != 0,
                                          .amx_tile = features.amx_tile != 0,
                                          .amx_bf16 = features.amx_bf16 != 0,
                                          .amx_int8 = features.amx_int8 != 0,
                                          .amx_fp16 = features.amx_fp16 != 0};
#else
    result.cpu_simd = {};
#endif
}
}
