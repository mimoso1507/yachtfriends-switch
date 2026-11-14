// 4 en línea en cubo 3D (4×4×4): 16 postes, cada ficha cae al nivel libre más bajo. Gana la primera línea de 4 en cualquier dirección.
// Se muestran los 4 niveles lado a lado; el poste elegido se marca en todos.
#include "../app.h"
#include <algorithm>
#include <array>

namespace {
typedef std::array<std::array<int, 3>, 4> Ln;
std::vector<Ln> LINES;
void initLines() {
    if (!LINES.empty()) return;
    std::vector<std::array<int, 3>> dirs; for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) for (int dz = -1; dz <= 1; dz++) { if (!dx && !dy && !dz) continue; if (dx < 0 || (dx == 0 && dy < 0) || (dx == 0 && dy == 0 && dz < 0)) continue; dirs.push_back({ dx, dy, dz }); }
    for (int x = 0; x < 4; x++) for (int y = 0; y < 4; y++) for (int z = 0; z < 4; z++) for (auto& d : dirs) { int ex = x + 3 * d[0], ey = y + 3 * d[1], ez = z + 3 * d[2]; if (ex < 0 || ex > 3 || ey < 0 || ey > 3 || ez < 0 || ez > 3) continue; Ln l; for (int k = 0; k < 4; k++) l[k] = { x + k * d[0], y + k * d[1], z + k * d[2] }; LINES.push_back(l); }
}
struct Cubo : Game {
    int cell[4][4][4] = {}; int h[16] = {}; int turn = 0, cx = 1, cy = 1, winner = -1, count = 0; bool over = false; int line = -1; float aiT = 0, endT = 0, stickT = 0; std::string msg; bool stickHeld = false;
    void start() override { initLines(); turn = 0; msg = pl[0].name + " empieza"; }
    int turnPlayer() const override { return over ? -1 : turn; }
    bool drop(int x, int y) {
        if (over || h[y * 4 + x] >= 4) return false; int z = h[y * 4 + x]++; cell[x][y][z] = turn + 1; count++; app->audio.play("place");
        for (size_t i = 0; i < LINES.size(); i++) { bool all = true; for (auto& c : LINES[i]) if (cell[c[0]][c[1]][c[2]] != turn + 1) all = false; if (all) { over = true; winner = turn; line = (int)i; app->audio.play("win"); return true; } }
        if (count == 64) { over = true; return true; } turn = 1 - turn; aiT = 0; return true; }
    void resign(int p) override { if (over) return; over = true; winner = 1 - p; }
    // ---------- IA ----------
    float evalFor(int p) const { float s = 0; for (auto& l : LINES) { int me = 0, op = 0; for (auto& c : l) { int v = cell[c[0]][c[1]][c[2]]; if (v == p + 1) me++; else if (v) op++; } if (!op) { static const float W[5] = { 0, 1, 6, 40, 0 }; s += W[me]; } } return s; }
    bool winsIf(int p, int x, int y) const { int z = h[y * 4 + x]; if (z >= 4) return false; for (auto& l : LINES) { bool all = true; for (auto& c : l) { int v = (c[0] == x && c[1] == y && c[2] == z) ? p + 1 : cell[c[0]][c[1]][c[2]]; if (v != p + 1) { all = false; break; } } if (all) return true; } return false; }
    void aiMove() {
        int lv = std::max(1, std::min(3, pl[turn].level)); std::vector<int> ok; for (int i = 0; i < 16; i++) if (h[i] < 4) ok.push_back(i);
        for (int i : ok) if (winsIf(turn, i % 4, i / 4)) { drop(i % 4, i / 4); return; }
        if (lv >= 2 || rnd01() < 0.6f) for (int i : ok) if (winsIf(1 - turn, i % 4, i / 4)) { drop(i % 4, i / 4); return; }
        if (lv == 1 && rnd01() < 0.5f) { int i = ok[rndInt((int)ok.size())]; drop(i % 4, i / 4); return; }
        int best = ok[0]; float bs = -1e9f;
        for (int i : ok) { int x = i % 4, y = i / 4, z = h[i]; cell[x][y][z] = turn + 1; h[i]++; float sc = evalFor(turn) - evalFor(1 - turn) + rnd01() * (lv == 2 ? 3.f : 0.5f);
            if (lv >= 2 && h[i] < 4 && winsIf(1 - turn, x, y)) sc -= 500; // no regalar la victoria
            if (lv >= 3) { float worst = 1e9f; for (int j = 0; j < 16; j++) if (h[j] < 4) { int xx = j % 4, yy = j / 4, zz = h[j]; cell[xx][yy][zz] = 2 - turn; h[j]++; worst = std::min(worst, evalFor(turn) - evalFor(1 - turn)); h[j]--; cell[xx][yy][zz] = 0; } sc = sc * 0.4f + worst * 0.6f; }
            h[i]--; cell[x][y][z] = 0; if (sc > bs) { bs = sc; best = i; } }
        drop(best % 4, best / 4); }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; for (int p = 0; p < 2; p++) r.push_back({ winner < 0 ? 0 : p == winner ? 1 : 0, winner < 0 ? 'd' : p == winner ? 'w' : 'l' }); finish(r, winner < 0 ? "Empate" : "Ganó " + pl[winner].name); } return; }
        if (pl[turn].ai) { aiT += dt; if (aiT > 0.8f) { aiT = 0; aiMove(); } return; }
        Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (!p) return;
        int dx = (p->down[B_RIGHT] ? 1 : 0) - (p->down[B_LEFT] ? 1 : 0), dy = (p->down[B_DOWN] ? 1 : 0) - (p->down[B_UP] ? 1 : 0);
        stickT -= dt; float ax = p->lx, ay = p->ly; if (std::fabs(ax) > 0.6f || std::fabs(ay) > 0.6f) { if (stickT <= 0) { if (std::fabs(ax) >= std::fabs(ay)) dx += ax > 0 ? 1 : -1; else dy += ay > 0 ? 1 : -1; stickT = stickHeld ? 0.23f : 0.46f; stickHeld = true; } } else { stickHeld = false; stickT = 0; }
        cx = std::max(0, std::min(3, cx + dx)); cy = std::max(0, std::min(3, cy + dy));
        if (p->down[B_A]) drop(cx, cy);
        if (in.touchDown) for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (cellRect(z, x, y).in(in.tx, in.ty)) { cx = x; cy = y; drop(x, y); return; }
    }
    Rect cellRect(int z, int x, int y) const { const float S = 52; return { 300.f + z * 228 + x * S, 190.f + (3 - y) * S, S, S }; }
    void draw(Gfx& g, UI&) override {
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(over ? (winner < 0 ? "Empate" : "¡Ganó " + pl[winner].name + "!") : pl[turn].name + (pl[turn].ai ? " piensa…" : ": elige un poste y suelta la ficha (A)"), 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        for (int z = 0; z < 4; z++) { Rect pr{ 292.f + z * 228, 160, 224, 240 }; g.rrect(pr, 12, rgb(0x120c08, 170)); g.text("Nivel " + std::to_string(z + 1), pr.cx(), 176, 16, pal::gold, F_BOLD, AL_C);
            for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { Rect r = cellRect(z, x, y); bool cur = x == cx && y == cy && !over && !pl[turn].ai; g.rrect(r.inset(1), 8, cur ? rgb(0xe0b84c) : rgb(0xf0e4c0)); g.rrect(r.inset(3), 7, cur ? rgb(0x7a5a1c) : rgb(0x3a2a1c)); int v = cell[x][y][z];
                if (v) { bool wl = false; if (line >= 0) for (auto& c : LINES[line]) if (c[0] == x && c[1] == y && c[2] == z) wl = true; g.circle(r.cx(), r.cy(), 19, rgb(v == 1 ? 0x3ddc97 : 0xff9a3c)); g.circle(r.cx() - 5, r.cy() - 6, 6, rgb(0xffffff, 120)); if (wl) g.ring(r.cx(), r.cy(), 22, 4, rgb(0xffffff)); g.text(v == 1 ? "★" : "●", r.cx(), r.cy() + 1, 16, rgb(0xffffff), F_BOLD, AL_C); }
                else if (h[y * 4 + x] == z) g.circle(r.cx(), r.cy(), 4, rgb(0xffffff, 70)); } }
        g.text(std::string("Verde ★ = ") + pl[0].name + "   ·   Naranja ● = " + pl[1].name, 765, 440, 16, pal::cream, F_BOLD, AL_C);
        g.text("Poste elegido: columna " + std::to_string(cx + 1) + ", fila " + std::to_string(cy + 1) + " (altura " + std::to_string(h[cy * 4 + cx]) + "/4)", 765, 470, 15, rgb(0xd8b46a), F_BODY, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir poste" }, { "A", "Soltar ficha" }, { "TOUCH", "Tocar un poste" } }; }
    std::string help() const override { return "Cubo de 4×4×4 con 16 postes. En tu turno eliges un poste y la ficha cae hasta el nivel libre más bajo. Gana quien arme primero una línea de 4 fichas propias en cualquier dirección: filas, columnas, pilas o diagonales, dentro de un nivel o atravesando niveles."; }
};
}
Game* makeCubo4() { return new Cubo(); }
