#include "qr.h"
#include <cstdint>
#include <algorithm>
#include <cstdlib>

namespace {
const int ECC_PER_BLOCK[10] = { 7, 10, 15, 20, 26, 18, 20, 24, 30, 18 };   // nivel L, versiones 1..10
const int NUM_BLOCKS[10] = { 1, 1, 1, 1, 1, 2, 2, 2, 2, 4 };

int rawModules(int ver) {
    int r = (16 * ver + 128) * ver + 64;
    if (ver >= 2) { int na = ver / 7 + 2; r -= (25 * na - 10) * na - 55; if (ver >= 7) r -= 36; }
    return r;
}
int dataCodewords(int ver) { return rawModules(ver) / 8 - ECC_PER_BLOCK[ver - 1] * NUM_BLOCKS[ver - 1]; }
uint8_t gfMul(uint8_t x, uint8_t y) { int z = 0; for (int i = 7; i >= 0; i--) { z = (z << 1) ^ ((z >> 7) * 0x11D); z ^= ((y >> i) & 1) * x; } return (uint8_t)z; }
std::vector<uint8_t> rsDivisor(int deg) {
    std::vector<uint8_t> r(deg, 0); r[deg - 1] = 1; uint8_t root = 1;
    for (int i = 0; i < deg; i++) { for (int j = 0; j < deg; j++) { r[j] = gfMul(r[j], root); if (j + 1 < deg) r[j] ^= r[j + 1]; } root = gfMul(root, 2); }
    return r;
}
std::vector<uint8_t> rsRemainder(const std::vector<uint8_t>& data, const std::vector<uint8_t>& div) {
    std::vector<uint8_t> r(div.size(), 0);
    for (uint8_t b : data) { uint8_t f = b ^ r[0]; r.erase(r.begin()); r.push_back(0); for (size_t i = 0; i < r.size(); i++) r[i] ^= gfMul(div[i], f); }
    return r;
}
bool bitOf(uint32_t v, int i) { return (v >> i) & 1; }

struct Qr {
    int ver, size; std::vector<std::vector<bool>> mod, fn;
    explicit Qr(int v) : ver(v), size(v * 4 + 17), mod(size, std::vector<bool>(size)), fn(size, std::vector<bool>(size)) {}
    void setFn(int x, int y, bool dark) { mod[y][x] = dark; fn[y][x] = true; }
    void drawFinder(int cx, int cy) {
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int d = std::max(std::abs(dx), std::abs(dy)), x = cx + dx, y = cy + dy;
            if (x >= 0 && x < size && y >= 0 && y < size) setFn(x, y, d != 2 && d != 4);
        }
    }
    void drawAlign(int cx, int cy) { for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) setFn(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1); }
    std::vector<int> alignPos() const {
        std::vector<int> r; if (ver == 1) return r;
        int na = ver / 7 + 2, step = (ver * 4 + na * 2 + 1) / (na * 2 - 2) * 2;
        for (int i = 0, pos = size - 7; i < na - 1; i++, pos -= step) r.insert(r.begin(), pos);
        r.insert(r.begin(), 6); return r;
    }
    void drawFormat(int mask) {
        uint32_t data = (1u << 3) | mask;   // nivel L = 01
        uint32_t rem = data; for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        uint32_t bits = ((data << 10) | rem) ^ 0x5412;
        for (int i = 0; i <= 5; i++) setFn(8, i, bitOf(bits, i));
        setFn(8, 7, bitOf(bits, 6)); setFn(8, 8, bitOf(bits, 7)); setFn(7, 8, bitOf(bits, 8));
        for (int i = 9; i < 15; i++) setFn(14 - i, 8, bitOf(bits, i));
        for (int i = 0; i < 8; i++) setFn(size - 1 - i, 8, bitOf(bits, i));
        for (int i = 8; i < 15; i++) setFn(8, size - 15 + i, bitOf(bits, i));
        setFn(8, size - 8, true);
    }
    void drawFunctions() {
        for (int i = 0; i < size; i++) { setFn(6, i, i % 2 == 0); setFn(i, 6, i % 2 == 0); }
        drawFinder(3, 3); drawFinder(size - 4, 3); drawFinder(3, size - 4);
        auto ap = alignPos(); int n = (int)ap.size();
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (!((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))) drawAlign(ap[i], ap[j]);
        drawFormat(0);
        if (ver >= 7) {
            uint32_t rem = ver; for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
            uint32_t bits = ((uint32_t)ver << 12) | rem;
            for (int i = 0; i < 18; i++) { bool b = bitOf(bits, i); int a = size - 11 + i % 3, c = i / 3; setFn(a, c, b); setFn(c, a, b); }
        }
    }
    void drawCodewords(const std::vector<uint8_t>& data) {
        size_t i = 0;
        for (int right = size - 1; right >= 1; right -= 2) {
            if (right == 6) right = 5;
            for (int vert = 0; vert < size; vert++) for (int j = 0; j < 2; j++) {
                int x = right - j; bool up = ((right + 1) & 2) == 0; int y = up ? size - 1 - vert : vert;
                if (!fn[y][x] && i < data.size() * 8) { mod[y][x] = bitOf(data[i >> 3], 7 - (int)(i & 7)); i++; }
            }
        }
    }
    void applyMask(int m) {
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {
            bool inv;
            switch (m) {
            case 0: inv = (x + y) % 2 == 0; break; case 1: inv = y % 2 == 0; break; case 2: inv = x % 3 == 0; break; case 3: inv = (x + y) % 3 == 0; break;
            case 4: inv = (x / 3 + y / 2) % 2 == 0; break; case 5: inv = x * y % 2 + x * y % 3 == 0; break; case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
            default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; }
            if (!fn[y][x] && inv) mod[y][x] = !mod[y][x];
        }
    }
    int penalty() const {
        int p = 0;
        for (int pass = 0; pass < 2; pass++) for (int a = 0; a < size; a++) {
            int run = 1; for (int b = 1; b < size; b++) {
                bool cur = pass ? mod[b][a] : mod[a][b], prev = pass ? mod[b - 1][a] : mod[a][b - 1];
                if (cur == prev) { run++; if (run == 5) p += 3; else if (run > 5) p++; } else run = 1;
            }
        }
        for (int y = 0; y + 1 < size; y++) for (int x = 0; x + 1 < size; x++) { bool c = mod[y][x]; if (c == mod[y][x + 1] && c == mod[y + 1][x] && c == mod[y + 1][x + 1]) p += 3; }
        int dark = 0; for (auto& r : mod) for (bool b : r) dark += b; int total = size * size, k = (std::abs(dark * 20 - total * 10) + total - 1) / total - 1; p += k * 10;
        return p;
    }
};
}

