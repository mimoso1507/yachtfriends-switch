// Cierra la caja: cada jugador juega su propia caja (1 a 8); gana quien saque menos puntos. IA: programación dinámica exacta.
#include "dice.h"
#include <algorithm>
#include <cmath>

namespace {
double E[512]; int SUMOPEN[512]; bool built = false;
void build() {
    if (built) return; built = true;
    for (int m = 0; m < 512; m++) { int s = 0; for (int i = 0; i < 9; i++) if (m & (1 << i)) s += i + 1; SUMOPEN[m] = s; }
    E[0] = 0;
    for (int m = 1; m < 512; m++) {
        double best[13]; for (double& b : best) b = 1e18;
        for (int sub = m; sub > 0; sub = (sub - 1) & m) { int s = SUMOPEN[sub]; if (s <= 12) best[s] = std::min(best[s], E[m ^ sub]); }
        auto out = [&](int t) { return best[t] >= 1e17 ? (double)SUMOPEN[m] : best[t]; };
        double e = 0;
        if (m & 63) { for (int a = 1; a <= 6; a++) for (int b = 1; b <= 6; b++) e += out(a + b) / 36; } else { for (int a = 1; a <= 6; a++) e += out(a) / 6; }
        E[m] = e;
    }
}
int sumMask(int m) { int s = 0; for (int i = 0; i < 9; i++) if (m & (1 << i)) s += i + 1; return s; }
int choose(int m, int target, int lv) {
    build(); std::vector<std::pair<double, int>> o;
    for (int sub = m; sub > 0; sub = (sub - 1) & m) if (SUMOPEN[sub] == target) o.push_back({ E[m ^ sub], sub });
    if (o.empty()) return 0; std::sort(o.begin(), o.end());
    const float ER[4] = { 0, 0.4f, 0.1f, 0.01f }; size_t k = 0;
    if (o.size() > 1 && rnd01() < ER[lv]) k = lv == 1 ? rndInt((int)o.size()) : 1 + rndInt((int)std::min<size_t>(2, o.size() - 1));
    return o[k].second;
}

struct Box : Game {
    int N = 0, rounds = 1, round = 1, turn = 0, mask = 511, pick = 0, d[2] = { 1, 1 }; std::vector<int> totals; bool rolled = false, stuck = false, over = false; float rollAnim = 0, stuckT = 0, aiT = 0, aiWait = 1, endT = 0; int resigned = -1;
    void start() override { N = (int)pl.size(); rounds = opts.get("rounds", 0) == 1 ? 3 : 1; totals.assign(N, 0); resetAi(); }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = lv == 1 ? 0.9f + rnd01() * 1.3f : lv == 3 ? 0.5f + rnd01() * 0.5f : 0.6f + rnd01() * 0.8f; }
    int turnPlayer() const override { return over ? -1 : turn; }
    int target() const { return d[1] ? d[0] + d[1] : d[0]; }
    void roll() {
        if (over || rolled) return; bool two = (mask & 63) != 0; d[0] = 1 + rndInt(6); d[1] = two ? 1 + rndInt(6) : 0; rolled = true; rollAnim = 0.5f; pick = 0; app->audio.play("dice");
        stuck = true; build(); for (int sub = mask; sub > 0; sub = (sub - 1) & mask) if (SUMOPEN[sub] == target()) { stuck = false; break; } stuckT = 0; resetAi();
    }
    void nextTurn() {
        if (turn == N - 1) { if (round >= rounds) { over = true; return; } round++; }
        turn = (turn + 1) % N; mask = 511; rolled = false; stuck = false; pick = 0; resetAi();
    }
    void confirm() {
        if (!rolled || stuck || rollAnim > 0 || sumMask(pick) != target() || !pick) return;
        mask &= ~pick; rolled = false; pick = 0; app->audio.play("place");
        if (!mask) nextTurn(); else resetAi();
    }
    void tap(int i) { if (!rolled || stuck || rollAnim > 0 || !(mask & (1 << i))) return; pick ^= 1 << i; app->audio.play("click"); }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input& in, UI&) override {
        rollAnim = std::max(0.f, rollAnim - dt);
        if (over) { endT += dt; if (endT > 1.4f && !finished) {
            int best = *std::min_element(totals.begin(), totals.end()); int cnt = (int)std::count(totals.begin(), totals.end(), best);
            std::vector<PResult> r; std::string sm = "Menos puntos gana:"; for (int i = 0; i < N; i++) { r.push_back({ totals[i], totals[i] == best ? (cnt > 1 ? 'd' : 'w') : 'l' }); sm += " " + std::to_string(totals[i]); }
            if (resigned >= 0) { for (auto& x : r) if (x.r == 'w') x.r = 'l'; r[resigned].r = 'x'; r[(resigned + 1) % N].r = 'w'; } finish(r, sm); } return; }
        if (rolled && stuck && rollAnim <= 0) { stuckT += dt; if (stuckT > 1.6f) { totals[turn] += sumMask(mask); nextTurn(); } return; }
        if (!pl[turn].ai) { Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (p) { if (!rolled && p->rollDown()) roll(); else if (rolled && p->down[B_X]) confirm(); } return; }
        aiT += dt; if (aiT < aiWait) return;
        if (!rolled) roll(); else if (rollAnim <= 0) { int c = choose(mask, target(), std::max(1, std::min(3, pl[turn].level))); if (c) { pick = c; confirm(); } }
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai;
        std::string banner = over ? "Fin: gana quien sacó menos puntos" : rolled && stuck ? (pl[turn].name + " no tiene combinación: suma " + std::to_string(sumMask(mask)) + " puntos") : mine ? "Tu turno" : "Turno de " + pl[turn].name;
        g.rrect({ 300, 30, 940, 50 }, 25, rgb(0x120c08, 210)); g.text(banner + (rounds > 1 ? "   ·   Ronda " + std::to_string(round) + "/" + std::to_string(rounds) : ""), 770, 55, 20, pal::gold, F_BOLD, AL_C, 920);
        // la caja
        Rect B{ 300, 150, 940, 200 }; g.rrect({ B.x + 5, B.y + 9, B.w, B.h }, 22, rgb(0, 110)); g.rrectGrad(B, 22, rgb(0x9a6a3c), rgb(0x5a3a1c)); g.rrectGrad(B.inset(12), 16, rgb(0x6b4224), rgb(0x40240f));
        float tw = 88, gap = 12, x0 = B.cx() - (9 * tw + 8 * gap) / 2; int firstOpen = -1;
        for (int i = 0; i < 9; i++) {
            Rect r{ x0 + i * (tw + gap), B.y + 30, tw, 140 }; bool open = mask & (1 << i), picked = pick & (1 << i); uint32_t id = hid("cc", i);
            bool ok = mine && rolled && !stuck && rollAnim <= 0 && open; if (ok && firstOpen < 0) firstOpen = i; bool hit = ui.hit(id, r, ok);
            if (open) { g.rrectGrad(r, 12, picked ? rgb(0xffe08a) : rgb(0xfff4d6), picked ? rgb(0xe8b84a) : rgb(0xecd6a0)); g.rrectLine(r, 12, 3, picked ? pal::gold : rgb(0x8a5a2c)); g.text(std::to_string(i + 1), r.cx(), r.cy(), 54, rgb(0x4a2e18), F_TITLE, AL_C); }
            else { g.rrectGrad(r.inset(0), 12, rgb(0x2a1a0e), rgb(0x1c1008)); g.rrect({ r.x + 6, r.y + r.h - 22, r.w - 12, 14 }, 6, rgb(0x120a06)); }
            if (ui.focused(id)) ui.drawFocus(r, 12); if (hit) tap(i);
        }
        if (firstOpen >= 0) ui.setFocusIfNone(hid("cc", firstOpen));
        // dados
        float dx = 770 - (d[1] ? 80 : 36); if (rolled) { for (int k = 0; k < (d[1] ? 2 : 1); k++) dice::draw(g, { dx + k * 90, 376, 72, 72 }, rollAnim > 0 ? 1 + rndInt(6) : d[k]); }
        std::string sm = pick ? "Elegiste: " + std::to_string(sumMask(pick)) + (rolled ? " de " + std::to_string(target()) : "") : rolled ? "Sumá " + std::to_string(target()) + " con las fichas abiertas" : "";
        g.text(sm, 770, 470, 18, pal::cream, F_BOLD, AL_C, 600);
        bool canConfirm = mine && rolled && !stuck && rollAnim <= 0 && pick && sumMask(pick) == target();
        if (!rolled) { if (ui.button("🎲 Tirar", { 630, 490, 280, 58 }, 1, mine && rollAnim <= 0)) roll(); if (mine) ui.setFocusIfNone(hid("Tirar🎲", (int)(630 * 7 + 490))); }
        else if (ui.button("Cerrar fichas", { 630, 490, 280, 58 }, 3, canConfirm)) confirm();
        // marcador
        float y = 570; g.rrect({ 300, y, 940, 78 }, 12, rgb(0x120c08, 200)); g.rrectLine({ 300, y, 940, 78 }, 12, 1.5f, rgb(0xd8b46a, 110));
        for (int i = 0; i < N && i < 8; i++) { float cx = 316 + (i % 4) * 232, cy = y + 22 + (i / 4) * 30; g.circle(cx, cy, 6, PCOL[i % 8]); g.text(pl[i].name, cx + 14, cy, 14, i == turn && !over ? pal::gold : pal::white, F_BOLD, AL_L, 120); g.text(std::to_string(totals[i]), cx + 210, cy, 16, pal::gold, F_BOLD, AL_R); }
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir ficha" }, { "A", "Marcar / desmarcar" }, { "ROLL", "Tirar los dados" }, { "X", "Cerrar fichas" } }; }
    std::string help() const override { return "Tirá los dados y cerrá fichas numeradas cuyos números sumen exacto el resultado. Mientras tengas abiertas fichas del 1 al 6 se tiran dos dados; si no, uno. Si no podés sumar, anotás como puntos la suma de las fichas abiertas y sigue el próximo. Cerrar todas las fichas son 0 puntos. Gana quien junta menos puntos."; }
};
}
Game* makeCierraCaja() { return new Box(); }
