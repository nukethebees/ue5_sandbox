#include "SandboxEditor/material/MaterialSourceHash.h"

namespace material_synth {
namespace {

constexpr uint32 round_constants[]{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

auto rotate_right(uint32 const value, uint32 const amount) -> uint32 {
    return (value >> amount) | (value << (32 - amount));
}

}

auto sha256(TConstArrayView<uint8> const bytes) -> FString {
    TArray<uint8> padded{bytes};
    uint64 const bit_count{static_cast<uint64>(bytes.Num()) * 8};
    padded.Add(0x80);
    while (padded.Num() % 64 != 56) {
        padded.Add(0);
    }
    for (int32 shift{56}; shift >= 0; shift -= 8) {
        padded.Add(static_cast<uint8>(bit_count >> shift));
    }

    uint32 hash[]{0x6a09e667,
                  0xbb67ae85,
                  0x3c6ef372,
                  0xa54ff53a,
                  0x510e527f,
                  0x9b05688c,
                  0x1f83d9ab,
                  0x5be0cd19};
    for (int32 offset{}; offset < padded.Num(); offset += 64) {
        uint32 words[64]{};
        for (int32 index{}; index < 16; ++index) {
            auto const byte{offset + index * 4};
            words[index] = static_cast<uint32>(padded[byte]) << 24 |
                           static_cast<uint32>(padded[byte + 1]) << 16 |
                           static_cast<uint32>(padded[byte + 2]) << 8 |
                           static_cast<uint32>(padded[byte + 3]);
        }
        for (int32 index{16}; index < 64; ++index) {
            auto const first{rotate_right(words[index - 15], 7) ^
                             rotate_right(words[index - 15], 18) ^ (words[index - 15] >> 3)};
            auto const second{rotate_right(words[index - 2], 17) ^
                              rotate_right(words[index - 2], 19) ^ (words[index - 2] >> 10)};
            words[index] = words[index - 16] + first + words[index - 7] + second;
        }

        auto a{hash[0]};
        auto b{hash[1]};
        auto c{hash[2]};
        auto d{hash[3]};
        auto e{hash[4]};
        auto f{hash[5]};
        auto g{hash[6]};
        auto h{hash[7]};
        for (int32 index{}; index < 64; ++index) {
            auto const sum1{rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25)};
            auto const choice{(e & f) ^ (~e & g)};
            auto const temporary1{h + sum1 + choice + round_constants[index] + words[index]};
            auto const sum0{rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22)};
            auto const majority{(a & b) ^ (a & c) ^ (b & c)};
            auto const temporary2{sum0 + majority};
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    FString result;
    result.Reserve(64);
    for (auto const value : hash) {
        result += FString::Printf(TEXT("%08x"), value);
    }
    return result;
}

}
