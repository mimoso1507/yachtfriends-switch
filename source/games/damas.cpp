// Damas 8x8 o 10x10 vistas desde arriba: captura obligatoria, ley de mayoría, dama voladora y comer para atrás (elegibles).
// Cada jugador humano ve sus fichas abajo en su turno. IA minimax alfa-beta igual a la versión de PC.
#include "../app.h"
#include <algorithm>
#include <cfloat>

namespace {
const int LOSS = 100000;
struct Mv { int from, to, cap; };   // cap = -1 si no captura
inline int own(int v) { return (v == 1 || v == 3) ? 0 : (v == 2 || v == 4) ? 1 : -1; }
inline bool isKing(int v) { return v >= 3; }
struct Rules {
    int size = 8; bool force = true, majority = true, flying = true, back = true;
    bool inb(int r, int c) const { return r >= 0 && r < size && c >= 0 && c < size; }
    void dirsFor(int v, std::vector<std::pair<int, int>>& d) const {
        d.clear(); if (isKing(v)) d = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } }; else if (own(v) == 0) d = { { -1, -1 }, { -1, 1 } }; else d = { { 1, -1 }, { 1, 1 } };
    }
    void caps(const std::vector<int8_t>& b, int i, std::vector<Mv>& out) const {
        int v = b[i]; if (!v) return; int r0 = i / size, c0 = i % size; std::vector<std::pair<int, int>> D;
        if (isKing(v) && flying) {
            dirsFor(v, D);
            for (auto& d : D) {
                int r = r0 + d.first, c = c0 + d.second, seen = -1;
                while (inb(r, c)) {
                    int w = b[r * size + c];
                    if (!w) { if (seen >= 0) out.push_back({ i, r * size + c, seen }); r += d.first; c += d.second; continue; }
                    if (seen < 0 && own(w) != own(v)) { seen = r * size + c; r += d.first; c += d.second; continue; }
                    break;
                }
            }
        } else {
            if (back) D = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } }; else dirsFor(v, D);
            for (auto& d : D) {
                int r2 = r0 + d.first * 2, c2 = c0 + d.second * 2; if (!inb(r2, c2)) continue;
                int mid = b[(r0 + d.first) * size + c0 + d.second];
                if (mid && own(mid) != own(v) && b[r2 * size + c2] == 0) out.push_back({ i, r2 * size + c2, (r0 + d.first) * size + c0 + d.second });
            }
        }
    }
    void simple(const std::vector<int8_t>& b, int i, std::vector<Mv>& out) const {
        int v = b[i]; if (!v) return; int r0 = i / size, c0 = i % size; std::vector<std::pair<int, int>> D; dirsFor(v, D);
        for (auto& d : D) {
            if (isKing(v) && flying) { int r = r0 + d.first, c = c0 + d.second; while (inb(r, c) && b[r * size + c] == 0) { out.push_back({ i, r * size + c, -1 }); r += d.first; c += d.second; } }
            else { int r = r0 + d.first, c = c0 + d.second; if (inb(r, c) && b[r * size + c] == 0) out.push_back({ i, r * size + c, -1 }); }
        }
    }
    int chainLen(const std::vector<int8_t>& b, const Mv& m, int depth) const {
        std::vector<int8_t> b2 = b; b2[m.cap] = 0; b2[m.to] = b2[m.from]; b2[m.from] = 0;
        std::vector<Mv> f; caps(b2, m.to, f); if (f.empty()) return depth;
        int mx = 0; for (auto& x : f) mx = std::max(mx, chainLen(b2, x, depth + 1)); return mx;
    }
    void allCaps(const std::vector<int8_t>& b, int p, std::vector<Mv>& out) const {
        for (int i = 0; i < (int)b.size(); i++) if (b[i] && own(b[i]) == p) caps(b, i, out);
        if (majority && !out.empty()) {
            std::vector<int> len; int best = 0; for (auto& m : out) { len.push_back(chainLen(b, m, 1)); best = std::max(best, len.back()); }
            std::vector<Mv> f; for (size_t k = 0; k < out.size(); k++) if (len[k] == best) f.push_back(out[k]); out = f;
        }
    }
    struct St { std::vector<int8_t> b; int turn; int chain; };
    void legal(const St& st, std::vector<Mv>& out) const {
        out.clear();
        if (st.chain >= 0) { caps(st.b, st.chain, out); return; }
        allCaps(st.b, st.turn, out);
        if (force && !out.empty()) return;
        for (int i = 0; i < (int)st.b.size(); i++) if (st.b[i] && own(st.b[i]) == st.turn) simple(st.b, i, out);
    }
    St step(const St& st, const Mv& m, bool* crownedOut = nullptr) const {
        St n{ st.b, st.turn, -1 }; int v = n.b[m.from]; n.b[m.from] = 0; n.b[m.to] = v; bool capd = m.cap >= 0; if (capd) n.b[m.cap] = 0;
        int r = m.to / size; bool crowned = false;
        if (v == 1 && r == 0) { n.b[m.to] = 3; crowned = true; } if (v == 2 && r == size - 1) { n.b[m.to] = 4; crowned = true; }
        if (crownedOut) *crownedOut = crowned;
        if (capd && !crowned) { std::vector<Mv> f; caps(n.b, m.to, f); if (!f.empty()) { n.chain = m.to; return n; } }
        n.turn = 1 - st.turn; return n;
    }
    double evaluate(const std::vector<int8_t>& b) const {
        double KV = flying ? 320 : 230, sc = 0;
        for (int i = 0; i < (int)b.size(); i++) {
            int v = b[i]; if (!v) continue; int r = i / size, c = i % size; double cen = 3.5 - fabs(c - (size - 1) / 2.0);
            if (v == 1) sc += 100 + (size - 1 - r) * 4 + (r == size - 1 ? 7 : 0) + cen;
            else if (v == 2) sc -= 100 + r * 4 + (r == 0 ? 7 : 0) + cen;
            else if (v == 3) sc += KV + cen * 2; else sc -= KV + cen * 2;
        }
        return sc;
    }
    double search(const St& st, int depth, double alpha, double beta, int& n, int lim) const {
        n++;
        std::vector<Mv> ms; legal(st, ms);
        if (ms.empty()) return st.turn == 0 ? -LOSS - depth : LOSS + depth;
        if ((depth <= 0 && st.chain < 0) || n > lim) return evaluate(st.b);
        std::stable_sort(ms.begin(), ms.end(), [](const Mv& x, const Mv& y) { return (x.cap >= 0) > (y.cap >= 0); });
        if (st.turn == 0) {
            double best = -DBL_MAX;
            for (auto& m : ms) { St ns = step(st, m); double sc = search(ns, ns.turn == st.turn ? depth : depth - 1, alpha, beta, n, lim); best = std::max(best, sc); alpha = std::max(alpha, best); if (alpha >= beta) break; }
            return best;
        }
        double best = DBL_MAX;
        for (auto& m : ms) { St ns = step(st, m); double sc = search(ns, ns.turn == st.turn ? depth : depth - 1, alpha, beta, n, lim); best = std::min(best, sc); beta = std::min(beta, best); if (alpha >= beta) break; }
        return best;
    }
};
struct Lvl { int fix, dd; double limK, err, noise; int top; float d; };
const Lvl LVL[4] = { {}, { 2, 0, 0.5, 0.42, 30, 99, 1.4f }, { 0, 0, 1, 0.12, 2, 3, 1.0f }, { 0, 1, 2, 0.015, 0.5, 2, 0.75f } };

