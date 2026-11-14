// Okey, 2-4 jugadores, 106 fichas (4 colores x 1-13 x 2 + 2 falsos comodines). El OKEY es la ficha del mismo color que el indicador y número +1 (el 13 pasa al 1): las reales son comodines.
// Cada turno robas (mazo o último descarte del jugador anterior) y descartas. Se cierra con 14 fichas todas armadas (grupos de 3-4 del mismo número y colores distintos, o escaleras de 3+ del mismo color).
// Gana la mano quien cierra; se suman los puntos sueltos de los demás y al final gana el menor puntaje. Tu atril se ordena solo mostrando las jugadas armadas.
#include "../app.h"
#include <algorithm>
#include <map>

namespace {
const int WILD_PTS = 25;
struct Tl { int id, c, n; bool fake; };
struct An { int dead = 0; std::vector<std::vector<int>> melds; std::vector<int> loose; };
struct Fr { int dead; int m, u; long nx; };
An analyzeT(const std::vector<Tl>& hand, int okC, int okN) {
    std::vector<int> nt, wl; std::vector<int> col, num;
    for (size_t i = 0; i < hand.size(); i++) { const Tl& t = hand[i]; bool wild = !t.fake && t.c == okC && t.n == okN; if (wild) wl.push_back((int)i); else { nt.push_back((int)i); col.push_back(t.fake ? okC : t.c); num.push_back(t.fake ? okN : t.n); } }
    int nn = (int)nt.size(), W = (int)wl.size(); std::map<int, std::vector<int>> look; for (int k = 0; k < nn; k++) look[col[k] * 14 + num[k]].push_back(k);
    std::map<long, Fr> memo; auto find = [&](int rem, int c, int n) { auto it = look.find(c * 14 + n); if (it != look.end()) for (int j : it->second) if (rem & (1 << j)) return j; return -1; };
    std::function<Fr(int, int)> f = [&](int rem, int w) -> Fr {
        if (!rem) return { WILD_PTS * w, 0, 0, -1 }; long key = (long)rem * 16 + w; auto h = memo.find(key); if (h != memo.end()) return h->second;
        int i = 0; while (!(rem & (1 << i))) i++; int c = col[i], v = num[i], bit = 1 << i; Fr r0 = f(rem & ~bit, w); Fr best{ r0.dead + v, 0, 0, (long)(rem & ~bit) * 16 + w };
        auto tryMeld = [&](int m, int u) { if (u > w) return; int nr = rem & ~m; Fr r = f(nr, w - u); if (r.dead < best.dead) best = { r.dead, m, u, (long)nr * 16 + (w - u) }; };
        int oth[3], on = 0; for (int x = 0; x < 4; x++) if (x != c) oth[on++] = x;
        for (int s = 1; s < 8; s++) { int cnt = (s & 1) + ((s >> 1) & 1) + ((s >> 2) & 1); if (cnt < 2) continue; int m = bit, u = 0; for (int x = 0; x < 3; x++) if (s & (1 << x)) { int k = find(rem, oth[x], v); if (k >= 0) m |= 1 << k; else u++; } tryMeld(m, u); }
        for (int s = 1; s <= 12; s++) for (int e = s + 2; e <= 14 && e - s + 1 <= 13; e++) { int m = 0, u = 0; bool has = false; for (int p = s; p <= e; p++) { int n = p > 13 ? 1 : p; if (n == v && !has) { m |= bit; has = true; continue; } int k = find(rem, c, n); if (k >= 0 && k != i) m |= 1 << k; else u++; if (u > w) break; } if (has && u <= w) tryMeld(m, u); }
        memo[key] = best; return best;
    };
    int full = nn ? (1 << nn) - 1 : 0; Fr top = f(full, W); An a; a.dead = top.dead; int rem = full, w = W; std::vector<bool> used(hand.size(), false);
    for (int g = 0; g < 200; g++) { Fr r = f(rem, w); if (!r.m && !r.u) { if (!rem) break; int i2 = 0; while (!(rem & (1 << i2))) i2++; rem &= ~(1 << i2); continue; }
        std::vector<int> grp; for (int k = 0; k < nn; k++) if (r.m & (1 << k)) grp.push_back(nt[k]); std::sort(grp.begin(), grp.end(), [&](int x, int y) { int nx = hand[x].fake ? okN : hand[x].n, ny = hand[y].fake ? okN : hand[y].n; return nx != ny ? nx < ny : x < y; });
        for (int u = 0; u < r.u; u++) grp.push_back(wl[wl.size() - 1 - (W - w) - u]); for (int x : grp) used[x] = true; a.melds.push_back(grp); rem &= ~r.m; w -= r.u; }
    for (size_t i = 0; i < hand.size(); i++) if (!used[i]) a.loose.push_back((int)i); return a;
}
struct Okey : Game {
    int N = 0, turn = 0, hands = 3, handNo = 1, starter = -1, winner = -1, resigned = -1, indC = 0, indN = 1, okC = 0, okN = 2, sel = -1; bool over = false, handEnd = false, hasDrawn = false; std::vector<Tl> all; std::vector<int> deck; std::vector<std::vector<Tl>> H; std::vector<std::vector<Tl>> disc; std::vector<int> scores; std::vector<bool> gone;
    std::string msg, info; float aiT = 0, aiWait = 1, endT = 0, showT = 0; Tl ind{};
    void start() override { N = (int)pl.size(); int o = opts.get("hands", 1); hands = o == 0 ? 1 : o == 2 ? 5 : 3; scores.assign(N, 0); gone.assign(N, false); for (int c = 0; c < 4; c++) for (int n = 1; n <= 13; n++) for (int k = 0; k < 2; k++) all.push_back({ c * 26 + (n - 1) * 2 + k, c, n, false }); all.push_back({ 104, 0, 1, true }); all.push_back({ 105, 0, 1, true }); newHand(); }
    bool isWild(const Tl& t) const { return !t.fake && t.c == okC && t.n == okN; }
    void newHand() {
        std::vector<Tl> d = all; for (int i = (int)d.size() - 1; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]); int ii = 0; while (d[ii].fake) ii++; ind = d[ii]; d.erase(d.begin() + ii); indC = ind.c; indN = ind.n; okC = indC; okN = indN % 13 + 1;
        starter = (starter + 1) % N; H.assign(N, {}); disc.assign(N, {}); for (int p = 0; p < N; p++) { int cnt = p == starter ? 15 : 14; for (int k = 0; k < cnt; k++) { H[p].push_back(d.back()); d.pop_back(); } }
        deck.clear(); for (auto& t : d) deck.push_back(t.id); deckT = d; turn = starter; hasDrawn = true; handEnd = false; sel = -1; msg = pl[turn].name + " empieza (15 fichas): descarta una"; resetAi();
    }
    std::vector<Tl> deckT;
    void resetAi() { aiT = 0; aiWait = 0.9f + rnd01() * 0.8f; }
    int turnPlayer() const override { return over || handEnd ? -1 : turn; }
    int loosePts(int p) { auto a = analyzeT(H[p], okC, okN); int s = 0; for (int i : a.loose) s += isWild(H[p][i]) ? WILD_PTS : (H[p][i].fake ? okN : H[p][i].n); return s + (int)a.dead * 0; }
    int deadOf(const std::vector<Tl>& h) { return analyzeT(h, okC, okN).dead; }
    void finishHand(int w, bool wildClose) {
        handEnd = true; showT = 0; winner = w; std::string s; for (int p = 0; p < N; p++) { if (gone[p]) continue; int pts; if (p == w) pts = wildClose ? -4 : -2; else { pts = deadOf(H[p]); if (w >= 0 && wildClose) pts *= 2; } scores[p] += pts; s += pl[p].name + " " + (pts >= 0 ? "+" : "") + std::to_string(pts) + "   "; }
        info = (w >= 0 ? pl[w].name + " cerró la mano" : "Se acabó el mazo") + (wildClose ? " con comodín (doble)" : ""); info += "\n" + s; app->audio.play("win"); if (handNo >= hands) { over = true; int bs = 1 << 30; for (int p = 0; p < N; p++) if (!gone[p] && scores[p] < bs) { bs = scores[p]; winner = p; } }
    }
    void drawDeck() { if (hasDrawn || deckT.empty()) return; H[turn].push_back(deckT.back()); deckT.pop_back(); hasDrawn = true; msg = pl[turn].name + " robó del mazo"; sel = -1; app->audio.play("click"); }
    void takeDiscard() { int pv = (turn - 1 + N) % N; if (hasDrawn || disc[pv].empty()) return; H[turn].push_back(disc[pv].back()); disc[pv].pop_back(); hasDrawn = true; msg = pl[turn].name + " tomó el descarte"; sel = -1; app->audio.play("click"); }
    void discard(int idx) {
        if (!hasDrawn || idx < 0 || idx >= (int)H[turn].size()) return; Tl t = H[turn][idx]; H[turn].erase(H[turn].begin() + idx); app->audio.play("place");
        if (H[turn].size() == 14 && deadOf(H[turn]) == 0) { disc[turn].push_back(t); finishHand(turn, isWild(t)); return; }
        disc[turn].push_back(t); msg = pl[turn].name + " descartó"; sel = -1;
        if (deckT.empty()) { finishHand(-1, false); return; }
        do { turn = (turn + 1) % N; } while (gone[turn]); hasDrawn = false; resetAi();
    }
    void resign(int p) override { gone[p] = true; resigned = p; int left = 0, w = -1; for (int q = 0; q < N; q++) if (!gone[q]) { left++; w = q; } if (left <= 1) { over = true; handEnd = true; winner = w; return; } if (turn == p) { do { turn = (turn + 1) % N; } while (gone[turn]); hasDrawn = false; resetAi(); } }
    int bestDiscard(int p, int lv, int& deadOut) {
        auto& h = H[p]; int bi = 0, bd = 1 << 30; float bs = -1e9f; for (int i = 0; i < (int)h.size(); i++) { auto t = h; t.erase(t.begin() + i); int d = analyzeT(t, okC, okN).dead; float sc = -d * 10.f + (h[i].fake ? 0 : h[i].n * 0.1f) + rnd01() * 0.05f; if (isWild(h[i])) sc -= 100; if (sc > bs) { bs = sc; bi = i; bd = d; } } deadOut = bd; return bi;
    }
    void aiAct() {
        int p = turn; int lv = std::max(1, std::min(3, pl[p].level));
        if (!hasDrawn) { int pv = (turn - 1 + N) % N; bool take = false;
            if (!disc[pv].empty() && lv >= 2) { int d0; bestDiscard(p, lv, d0); H[p].push_back(disc[pv].back()); int d1; bestDiscard(p, lv, d1); H[p].pop_back(); take = d1 < d0 || (isWild(disc[pv].back())); }
            if (take) takeDiscard(); else drawDeck(); aiT = 0; aiWait = 0.8f; return; }
        int d; int bi = bestDiscard(p, lv, d); if (lv == 1 && rnd01() < 0.4f && H[p].size() > 1) bi = rndInt((int)H[p].size()); discard(bi);
    }
    void update(float dt, Input& in, UI&) override {
        if (handEnd) { showT += dt; if (over) { endT += dt; if (endT > 3.f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) r.push_back({ scores[p], p == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + pl[winner].name + " (" + std::to_string(scores[winner]) + " pts)"); } return; }
            bool adv = showT > 6.f; for (int i = 0; i < N; i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && showT > 1.f) adv = true; if (adv) { handNo++; newHand(); } return; }
        if (pl[turn].ai) { aiT += dt; if (aiT >= aiWait) aiAct(); return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (pd && hasDrawn && pd->down[B_X] && sel >= 0) discard(sel);
    }
    void tileDraw(Gfx& g, Rect r, const Tl& t, bool glow, bool dim = false) {
        static const uint32_t C[4] = { 0xc8332a, 0x1f5fb0, 0xe58a1f, 0x222222 }; g.rrect({ r.x + 1, r.y + 3, r.w, r.h }, 6, rgb(0, 90)); g.rrect(r, 6, glow ? pal::gold : rgb(0x8a7a50)); g.rrectGrad(r.inset(glow ? 3.f : 1.5f), 5, rgb(0xfff8e4), rgb(0xe6d8b4));
        if (t.fake) { g.text("✱", r.cx(), r.cy(), r.h * 0.5f, rgb(0x2a8a50), F_BOLD, AL_C); g.text("falso", r.cx(), r.y + r.h * 0.85f, r.h * 0.16f, rgb(0x444444), F_BOLD, AL_C); }
        else { g.text(std::to_string(t.n), r.cx(), r.y + r.h * 0.42f, r.h * 0.46f, rgb(C[t.c]), F_TITLE, AL_C); g.circle(r.cx(), r.y + r.h * 0.8f, r.h * 0.09f, rgb(C[t.c])); if (isWild(t)) g.text("★", r.x + r.w - 7, r.y + 9, r.h * 0.22f, rgb(0xd9a400), F_BOLD, AL_C); }
        if (dim) g.rrect(r, 6, rgb(0, 100));
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !handEnd && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(over ? "¡Ganó " + pl[winner].name + "!" : handEnd ? "Fin de la mano" : mine ? pl[turn].name + (hasDrawn ? ": descarta una ficha" : ": roba una ficha") : "Turno de " + pl[turn].name, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(230.f, 940.f / N); for (int p = 0; p < N; p++) { Rect r{ 290 + p * (pw + 2), 64, pw - 4, 46 }; bool on = p == turn && !over && !handEnd; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(pl[p].emoji + " " + pl[p].name, r.x + 10, r.y + 15, 14, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 20); g.text(std::to_string(H[p].size()) + " fichas · " + std::to_string(scores[p]) + " pts", r.x + 10, r.y + 34, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 20); }
        g.text(msg, 765, 128, 16, pal::cream, F_BODY, AL_C, 800); g.text("Mano " + std::to_string(handNo) + "/" + std::to_string(hands), 1230, 128, 14, pal::cream, F_BODY, AL_R);
        // centro: indicador, mazo, descartes
        g.rrectGrad({ 290, 142, 950, 190 }, 18, rgb(0x2f7a4a), rgb(0x1f5a36)); g.text("Indicador", 360, 160, 14, pal::cream, F_BOLD, AL_C); tileDraw(g, { 330, 172, 60, 82 }, ind, false); g.text("OKEY: " + std::string(okC == 0 ? "rojo" : okC == 1 ? "azul" : okC == 2 ? "naranja" : "negro") + " " + std::to_string(okN), 360, 280, 14, pal::gold, F_BOLD, AL_C, 120);
        for (int p = 0; p < N; p++) { Rect r{ 470.f + p * 175, 172, 60, 82 }; g.text(pl[p].name, r.cx() + 20, 160, 13, pal::cream, F_BODY, AL_C, 160); if (!disc[p].empty()) tileDraw(g, r, disc[p].back(), false); else g.rrectLine(r, 6, 2, rgb(0xffffff, 80)); g.text(std::to_string(disc[p].size()), r.cx() + 40, r.y + 70, 13, pal::cream, F_BODY, AL_C); }
        if (handEnd) { g.rrect({ 330, 200, 870, 190 }, 18, pal::panelEdge); g.rrectGrad({ 333, 203, 864, 184 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow(over ? "Fin del juego" : "Fin de la mano", 765, 236, 26, pal::gold, F_TITLE, AL_C); g.textBox(info, { 360, 262, 810, 100 }, 17, pal::cream, F_BODY, AL_C); if (!over) g.text("A: siguiente mano", 765, 364, 14, pal::cream, F_BODY, AL_C); return; }
        if (mine) {
            int pv = (turn - 1 + N) % N; bool canTake = !hasDrawn && !disc[pv].empty(); if (!hasDrawn) { g.text("Roba del mazo (" + std::to_string(deckT.size()) + ") o toma el último descarte de " + pl[pv].name, 765, 350, 16, pal::cream, F_BOLD, AL_C);
                if (ui.button("Robar del mazo", { 430, 360, 220, 44 }, 3, !deckT.empty())) { drawDeck(); return; } if (ui.button("Tomar descarte", { 660, 360, 220, 44 }, 3, canTake)) { takeDiscard(); return; } ui.setFocusIfNone(hid("Robar del mazo", 430 * 7 + 360)); }
            else { if (ui.button("Descartar ficha elegida", { 430, 360, 300, 44 }, 3, sel >= 0)) { discard(sel); return; } }
        }
        if (peek) { auto& h = H[turn]; auto a = analyzeT(h, okC, okN); std::vector<int> order; std::vector<int> gapAfter; for (auto& m : a.melds) { for (int x : m) order.push_back(x); gapAfter.push_back((int)order.size() - 1); } std::vector<int> lo = a.loose; std::sort(lo.begin(), lo.end(), [&](int x, int y) { int cx = h[x].fake ? okC : h[x].c, cy = h[y].fake ? okC : h[y].c; if (cx != cy) return cx < cy; return (h[x].fake ? okN : h[x].n) < (h[y].fake ? okN : h[y].n); }); for (int x : lo) order.push_back(x);
            int n = (int)order.size(); int gaps = (int)gapAfter.size(); float hw = std::min(54.f, (980.f - gaps * 14) / std::max(1, n) - 3), hh = hw * 1.4f; float total = n * (hw + 3) + gaps * 14; float x = 765 - total / 2; int gi = 0;
            g.text(h.size() == 14 && a.dead == 0 ? "¡Todas armadas! Descarta una para cerrar" : "Puntos sueltos: " + std::to_string(a.dead), 765, 520, 15, pal::gold, F_BOLD, AL_C);
            for (int k = 0; k < n; k++) { int idx = order[k]; Rect r{ x, 540.f - (sel == idx ? 16 : 0), hw, hh }; uint32_t id = hid("oh", k); bool hit = ui.hit(id, r, mine); tileDraw(g, r, h[idx], sel == idx); if (ui.focused(id)) ui.drawFocus(r, 6); if (hit) sel = (sel == idx ? -1 : idx); x += hw + 3; if (gi < gaps && gapAfter[gi] == k) { x += 14; gi++; } }
            shownOrder = order; if (onPhone(turn)) coverHand(g, ui, "oh", n, turn);
            if (n && hasDrawn) ui.setFocusIfNone(hid("oh", 0)); }
        else if (mine) g.text("Mantén L para ver tus fichas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::vector<int> shownOrder;
    std::string secret(int p) const override { if (p < 0 || p >= (int)H.size() || pl[p].ai || H[p].empty()) return ""; bool me = p == turn; static const char* CE[4] = { "🔴", "🔵", "🟠", "⚫" };
        std::vector<int> ord; if (me && shownOrder.size() == H[p].size()) ord = shownOrder; else for (size_t i = 0; i < H[p].size(); i++) ord.push_back((int)i);
        std::string s = me ? "Tu turno · tus fichas:\n" : "Tus fichas:\n"; for (size_t k = 0; k < ord.size(); k++) { const Tl& t = H[p][ord[k]]; s += mark("oh", (int)k, me) + (t.fake ? std::string("✱ falso") : std::string(CE[t.c & 3]) + " " + std::to_string(t.n)) + (isWild(t) ? " (okey)" : "") + (me && ord[k] == sel ? " ⬆" : "") + ((k % 2) ? "\n" : "    "); } return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir ficha" }, { "X", "Descartar" }, { "L", "Ver tus fichas" } }; }
    std::string help() const override { return "Cada uno arma 14 fichas. La ficha indicadora define el OKEY: mismo color y número +1; las fichas okey son comodines y los 2 falsos comodines hacen de la ficha okey. En tu turno robas (mazo o último descarte del jugador anterior) y descartas una. Cierras cuando tus 14 fichas forman grupos (3-4 del mismo número y colores distintos) o escaleras (3+ del mismo color). El atril se ordena solo. Gana la mano quien cierra (-2 puntos); los demás suman sus puntos sueltos. Gana el menor puntaje."; }
};
}
Game* makeOkey() { return new Okey(); }
