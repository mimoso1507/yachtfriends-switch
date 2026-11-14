// Chinchón: 2 a 4 jugadores, baraja española, 7 cartas. Se roba (mazo o descarte) y se descarta. Juegos: 3-4 del mismo número o escaleras de 3+ del mismo palo.
// Se puede cerrar cuando lo suelto suma 5 o menos. Los demás se anotan sus puntos sueltos; quien pasa el límite queda eliminado.
#include "spanish.h"
#include <algorithm>

namespace {
struct Card { int s, r; };
int pos(int r) { for (int i = 0; i < 10; i++) if (sp::RANKS[i] == r) return i; return 0; }
int pts(const Card& c) { return c.r <= 7 ? c.r : 10; }
bool cmpCard(const Card& a, const Card& b) { return a.s != b.s ? a.s < b.s : pos(a.r) < pos(b.r); }
std::vector<int> allMelds(const std::vector<Card>& h) {
    std::vector<int> out; int n = (int)h.size();
    for (int r : sp::RANKS) { std::vector<int> ix; for (int i = 0; i < n; i++) if (h[i].r == r) ix.push_back(i); int m = (int)ix.size();
        for (int a = 0; a < m; a++) for (int b = a + 1; b < m; b++) for (int c = b + 1; c < m; c++) { out.push_back((1 << ix[a]) | (1 << ix[b]) | (1 << ix[c])); for (int d = c + 1; d < m; d++) out.push_back((1 << ix[a]) | (1 << ix[b]) | (1 << ix[c]) | (1 << ix[d])); } }
    for (int s = 0; s < 4; s++) { std::vector<int> ix; for (int i = 0; i < n; i++) if (h[i].s == s) ix.push_back(i); std::sort(ix.begin(), ix.end(), [&](int a, int b) { return pos(h[a].r) < pos(h[b].r); });
        for (size_t i = 0; i < ix.size(); i++) { int m = 1 << ix[i]; for (size_t j = i + 1; j < ix.size() && pos(h[ix[j]].r) == pos(h[ix[j - 1]].r) + 1; j++) { m |= 1 << ix[j]; if (j - i >= 2) out.push_back(m); } } }
    return out;
}
struct Analysis { int dead = 0; std::vector<int> meldMasks; int usedMask = 0; };
Analysis analyze(const std::vector<Card>& h) {
    auto ms = allMelds(h); int n = (int)h.size(); std::vector<int> memoD(1 << n, -1), memoM(1 << n, 0); std::vector<std::vector<int>> memoC(1 << n);
    std::function<int(int)> f = [&](int rem) -> int {
        if (!rem) return 0; if (memoD[rem] >= 0) return memoD[rem]; int i = 0; while (!(rem & (1 << i))) i++;
        int best = f(rem & ~(1 << i)) + pts(h[i]); std::vector<int> bc = memoC[rem & ~(1 << i)]; bool have = false; (void)have;
        for (int m : ms) if ((m & (1 << i)) && (m & ~rem) == 0) { int r = f(rem & ~m); if (r < best) { best = r; bc = memoC[rem & ~m]; bc.push_back(m); } }
        memoD[rem] = best; memoC[rem] = bc; return best; };
    Analysis a; a.dead = f((1 << n) - 1); a.meldMasks = memoC[(1 << n) - 1]; for (int m : a.meldMasks) a.usedMask |= m; return a;
}
bool isChinchon(const std::vector<Card>& h) { if (h.size() != 7) return false; for (auto& c : h) if (c.s != h[0].s) return false; std::vector<int> p; for (auto& c : h) p.push_back(pos(c.r)); std::sort(p.begin(), p.end()); for (size_t i = 1; i < p.size(); i++) if (p[i] != p[i - 1] + 1) return false; return true; }

struct Chin : Game {
    enum Ph { DRAW, DISCARD, ROUNDEND, OVER } ph = DRAW;
    int N = 0, limit = 100, dealer = 0, turn = 0, winner = -1, resigned = -1, sel = -1, roundNo = 0, closer = -1; bool chin = false;
    std::vector<Card> stock, disc; std::vector<std::vector<Card>> hands; std::vector<int> scores, gain, dead; std::vector<bool> active; float aiT = 0, aiWait = 1, endT = 0; std::string msg; std::vector<std::vector<Card>> finalHands;
    void start() override { N = (int)pl.size(); limit = opts.get("limit", 1) == 0 ? 50 : opts.get("limit", 1) == 2 ? 150 : 100; scores.assign(N, 0); active.assign(N, true); dealer = N - 1; newRound(); }
    int nextP(int p) const { for (int i = 1; i <= N; i++) { int q = (p + i) % N; if (active[q]) return q; } return p; }
    void newRound() {
        roundNo++; stock.clear(); for (int s = 0; s < 4; s++) for (int r : sp::RANKS) stock.push_back({ s, r }); for (int i = (int)stock.size() - 1; i > 0; i--) std::swap(stock[i], stock[rndInt(i + 1)]);
        hands.assign(N, {}); dealer = nextP(dealer); for (int k = 0; k < 7; k++) for (int p = 0; p < N; p++) if (active[p]) { hands[p].push_back(stock.back()); stock.pop_back(); }
        for (auto& h : hands) std::sort(h.begin(), h.end(), cmpCard); disc = { stock.back() }; stock.pop_back(); turn = nextP(dealer); ph = DRAW; sel = -1; msg.clear(); resetAi();
    }
    void resetAi() { aiT = 0; aiWait = 0.9f + rnd01() * 0.8f; }
    int turnPlayer() const override { return (ph == DRAW || ph == DISCARD) ? turn : -1; }
    int reshuf = 0, stall = 0;
    void refill() { if (stock.empty() && disc.size() > 1) { if (++reshuf >= 3) { endVoid(); return; } Card top = disc.back(); disc.pop_back(); stock = disc; disc = { top }; for (int i = (int)stock.size() - 1; i > 0; i--) std::swap(stock[i], stock[rndInt(i + 1)]); } }
    void endVoid() {   // mazo agotado sin que nadie cierre: todos suman lo suelto
        closer = -1; chin = false; gain.assign(N, 0); dead.assign(N, 0); for (int q = 0; q < N; q++) { if (!active[q]) continue; dead[q] = analyze(hands[q]).dead; gain[q] = dead[q]; scores[q] += gain[q]; }
        finalHands = hands; ph = ROUNDEND; endT = 0; stall = 0; checkOver(); }
    void checkOver() { int alive = 0, w = -1; for (int i = 0; i < N; i++) if (active[i] && scores[i] <= limit) { alive++; w = i; } if (alive == 1) { winner = w; ph = OVER; } else if (alive == 0) { int bm = 1 << 30; for (int i = 0; i < N; i++) if (active[i] && scores[i] < bm) { bm = scores[i]; winner = i; } ph = OVER; } }
    void drawStock() { if (ph != DRAW) return; refill(); if (ph != DRAW) return; if (stock.empty()) { if (++stall > 2 * N + 2) { endVoid(); return; } drawDisc(); return; } hands[turn].push_back(stock.back()); stock.pop_back(); std::sort(hands[turn].begin(), hands[turn].end(), cmpCard); ph = DISCARD; sel = -1; app->audio.play("flip"); msg = pl[turn].name + " robó del mazo"; resetAi(); }
    void drawDisc() { if (ph != DRAW || disc.empty()) return; hands[turn].push_back(disc.back()); msg = pl[turn].name + " tomó " + std::to_string(disc.back().r) + " de " + sp::suitName(disc.back().s); disc.pop_back(); std::sort(hands[turn].begin(), hands[turn].end(), cmpCard); ph = DISCARD; sel = -1; app->audio.play("flip"); resetAi(); }
    void discard(int ci) { if (ph != DISCARD || ci < 0 || ci >= (int)hands[turn].size()) return; disc.push_back(hands[turn][ci]); hands[turn].erase(hands[turn].begin() + ci); turn = nextP(turn); ph = DRAW; sel = -1; app->audio.play("place"); resetAi(); }
    int canClose(int ci) const { if (ph != DISCARD || ci < 0 || ci >= (int)hands[turn].size()) return -1; auto rest = hands[turn]; rest.erase(rest.begin() + ci); int d = analyze(rest).dead; return d <= 5 ? d : -1; }
    void closeWith(int ci) {
        if (canClose(ci) < 0) return; int p = turn; disc.push_back(hands[p][ci]); hands[p].erase(hands[p].begin() + ci); chin = isChinchon(hands[p]); closer = p; gain.assign(N, 0); dead.assign(N, 0);
        for (int q = 0; q < N; q++) { if (!active[q]) continue; dead[q] = analyze(hands[q]).dead; gain[q] = q == p ? (chin ? -25 : dead[q] == 0 ? -10 : 0) : dead[q]; scores[q] += gain[q]; }
        finalHands = hands; ph = ROUNDEND; endT = 0; app->audio.play("win");
        checkOver();
    }
    void resign(int p) override { active[p] = false; resigned = p; int a = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { a++; w = i; } if (a <= 1) { winner = w; ph = OVER; return; } if ((ph == DRAW || ph == DISCARD) && turn == p) { for (auto& c : hands[p]) disc.insert(disc.begin(), c); hands[p].clear(); turn = nextP(p); ph = DRAW; resetAi(); } }
    void aiMove() {
        int lv = std::max(1, std::min(3, pl[turn].level)); auto& h = hands[turn];
        if (ph == DRAW) { refill(); if (ph != DRAW) return; if (stock.empty()) { drawStock(); return; } int cur = analyze(h).dead; auto t = h; t.push_back(disc.back()); int best = 1 << 20; for (size_t i = 0; i < t.size(); i++) { auto r = t; r.erase(r.begin() + i); best = std::min(best, analyze(r).dead); }
            bool take = !disc.empty() && (best < cur - (lv == 1 ? 3 : 0) || (best < cur && rnd01() < 0.8f)); if (lv == 1 && rnd01() < 0.25f) take = !take && !disc.empty(); if (take) drawDisc(); else drawStock(); return; }
        int bi = 0, bd = 1 << 20, bp = -1; for (int i = 0; i < (int)h.size(); i++) { auto r = h; r.erase(r.begin() + i); int d = analyze(r).dead; if (d < bd || (d == bd && pts(h[i]) > bp)) { bd = d; bi = i; bp = pts(h[i]); } }
        if (bd <= 5 && (lv >= 2 || rnd01() < 0.6f) && (bd <= 2 || lv == 3 || rnd01() < 0.5f)) { closeWith(bi); return; } discard(bi);
    }
    void update(float dt, Input& in, UI&) override {
        if (ph == OVER) { endT += dt; if (endT > 4.f && !finished) { std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ std::max(0, limit - scores[i]), i == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + pl[winner].name); } return; }
        if (ph == ROUNDEND) { endT += dt; bool adv = endT > 8.f; for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && endT > 1.2f) adv = true; if (adv) { for (int i = 0; i < N; i++) if (active[i] && scores[i] > limit) active[i] = false; newRound(); } return; }
        if (!pl[turn].ai) { Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (pd && ph == DISCARD && sel >= 0) { if (pd->down[B_X]) discard(sel); if (pd->down[B_Y] && canClose(sel) >= 0) closeWith(sel); } return; }
        aiT += dt; if (aiT >= aiWait) aiMove();
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = (ph == DRAW || ph == DISCARD) && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = ph == OVER ? "¡Ganó " + pl[winner].name + "!" : ph == ROUNDEND ? (closer < 0 ? std::string("Se acabó el mazo") : (chin ? "¡CHINCHÓN de " : "Cerró ") + pl[closer].name) : mine ? pl[turn].name + (ph == DRAW ? ": roba una carta" : ": descarta una carta (o cierra)") : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(200.f, 940.f / N);
        for (int i = 0; i < N; i++) { Rect r{ 290 + i * (pw + 2), 66, pw - 4, 52 }; bool on = i == turn && (ph == DRAW || ph == DISCARD); g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 20, r.cy(), 22); g.text(pl[i].name, r.x + 38, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 50); g.text(active[i] ? std::to_string(scores[i]) + " pts" : "eliminado", r.x + 38, r.y + 36, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 50); }
        g.text("Límite: " + std::to_string(limit) + " · Ronda " + std::to_string(roundNo), 1230, 130, 14, pal::cream, F_BODY, AL_R); g.text(msg, 765, 150, 16, pal::cream, F_BODY, AL_C, 930);
        if (ph == ROUNDEND || ph == OVER) { g.rrect({ 300, 170, 930, 420 }, 18, pal::panelEdge); g.rrectGrad({ 303, 173, 924, 414 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); float y = 200;
            for (int p = 0; p < N; p++) { if (finalHands[p].empty()) continue; g.text(pl[p].name + (p == closer ? " (cerró)" : "") + "  ·  sueltas: " + std::to_string(dead[p]) + "  ·  " + (gain[p] >= 0 ? "+" : "") + std::to_string(gain[p]) + "  ·  total " + std::to_string(scores[p]), 330, y, 16, p == closer ? pal::gold : pal::cream, F_BOLD, AL_L, 880);
                auto a = analyze(finalHands[p]); for (size_t i = 0; i < finalHands[p].size(); i++) sp::draw(g, { 330.f + i * 50, y + 14, 44, 62 }, finalHands[p][i].s, finalHands[p][i].r, true, a.usedMask >> i & 1, !(a.usedMask >> i & 1)); y += 100; }
            g.text(ph == OVER ? "Fin de la partida" : "A para la próxima ronda", 765, 574, 16, rgb(0xd8b46a), F_BODY, AL_C); return; }
        // mesa
        float dx = 480; sp::draw(g, { dx, 190, 96, 134 }, 0, 1, false); g.text(std::to_string(stock.size()) + " en el mazo", dx + 48, 342, 14, pal::cream, F_BOLD, AL_C);
        if (!disc.empty()) sp::draw(g, { dx + 190, 190, 96, 134 }, disc.back().s, disc.back().r, true); g.text("descarte (" + std::to_string(disc.size()) + ")", dx + 238, 342, 14, pal::cream, F_BOLD, AL_C);
        if (mine && ph == DRAW) { Rect b1{ dx - 30, 360, 160, 48 }, b2{ dx + 160, 360, 160, 48 }; if (ui.button("Robar", b1, 3, true, "🃏")) { drawStock(); return; } if (ui.button("Tomar descarte", b2, 2, !disc.empty())) { drawDisc(); return; } ui.setFocusIfNone(hid("Robar🃏", (int)(b1.x * 7 + b1.y))); }
        auto& h = hands[turn]; Analysis an; if (peek) an = analyze(h);
        if (peek) { int n = (int)h.size(); float hw = 90, hh = 126, x0 = 765 - (n * (hw + 10) - 10) / 2;
            for (int i = 0; i < n; i++) { Rect r{ x0 + i * (hw + 10), 470.f - (i == sel ? 22 : 0), hw, hh }; uint32_t id = hid("ch", i); bool hit = ui.hit(id, r, mine && ph == DISCARD); sp::draw(g, r, h[i].s, h[i].r, true, i == sel); if (an.usedMask >> i & 1) g.rrect({ r.x + 8, r.y + r.h + 4, r.w - 16, 6 }, 3, rgb(0x4caf50)); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { sel = i == sel ? -1 : i; app->audio.play("click"); } }
            if (onPhone(turn)) coverHand(g, ui, "ch", n, turn);
            g.text("Sueltas: " + std::to_string(an.dead) + " puntos (las cartas con barra verde forman juegos)", 765, 440, 15, pal::cream, F_BODY, AL_C);
            if (mine && ph == DISCARD) { if (ui.button("Descartar", { 520, 420 + 0, 0, 0 }, 3, false)) {} }
            if (mine && ph == DISCARD && sel >= 0) { float by = 625; if (ui.button("Descartar", { 520, by, 220, 46 }, 3, true)) { discard(sel); return; } int cc = canClose(sel); if (ui.button(cc >= 0 ? "Cerrar (" + std::to_string(cc) + ")" : "Cerrar", { 780, by, 220, 46 }, 1, cc >= 0)) { closeWith(sel); return; } }
            if (mine && n) ui.setFocusIfNone(hid("ch", 0)); }
        else if (mine) g.text("Mantén L para ver tus cartas", 765, 520, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("ch", (int)i, me) + std::to_string(hands[p][i].r) + " de " + sp::suitName(hands[p][i].s) + (me && (int)i == sel ? "  ⬆" : "") + "\n"; return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" }, { "X", "Descartar la elegida" }, { "Y", "Cerrar con la elegida" }, { "L", "Ver tus cartas" } }; }
    std::string help() const override { return "Cada uno recibe 7 cartas. En tu turno robas (del mazo o el descarte) y descartas una. Los juegos son 3 o 4 cartas del mismo número, o escaleras de 3 o más del mismo palo (… 6, 7, sota, caballo, rey). Puedes cerrar cuando lo que te queda suelto suma 5 o menos: los demás se anotan sus puntos sueltos. Cerrar con todo armado resta 10; un chinchón (7 en escalera) resta 25. Quien pasa el límite queda eliminado."; }
};
}
Game* makeChinchon() { return new Chin(); }