struct Damas : Game {
    Rules R; Rules::St st; bool over = false; int winner = -1, plies = 0, sel = -1, bottom = 0, lastFrom = -1, lastTo = -1;
    std::vector<Mv> legalNow; float aiT = 0, endT = -1, anim = 0;
    void start() override {
        int preset = opts.get("preset"); R.size = preset == 0 ? 10 : 8;
        R.force = opts.get("forceCapture", 1); R.majority = opts.get("majorityCapture", 1); R.flying = opts.get("flyingKing", 1); R.back = opts.get("backCapture", 1);
        int n = R.size, rows = n == 10 ? 4 : 3; st.b.assign(n * n, 0); st.turn = 0; st.chain = -1;
        for (int r = 0; r < rows; r++) for (int c = 0; c < n; c++) if ((r + c) % 2 == 1) st.b[r * n + c] = 2;
        for (int r = n - rows; r < n; r++) for (int c = 0; c < n; c++) if ((r + c) % 2 == 1) st.b[r * n + c] = 1;
        R.legal(st, legalNow);
        bottom = !pl[0].ai ? 0 : !pl[1].ai ? 1 : 0;
    }
    int turnPlayer() const override { return over ? -1 : st.turn; }
    void doMove(const Mv& m) {
        bool crowned = false; Rules::St n = R.step(st, m, &crowned);
        lastFrom = m.from; lastTo = m.to; anim = 0.22f; app->audio.play(m.cap >= 0 ? "flip" : "place");
        bool sameTurn = n.turn == st.turn; st = n; sel = sameTurn ? st.chain : -1;
        if (!sameTurn) {
            plies++;
            bool w = false, b = false; for (int v : st.b) { if (own(v) == 0) w = true; if (own(v) == 1) b = true; }
            R.legal(st, legalNow);
            if (!w) { over = true; winner = 1; } else if (!b) { over = true; winner = 0; } else if (legalNow.empty()) { over = true; winner = 1 - st.turn; }
            else if (plies >= 300) { over = true; winner = -1; }
            if (!over && !pl[st.turn].ai) bottom = st.turn;   // el humano de turno ve sus fichas abajo
            if (over) endT = 0;
        } else R.legal(st, legalNow);
        aiT = 0;
    }
    void resign(int p) override { over = true; winner = 1 - p; endT = 0.5f; }
    Mv aiMove(int lv) {
        const Lvl& P = LVL[lv]; if (legalNow.size() == 1) return legalNow[0];
        int cnt = 0; for (int v : st.b) if (v) cnt++;
        int depth = R.size == 10 ? 4 : 5; if (cnt <= 8) depth += 2; else if (cnt <= 12) depth += 1;
        if (P.fix) depth = P.fix; else depth += P.dd;
        int lim = (int)lround((R.size == 10 ? 1500 : 3000) * P.limK);
        struct O { Mv m; double s; }; std::vector<O> out;
        for (auto& m : legalNow) { int n = 0; Rules::St ns = R.step(st, m); double sc = R.search(ns, ns.turn == st.turn ? depth : depth - 1, -DBL_MAX, DBL_MAX, n, lim); out.push_back({ m, (st.turn == 0 ? sc : -sc) + rnd01() * P.noise }); }
        std::sort(out.begin(), out.end(), [](const O& a, const O& b) { return a.s > b.s; });
        if (P.top < 99 && out[0].s >= LOSS / 2) return out[0].m;
        if (out.size() > 1 && rnd01() < P.err) {
            if (P.top >= 99) return out[rndInt((int)out.size())].m;
            std::vector<O> ok; for (size_t i = 0; i < out.size() && (int)i < P.top; i++) if (out[i].s > -LOSS / 2) ok.push_back(out[i]);
            if (!ok.empty()) return ok[rndInt((int)ok.size())].m;
        }
        return out[0].m;
    }
    void update(float dt, Input&, UI&) override {
        anim = std::max(0.f, anim - dt);
        if (over) {
            if (endT >= 0 && (endT += dt) > 1.6f && !finished) {
                std::vector<PResult> r(2); int cnt[2] = {}; for (int v : st.b) if (own(v) >= 0) cnt[own(v)]++;
                for (int i = 0; i < 2; i++) { r[i].pts = cnt[i]; r[i].r = winner < 0 ? 'd' : winner == i ? 'w' : 'l'; }
                finish(r, winner < 0 ? "Empate por cantidad de jugadas" : "Ganó " + pl[winner].name);
            }
            return;
        }
        if (!pl[st.turn].ai || anim > 0) return;
        aiT += dt; if (aiT < LVL[pl[st.turn].level].d * (st.chain >= 0 ? 0.6f : 1.f)) return;
        doMove(aiMove(pl[st.turn].level));
    }
    void draw(Gfx& g, UI& ui) override {
        int n = R.size; float S = 640, cs = S / n, x0 = 790 - S / 2, y0 = 34; bool flip = bottom == 1;
        auto cellRect = [&](int i) { int r = i / n, c = i % n; if (flip) { r = n - 1 - r; c = n - 1 - c; } return Rect{ x0 + c * cs, y0 + r * cs, cs, cs }; };
        g.rrect({ x0 - 22, y0 - 18, S + 44, S + 44 }, 16, rgb(0, 110)); g.rrectGrad({ x0 - 24, y0 - 24, S + 48, S + 48 }, 16, rgb(0x8a5a2c), rgb(0x4a2c12));
        bool mine = !over && !pl[st.turn].ai && anim <= 0;
        std::vector<int> movable; for (auto& m : legalNow) if (std::find(movable.begin(), movable.end(), m.from) == movable.end()) movable.push_back(m.from);
        for (int i = 0; i < n * n; i++) {
            Rect r = cellRect(i); bool dark = ((i / n) + (i % n)) % 2 == 1;
            g.rect(r, dark ? rgb(0x7a4a26) : rgb(0xe8cfa0));
            if (dark) g.gradRect(r, rgb(0xffffff, 10), rgb(0, 30));
            if (i == lastFrom || i == lastTo) g.rect(r, rgb(0xffe27a, 70));
        }
        // destinos de la ficha elegida
        std::vector<Mv> targets; if (sel >= 0) for (auto& m : legalNow) if (m.from == sel) targets.push_back(m);
        for (int i = 0; i < n * n; i++) {
            if (((i / n) + (i % n)) % 2 != 1) continue;
            Rect r = cellRect(i); int v = st.b[i]; uint32_t id = hid("dm", i);
            bool canPick = mine && std::find(movable.begin(), movable.end(), i) != movable.end() && st.chain < 0;
            bool isTarget = false; for (auto& m : targets) if (m.to == i) isTarget = true;
            bool hit = ui.hit(id, r, mine && (canPick || isTarget || (st.chain >= 0 && i == st.chain)));
            if (v) {
                float cx = r.cx(), cy = r.cy(), rad = cs * 0.4f; bool white = own(v) == 0;
                if (anim > 0 && i == lastTo) { Rect f = cellRect(lastFrom); float t = anim / 0.22f; cx = cx + (f.cx() - cx) * t; cy = cy + (f.cy() - cy) * t; }
                g.circle(cx + 2, cy + 4, rad, rgb(0, 100));
                g.circleGrad(cx - rad * 0.2f, cy - rad * 0.25f, rad, white ? rgb(0xfff8e8) : rgb(0x54443c), white ? rgb(0xd8c9a2) : rgb(0x1e1614));
                g.ring(cx, cy, rad * 0.72f, 3, white ? rgb(0xb8a678) : rgb(0x6a5a50));
                if (isKing(v)) { g.ring(cx, cy, rad * 0.5f, rad * 0.16f, white ? rgb(0x9a7a3a) : rgb(0xd9a93f)); g.circle(cx, cy, rad * 0.17f, rgb(0xff5a4a)); }
                if (canPick && !ui.padNav) g.ring(cx, cy, rad + 3, 3, rgb(0xffe27a, 170));
                if (i == sel) g.ring(cx, cy, rad + 5, 5, pal::gold);
            }
            if (isTarget) g.circle(r.cx(), r.cy(), cs * 0.16f, rgb(0xffe27a, 200));
            if (ui.focused(id)) ui.drawFocus(r.inset(3), 6);
            if (hit) {
                if (isTarget) { for (auto& m : targets) if (m.to == i) { doMove(m); break; } return; }
                if (canPick) { sel = (sel == i) ? -1 : i; app->audio.play("click"); }
            }
        }
        if (mine && !movable.empty()) ui.setFocusIfNone(hid("dm", st.chain >= 0 ? st.chain : movable[0]));
        if (mine && sel >= 0 && st.chain < 0 && ui.padDown(B_B)) { sel = -1; app->audio.play("click"); }   // B: soltar la ficha elegida
        if (st.chain >= 0 && mine) sel = st.chain;
        std::string msg = over ? (winner < 0 ? "¡Empate!" : "¡Ganó " + pl[winner].name + "!") : (pl[st.turn].ai ? "Piensa " + pl[st.turn].name + "…" : "Turno de " + pl[st.turn].name + (st.turn == 0 ? " (blancas)" : " (negras)"));
        if (st.chain >= 0 && !over) msg += " · ¡seguí comiendo!";
        g.textBox(msg, { 18, 230, 228, 120 }, 17, pal::gold, F_BOLD);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir ficha / destino" }, { "B", "Soltar la ficha elegida" } }; }
    std::string help() const override { return "Movés en diagonal hacia adelante por los casilleros oscuros. Se come saltando una ficha rival (si se puede, es obligatorio). Al llegar al otro lado, la ficha se corona dama y se mueve en cualquier diagonal. Gana quien deja al rival sin fichas o sin jugadas. Elegí la ficha y después el casillero de destino (con el mando: A elige, B suelta la ficha)."; }
};
}
Game* makeDamas() { return new Damas(); }
