// Ajedrez completo (enroque, al paso, jaque mate, tablas). La IA usa alfa-beta de profundidad 1-3. La coronación es siempre a dama.
#include "../app.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace {
enum { P = 1, N = 2, B = 3, R = 4, Q = 5, K = 6 };
struct Mv { int8_t from, to, promo; int8_t cap; };
struct St {
    int8_t b[64]; int turn = 0; int castle = 15; int ep = -1; int half = 0;   // castle: 1 K blanco, 2 Q blanco, 4 k negro, 8 q negro
    void init() {
        static const int back[8] = { R, N, B, Q, K, B, N, R };
        for (int i = 0; i < 64; i++) b[i] = 0; for (int c = 0; c < 8; c++) { b[c] = -back[c]; b[8 + c] = -P; b[48 + c] = P; b[56 + c] = back[c]; } turn = 0; castle = 15; ep = -1; half = 0;
    }
};
inline int colorOf(int v) { return v > 0 ? 0 : v < 0 ? 1 : -1; }
bool attacked(const St& s, int sq, int by) {   // ¿la casilla sq está atacada por el bando 'by'?
    int r = sq / 8, c = sq % 8, sg = by == 0 ? 1 : -1;
    int pr = r + (by == 0 ? 1 : -1); for (int dc = -1; dc <= 1; dc += 2) { int cc = c + dc; if (pr >= 0 && pr < 8 && cc >= 0 && cc < 8 && s.b[pr * 8 + cc] == sg * P) return true; }
    static const int KN[8][2] = { {1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1} }; for (auto& d : KN) { int rr = r + d[0], cc = c + d[1]; if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && s.b[rr * 8 + cc] == sg * N) return true; }
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { if (!dr && !dc) continue; int rr = r + dr, cc = c + dc; if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && s.b[rr * 8 + cc] == sg * K) return true; }
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { if (!dr && !dc) continue; bool diag = dr && dc; int rr = r + dr, cc = c + dc;
        while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) { int v = s.b[rr * 8 + cc]; if (v) { if (colorOf(v) == by) { int a = std::abs(v); if (a == Q || (diag && a == B) || (!diag && a == R)) return true; } break; } rr += dr; cc += dc; } }
    return false;
}
int kingSq(const St& s, int col) { int k = (col == 0 ? K : -K); for (int i = 0; i < 64; i++) if (s.b[i] == k) return i; return -1; }
bool inCheck(const St& s, int col) { int k = kingSq(s, col); return k >= 0 && attacked(s, k, 1 - col); }
St apply(const St& s, const Mv& m) {
    St n = s; int v = n.b[m.from], a = std::abs(v), col = colorOf(v); int fr = m.from / 8, tc = m.to % 8, fc = m.from % 8;
    n.half = (a == P || s.b[m.to]) ? 0 : s.half + 1; n.b[m.to] = v; n.b[m.from] = 0; n.ep = -1;
    if (a == P) { if (m.to == s.ep && fc != tc) n.b[(fr) * 8 + tc] = 0; if (std::abs(m.to / 8 - fr) == 2) n.ep = (fr + m.to / 8) / 2 * 8 + fc; if (m.promo) n.b[m.to] = col == 0 ? m.promo : -m.promo; }
    if (a == K) { if (std::abs(tc - fc) == 2) { if (tc > fc) { n.b[m.to - 1] = n.b[m.to + 1]; n.b[m.to + 1] = 0; } else { n.b[m.to + 1] = n.b[m.to - 2]; n.b[m.to - 2] = 0; } } n.castle &= col == 0 ? ~3 : ~12; }
    if (m.from == 63 || m.to == 63) n.castle &= ~1; if (m.from == 56 || m.to == 56) n.castle &= ~2; if (m.from == 7 || m.to == 7) n.castle &= ~4; if (m.from == 0 || m.to == 0) n.castle &= ~8;
    n.turn = 1 - s.turn; return n;
}
void pseudo(const St& s, std::vector<Mv>& out) {
    int col = s.turn, sg = col == 0 ? 1 : -1;
    for (int i = 0; i < 64; i++) { int v = s.b[i]; if (!v || colorOf(v) != col) continue; int a = std::abs(v), r = i / 8, c = i % 8;
        auto add = [&](int to, int promo = 0) { out.push_back({ (int8_t)i, (int8_t)to, (int8_t)promo, (int8_t)std::abs(s.b[to]) }); };
        if (a == P) { int dir = col == 0 ? -1 : 1, nr = r + dir; bool last = nr == 0 || nr == 7;
            if (nr >= 0 && nr < 8 && !s.b[nr * 8 + c]) { add(nr * 8 + c, last ? Q : 0); int sr = col == 0 ? 6 : 1; if (r == sr && !s.b[(r + 2 * dir) * 8 + c]) add((r + 2 * dir) * 8 + c); }
            for (int dc = -1; dc <= 1; dc += 2) { int cc = c + dc; if (nr < 0 || nr > 7 || cc < 0 || cc > 7) continue; int to = nr * 8 + cc; if ((s.b[to] && colorOf(s.b[to]) != col) || to == s.ep) { out.push_back({ (int8_t)i, (int8_t)to, (int8_t)(last ? Q : 0), (int8_t)(s.b[to] ? std::abs(s.b[to]) : P) }); } } }
        else if (a == N || a == K) { static const int KN[8][2] = { {1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1} }, KG[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
            for (int k = 0; k < 8; k++) { int rr = r + (a == N ? KN[k][0] : KG[k][0]), cc = c + (a == N ? KN[k][1] : KG[k][1]); if (rr < 0 || rr > 7 || cc < 0 || cc > 7) continue; int to = rr * 8 + cc; if (colorOf(s.b[to]) != col) add(to); }
            if (a == K) { int hr = col == 0 ? 7 : 0; if (i == hr * 8 + 4 && !attacked(s, i, 1 - col)) {
                if ((s.castle & (col == 0 ? 1 : 4)) && s.b[i + 1] == 0 && s.b[i + 2] == 0 && s.b[i + 3] == sg * R && !attacked(s, i + 1, 1 - col) && !attacked(s, i + 2, 1 - col)) add(i + 2);
                if ((s.castle & (col == 0 ? 2 : 8)) && s.b[i - 1] == 0 && s.b[i - 2] == 0 && s.b[i - 3] == 0 && s.b[i - 4] == sg * R && !attacked(s, i - 1, 1 - col) && !attacked(s, i - 2, 1 - col)) add(i - 2); } } }
        else for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) { if (!dr && !dc) continue; bool diag = dr && dc; if (a == B && !diag) continue; if (a == R && diag) continue; int rr = r + dr, cc = c + dc;
            while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) { int to = rr * 8 + cc; if (s.b[to]) { if (colorOf(s.b[to]) != col) add(to); break; } add(to); rr += dr; cc += dc; } }
    }
}
std::vector<Mv> legal(const St& s) { std::vector<Mv> ps, out; pseudo(s, ps); for (auto& m : ps) { St n = apply(s, m); if (!inCheck(n, s.turn)) out.push_back(m); } return out; }
const int VAL[7] = { 0, 100, 320, 330, 500, 900, 0 };
int evalS(const St& s) {   // desde el punto de vista del bando que mueve
    int sc = 0; for (int i = 0; i < 64; i++) { int v = s.b[i]; if (!v) continue; int a = std::abs(v), r = i / 8, c = i % 8, col = colorOf(v), rr = col == 0 ? 7 - r : r; int x = VAL[a]; float cen = 3.5f - std::max(std::abs(c - 3.5f), std::abs(r - 3.5f)) + 0.f; (void)cen;
        int dc = (int)(3.5f - std::abs(c - 3.5f)), dr = (int)(3.5f - std::abs(r - 3.5f)); if (a == P) x += rr * 6 + dc * 3; else if (a == N || a == B) x += (dc + dr) * 5; else if (a == Q) x += (dc + dr); else if (a == K) x -= (dc + dr) * 2; if (a == R && rr == 6) x += 15;
        sc += col == s.turn ? x : -x; }
    return sc;
}
long nodes = 0, budget = 0;
int search(const St& s, int depth, int alpha, int beta, int ply) {
    nodes++; auto ms = legal(s);
    if (ms.empty()) return inCheck(s, s.turn) ? -30000 + ply : 0;
    if (depth <= 0) { int st = evalS(s); if (st >= beta) return st; if (nodes > budget) return st; alpha = std::max(alpha, st);
        std::vector<Mv> caps; for (auto& m : ms) if (m.cap && ply < 8) caps.push_back(m); std::sort(caps.begin(), caps.end(), [](const Mv& a, const Mv& b) { return a.cap > b.cap; });
        for (auto& m : caps) { int v = -search(apply(s, m), 0, -beta, -alpha, ply + 1); if (v > alpha) { alpha = v; if (alpha >= beta) break; } } return alpha; }
    std::sort(ms.begin(), ms.end(), [&](const Mv& a, const Mv& b) { return a.cap * 10 - std::abs(s.b[a.from]) > b.cap * 10 - std::abs(s.b[b.from]); });
    for (auto& m : ms) { int v = -search(apply(s, m), depth - 1, -beta, -alpha, ply + 1); if (v > alpha) { alpha = v; if (alpha >= beta) break; } }
    return alpha;
}
Mv chooseMove(const St& s, int lv, bool& ok) {
    auto ms = legal(s); ok = !ms.empty(); if (ms.empty()) return {}; int depth = lv <= 1 ? 1 : lv == 2 ? 2 : 3; float err = lv == 1 ? 0.25f : lv == 2 ? 0.05f : 0.f; if (rnd01() < err) return ms[rndInt((int)ms.size())];
    nodes = 0; budget = lv == 1 ? 3000 : lv == 2 ? 30000 : 120000; int best = -1000000; Mv bm = ms[0];
    std::sort(ms.begin(), ms.end(), [&](const Mv& a, const Mv& b) { return a.cap > b.cap; });
    for (auto& m : ms) { int v = -search(apply(s, m), depth - 1, -1000000, 1000000, 1) + (int)(rnd01() * (lv == 1 ? 60 : lv == 2 ? 12 : 3)); if (v > best) { best = v; bm = m; } }
    return bm;
}
uint64_t hashS(const St& s) { uint64_t h = 1469598103934665603ull; for (int i = 0; i < 64; i++) { h ^= (uint8_t)(s.b[i] + 8); h *= 1099511628211ull; } h ^= s.turn + 4 * s.castle + 64 * (s.ep + 1); h *= 1099511628211ull; return h; }

