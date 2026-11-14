// Big Parch: Parchís de 4 brazos y 68 casillas con 2 dados (2 a 4 jugadores). Dibujo y animales propios.
// Con un 5 sale una ficha (obligatorio si se puede). Dobles repiten (tres seguidos: la última ficha vuelve a casa).
// Comer fuera de casillas seguras da 20 movidas gratis; llegar a la meta da 10. Dos fichas del mismo color forman una barrera.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const int FIN = 71, RING = 68;
const uint32_t COL[4] = { 0xe8524a, 0x3ddc97, 0xf5c542, 0x5aa9ff }, DARKC[4] = { 0x9a2b25, 0x1f8f60, 0xa07d10, 0x2a6fc0 };
const char* CN[4] = { "rojo", "verde", "amarillo", "azul" };
const int UA[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } }, VA[4][2] = { { 0, 1 }, { -1, 0 }, { 0, -1 }, { 1, 0 } };
inline int startIdx(int a) { return 17 * a + 13; }
inline int ringOf(int a, int t) { return (startIdx(a) + t) % RING; }
inline bool safeRing(int i) { int k = i % 17; return k == 4 || k == 8 || k == 13; }
void cellOf(int a, int u, int v, int& x, int& y) { x = 9 + u * UA[a][0] + v * VA[a][0]; y = 9 + u * UA[a][1] + v * VA[a][1]; }
void ringCell(int i, int& x, int& y) { int a = i / 17, k = i % 17; if (k < 8) cellOf(a, 1, 2 + k, x, y); else if (k == 8) cellOf(a, 0, 9, x, y); else cellOf(a, -1, 9 - (k - 9), x, y); }
void homeCell(int a, int t, int& x, int& y) { cellOf(a, 0, 8 - (t - 64), x, y); }
struct Mv { int d, t; };
struct Res { int to; int cp = -1, ct = -1; bool goal; };

