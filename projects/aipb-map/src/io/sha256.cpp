#include "sha256.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <vector>

namespace aipb::map::io {
namespace {
constexpr std::array<std::uint32_t, 64> kConstants {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

std::uint32_t rotate(std::uint32_t value, unsigned distance) {
    return (value >> distance) | (value << (32 - distance));
}
} // namespace

std::string sha256Hex(std::string_view data) {
    std::vector<std::uint8_t> bytes(data.begin(), data.end());
    const auto bits = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) bytes.push_back(0);
    for (int i = 7; i >= 0; --i) bytes.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
    std::array<std::uint32_t, 8> state {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    for (std::size_t offset = 0; offset < bytes.size(); offset += 64) {
        std::array<std::uint32_t, 64> words {};
        for (std::size_t i = 0; i < 16; ++i) {
            const auto j = offset + i * 4;
            words[i] = (static_cast<std::uint32_t>(bytes[j]) << 24) |
                       (static_cast<std::uint32_t>(bytes[j + 1]) << 16) |
                       (static_cast<std::uint32_t>(bytes[j + 2]) << 8) | bytes[j + 3];
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto a = words[i - 15];
            const auto b = words[i - 2];
            words[i] = words[i - 16] + (rotate(a,7) ^ rotate(a,18) ^ (a >> 3)) +
                       words[i - 7] + (rotate(b,17) ^ rotate(b,19) ^ (b >> 10));
        }
        auto [a,b,c,d,e,f,g,h] = std::tuple {state[0],state[1],state[2],state[3],state[4],state[5],state[6],state[7]};
        for (std::size_t i = 0; i < 64; ++i) {
            const auto s1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temp1 = h + s1 + choice + kConstants[i] + words[i];
            const auto s0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + majority;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto word : state) out << std::setw(8) << word;
    return out.str();
}

} // namespace aipb::map::io