void drawShape(Gfx& g, float cx, float cy, float s, int a, Col c) {
    g.rrect({ cx - 0.34f * s, cy + 0.26f * s, 0.68f * s, 0.12f * s }, 0.05f * s, c);
    switch (a) {
    case P: g.circle(cx, cy - 0.16f * s, 0.15f * s, c); g.tri(cx - 0.2f * s, cy + 0.26f * s, cx + 0.2f * s, cy + 0.26f * s, cx, cy - 0.08f * s, c); break;
    case R: g.rect({ cx - 0.2f * s, cy - 0.12f * s, 0.4f * s, 0.4f * s }, c); g.rect({ cx - 0.27f * s, cy - 0.3f * s, 0.54f * s, 0.2f * s }, c); break;
    case N: g.tri(cx - 0.22f * s, cy + 0.26f * s, cx + 0.24f * s, cy + 0.26f * s, cx + 0.06f * s, cy - 0.32f * s, c); g.tri(cx - 0.3f * s, cy - 0.02f * s, cx + 0.06f * s, cy - 0.32f * s, cx + 0.1f * s, cy + 0.1f * s, c); g.circle(cx + 0.06f * s, cy - 0.2f * s, 0.12f * s, c); break;
    case B: g.circle(cx, cy - 0.22f * s, 0.12f * s, c); g.tri(cx - 0.22f * s, cy + 0.26f * s, cx + 0.22f * s, cy + 0.26f * s, cx, cy - 0.28f * s, c); g.circle(cx, cy - 0.04f * s, 0.13f * s, c); break;
    case Q: g.tri(cx - 0.24f * s, cy + 0.26f * s, cx + 0.24f * s, cy + 0.26f * s, cx, cy - 0.12f * s, c); for (int k = -2; k <= 2; k++) g.circle(cx + k * 0.12f * s, cy - 0.22f * s + std::abs(k) * 0.03f * s, 0.07f * s, c); g.circle(cx, cy - 0.06f * s, 0.15f * s, c); break;
    case K: g.tri(cx - 0.24f * s, cy + 0.26f * s, cx + 0.24f * s, cy + 0.26f * s, cx, cy - 0.16f * s, c); g.circle(cx, cy - 0.1f * s, 0.13f * s, c); g.rect({ cx - 0.03f * s, cy - 0.4f * s, 0.06f * s, 0.2f * s }, c); g.rect({ cx - 0.1f * s, cy - 0.34f * s, 0.2f * s, 0.06f * s }, c); break;
    }
}
void drawPiece(Gfx& g, float cx, float cy, float s, int a, bool white) {
    g.circle(cx + 2, cy + 0.32f * s, 0.3f * s, rgb(0, 70)); drawShape(g, cx, cy, s * 1.1f, a, white ? rgb(0x4a3a24) : rgb(0xd8c8a0)); drawShape(g, cx, cy, s, a, white ? rgb(0xfff8e8) : rgb(0x2a2220));
}

