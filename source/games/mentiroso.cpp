// Mentiroso de cartas: 3 a 8 jugadores. Se reparte todo el mazo; se tiran 1 a 4 cartas boca abajo diciendo que son del número que toca.
// El siguiente puede gritar "¡Mentiroso!". IA: probabilidades (hipergeométrica + modelo bayesiano), igual que la versión de PC.
#include "cards.h"
#include <algorithm>
#include <cmath>

namespace {
const char* const RK[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
struct Card { int s, v; };   // v: 0=A .. 12=K
bool cmpCard(const Card& a, const Card& b) { return a.v != b.v ? a.v < b.v : a.s < b.s; }
double comb(int n, int k) { if (k < 0 || k > n) return 0; double r = 1; for (int i = 1; i <= k; i++) r = r * (n - k + i) / i; return r; }
struct Last { bool has = false; int p = 0, count = 0, rank = 0; std::vector<Card> cards; };
struct Reveal { bool has = false; int challenger = 0, claimant = 0, rank = 0, loser = 0, taken = 0; bool liar = false; std::vector<Card> cards; };

struct Liar : Game {
    int N = 0, turn = 0, rankIdx = 0, pendingWin = -1, winner = -1, resigned = -1; std::vector<std::vector<Card>> hands; std::vector<Card> pile; Last last; Reveal rev; std::vector<bool> active, sel; bool over = false;
    float aiT = 0, aiWait = 1, revT = 0, endT = 0;
    void start() override {
        N = (int)pl.size(); std::vector<Card> d; for (int s = 0; s < 4; s++) for (int v = 0; v < 13; v++) d.push_back({ s, v }); for (int i = (int)d.size() - 1; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]);
        hands.assign(N, {}); for (size_t i = 0; i < d.size(); i++) hands[i % N].push_back(d[i]); for (auto& h : hands) std::sort(h.begin(), h.end(), cmpCard); active.assign(N, true); sel.assign(52 * 2, false); resetAi();
    }
    int rank() const { return rankIdx % 13; }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = lv == 1 ? 0.9f + rnd01() * 1.3f : lv == 3 ? 0.5f + rnd01() * 0.6f : 0.9f + rnd01() * 0.9f; }
    int turnPlayer() const override { return over || revT > 0 ? -1 : turn; }
    int nextP(int p) const { for (int i = 1; i <= N; i++) { int q = (p + i) % N; if (active[q]) return q; } return p; }
    int nActive() const { int n = 0; for (bool a : active) if (a) n++; return std::max(1, n); }
    // ---- reglas
    void play(const std::vector<int>& idx) {
        if (over || revT > 0 || pendingWin >= 0 || idx.empty() || idx.size() > 4) return; int p = turn; std::vector<int> u = idx; std::sort(u.begin(), u.end(), std::greater<int>());
        std::vector<Card> cs; for (int i : u) { cs.push_back(hands[p][i]); hands[p].erase(hands[p].begin() + i); } for (auto& c : cs) pile.push_back(c);
        last.has = true; last.p = p; last.count = (int)cs.size(); last.rank = rank(); last.cards = cs; rankIdx++; rev.has = false; std::fill(sel.begin(), sel.end(), false);
        if (hands[p].empty()) pendingWin = p; turn = nextP(p); app->audio.play("place"); resetAi();
    }
    void challenge() {
        if (over || revT > 0 || !last.has) return; int p = turn; bool truth = true; for (auto& c : last.cards) if (c.v != last.rank) truth = false; int loser = truth ? p : last.p;
        rev = { true, p, last.p, last.rank, loser, (int)pile.size(), !truth, last.cards }; for (auto& c : pile) hands[loser].push_back(c); std::sort(hands[loser].begin(), hands[loser].end(), cmpCard); pile.clear();
        bool wasWin = truth && pendingWin == last.p; last.has = false; if (wasWin) { over = true; winner = last.p; } pendingWin = -1; turn = p; revT = 3.4f; app->audio.play(truth ? "lose" : "win"); resetAi();
    }
    void accept() { if (over || revT > 0 || pendingWin < 0) return; over = true; winner = pendingWin; app->audio.play("win"); }
    void resign(int p) override { active[p] = false; resigned = p; for (auto& c : hands[p]) pile.push_back(c); hands[p].clear(); if (last.has && last.p == p) { last.has = false; if (pendingWin == p) pendingWin = -1; }
        int a = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { a++; w = i; } if (a <= 1) { over = true; winner = w; return; } if (turn == p) { turn = nextP(p); resetAi(); } }
    // ---- IA
    double lieProb(const std::vector<Card>& my) const {
        if (!last.has) return 0; int h = 0; for (auto& c : my) if (c.v == last.rank) h++; int avail = 4 - h; if (last.count > avail) return 1;
        int u = std::max(1, 52 - (int)my.size()), k = std::min(u, (int)hands[last.p].size() + last.count); double tot = comb(u, k);
        auto pmf = [&](int j) { return (j < 0 || j > avail || !tot) ? 0.0 : comb(avail, j) * comb(u - avail, k - j) / tot; };
        double above = 0; for (int j = last.count + 1; j <= avail; j++) above += pmf(j);
        double like = pmf(last.count) * 0.75 + above * 0.10, liar = pmf(0) * (last.count == 1 ? 0.85 : last.count == 2 ? 0.15 : 0.02) + (last.count >= 2 ? pmf(last.count - 1) * 0.16 : 0);
        double pt = like / ((like + liar) != 0 ? (like + liar) : 1); return 1 - pt;
    }
    bool wantChallenge(const std::vector<Card>& my, int lv) {
        if (!last.has) return false; const float NOISE[4] = { 0, 0.5f, 0.16f, 0.04f }; double pl2 = lieProb(my); if (pl2 >= 1) return lv == 1 ? rnd01() < 0.65f : true;
        if (lv == 1 && rnd01() < 0.35f) return rnd01() < 0.3f;
        if (pendingWin >= 0) return rnd01() < (lv == 3 ? 0.6 + 0.4 * pl2 : 0.3 + 0.7 * pl2);
        double thr = 0.62 + (rnd01() - 0.5) * NOISE[lv]; if ((int)pile.size() <= 3) thr -= 0.10; else if ((int)pile.size() >= 12) thr += 0.08;
        if ((int)hands[last.p].size() <= 2) thr -= 0.15; if ((int)my.size() <= 3) thr += 0.05; return pl2 > thr;
    }
    int worst(const std::vector<Card>& hand, const std::vector<int>& cand, int ri) const {
        int na = nActive(); int cnt[13] = {}; for (auto& c : hand) cnt[c.v]++; int best = cand[0]; double bv = 1e9;
        for (int i : cand) { auto& c = hand[i]; int d = (c.v - ri + 130) % 13; double v = cnt[c.v] * 2 + ((d % na == 0) ? 3 : 0) + (d == 0 ? 5 : 0); if (v < bv) { bv = v; best = i; } }
        return best;
    }
    std::vector<int> chooseCards(const std::vector<Card>& hand, int lv) {
        const int ERRK[4] = { 0, 3, 1, 0 }; int K = ERRK[lv], R = rank(); std::vector<int> truth, others; for (size_t i = 0; i < hand.size(); i++) (hand[i].v == R ? truth : others).push_back((int)i);
        if (!truth.empty()) {
            int n = std::min(4, (int)truth.size()); if (n > 1 && rnd01() < 0.12f * K) n = 1 + rndInt(n - 1); std::vector<int> pick(truth.begin(), truth.begin() + n);
            if (pick.size() < 4 && pick.size() == truth.size() && !others.empty() && hand.size() > pick.size() + 1 && rnd01() < (lv == 3 ? 0.35f : 0.18f * K)) pick.push_back(worst(hand, others, R));
            return pick;
        }
        std::vector<int> pick{ (K > 1 && rnd01() < 0.4f) ? others[rndInt((int)others.size())] : worst(hand, others, R) };
        if (hand.size() > 3 && others.size() > 1 && rnd01() < (lv == 3 ? 0.4f : 0.15f * K)) { std::vector<int> rest; for (int i : others) if (i != pick[0]) rest.push_back(i); pick.push_back(worst(hand, rest, R)); }
        return pick;
    }
    void update(float dt, Input& in, UI&) override {
        if (revT > 0) { revT -= dt; return; }
        if (over) { endT += dt; if (endT > 1.2f && !finished) {
            std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ 52 - (int)hands[i].size(), i == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, winner >= 0 ? "Ganó " + pl[winner].name : "Fin"); } return; }
        if (!pl[turn].ai) {
            Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
            if (p) { if (p->down[B_X] && pendingWin < 0) { std::vector<int> idx; for (size_t i = 0; i < hands[turn].size(); i++) if (sel[i]) idx.push_back((int)i); play(idx); } if (p->down[B_Y] && last.has) challenge(); }
            return;
        }
        aiT += dt; if (aiT < aiWait) return; int lv = std::max(1, std::min(3, pl[turn].level)); auto& h = hands[turn];
        if (pendingWin >= 0) { if (pendingWin == turn) return; if (wantChallenge(h, lv)) challenge(); else accept(); return; }
        if (last.has && wantChallenge(h, lv)) { challenge(); return; }
        if (h.empty()) { if (last.has) challenge(); return; }
        play(chooseCards(h, lv));
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && revT <= 0 && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++;
        bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : revT > 0 ? "¡Mentiroso! Se destapan las cartas" : mine ? pl[turn].name + ": tirá cartas como " + std::string(RK[rank()]) + (last.has ? " o decí ¡Mentiroso!" : "") : "Turno de " + pl[turn].name + " (toca " + RK[rank()] + ")";
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 19, pal::gold, F_BOLD, AL_C, 930);
        // jugadores
        float pw = std::min(180.f, 940.f / N);
        for (int i = 0; i < N; i++) { Rect r{ 290 + i * (pw + 2), 66, pw - 4, 52 }; bool on = i == turn && !over && revT <= 0; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 20, r.cy(), 22); g.text(pl[i].name, r.x + 38, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 50); g.text(active[i] ? std::to_string(hands[i].size()) + " cartas" : "fuera", r.x + 38, r.y + 36, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 50); }
        // mesa
        cards::draw(g, { 400, 140, 76, 106 }, 0, 0, false); g.text(std::to_string(pile.size()) + " en la pila", 438, 266, 15, pal::white, F_BOLD, AL_C);
        std::string claim = last.has ? pl[last.p].name + " dijo: " + std::to_string(last.count) + (last.count == 1 ? " carta" : " cartas") + " de " + RK[last.rank] : "Todavía no se tiró nada";
        g.rrect({ 520, 150, 640, 60 }, 14, rgb(0x120c08, 200)); g.text(claim, 840, 180, 22, pal::cream, F_BOLD, AL_C, 620);
        if (pendingWin >= 0 && !over) { g.rrect({ 520, 218, 640, 34 }, 12, rgb(0x120c08, 200)); g.text(pl[pendingWin].name + " se quedó sin cartas: ¿le creés?", 840, 235, 17, pal::gold, F_BOLD, AL_C, 620); }
        if (revT > 0 && rev.has) {
            g.rrect({ 290, 270, 950, 330 }, 18, pal::panelEdge); g.rrectGrad({ 293, 273, 944, 324 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c));
            g.textShadow(rev.liar ? pl[rev.claimant].name + " mentía" : pl[rev.claimant].name + " decía la verdad", 765, 306, 26, pal::gold, F_TITLE, AL_C);
            float cw = 90; float tw = rev.cards.size() * (cw + 14) - 14; for (size_t k = 0; k < rev.cards.size(); k++) cards::draw(g, { 765 - tw / 2 + k * (cw + 14), 335, cw, 126 }, rev.cards[k].v == 0 ? 12 : rev.cards[k].v - 1, rev.cards[k].s, true, rev.cards[k].v == rev.rank);
            g.text(pl[rev.loser].name + " se lleva la pila (" + std::to_string(rev.taken) + " cartas)", 765, 500, 20, pal::cream, F_BOLD, AL_C, 900); return;
        }
        // acciones
        auto& h = hands[turn]; int ns = 0; if (mine) for (size_t i = 0; i < h.size(); i++) if (sel[i]) ns++;
        float by = 290;
        if (mine) {
            if (pendingWin < 0 && ui.button("Tirar " + std::to_string(ns) + (ns == 1 ? " carta" : " cartas") + " como " + RK[rank()], { 330, by, 360, 50 }, 3, ns >= 1 && ns <= 4)) { std::vector<int> idx; for (size_t i = 0; i < h.size(); i++) if (sel[i]) idx.push_back((int)i); play(idx); return; }
            if (last.has && ui.button(pendingWin >= 0 ? "🤥 ¡Mentiroso!" : "🤥 ¡Mentiroso!", { pendingWin >= 0 ? 330.f : 720.f, by, 300, 50 }, 1, true)) { challenge(); return; }
            if (pendingWin >= 0 && ui.button("Le creo", { 660, by, 300, 50 }, 3, true)) { accept(); return; }
            if (pendingWin >= 0 && pendingWin != turn) ui.setFocusIfNone(hid("Le creo", (int)(660 * 7 + by)));
        }
        // mano
        if (peek || (mine && humans <= 1)) {
            int n = (int)h.size(); int perRow = n > 26 ? 18 : 13; float cw = n > 26 ? 52 : 64, ch = cw * 1.4f, stepX = std::min(cw + 8, 940.f / perRow); int rows = (n + perRow - 1) / perRow; float y0 = 360;
            for (int i = 0; i < n; i++) {
                int rr = i / perRow, cc = i % perRow; Rect r{ 290 + cc * stepX, y0 + rr * (ch * 0.6f + 22) - (sel[i] ? 18 : 0), cw, ch }; uint32_t id = hid("lc", i);
                bool hit = ui.hit(id, { r.x, r.y, stepX - 2, ch }, mine); cards::draw(g, r, h[i].v == 0 ? 12 : h[i].v - 1, h[i].s, true, sel[i]);
                if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { if (!sel[i] && ns >= 4) { app->toast("Máximo 4 cartas"); } else sel[i] = !sel[i]; app->audio.play("click"); }
            }
            if (onPhone(turn)) coverHand(g, ui, "lc", n, turn);
            if (mine && !h.empty() && !(ui.focus)) ui.setFocusIfNone(hid("lc", 0)); (void)rows;
        } else if (mine) g.text("Mantené L para ver tus cartas", 765, 420, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; static const char* RK[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" }; static const char* SU[4] = { "♠", "♥", "♦", "♣" };
        std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("lc", (int)i, me) + RK[hands[p][i].v % 13] + SU[hands[p][i].s & 3] + (me && i < sel.size() && sel[i] ? "  ✔" : "") + ((i % 2) ? "\n" : "    "); return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Marcar carta / botón" }, { "X", "Tirar las marcadas" }, { "Y", "¡Mentiroso!" }, { "L", "Ver tus cartas" } }; }
    std::string help() const override { return "Se reparte todo el mazo. En tu turno tirás de 1 a 4 cartas boca abajo diciendo que son del número que toca (A, 2, 3… K y vuelve a empezar). Podés mentir. El siguiente jugador puede gritar ¡Mentiroso!: si mentiste te llevás la pila; si decías la verdad se la lleva quien te acusó. Gana el primero que se queda sin cartas sin que le descubran una mentira."; }
};
}
Game* makeMentiroso() { return new Liar(); }
