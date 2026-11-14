// Reversi 8x8. IA negamax alfa-beta (tabla posicional + movilidad + discos al final), igual que la versión de PC.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const int DIRS[8][2] = { { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } };
const int WT[64] = { 100, -20, 10, 5, 5, 10, -20, 100, -20, -50, -2, -2, -2, -2, -50, -20, 10, -2, -1, -1, -1, -1, -2, 10, 5, -2, -1, -1, -1, -1, -2, 5,
                     5, -2, -1, -1, -1, -1, -2, 5, 10, -2, -1, -1, -1, -1, -2, 10, -20, -50, -2, -2, -2, -2, -50, -20, 100, -20, 10, 5, 5, 10, -20, 100 };
struct Lvl { int fix, dd, exact, lim; float err, noise; int top; float d0, d1; };
const Lvl LVL[4] = { {}, { 1, 0, 0, 14000, 0.42f, 40, 99, 0.9f, 1.3f }, { 0, 0, 6, 14000, 0.12f, 3, 3, 0.6f, 0.9f }, { 0, 1, 8, 14000, 0.015f, 0.5f, 2, 0.5f, 0.6f } };
typedef signed char B64[64];
struct Mv { int i; std::vector<int> f; };
bool flips(const B64 b, int i, int c, std::vector<int>* out) {
    if (b[i]) return false; int r = i >> 3, col = i & 7; bool any = false;
    for (auto& d : DIRS) {
        int rr = r + d[0], cc = col + d[1], n = 0;
        while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && b[rr * 8 + cc] == 3 - c) { rr += d[0]; cc += d[1]; n++; }
        if (n && rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && b[rr * 8 + cc] == c) {
            any = true; if (!out) return true;
            int x = r + d[0], y = col + d[1]; for (int k = 0; k < n; k++) { out->push_back(x * 8 + y); x += d[0]; y += d[1]; }
        }
    }
    return any;
}
std::vector<Mv> gen(const B64 b, int c) { std::vector<Mv> ms; for (int i = 0; i < 64; i++) { Mv m{ i, {} }; if (flips(b, i, c, &m.f)) ms.push_back(m); } return ms; }
int mobility(const B64 b, int c) { int n = 0; for (int i = 0; i < 64; i++) if (flips(b, i, c, nullptr)) n++; return n; }
void apply(B64 b, const Mv& m, int c) { b[m.i] = c; for (int k : m.f) b[k] = c; }
void undo(B64 b, const Mv& m, int c) { b[m.i] = 0; for (int k : m.f) b[k] = 3 - c; }
int discDiff(const B64 b, int c) { int d = 0; for (int i = 0; i < 64; i++) { if (b[i] == c) d++; else if (b[i]) d--; } return d; }
double evalR(const B64 b, int c, int empties) {
    double sc = 0; for (int i = 0; i < 64; i++) { if (b[i] == c) sc += WT[i]; else if (b[i]) sc -= WT[i]; }
    sc += (mobility(b, c) - mobility(b, 3 - c)) * 6;
    if (empties <= 14) sc += discDiff(b, c) * 3;
    return sc;
}
struct Ctr { int n, lim; };
double neg(B64 b, int c, int depth, double alpha, double beta, Ctr& ctr, int empties) {
    ctr.n++;
    if (depth <= 0 || ctr.n > ctr.lim) return evalR(b, c, empties);
    auto ms = gen(b, c);
    if (ms.empty()) {
        if (!mobility(b, 3 - c)) return discDiff(b, c) * 1000.0;
        return -neg(b, 3 - c, depth, -beta, -alpha, ctr, empties);
    }
    std::sort(ms.begin(), ms.end(), [](const Mv& x, const Mv& y) { return WT[y.i] < WT[x.i]; });
    double best = -1e18;
    for (auto& m : ms) {
        apply(b, m, c); double sc = -neg(b, 3 - c, depth - 1, -beta, -alpha, ctr, empties - 1); undo(b, m, c);
        if (sc > best) best = sc; if (best > alpha) alpha = best; if (alpha >= beta) break;
    }
    return best;
}
int aiChoose(const B64 board, int me, int lv) {
    const Lvl& P = LVL[std::max(1, std::min(3, lv))];
    B64 b; std::copy(board, board + 64, b); Ctr ctr{ 0, P.lim };
    int empties = 0; for (int i = 0; i < 64; i++) if (!b[i]) empties++;
    int depth = empties <= 6 ? empties + 1 : (empties <= 10 ? 5 : 4);
    if (P.fix) depth = P.fix; else { depth += P.dd; if (empties <= P.exact) depth = empties + 1; }
    std::vector<std::pair<int, double>> out;
    for (auto& m : gen(b, me)) { ctr.n = 0; apply(b, m, me); double sc = -neg(b, 3 - me, depth - 1, -1e18, 1e18, ctr, empties - 1); undo(b, m, me); out.push_back({ m.i, sc + rnd01() * P.noise }); }
    if (out.empty()) return -1;
    std::sort(out.begin(), out.end(), [](auto& a, auto& c) { return a.second > c.second; });
    if (out.size() > 1 && rnd01() < P.err) { int k = std::min((int)out.size(), P.top); return out[rndInt(k)].first; }
    return out[0].first;
}