struct Chess : Game {
    St st; std::vector<Mv> lg; int sel = -1, lastFrom = -1, lastTo = -1, winner = -1, resigned = -1, bottom = 0; bool over = false, draw_ = false; std::string result, msg; float aiT = 0, aiWait = 0.8f, endT = 0; std::map<uint64_t, int> seen; int plies = 0;
    void start() override { st.init(); lg = legal(st); seen[hashS(st)] = 1; resetAi(); }
    void resetAi() { aiT = 0; aiWait = 0.6f + rnd01() * 0.5f; }
    int turnPlayer() const override { return over ? -1 : st.turn; }
    void doMove(const Mv& m) {
        bool cap = st.b[m.from] != 0 && st.b[m.to] != 0; st = apply(st, m); lastFrom = m.from; lastTo = m.to; sel = -1; plies++; app->audio.play(cap || m.cap ? "win" : "place"); lg = legal(st); int rep = ++seen[hashS(st)];
        bool chk = inCheck(st, st.turn); msg = chk ? "¡Jaque!" : "";
        if (lg.empty()) { over = true; if (chk) { winner = 1 - st.turn; result = "Jaque mate. Ganó " + pl[winner].name; } else { draw_ = true; result = "Tablas por rey ahogado"; } }
        else if (rep >= 3) { over = true; draw_ = true; result = "Tablas por repetición"; } else if (st.half >= 100) { over = true; draw_ = true; result = "Tablas (50 jugadas)"; } else if (plies > 300) { over = true; draw_ = true; result = "Tablas (partida larga)"; }
        else { int cnt = 0; bool minor = false, other = false; for (int i = 0; i < 64; i++) { int a = std::abs(st.b[i]); if (!a || a == K) continue; cnt++; if (a == N || a == B) minor = true; else other = true; } if (!other && cnt <= 1) { (void)minor; over = true; draw_ = true; result = "Tablas por material insuficiente"; } }
        resetAi();
        bool bothHuman = !pl[0].ai && !pl[1].ai; if (bothHuman) bottom = st.turn;
    }
    void resign(int p) override { over = true; resigned = p; winner = 1 - p; result = pl[p].name + " se rindió"; }
    void update(float dt, Input& in, UI&) override {
        if (!over && !pl[st.turn].ai && pl[st.turn].pad >= 0 && pl[st.turn].pad < Input::MAXP && in.pads[pl[st.turn].pad].down[B_B]) sel = -1;
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r = { { winner == 0 ? 1 : 0, winner == 0 ? 'w' : winner < 0 ? 'd' : 'l' }, { winner == 1 ? 1 : 0, winner == 1 ? 'w' : winner < 0 ? 'd' : 'l' } }; if (resigned >= 0) r[resigned].r = 'x'; finish(r, result); } return; }
        if (!pl[st.turn].ai) return; aiT += dt; if (aiT < aiWait) return; bool ok; Mv m = chooseMove(st, pl[st.turn].level, ok); if (ok) doMove(m); else over = true;
    }
    void draw(Gfx& g, UI& ui) override {
        float S = 640, cs = S / 8, x0 = 790 - S / 2, y0 = 34; bool flip = bottom == 1; auto cellRect = [&](int i) { int r = i / 8, c = i % 8; if (flip) { r = 7 - r; c = 7 - c; } return Rect{ x0 + c * cs, y0 + r * cs, cs, cs }; };
        g.rrect({ x0 - 22, y0 - 18, S + 44, S + 44 }, 16, rgb(0, 110)); g.rrectGrad({ x0 - 24, y0 - 24, S + 48, S + 48 }, 16, rgb(0x8a5a2c), rgb(0x4a2c12));
        bool mine = !over && !pl[st.turn].ai; std::vector<int> movable, targets; for (auto& m : lg) { if (std::find(movable.begin(), movable.end(), (int)m.from) == movable.end()) movable.push_back(m.from); if (m.from == sel) targets.push_back(m.to); }
        int kchk = inCheck(st, st.turn) ? kingSq(st, st.turn) : -1; int firstId = -1;
        for (int i = 0; i < 64; i++) {
            Rect r = cellRect(i); bool dark = ((i / 8) + (i % 8)) % 2 == 1; g.rect(r, dark ? rgb(0x7a4a26) : rgb(0xe8cfa0)); if (i == lastFrom || i == lastTo) g.rect(r, rgb(0xffe27a, 80)); if (i == kchk) g.rect(r, rgb(0xff3a2a, 120));
            uint32_t id = hid("ch", i); bool isT = std::find(targets.begin(), targets.end(), i) != targets.end(); bool canPick = mine && std::find(movable.begin(), movable.end(), i) != movable.end(); bool hit = ui.hit(id, r, mine && (canPick || isT)); if (firstId < 0 && canPick) firstId = i;
            int v = st.b[i]; if (v) drawPiece(g, r.cx(), r.cy(), cs * 1.2f, std::abs(v), v > 0);
            if (isT) { if (v) g.ring(r.cx(), r.cy(), cs * 0.44f, 4, rgb(0xff6a4a, 220)); else g.circle(r.cx(), r.cy(), cs * 0.13f, rgb(0x2a8a50, 200)); }
            if (canPick && !ui.padNav && i != sel) g.ring(r.cx(), r.cy(), cs * 0.44f, 2, rgb(0xffe27a, 140)); if (i == sel) g.ring(r.cx(), r.cy(), cs * 0.46f, 5, pal::gold);
            if (ui.focused(id)) ui.drawFocus(r, 4);
            if (hit) { if (isT) { Mv mm{}; for (auto& m : lg) if (m.from == sel && m.to == i) mm = m; doMove(mm); return; } else if (canPick) sel = (sel == i) ? -1 : i; }
        }
        if (firstId >= 0) ui.setFocusIfNone(hid("ch", firstId));
        for (int p = 0; p < 2; p++) { float y = (p == bottom) ? y0 + S + 34 : y0 - 22; bool on = p == st.turn && !over; g.rrect({ 790 - 220, y - 15, 440, 30 }, 15, rgb(0x120c08, 200)); g.text(pl[p].emoji + " " + pl[p].name + (p == 0 ? "  (blancas)" : "  (negras)"), 790, y, 17, on ? pal::gold : pal::white, F_BOLD, AL_C); }
        std::string banner = over ? result : pl[st.turn].ai ? "Piensa " + pl[st.turn].name + "…" : "Turno de " + pl[st.turn].name; g.textBox(banner + (msg.empty() || over ? "" : "\n" + msg), { 1150, 260, 120, 200 }, 18, pal::gold, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir / mover" }, { "B", "Soltar" } }; }
    std::string help() const override { return "Ajedrez completo: enroque, captura al paso y jaque mate. Elige una pieza y luego su destino. Los peones coronan siempre a dama. Hay tablas por rey ahogado, repetición, 50 jugadas o material insuficiente."; }
};
}
Game* makeAjedrez() { return new Chess(); }
