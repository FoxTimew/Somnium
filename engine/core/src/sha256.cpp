// Implements the fixed SHA-256 compression function for artifact integrity checks.
// All working state is local; hashing neither depends on a platform SDK nor allocates scratch.
#include <array>
#include <bit>
#include <cstdint>
#include <somnium/core/sha256.hpp>

namespace somnium::core {
namespace {
constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
void compress(std::array<std::uint32_t, 8>& state, std::span<const std::byte, 64> block) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t i = 0; i < 16; ++i)
        for (std::size_t byte = 0; byte < 4; ++byte)
            words[i] = (words[i] << 8) | std::to_integer<std::uint32_t>(block[i * 4 + byte]);
    for (std::size_t i = 16; i < words.size(); ++i) {
        const auto a = words[i - 15], b = words[i - 2];
        words[i] = words[i - 16] + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3)) + words[i - 7] +
                   (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
    }
    auto work = state;
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto [a, b, c, d, e, f, g, h] = work;
        const auto first = h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) +
                           ((e & f) ^ (~e & g)) + kRound[i] + words[i];
        const auto second =
            (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        work = {first + second, a, b, c, d + first, e, f, g};
    }
    for (std::size_t i = 0; i < state.size(); ++i)
        state[i] += work[i];
}
} // namespace
std::string sha256(std::span<const std::byte> bytes) {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto bits = static_cast<std::uint64_t>(bytes.size()) * 8;
    while (bytes.size() >= 64) {
        compress(state, bytes.first<64>());
        bytes = bytes.subspan(64);
    }
    std::array<std::byte, 128> tail{};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        tail[i] = bytes[i];
    tail[bytes.size()] = std::byte{0x80};
    const std::size_t size = bytes.size() < 56 ? 64 : 128;
    for (std::size_t i = 0; i < 8; ++i)
        tail[size - 1 - i] = static_cast<std::byte>((bits >> (8 * i)) & 0xff);
    compress(state, std::span(tail).first<64>());
    if (size == 128)
        compress(state, std::span(tail).last<64>());
    constexpr char kHex[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t i = 0; i < state.size(); ++i)
        for (std::size_t digit = 0; digit < 8; ++digit)
            result[i * 8 + digit] = kHex[(state[i] >> (28 - 4 * digit)) & 15];
    return result;
}
} // namespace somnium::core
