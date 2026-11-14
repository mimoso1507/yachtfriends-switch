// Dudo (Perudo): cada jugador tiene 5 dados ocultos; se apuesta cuántos dados de una cara hay en total (los 1 son comodines).
// IA: probabilidad binomial de que la apuesta sea cierta, con faroles y dudas, igual que la versión de PC.
#include "dice.h"
#include <algorithm>
#include <cmath>

namespace {
double comb(int n, int k) { if (k < 0 || k > n) return 0; double r = 1; for (int i = 1; i <= k; i++) r = r * (n - k + i) / i; return r; }
double binAtLeast(int n, double p, int k) { if (k <= 0) return 1; if (k > n) return 0; double t = 0; for (int x = k; x <= n; x++) t += comb(n, x) * std::pow(p, x) * std::pow(1 - p, n - x); return std::min(1.0, t); }
struct Bid { int qty = 0, face = 0, by = -1; bool has = false; };
bool validBid(const Bid& b, int qty, int face) {
    if (face < 1 || face > 6 || qty < 1) return false; if (!b.has) return true;
    int bq = b.qty, bf = b.face;
    if (bf == 1 && face == 1) return qty > bq; if (bf == 1 && face != 1) return qty >= bq * 2 + 1;
    if (bf != 1 && face == 1) return qty >= (bq + 1) / 2; return qty > bq || (qty == bq && face > bf);
}
double strength(int q, int f) { return (f == 1 ? q * 2 - 0.5 : q) + f / 100.0; }
double pTrue(int qty, int face, const std::vector<int>& my, int N) { int mine = 0; for (int v : my) if (v == face || (face != 1 && v == 1)) mine++; return binAtLeast(N - (int)my.size(), face == 1 ? 1.0 / 6 : 1.0 / 3, qty - mine); }
struct LP { float rnd, tn, fol, cn; }; const LP LVP[4] = { {}, { 0.4f, 0.35f, 0.25f, 0.4f }, { 0, 0.15f, 0.10f, 1 }, { 0.02f, 0.05f, 0.05f, 1 } };
struct Act { bool challenge; int qty, face; };
struct Cand { int qty, face; double p; };
Act decide(const Bid& bid, const std::vector<int>& my, int N, int myIdx, int lv) {
    const LP& P = LVP[std::max(1, std::min(3, lv))]; double t = 0.55 + rnd01() * P.tn;
    std::vector<Cand> cands; for (int q = 1; q <= N; q++) for (int f = 1; f <= 6; f++) if (validBid(bid, q, f)) cands.push_back({ q, f, pTrue(q, f, my, N) });
    double pc = 0;
    if (bid.has) { double p0 = pTrue(bid.qty, bid.face, my, N), p1 = bid.by != myIdx ? pTrue(bid.qty - 1, bid.face, my, N) : p0; pc = 0.6 * p0 + 0.4 * p1; if (bid.qty > N) return { true, 0, 0 }; }
    std::vector<Cand> good; for (auto& c : cands) if (c.p >= t) good.push_back(c);
    if (!cands.empty() && rnd01() < P.rnd) { if (bid.has && rnd01() < 0.5) return { true, 0, 0 }; auto& c = cands[rndInt((int)std::min<size_t>(cands.size(), 6))]; return { false, c.qty, c.face }; }
    if (bid.has) {
        double bestP = 0; for (auto& c : cands) bestP = std::max(bestP, c.p);
        if (cands.empty()) return { true, 0, 0 };
        if (pc < 0.28 || (pc < 0.5 && bestP < 0.4) || (pc < 0.4 && bestP < 0.55 && rnd01() < (P.cn == 1 ? 0.5 : 0.3))) return { true, 0, 0 };
        if (pc > 0.9 && good.empty()) return { true, 0, 0 };
        if (rnd01() < P.fol) { std::vector<Cand> fl; for (auto& c : cands) if (c.p >= 0.3 && c.p < t) fl.push_back(c); std::sort(fl.begin(), fl.end(), [](const Cand& a, const Cand& b) { return strength(a.qty, a.face) < strength(b.qty, b.face); }); if (!fl.empty()) return { false, fl[0].qty, fl[0].face }; }
        std::vector<Cand> pool = good; if (pool.empty()) { auto s = cands; std::sort(s.begin(), s.end(), [](const Cand& a, const Cand& b) { return a.p > b.p; }); pool.push_back(s[0]); }
        std::sort(pool.begin(), pool.end(), [](const Cand& a, const Cand& b) { return strength(a.qty, a.face) < strength(b.qty, b.face); }); return { false, pool[0].qty, pool[0].face };
    }
    std::vector<Cand> pool = good; if (pool.empty()) { auto s = cands; std::sort(s.begin(), s.end(), [](const Cand& a, const Cand& b) { return a.p > b.p; }); pool.assign(s.begin(), s.begin() + std::min<size_t>(3, s.size())); }
    std::sort(pool.begin(), pool.end(), [](const Cand& a, const Cand& b) { return a.qty != b.qty ? a.qty > b.qty : a.p > b.p; });
    std::vector<Cand> top; for (auto& c : pool) if (c.qty == pool[0].qty) top.push_back(c); Cand c = top[rndInt((int)top.size())];
    if (rnd01() < 0.2 && pool.size() > 1) c = pool[rndInt((int)std::min<size_t>(pool.size(), 4))];
    return { false, c.qty, c.face };
}

struct Dudo : Game {
    int N = 0, turn = 0, winner = -1, qty = 1, face = 2, resigned = -1; std::vector<std::vector<int>> dice; std::vector<bool> active; Bid bid; bool over = false; float aiT = 0, aiWait = 1, endT = 0, revT = 0;
    struct Last { bool has = false; int total = 0, qty = 0, face = 0, loser = 0; std::vector<std::vector<int>> shown; } last; std::string lastMsg;
    void start() override { N = (int)pl.size(); active.assign(N, true); dice.assign(N, {}); for (auto& d : dice) for (int i = 0; i < 5; i++) d.push_back(1 + rndInt(6)); resetAi(); }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = (lv == 1 ? 1.4f : lv == 3 ? 0.9f : 1.2f) + rnd01() * 0.8f; }
    int turnPlayer() const override { return over || revT > 0 ? -1 : turn; }
    int totalDice() const { int n = 0; for (int i = 0; i < N; i++) if (active[i]) n += (int)dice[i].size(); return n; }
    void nextTurn() { for (int s = 1; s <= N; s++) { int q = (turn + s) % N; if (active[q]) { turn = q; return; } } }
    void doBid(int q, int f) { if (over || revT > 0 || !validBid(bid, q, f)) return; bid = { q, f, turn, true }; qty = q; face = f; app->audio.play("click"); nextTurn(); resetAi(); }
    void challenge() {
        if (over || revT > 0 || !bid.has) return; int total = 0; for (int i = 0; i < N; i++) if (active[i]) for (int v : dice[i]) if (v == bid.face || (bid.face != 1 && v == 1)) total++;
        int loser = total >= bid.qty ? turn : bid.by; last.has = true; last.total = total; last.qty = bid.qty; last.face = bid.face; last.loser = loser; last.shown = dice; for (int i = 0; i < N; i++) if (!active[i]) last.shown[i].clear();
        lastMsg = "Había " + std::to_string(total) + " de " + (bid.face == 1 ? std::string("1 (comodín)") : std::to_string(bid.face)) + ", se apostaban " + std::to_string(bid.qty) + ". Pierde un dado: " + pl[loser].name;
        dice[loser].pop_back(); if (dice[loser].empty()) active[loser] = false; app->audio.play(loser == turn ? "lose" : "win");
        int alive = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { alive++; w = i; }
        if (alive == 1) { over = true; winner = w; } else { turn = loser; if (!active[turn]) nextTurn(); }
        revT = 3.6f;
    }
    void newRound() { for (int i = 0; i < N; i++) { int n = (int)dice[i].size(); for (int& v : dice[i]) v = 1 + rndInt(6); (void)n; } bid = Bid(); qty = 1; face = 2; resetAi(); }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input&, UI&) override {
        if (revT > 0) { revT -= dt; if (revT <= 0 && !over) newRound(); return; }
        if (over) { endT += dt; if (endT > 1.0f && !finished) {
            std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ (int)dice[i].size(), i == winner ? 'w' : 'l' });
            if (resigned >= 0) { for (auto& x : r) x.r = 'l'; r[resigned].r = 'x'; r[(resigned + 1) % N].r = 'w'; winner = (resigned + 1) % N; }
            finish(r, "Ganó " + pl[winner].name); } return; }
        if (!pl[turn].ai) return; aiT += dt; if (aiT < aiWait) return;
        Act a = decide(bid, dice[turn], totalDice(), turn, pl[turn].level);
        if (a.challenge && bid.has) challenge(); else if (!a.challenge && validBid(bid, a.qty, a.face)) doBid(a.qty, a.face); else if (bid.has) challenge(); else doBid(1, 2);
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && revT <= 0 && !pl[turn].ai;
        std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : revT > 0 ? "¡Dudo! Se destapan los dados" : bid.has ? "Última apuesta: " + std::to_string(bid.qty) + " × " + (bid.face == 1 ? std::string("1 (comodín)") : std::to_string(bid.face)) + " de " + pl[bid.by].name : mine ? "Abrí la apuesta" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 20, 950, 46 }, 23, rgb(0x120c08, 210)); g.text(banner, 765, 43, 20, pal::gold, F_BOLD, AL_C, 930);
        // jugadores y sus dados
        int humans = 0; for (auto& p : pl) if (!p.ai) humans++;
        bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        float y = 84; for (int i = 0; i < N; i++) {
            Rect r{ 290, y, 950, 56 }; bool on = i == turn && !over && revT <= 0; g.rrect(r, 12, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 10, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 28, r.cy(), 28); g.text(pl[i].name + (active[i] ? "" : "  (eliminado)"), r.x + 54, r.cy(), 17, on ? pal::gold : pal::white, F_BOLD, AL_L, 180);
            for (size_t k = 0; k < dice[i].size(); k++) {
                Rect d{ r.x + 260 + k * 52, r.cy() - 21, 42, 42 };
                bool show = (revT > 0 && last.has && !last.shown[i].empty()) || (peek && i == turn && !onPhone(i));
                if (show) { int v = revT > 0 ? last.shown[i][k < last.shown[i].size() ? k : 0] : dice[i][k]; bool hl = revT > 0 && (v == last.face || (last.face != 1 && v == 1)); dice::draw(g, d, v, hl); }
                else { g.rrect({ d.x + 2, d.y + 4, d.w, d.h }, 8, rgb(0, 100)); g.rrectGrad(d, 8, rgb(0x6b4224), rgb(0x40240f)); g.text("?", d.cx(), d.cy(), 20, rgb(0xd8b46a), F_TITLE, AL_C); }
            }
            if (!pl[i].ai && humans > 1 && pl[i].pad >= 0) g.text("🎮" + std::to_string(pl[i].pad + 1), r.x + r.w - 14, r.cy(), 14, rgb(0xd8b46a), F_BODY, AL_R);
            y += 62;
        }
        float by = std::max(y + 10, 340.f);
        if (revT > 0) { g.rrect({ 290, by, 950, 70 }, 14, rgb(0x120c08, 220)); g.textBox(lastMsg, { 310, by + 8, 910, 56 }, 18, pal::cream, F_BOLD, AL_C); return; }
        if (mine) {
            Rect P{ 290, by, 950, 190 }; g.rrect(P, 16, pal::panelEdge); g.rrectGrad(P.inset(3), 14, rgb(0x4a2e18), rgb(0x2e1a0c));
            int d; g.text(humans > 1 ? "Mantené L para ver tus dados" : "Tus dados están arriba", P.cx(), P.y + 22, 16, rgb(0xd8b46a), F_BODY, AL_C);
            if (ui.stepper("Cantidad", std::to_string(qty), { P.x + 20, P.y + 44, 450, 44 }, d)) { qty = std::max(1, std::min(totalDice() + 4, qty + d)); }
            if (ui.stepper("Cara", face == 1 ? std::string("1 (comodín)") : std::to_string(face), { P.x + 480, P.y + 44, 450, 44 }, d)) { face = (face - 1 + d + 6) % 6 + 1; }
            bool ok = validBid(bid, qty, face);
            if (ui.button("Apostar", { P.x + 60, P.y + 108, 380, 56 }, 3, ok)) doBid(qty, face);
            if (ui.button("¡Dudo!", { P.x + 510, P.y + 108, 380, 56 }, 1, bid.has)) challenge();
            if (!ok) g.text("Esa apuesta no es válida: subí la cantidad o la cara", P.cx(), P.y + 178, 13, rgb(0xff8a80), F_BODY, AL_C);
            ui.setFocusIfNone(hid("Apostar", (int)((P.x + 60) * 7 + P.y + 108)));
        }
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)dice.size() || pl[p].ai || dice[p].empty()) return ""; std::string s = "Tus dados:\n"; static const char* F[7] = { "", "⚀", "⚁", "⚂", "⚃", "⚄", "⚅" }; for (int v : dice[p]) s += std::string(F[std::max(1, std::min(6, v))]) + " " + std::to_string(v) + "   "; return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "LR", "Cambiar cantidad o cara" }, { "A", "Apostar / ¡Dudo!" }, { "L", "Ver tus dados" } }; }
    std::string help() const override { return "Cada jugador tiene 5 dados ocultos. Por turnos se apuesta cuántos dados de una cara hay en total entre todos (los 1 son comodines). Cada apuesta debe subir la cantidad o la cara. Si no le creés al anterior, cantá ¡Dudo!: se destapan los dados. Quien se equivocó pierde un dado. Sobrevive el último con dados. Subir de comodín a otra cara pide el doble más uno; pasar a comodín pide la mitad."; }
};
}
Game* makeDudo() { return new Dudo(); }