std::vector<std::vector<bool>> qrEncode(const std::string& text) {
    int ver = 0; for (int v = 1; v <= 10; v++) { int cap = dataCodewords(v) * 8; int need = 4 + (v < 10 ? 8 : 16) + (int)text.size() * 8; if (need <= cap) { ver = v; break; } }
    if (!ver) return {};
    std::vector<bool> bits; auto put = [&](uint32_t v, int n) { for (int i = n - 1; i >= 0; i--) bits.push_back((v >> i) & 1); };
    put(4, 4); put((uint32_t)text.size(), ver < 10 ? 8 : 16); for (unsigned char c : text) put(c, 8);
    int cap = dataCodewords(ver) * 8; put(0, std::min(4, cap - (int)bits.size())); while (bits.size() % 8) bits.push_back(false);
    std::vector<uint8_t> data; for (size_t i = 0; i < bits.size(); i += 8) { uint8_t b = 0; for (int j = 0; j < 8; j++) b = (b << 1) | bits[i + j]; data.push_back(b); }
    for (uint8_t pad = 0xEC; (int)data.size() < cap / 8; pad ^= 0xEC ^ 0x11) data.push_back(pad);
    // bloques + corrección de errores
    int nb = NUM_BLOCKS[ver - 1], eb = ECC_PER_BLOCK[ver - 1], raw = rawModules(ver) / 8, nShort = nb - raw % nb, shortLen = raw / nb;
    std::vector<std::vector<uint8_t>> blocks; auto div = rsDivisor(eb); size_t k = 0;
    for (int i = 0; i < nb; i++) {
        int len = shortLen - eb + (i < nShort ? 0 : 1); std::vector<uint8_t> d(data.begin() + k, data.begin() + k + len); k += len;
        auto ecc = rsRemainder(d, div); if (i < nShort) d.push_back(0); d.insert(d.end(), ecc.begin(), ecc.end()); blocks.push_back(d);
    }
    std::vector<uint8_t> all;
    for (size_t i = 0; i < blocks[0].size(); i++) for (int j = 0; j < nb; j++) if (i != (size_t)(shortLen - eb) || j >= nShort) all.push_back(blocks[j][i]);
    Qr q(ver); q.drawFunctions(); q.drawCodewords(all);
    int best = 0, bestP = 1 << 30;
    for (int m = 0; m < 8; m++) { q.applyMask(m); q.drawFormat(m); int p = q.penalty(); if (p < bestP) { bestP = p; best = m; } q.applyMask(m); }
    q.applyMask(best); q.drawFormat(best);
    return q.mod;
}