struct BigParch : Game {
    int N = 0, turn = 0, dbl = 0, lastMoved = -1, winner = -1, sel = 0; bool over = false, extra = false, rolled = false, autoMv = false; int seats[4] = {}; int pos[4][4]; bool active[4];
    struct Av { int v; bool b; bool s = false; bool r = false; int x0 = 0, x1 = 0; }; int used = 0; std::vector<Av> avail; std::string msg; float aiT = 0, aiWait = 0.8f, endT = 0, dieAnim = 0, anim = 0; int ad[2] = { 1, 1 };
    int animP = -1, animT = -1; float ax0 = 0, ay0 = 0, ax1 = 0, ay1 = 0;
    void start() override {
        N = (int)pl.size(); static const int S2[2] = { 0, 2 }, S3[3] = { 0, 1, 2 }, S4[4] = { 0, 1, 2, 3 }; autoMv = opts.get("auto", 0) != 0;
        for (int p = 0; p < N; p++) { seats[p] = N == 2 ? S2[p] : N == 3 ? S3[p] : S4[p]; active[p] = true; for (int t = 0; t < 4; t++) pos[p][t] = -1; }
        turn = rndInt(N); msg = pl[turn].name + " empieza"; resetAi();
    }
    void resetAi() { aiT = 0; aiWait = 0.7f + rnd01() * 0.7f; }
    int turnPlayer() const override { return over ? -1 : turn; }
    // ---------- reglas ----------
    std::vector<std::pair<int,int>> at(int ring) const { std::vector<std::pair<int,int>> o; for (int p = 0; p < N; p++) if (active[p]) for (int t = 0; t < 4; t++) { int x = pos[p][t]; if (x >= 0 && x <= 63 && ringOf(seats[p], x) == ring) o.push_back({ p, t }); } return o; }
    bool barrierAt(int ring, int ep, int et) const { std::vector<std::pair<int,int>> o; for (auto& c : at(ring)) if (!(c.first == ep && c.second == et)) o.push_back(c); if (o.size() < 2) return false; for (auto& c : o) if (c.first != o[0].first) return false; return true; }
    bool tryMove(int p, int t, int v, bool b, Res& r) const {
        int x = pos[p][t], a = seats[p]; if (x == FIN) return false;
        if (x < 0) { if (b || v != 5) return false; auto o = at(ringOf(a, 0)); if (o.size() >= 2 && o[0].first == o[1].first) return false; r.to = 0; r.goal = false; for (auto& c : o) if (c.first != p) { r.cp = c.first; r.ct = c.second; break; } return true; }
        int y = x + v; if (y > FIN) return false; for (int s = x + 1; s <= std::min(y, 63); s++) if (barrierAt(ringOf(a, s), p, t)) return false;
        r.to = y; r.goal = y == FIN; if (y <= 63) { int rg = ringOf(a, y); std::vector<std::pair<int,int>> o; for (auto& c : at(rg)) if (!(c.first == p && c.second == t)) o.push_back(c);
            if (safeRing(rg)) { if (o.size() >= 2) return false; } else for (auto& c : o) if (c.first != p) { r.cp = c.first; r.ct = c.second; break; } }
        return true; }
    bool canExit(int p) const { for (int t = 0; t < 4; t++) if (pos[p][t] < 0) { Res r; if (tryMove(p, t, 5, false, r)) return true; } return false; }
    std::vector<int> legalFor(int p, int d) const { std::vector<int> o; if (d < 0 || d >= (int)avail.size()) return o; auto& it = avail[d]; if (it.s) { for (int t = 0; t < 4; t++) { Res r; if (pos[p][t] < 0 && tryMove(p, t, 5, false, r)) o.push_back(t); } return o; } bool forced = !it.b && it.v == 5 && canExit(p); for (int t = 0; t < 4; t++) { if (forced && pos[p][t] >= 0) continue; Res r; if (tryMove(p, t, it.v, it.b, r)) o.push_back(t); } return o; }
    std::vector<Mv> legal(int p) const { std::vector<Mv> o; for (int d = 0; d < (int)avail.size(); d++) for (int t : legalFor(p, d)) o.push_back({ d, t }); return o; }
    bool canPass() const { bool any = false, first = false; for (auto& a : avail) if (a.r || a.s) any = true; if (!any) return false; if (used >= 1) return true;
        for (int d = 0; d < (int)avail.size(); d++) if (!avail[d].s && !avail[d].b && !legalFor(turn, d).empty()) first = true; return !first; }
    bool pass() { if (over || avail.empty() || !canPass()) return false; std::vector<Av> k; for (auto& a : avail) if (a.b) k.push_back(a); avail = k; sel = 0; msg = pl[turn].name + " pasa"; settle(); return true; }
    void endTurn() { used = 0; int i = turn; do { i = (i + 1) % N; } while (!active[i]); turn = i; dbl = 0; extra = false; avail.clear(); lastMoved = -1; resetAi(); sel = 0; }
    void settle() { while (!avail.empty() && legal(turn).empty()) avail.clear(); if (avail.empty()) { if (extra && dbl < 3) { extra = false; resetAi(); } else endTurn(); } }
    void doRoll() {
        if (over || !avail.empty()) return; used = 0; int d1 = 1 + rndInt(6), d2 = 1 + rndInt(6); ad[0] = d1; ad[1] = d2; dieAnim = 0.5f; app->audio.play("dice");
        if (d1 == d2) { dbl++; if (dbl >= 3) { int t = lastMoved; if (t >= 0 && pos[turn][t] > -1 && pos[turn][t] < FIN) pos[turn][t] = -1; msg = "Tres dobles: ¡la última ficha vuelve a casa!"; endTurn(); return; } extra = true; } else extra = false;
        avail = { { d1, false, false, true }, { d2, false, false, true } }; if (d1 + d2 == 5) { Av sm{ 5, false, true, false }; sm.x0 = d1; sm.x1 = d2; avail.push_back(sm); } msg = pl[turn].name + " sacó " + std::to_string(d1) + " y " + std::to_string(d2) + (d1 == d2 ? " (doble)" : ""); sel = 0; settle(); }
    void xyOf(int p, int t, float& x, float& y) const {
        int ps = pos[p][t], s = seats[p];
        if (ps < 0) { float bx = 9 + (-5.5f) * UA[s][0] + 5.5f * VA[s][0], by = 9 + (-5.5f) * UA[s][1] + 5.5f * VA[s][1]; static const float SP[4][2] = { { -1.3f, -1.3f }, { 1.3f, -1.3f }, { -1.3f, 1.3f }, { 1.3f, 1.3f } }; x = bx + SP[t][0]; y = by + SP[t][1]; }
        else if (ps <= 63) { int cx, cy; ringCell(ringOf(s, ps), cx, cy); x = cx + .5f; y = cy + .5f; }
        else if (ps == FIN) { static const float DX[4] = { 0, -0.4f, 0, 0.4f }, DY[4] = { 0.4f, 0, -0.4f, 0 }; x = 9.5f + DX[s]; y = 9.5f + DY[s]; }
        else { int cx, cy; homeCell(s, ps, cx, cy); x = cx + .5f; y = cy + .5f; } }
    bool move(int p, int t, int d) {
        auto lg = legalFor(p, d); if (std::find(lg.begin(), lg.end(), t) == lg.end()) return false; Av it = avail[d]; Res r; tryMove(p, t, it.v, it.b, r); int from = pos[p][t];
        xyOf(p, t, ax0, ay0); pos[p][t] = r.to; xyOf(p, t, ax1, ay1); animP = p; animT = t; anim = 0.4f; lastMoved = t; sel = 0; if (it.s) { std::vector<Av> k; for (auto& a : avail) if (!a.r && !a.s) k.push_back(a); avail = k; used = 2; } else { bool rr = it.r; avail.erase(avail.begin() + d); std::vector<Av> k; for (auto& a : avail) if (!a.s) k.push_back(a); avail = k; if (rr) used++; }
        if (r.cp >= 0) { pos[r.cp][r.ct] = -1; avail.push_back({ 20, true }); msg = pl[p].name + " comió una ficha de " + pl[r.cp].name + " (+20)"; app->audio.play("lose"); } else { app->audio.play("place"); msg = pl[p].name + " mueve " + std::string(from < 0 ? "(sale de la base)" : ""); }
        if (r.goal) { avail.push_back({ 10, true }); msg = pl[p].name + " llegó a la meta (+10)"; app->audio.play("win"); }
        bool all = true; for (int k = 0; k < 4; k++) if (pos[p][k] != FIN) all = false; if (all) { over = true; winner = p; return true; }
        settle(); return true; }
    void resign(int p) override { if (p < 0 || p >= N || !active[p]) return; active[p] = false; for (int t = 0; t < 4; t++) pos[p][t] = -1; int al = 0, last = -1; for (int i = 0; i < N; i++) if (active[i]) { al++; last = i; } if (al <= 1) { over = true; winner = last; return; } if (turn == p) endTurn(); }
    // ---------- IA ----------
    float danger(int p, int ring) const { if (safeRing(ring)) return 0; float d = 0; for (int o = 0; o < N; o++) if (o != p && active[o]) for (int x : pos[o]) if (x >= 0 && x <= 63) { int dist = (ring - ringOf(seats[o], x) + RING) % RING; if (dist >= 1 && dist <= 6) d += 1; else if (dist <= 12 && dist > 0) d += 0.35f; } return d; }
    float scoreMove(int p, Mv m, float dang) const {
        Av it = avail[m.d]; Res r; tryMove(p, m.t, it.v, it.b, r); int x = pos[p][m.t], a = seats[p]; float sc = 0; if (it.s) sc -= 15; if (r.cp >= 0) sc += 60; if (r.goal) sc += 45; if (x < 0) sc += 38; if (r.to >= 64 && x < 64) sc += 22;
        if (r.to <= 63) { int ring = ringOf(a, r.to); if (safeRing(ring)) sc += 14; else sc -= 20 * dang * danger(p, ring); for (int k = 0; k < 4; k++) if (k != m.t && pos[p][k] >= 0 && pos[p][k] <= 63 && ringOf(a, pos[p][k]) == ring) sc += 16; }
        if (x >= 0 && x <= 63) { int cur = ringOf(a, x); if (!safeRing(cur)) sc += 12 * dang * std::min(2.f, danger(p, cur)); for (int k = 0; k < 4; k++) if (k != m.t && pos[p][k] >= 0 && pos[p][k] <= 63 && ringOf(a, pos[p][k]) == cur) sc -= 9; }
        return sc + r.to * 0.25f; }
    bool aiAct() {
        auto lg = legal(turn); if (lg.empty()) return false; int lv = std::max(1, std::min(3, pl[turn].level)); float err = lv == 1 ? 0.4f : lv == 2 ? 0.12f : 0.01f, noise = lv == 1 ? 30.f : lv == 2 ? 8.f : 1.f, dg = lv == 1 ? 0.f : lv == 2 ? 1.f : 1.3f;
        Mv best = lg[0]; if (lg.size() > 1) { if (rnd01() < err) best = lg[rndInt((int)lg.size())]; else { float bs = -1e9f; for (auto& m : lg) { float v = scoreMove(turn, m, dg) + rnd01() * noise; if (v > bs) { bs = v; best = m; } } } }
        return move(turn, best.t, best.d); }
    // ---------- control y dibujo ----------
    void update(float dt, Input& in, UI&) override {
        dieAnim = std::max(0.f, dieAnim - dt); anim = std::max(0.f, anim - dt); if (anim <= 0) animP = -1;
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) { int s = 0; for (int t = 0; t < 4; t++) if (pos[p][t] == FIN) s++; r.push_back({ s, p == winner ? 'w' : 'l' }); } finish(r, "Ganó " + (winner >= 0 ? pl[winner].name : std::string("nadie"))); } return; }
        if (dieAnim > 0 || anim > 0) return;
        if (pl[turn].ai) { aiT += dt; if (aiT < aiWait) return; aiT = 0; if (avail.empty()) doRoll(); else { if (!aiAct()) settle(); } return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
        if (pd && avail.empty() && (pd->rollDown())) doRoll();
        else if (pd && !avail.empty() && pd->down[B_Y] && canPass()) pass();
        else if (pd && !avail.empty() && pd->down[B_X] && avail.size() > 1) sel = (sel + 1) % (int)avail.size();
        if (autoMv && !avail.empty()) { auto l = legal(turn); if (l.size() == 1) { aiT += dt; if (aiT > 0.6f) { aiT = 0; move(turn, l[0].t, l[0].d); } } }
    }
    void animal(Gfx& g, int k, float x, float y, float r) {
        if (k == 0) { g.tri(x - r, y - r * .1f, x - r * .75f, y - r * 1.1f, x - r * .15f, y - r * .7f, rgb(0xd9661f)); g.tri(x + r, y - r * .1f, x + r * .75f, y - r * 1.1f, x + r * .15f, y - r * .7f, rgb(0xd9661f)); g.circle(x, y, r * .82f, rgb(0xf08a2c)); g.tri(x - r * .6f, y + r * .05f, x + r * .6f, y + r * .05f, x, y + r * .8f, rgb(0xffffff)); g.circle(x, y + r * .62f, r * .13f, rgb(0x222222)); g.circle(x - r * .3f, y - r * .2f, r * .11f, rgb(0x222222)); g.circle(x + r * .3f, y - r * .2f, r * .11f, rgb(0x222222)); }
        else if (k == 1) { g.circle(x - r * .45f, y - r * .6f, r * .33f, rgb(0x4aa83a)); g.circle(x + r * .45f, y - r * .6f, r * .33f, rgb(0x4aa83a)); g.circle(x, y + r * .05f, r * .86f, rgb(0x5ec14a)); g.circle(x - r * .45f, y - r * .62f, r * .2f, rgb(0xffffff)); g.circle(x + r * .45f, y - r * .62f, r * .2f, rgb(0xffffff)); g.circle(x - r * .45f, y - r * .6f, r * .09f, rgb(0x111111)); g.circle(x + r * .45f, y - r * .6f, r * .09f, rgb(0x111111)); g.rect({ x - r * .4f, y + r * .3f, r * .8f, std::max(1.f, r * .1f) }, rgb(0x1b5e20)); }
        else if (k == 2) { g.tri(x - r * .1f, y - r * .85f, x + r * .1f, y - r * .85f, x, y - r * 1.15f, rgb(0xe8a010)); g.circle(x, y, r * .85f, rgb(0xffe066)); g.tri(x - r * .22f, y + r * .02f, x + r * .22f, y + r * .02f, x, y + r * .4f, rgb(0xf08a1c)); g.circle(x - r * .35f, y - r * .22f, r * .11f, rgb(0x222222)); g.circle(x + r * .35f, y - r * .22f, r * .11f, rgb(0x222222)); g.circle(x - r * .55f, y + r * .15f, r * .13f, rgb(0xf06464, 130)); g.circle(x + r * .55f, y + r * .15f, r * .13f, rgb(0xf06464, 130)); }
        else { g.circle(x - r * .62f, y - r * .62f, r * .3f, rgb(0x8a5a2c)); g.circle(x + r * .62f, y - r * .62f, r * .3f, rgb(0x8a5a2c)); g.circle(x, y, r * .85f, rgb(0xa8723a)); g.circle(x, y + r * .25f, r * .38f, rgb(0xe6c79a)); g.circle(x, y + r * .15f, r * .13f, rgb(0x222222)); g.circle(x - r * .32f, y - r * .15f, r * .1f, rgb(0x222222)); g.circle(x + r * .32f, y - r * .15f, r * .1f, rgb(0x222222)); } }
    void die(Gfx& g, float cx, float cy, int v, bool on, bool bonus, int sx = 0, int sy = 0) {
        g.rrect({ cx - 32, cy - 32, 64, 64 }, 12, on ? pal::gold : rgb(0x6a4a2a)); g.rrect({ cx - 29, cy - 29, 58, 58 }, 10, rgb(0xfaf3dc));
        if (sx) { g.text(std::to_string(sx) + "+" + std::to_string(sy), cx, cy - 6, 22, rgb(0x2a1a0c), F_TITLE, AL_C); g.text("=5 sacar", cx, cy + 16, 11, rgb(0x2a1a0c), F_BODY, AL_C); return; }
        if (bonus) { g.text("+" + std::to_string(v), cx, cy, 26, rgb(0x2a1a0c), F_TITLE, AL_C); return; }
        static const int L[7][9] = { { 0 }, { 4 }, { 0, 8 }, { 0, 4, 8 }, { 0, 2, 6, 8 }, { 0, 2, 4, 6, 8 }, { 0, 2, 3, 5, 6, 8 } }; static const int CN2[7] = { 0, 1, 2, 3, 4, 5, 6 };
        for (int i = 0; i < CN2[v]; i++) { int c = L[v][i]; g.circle(cx - 15 + (c % 3) * 15, cy - 15 + (c / 3) * 15, 5, rgb(0x222222)); } }
    void draw(Gfx& g, UI& ui) override {
        const float U = 31.f, bx = 300, by = 62; auto SX = [&](float c) { return bx + c * U; }; auto SY = [&](float c) { return by + c * U; };
        bool mine = !over && !pl[turn].ai && avail.size() > 0 && anim <= 0 && dieAnim <= 0;
        g.rrect({ bx - 8, by - 8, U * 19 + 16, U * 19 + 16 }, 12, rgb(0x6b4224)); g.rect({ bx, by, U * 19, U * 19 }, rgb(0xf4ecd8));
        for (int a = 0; a < 4; a++) { bool used = false; for (int q = 0; q < N; q++) if (seats[q] == a) used = true; float cx = 9 + (-5.5f) * UA[a][0] + 5.5f * VA[a][0], cy = 9 + (-5.5f) * UA[a][1] + 5.5f * VA[a][1];
            g.rrect({ SX(cx - 3.6f), SY(cy - 3.6f), 7.2f * U, 7.2f * U }, U, used ? rgb(COL[a]) : rgb(0xcfc8b4)); g.rrect({ SX(cx - 3.1f), SY(cy - 3.1f), 6.2f * U, 6.2f * U }, U * .8f, rgb(0xf4ecd8));
            static const float SP[4][2] = { { -1.3f, -1.3f }, { 1.3f, -1.3f }, { -1.3f, 1.3f }, { 1.3f, 1.3f } }; for (int k = 0; k < 4; k++) g.circle(SX(cx + SP[k][0]), SY(cy + SP[k][1]), U * .7f, used ? rgb(COL[a], 85) : rgb(0xbbbbbb, 85));
            for (int q = 0; q < N; q++) if (seats[q] == a) { g.text(pl[q].name, SX(cx), SY(cy - 2.55f), 14, rgb(0x3a2f1a), F_BOLD, AL_C, U * 6); if (q == turn && !over) g.rrectLine({ SX(cx - 3.6f), SY(cy - 3.6f), 7.2f * U, 7.2f * U }, U, 4, pal::gold); } }
        for (int i = 0; i < RING; i++) { int cx, cy; ringCell(i, cx, cy); int k = i % 17; g.rect({ SX((float)cx), SY((float)cy), U, U }, k == 13 ? rgb(COL[i / 17]) : rgb(0xffffff)); g.rrectLine({ SX((float)cx), SY((float)cy), U, U }, 0, 1, rgb(0x8a7a58)); if (safeRing(i)) g.text("★", SX(cx + .5f), SY(cy + .55f), 20, k == 13 ? rgb(0xffffff) : rgb(0xb8a36a), F_BOLD, AL_C); }
        for (int a = 0; a < 4; a++) for (int t = 64; t <= 70; t++) { int cx, cy; homeCell(a, t, cx, cy); g.rect({ SX((float)cx), SY((float)cy), U, U }, rgb(COL[a], t == 70 ? 255 : 190)); g.rrectLine({ SX((float)cx), SY((float)cy), U, U }, 0, 1, rgb(0x8a7a58)); }
        static const float TR[4][5] = { { 8, 8, 11, 8, 0 }, { 11, 8, 11, 11, 3 }, { 11, 11, 8, 11, 2 }, { 8, 11, 8, 8, 1 } }; for (int i = 0; i < 4; i++) g.tri(SX(TR[i][0]), SY(TR[i][1]), SX(TR[i][2]), SY(TR[i][3]), SX(9.5f), SY(9.5f), rgb(COL[(int)TR[i][4]]));
        std::vector<Mv> lg = mine ? legal(turn) : std::vector<Mv>(); std::vector<int> mt; for (auto& m : lg) if (std::find(mt.begin(), mt.end(), m.t) == mt.end()) mt.push_back(m.t);
        std::map<int, std::vector<std::pair<int,int>>> cells; for (int p = 0; p < N; p++) if (active[p]) for (int t = 0; t < 4; t++) { float x, y; xyOf(p, t, x, y); cells[(int)(x * 2) * 1000 + (int)(y * 2)].push_back({ p, t }); }
        int firstId = -1;
        for (auto& kv : cells) { auto& v = kv.second; bool bar = v.size() >= 2 && pos[v[0].first][v[0].second] >= 0 && pos[v[0].first][v[0].second] <= 63; for (auto& e : v) if (e.first != v[0].first) bar = false;
            for (size_t j = 0; j < v.size(); j++) { int p = v[j].first, t = v[j].second; float x, y, lift = 0, sc = 1; xyOf(p, t, x, y);
                if (p == animP && t == animT && anim > 0) { float k = 1 - anim / 0.4f, e = k * k * (3 - 2 * k); x = ax0 + (ax1 - ax0) * e; y = ay0 + (ay1 - ay0) * e; lift = std::sin(k * 3.14159f) * .6f; sc = 1.15f; }
                bool multi = v.size() > 1 && pos[p][t] >= 0 && pos[p][t] != FIN; float sx = SX(x), sy = SY(y - lift); if (multi) { sx += (j % 2 ? 1 : -1) * U * 0.17f; sy += (j > 1 ? 1 : -1) * U * 0.17f; }
                float r = U * (multi ? 0.32f : 0.42f) * sc; bool mv = mine && p == turn && std::find(mt.begin(), mt.end(), t) != mt.end(); uint32_t id = hid("bp", p * 4 + t);
                Rect hr{ sx - U * 0.5f, sy - U * 0.5f, U, U }; bool hit = ui.hit(id, hr, mv); if (mv && firstId < 0) firstId = p * 4 + t;
                g.circle(sx, sy + r * 0.95f + lift * U, r * 0.8f, rgb(0, 60)); g.circle(sx, sy, r, rgb(DARKC[seats[p]])); g.circle(sx, sy, r - 2, rgb(COL[seats[p]])); animal(g, seats[p], sx, sy, r * .85f);
                if (mv) g.ring(sx, sy, r + 3, 3, rgb(0xffffff)); if (bar) g.ring(sx, sy, r + 2, 2, rgb(0x111111)); if (ui.focused(id)) ui.drawFocus(hr, r);
                if (hit) { std::vector<int> ds; for (auto& m : lg) if (m.t == t) ds.push_back(m.d); int d = std::find(ds.begin(), ds.end(), sel) != ds.end() ? sel : ds[0]; if (move(turn, t, d)) return; } } }
        if (firstId >= 0) ui.setFocusIfNone(hid("bp", firstId));
        std::string banner = over ? "¡Ganó " + (winner >= 0 ? pl[winner].name : std::string("nadie")) + "!" : (!pl[turn].ai ? pl[turn].name + ": " + (avail.empty() ? "tira los dados" : "mueve una ficha") : "Turno de " + pl[turn].name);
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        for (int p = 0; p < N; p++) { Rect r{ 930, 84.f + p * 62, 310, 54 }; bool on = p == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.circle(r.x + 22, r.cy(), 13, rgb(COL[seats[p]])); animal(g, seats[p], r.x + 22, r.cy(), 11);
            int home = 0; for (int t = 0; t < 4; t++) if (pos[p][t] == FIN) home++; g.text(pl[p].emoji + " " + pl[p].name, r.x + 44, r.y + 18, 15, on ? pal::gold : pal::white, F_BOLD, AL_L, 250); g.text(std::string(CN[seats[p]]) + " · en meta " + std::to_string(home) + "/4", r.x + 44, r.y + 38, 13, rgb(0xd8b46a), F_BODY, AL_L, 250); }
        // dados
        float dy = 400; if (!avail.empty()) { float w = avail.size() * 76.f, x0 = 1085 - w / 2 + 38; for (size_t i = 0; i < avail.size(); i++) { float dx = x0 + i * 76; Rect hr{ dx - 32, dy - 32, 64, 64 }; die(g, dx, dy, avail[i].v, (int)i == sel && mine, avail[i].b, avail[i].s ? avail[i].x0 : 0, avail[i].x1); if (mine && avail.size() > 1 && ui.hit(hid("bpd", (int)i), hr, true)) sel = (int)i; } }
        else die(g, 1050, dy, dieAnim > 0 ? 1 + rndInt(6) : ad[0], false, false), die(g, 1120, dy, dieAnim > 0 ? 1 + rndInt(6) : ad[1], false, false);
        if (!over && !pl[turn].ai && avail.empty() && anim <= 0 && dieAnim <= 0) { if (ui.button("Tirar dados", { 930, 470, 300, 50 }, 3)) { doRoll(); return; } ui.setFocusIfNone(hid("Tirar dados", 930 * 7 + 470)); }
        if (mine && canPass() && ui.button("Pass (Y)", { 960, 520, 250, 36 }, 3)) { pass(); return; }
        g.text(msg, 1085, 570, 14, pal::cream, F_BODY, AL_C, 300);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Tirar dados" }, { "DPAD/STICK", "Elegir ficha" }, { "A", "Mover" }, { "X", "Cambiar dado" }, { "Y", "Pass" } }; }
    std::string help() const override { return "Cada jugador tiene 4 fichas y se tiran dos dados. Con un 5 (o si los dos dados suman 5) sale una ficha de la base (si se puede, es obligatorio). Podés omitir el segundo dado con Pass. Usás cada dado con una ficha. Los dobles repiten el turno (tres seguidos mandan a casa la última ficha movida). Caer sobre una ficha rival la manda a su base, salvo en las casillas con estrella, y da 20 movidas extra. Llegar a la meta da 10. Dos fichas del mismo color juntas forman una barrera que nadie puede pasar. Gana quien lleva sus 4 fichas a la meta."; }
};
}
Game* makeBigParch() { return new BigParch(); }