struct Reversi : Game {
    B64 b{}; int turn = 0, passed = -1, last = -1; bool over = false, hintsOn = true; float aiT = 0, aiWait = 1, endT = 0, flipT[64] = {}, msgT = 0;
    void start() override { std::fill(b, b + 64, 0); b[27] = 2; b[28] = 1; b[35] = 1; b[36] = 2; hintsOn = opts.get("hints", 1) != 0; aiWait = 0; }
    int turnPlayer() const override { return over ? -1 : turn; }
    int count(int c) const { int n = 0; for (int i = 0; i < 64; i++) if (b[i] == c) n++; return n; }
    void move(int i) {
        std::vector<int> f; if (over || !flips(b, i, turn + 1, &f)) return;
        b[i] = turn + 1; for (int k : f) { b[k] = turn + 1; flipT[k] = 0.45f; } flipT[i] = 0.25f; last = i; passed = -1;
        app->audio.play("place");
        int opp = 1 - turn;
        if (mobility(b, opp + 1)) turn = opp;
        else if (mobility(b, turn + 1)) { passed = opp; msgT = 2.2f; }
        else over = true;
        aiT = 0; const Lvl& P = LVL[std::max(1, std::min(3, pl[turn].level))]; aiWait = P.d0 + rnd01() * P.d1;
    }
    void resign(int p) override { over = true; resigned = p; }
    int resigned = -1;
    void update(float dt, Input&, UI&) override {
        for (float& f : flipT) f = std::max(0.f, f - dt); msgT = std::max(0.f, msgT - dt);
        if (over) {
            endT += dt;
            if (endT > 1.4f && !finished) {
                int a = count(1), c = count(2); int w = resigned >= 0 ? 1 - resigned : a > c ? 0 : c > a ? 1 : -1;
                std::vector<PResult> r = { { a, w == 0 ? 'w' : w < 0 ? 'd' : 'l' }, { c, w == 1 ? 'w' : w < 0 ? 'd' : 'l' } };
                if (resigned >= 0) r[resigned].r = 'x';
                finish(r, w < 0 ? "Empate " + std::to_string(a) + " a " + std::to_string(c) : "Ganó " + pl[w].name + " (" + std::to_string(std::max(a, c)) + " a " + std::to_string(std::min(a, c)) + ")");
            }
            return;
        }
        if (!pl[turn].ai) return;
        aiT += dt; if (aiT < aiWait) return;
        int i = aiChoose(b, turn + 1, pl[turn].level); if (i >= 0) move(i); else over = true;
    }
    void disc(Gfx& g, float cx, float cy, float rad, int v, float flip) {
        float sx = flip > 0 ? fabsf(cosf((1 - flip / 0.45f) * 3.14159f)) : 1; float rr = rad * std::max(0.15f, sx);
        g.circle(cx + 2, cy + 4, rad, rgb(0, 110));
        Rect e{ cx - rr, cy - rad, rr * 2, rad * 2 };
        Col top = v == 1 ? rgb(0x3a3a40) : rgb(0xffffff), bot = v == 1 ? rgb(0x0e0e12) : rgb(0xd6d6d0);
        g.rrectGrad(e, std::min(rr, rad), top, bot);
        if (sx > 0.6f) g.ring(cx, cy, rad * 0.78f, 2, v == 1 ? rgb(0x55555c) : rgb(0xbdbdb5));
    }
    void draw(Gfx& g, UI& ui) override {
        float S = 616, cs = (S - 32) / 8, x0 = 790 - S / 2, y0 = 30;
        g.rrect({ x0 + 4, y0 + 10, S, S }, 18, rgb(0, 110)); g.rrectGrad({ x0, y0, S, S }, 18, rgb(0x8a5a2c), rgb(0x4a2c12));
        Rect felt{ x0 + 16, y0 + 16, S - 32, S - 32 }; g.gradRect(felt, rgb(0x2f8a4a), rgb(0x1d5a30));
        for (int k = 0; k <= 8; k++) { g.line(felt.x + k * cs, felt.y, felt.x + k * cs, felt.y + felt.h, 2, rgb(0x0f3a1c)); g.line(felt.x, felt.y + k * cs, felt.x + felt.w, felt.y + k * cs, 2, rgb(0x0f3a1c)); }
        for (int k : { 2, 6 }) for (int j : { 2, 6 }) g.circle(felt.x + k * cs, felt.y + j * cs, 5, rgb(0x0f3a1c));
        bool mine = !over && !pl[turn].ai;
        int firstLegal = -1;
        for (int i = 0; i < 64; i++) {
            Rect c{ felt.x + (i % 8) * cs, felt.y + (i / 8) * cs, cs, cs }; uint32_t id = hid("rv", i);
            bool legal = !over && flips(b, i, turn + 1, nullptr);
            if (legal && firstLegal < 0) firstLegal = i;
            bool hit = ui.hit(id, c, mine && legal);
            if (i == last) g.rect(c.inset(2), rgb(0xffe27a, 60));
            if (b[i]) disc(g, c.cx(), c.cy(), cs * 0.4f, b[i], flipT[i]);
            else if (legal && mine && hintsOn) g.circle(c.cx(), c.cy(), cs * 0.12f, rgb(0xffe27a, 190));
            if (ui.focused(id)) ui.drawFocus(c.inset(4), 6);
            if (hit) move(i);
        }
        if (mine && firstLegal >= 0) ui.setFocusIfNone(hid("rv", firstLegal));
        // marcador a la izquierda
        Rect sc{ 16, 240, 230, 96 }; g.rrect(sc, 12, rgb(0x120c08, 200)); g.rrectLine(sc, 12, 1.5f, rgb(0xd8b46a, 110));
        for (int p = 0; p < 2; p++) {
            float y = sc.y + 26 + p * 44; disc(g, sc.x + 28, y, 15, p + 1, 0);
            g.text(pl[p].name, sc.x + 54, y, 16, p == turn && !over ? pal::gold : pal::white, F_BOLD, AL_L, 120);
            g.text(std::to_string(count(p + 1)), sc.x + sc.w - 14, y, 22, pal::gold, F_TITLE, AL_R);
        }
        std::string msg = over ? "Fin de la partida" : msgT > 0 && passed >= 0 ? pl[passed].name + " no tiene jugada: pasa" : pl[turn].ai ? "Piensa " + pl[turn].name + "…" : "Turno de " + pl[turn].name + (turn == 0 ? " (negras)" : " (blancas)");
        g.textBox(msg, { 18, 350, 228, 80 }, 17, pal::gold, F_BOLD);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir casilla" }, { "A", "Poner ficha" } }; }
    std::string help() const override { return "Poné una ficha de tu color de modo que encierres en línea recta (horizontal, vertical o diagonal) fichas del rival: esas fichas se dan vuelta y pasan a ser tuyas. Si no tenés jugada, pasás. Cuando nadie puede jugar, gana quien tiene más fichas. Los puntitos amarillos marcan dónde podés jugar."; }
};
}
Game* makeReversi() { return new Reversi(); }
