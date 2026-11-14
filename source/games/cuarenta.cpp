// Cuarenta: 2 o 4 jugadores (4 = parejas), baraja española, 5 cartas. Se juega una carta: si hay iguales en la mesa te las llevas (más la escalera que siga);
// si no, puedes llevarte cartas (1-7) que SUMEN su valor, o dejarla. Caída 2, limpia 2, y al final 6 para quien juntó más cartas. Gana el primero que llega a 40.
#include "spanish.h"
#include <algorithm>

namespace {
struct Card { int s, r; };
int pos(int r) { for (int i = 0; i < 10; i++) if (sp::RANKS[i] == r) return i; return 0; }
bool cmpCard(const Card& a, const Card& b) { return a.s != b.s ? a.s < b.s : pos(a.r) < pos(b.r); }
std::vector<int> matchCapture(const std::vector<Card>& t, const Card& c) {
    std::vector<int> m; for (size_t i = 0; i < t.size(); i++) if (t[i].r == c.r) m.push_back((int)i); if (m.empty()) return m;
    for (int k = pos(c.r) + 1; k < 10; k++) { std::vector<int> g; for (size_t i = 0; i < t.size(); i++) if (t[i].r == sp::RANKS[k]) g.push_back((int)i); if (g.empty()) break; m.insert(m.end(), g.begin(), g.end()); }
    return m;
}
std::vector<std::vector<int>> sumCaptures(const std::vector<Card>& t, const Card& c) {
    std::vector<std::vector<int>> out; if (c.r > 7) return out; int n = (int)t.size();
    for (int m = 1; m < (1 << n); m++) { int s = 0; bool ok = true; for (int i = 0; i < n; i++) if (m >> i & 1) { if (t[i].r > 7) { ok = false; break; } s += t[i].r; } if (ok && s == c.r) { std::vector<int> l; for (int i = 0; i < n; i++) if (m >> i & 1) l.push_back(i); out.push_back(l); } }
    return out;
}
struct Cuarenta : Game {
    int N = 0, U = 0, target = 40, dealer = 0, turn = 0, lastCap = -1, winner = -1, resigned = -1, selH = -1, roundNo = 0; std::vector<Card> deck, table; std::vector<std::vector<Card>> hands, cap; std::vector<int> scores;
    bool over = false, roundEnd = false, prevDrop = false; int prevP = -1; Card prevCard{ 0, 0 }; std::vector<bool> selT; std::string msg; float aiT = 0, aiWait = 1, endT = 0, showT = 0; std::string roundMsg;
    int team(int p) const { return N == 4 ? p % 2 : p; }
    void start() override { N = (int)pl.size(); U = N == 4 ? 2 : N; target = opts.get("target", 1) == 0 ? 30 : opts.get("target", 1) == 2 ? 50 : 40; scores.assign(U, 0); dealer = N - 1; newRound(); }
    void newRound() {
        roundNo++; deck.clear(); for (int s = 0; s < 4; s++) for (int r : sp::RANKS) deck.push_back({ s, r }); for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]);
        hands.assign(N, {}); cap.assign(U, {}); table.clear(); dealer = (dealer + 1) % N; turn = (dealer + 1) % N; lastCap = -1; prevDrop = false; roundEnd = false; selH = -1; selT.assign(40, false); deal(); resetAi(); msg.clear();
    }
    void deal() { for (int k = 0; k < 5; k++) for (int p = 0; p < N; p++) if (!deck.empty()) { hands[p].push_back(deck.back()); deck.pop_back(); } for (auto& h : hands) std::sort(h.begin(), h.end(), cmpCard); }
    void resetAi() { aiT = 0; aiWait = 0.8f + rnd01() * 0.9f; }
    int turnPlayer() const override { return over || roundEnd ? -1 : turn; }
    void award(int u, int pts) { scores[u] += pts; if (!over && scores[u] >= target) { over = true; winner = u; } }
    bool allEmpty() const { for (auto& h : hands) if (!h.empty()) return false; return true; }
    void playCard(int ci, std::vector<int> sum) {
        int p = turn, u = team(p); Card c = hands[p][ci]; auto match = matchCapture(table, c); std::vector<int> took;
        if (!match.empty()) took = match; else took = sum;
        hands[p].erase(hands[p].begin() + ci); selH = -1; std::fill(selT.begin(), selT.end(), false);
        if (!took.empty()) {
            std::sort(took.begin(), took.end()); bool caida = false; if (!match.empty() && prevDrop && team(prevP) != u && prevCard.r == c.r) for (int i : took) if (table[i].s == prevCard.s && table[i].r == prevCard.r) caida = true;
            cap[u].push_back(c); for (int i : took) cap[u].push_back(table[i]); for (int k = (int)took.size() - 1; k >= 0; k--) table.erase(table.begin() + took[k]); lastCap = u;
            bool finalPlay = deck.empty() && allEmpty(); msg = pl[p].name + " se llevó " + std::to_string(took.size() + 1) + " cartas"; app->audio.play("win");
            if (caida) { award(u, 2); msg += " · ¡CAÍDA! +2"; } if (table.empty() && !finalPlay) { award(u, 2); msg += " · ¡LIMPIA! +2"; } prevDrop = false;
        } else { table.push_back(c); prevDrop = true; prevP = p; prevCard = c; msg = pl[p].name + " dejó " + std::to_string(c.r) + " de " + sp::suitName(c.s); app->audio.play("place"); }
        if (over) return; advance(); resetAi();
    }
    void advance() { if (allEmpty()) { if (!deck.empty()) { deal(); turn = (dealer + 1) % N; while (hands[turn].empty()) turn = (turn + 1) % N; } else endRound(); } else { do { turn = (turn + 1) % N; } while (hands[turn].empty()); } }
    void endRound() {
        if (lastCap >= 0 && !table.empty()) { for (auto& c : table) cap[lastCap].push_back(c); table.clear(); }
        std::vector<int> pts(U, 0); int mx = 0, who = -1; bool tie = false; for (int u = 0; u < U; u++) { int n = (int)cap[u].size(); if (n > mx) { mx = n; who = u; tie = false; } else if (n == mx) tie = true; } if (!tie && who >= 0) pts[who] = 6;
        roundMsg = "Cartas juntadas:"; for (int u = 0; u < U; u++) roundMsg += "  " + unitName(u) + " " + std::to_string(cap[u].size()) + (pts[u] ? " (+6)" : ""); for (int u = 0; u < U; u++) award(u, pts[u]);
        roundEnd = true; showT = 0; if (over) return; int m = *std::max_element(scores.begin(), scores.end()); (void)m;
    }
    std::string unitName(int u) const { if (N == 4) return u == 0 ? pl[0].name + " y " + pl[2].name : pl[1].name + " y " + pl[3].name; return pl[u].name; }
    void resign(int p) override { resigned = p; over = true; winner = N == 4 ? 1 - team(p) : (p + 1) % N; }
    void aiMove() {
        int lv = std::max(1, std::min(3, pl[turn].level)); auto& h = hands[turn]; float bs = -1e9f; int bi = 0; std::vector<int> bsum;
        for (int i = 0; i < (int)h.size(); i++) { auto m = matchCapture(table, h[i]); float sc = 0; std::vector<int> sm;
            if (!m.empty()) { sc = 3 + m.size() * 1.5f; if (prevDrop && team(prevP) != team(turn) && prevCard.r == h[i].r) sc += 4; if (m.size() == table.size() && !(deck.empty() && h.size() == 1)) sc += 3; }
            else { auto cs = sumCaptures(table, h[i]); if (!cs.empty()) { std::vector<int> b = cs[0]; for (auto& v : cs) if (v.size() > b.size()) b = v; sc = 2 + b.size() * 1.5f; if (b.size() == table.size()) sc += 3; sm = b; } else { sc = -1 - (h[i].r <= 7 ? 0.3f : 0) ; for (auto& t : table) if (pos(t.r) + 1 == pos(h[i].r)) sc -= 0.5f; for (auto& t : table) if (t.r == h[i].r) sc -= 5; sc += (lv >= 2 ? 0 : rnd01() * 3) - (h[i].r <= 4 ? 0.8f : 0); } }
            sc += rnd01() * (lv == 1 ? 3.f : lv == 2 ? 0.6f : 0.1f); if (sc > bs) { bs = sc; bi = i; bsum = sm; } }
        playCard(bi, bsum);
    }
    void tryPlay() {
        if (selH < 0) { app->toast("Elige una carta de tu mano"); return; } Card c = hands[turn][selH]; auto m = matchCapture(table, c); std::vector<int> sum; for (size_t i = 0; i < table.size(); i++) if (selT[i]) sum.push_back((int)i);
        if (!m.empty()) { if (!sum.empty()) { app->toast("Hay iguales: te los llevas solos (con la escalera)"); return; } playCard(selH, {}); return; }
        if (sum.empty()) { playCard(selH, {}); return; } auto cs = sumCaptures(table, c); for (auto& v : cs) if (v == sum) { playCard(selH, sum); return; } app->toast("Esas cartas no suman " + std::to_string(c.r));
    }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.5f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) r.push_back({ scores[team(p)], team(p) == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + unitName(winner)); } return; }
        if (roundEnd) { showT += dt; bool adv = showT > 6.f; for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && showT > 1.2f) adv = true; if (adv) newRound(); return; }
        if (!pl[turn].ai) { Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (pd && pd->down[B_X]) tryPlay(); return; }
        aiT += dt; if (aiT >= aiWait) aiMove();
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !roundEnd && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = over ? "¡Ganó " + unitName(winner) + "!" : roundEnd ? "Fin de la mano" : mine ? pl[turn].name + ": juega una carta" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(300.f, 940.f / U);
        for (int u = 0; u < U; u++) { Rect r{ 290 + u * (pw + 2), 66, pw - 4, 52 }; bool on = team(turn) == u && !over && !roundEnd; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.text(unitName(u), r.x + 12, r.y + 16, 14, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 24); g.text(std::to_string(scores[u]) + " pts · " + std::to_string(cap[u].size()) + " cartas", r.x + 12, r.y + 37, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 24); }
        g.text("Meta: " + std::to_string(target) + " · mazo: " + std::to_string(deck.size()), 1230, 130, 14, pal::cream, F_BODY, AL_R); g.text(msg, 765, 150, 17, pal::cream, F_BODY, AL_C, 930);
        if (roundEnd) { g.rrect({ 330, 200, 870, 260 }, 18, pal::panelEdge); g.rrectGrad({ 333, 203, 864, 254 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow("Fin de la mano", 765, 240, 26, pal::gold, F_TITLE, AL_C); g.text(roundMsg, 765, 300, 18, pal::cream, F_BODY, AL_C, 820); g.text("A para seguir", 765, 420, 16, rgb(0xd8b46a), F_BODY, AL_C); return; }
        int nt = (int)table.size(); float cw = 76, ch = 108, step = std::min(cw + 10, 900.f / std::max(1, nt));
        for (int i = 0; i < nt; i++) { Rect r{ 330 + (i % 10) * step, 190 + (i / 10) * 118.f - (selT[i] ? 12 : 0), cw, ch }; uint32_t id = hid("tb", i); bool hit = ui.hit(id, { r.x, r.y, step - 2, ch }, mine && selH >= 0); sp::draw(g, r, table[i].s, table[i].r, true, selT[i]); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { selT[i] = !selT[i]; app->audio.play("click"); } }
        if (nt == 0) g.text("Mesa vacía", 765, 240, 20, pal::cream, F_BOLD, AL_C);
        auto& h = hands[turn];
        if (mine) { std::string hint = "Elige una carta de tu mano"; if (selH >= 0) { Card c = h[selH]; auto m = matchCapture(table, c); hint = !m.empty() ? "Se lleva " + std::to_string(m.size()) + " carta(s) igual(es)/escalera" : c.r <= 7 && !sumCaptures(table, c).empty() ? "Marca cartas de la mesa que sumen " + std::to_string(c.r) + " (o déjala)" : "Se deja en la mesa"; }
            g.text(hint, 765, 430, 19, pal::cream, F_BOLD, AL_C, 800); if (ui.button("Jugar", { 565, 448, 400, 48 }, 3, selH >= 0)) { tryPlay(); return; } }
        if (peek) { int n = (int)h.size(); float hw = 90, hh = 126, x0 = 765 - (n * (hw + 12) - 12) / 2;
            for (int i = 0; i < n; i++) { Rect r{ x0 + i * (hw + 12), 520.f - (i == selH ? 20 : 0), hw, hh }; uint32_t id = hid("hd", i); bool hit = ui.hit(id, r, mine); sp::draw(g, r, h[i].s, h[i].r, true, i == selH); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { selH = i == selH ? -1 : i; std::fill(selT.begin(), selT.end(), false); app->audio.play("click"); } }
            if (onPhone(turn)) coverHand(g, ui, "hd", n, turn);
            if (mine && n) ui.setFocusIfNone(hid("hd", 0)); }
        else if (mine) g.text("Mantén L para ver tus cartas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("hd", (int)i, me) + std::to_string(hands[p][i].r) + " de " + sp::suitName(hands[p][i].s) + (me && (int)i == selH ? "  ⬆" : "") + "\n"; return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir carta" }, { "X", "Jugar la carta" }, { "L", "Ver tus cartas" } }; }
    std::string help() const override { return "Baraja española, 5 cartas cada uno. Juegas una carta: si hay cartas del mismo número en la mesa te las llevas, más la escalera de números seguidos que siga. Si no hay iguales, puedes llevarte cartas (del 1 al 7) que sumen el valor de la tuya, o dejarla. Caída (llevarte con un igual la carta que el rival acaba de dejar): 2 puntos. Limpia (dejar la mesa vacía): 2. Al final de la mano, quien juntó más cartas gana 6. Con 4 jugadores se juega en parejas (1 y 3 contra 2 y 4)."; }
};
}
Game* makeCuarenta() { return new Cuarenta(); }
