// Cartas de colores: 2 a 8 jugadores. Se juega una carta del mismo color, número o símbolo que la de arriba.
// Especiales: 🚫 salta, ⇄ cambia el sentido (con 2 jugadores salta), +2 y +4 hacen robar y perder el turno, 🌈 comodín. Gana quien se queda sin cartas.
#include "cards.h"
#include <algorithm>

namespace {
const char COLS[4] = { 'r', 'y', 'g', 'b' };
const char* const CNAME[4] = { "rojo", "amarillo", "verde", "azul" };
Col colRGB(char c) { return c == 'r' ? rgb(0xe53935) : c == 'y' ? rgb(0xf9c80e) : c == 'g' ? rgb(0x2e9e4a) : c == 'b' ? rgb(0x1e6fd9) : rgb(0x222228); }
struct Card { char c; std::string k; };
bool isNum(const std::string& k) { return k.size() == 1 && k[0] >= '0' && k[0] <= '9'; }
bool cmpCard(const Card& a, const Card& b) { if (a.c != b.c) return std::string("rygbw").find(a.c) < std::string("rygbw").find(b.c); return a.k < b.k; }

struct Colors : Game {
    int N = 0, turn = 0, dir = 1, winner = -1, resigned = -1; char color = 'r'; bool multi = false, drawUntil = false, drew = false, over = false;
    std::vector<Card> deck, discard; std::vector<std::vector<Card>> hands; std::vector<bool> active, sel; std::string lastMsg; float aiT = 0, aiWait = 1, endT = 0;
    std::vector<int> pendingList; bool pickColor = false;
    void start() override {
        N = (int)pl.size(); multi = opts.get("multi", 0) != 0; drawUntil = opts.get("drawUntil", 0) != 0;
        for (int ci = 0; ci < 4; ci++) { char c = COLS[ci]; deck.push_back({ c, "0" }); for (int n = 1; n <= 9; n++) { deck.push_back({ c, std::to_string(n) }); deck.push_back({ c, std::to_string(n) }); }
            for (const char* k : { "skip", "rev", "p2" }) { deck.push_back({ c, k }); deck.push_back({ c, k }); } }
        for (int i = 0; i < 4; i++) { deck.push_back({ 'w', "w" }); deck.push_back({ 'w', "p4" }); }
        shuffleDeck(); hands.assign(N, {}); active.assign(N, true); sel.assign(120, false);
        for (int i = 0; i < 7; i++) for (int p = 0; p < N; p++) hands[p].push_back(draw1());
        Card f = draw1(); while (f.c == 'w' || !isNum(f.k)) { deck.insert(deck.begin(), f); shuffleDeck(); f = draw1(); }
        discard.push_back(f); color = f.c; for (auto& h : hands) std::sort(h.begin(), h.end(), cmpCard); resetAi();
    }
    void shuffleDeck() { for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]); }
    Card draw1() { if (deck.empty()) { if (discard.size() <= 1) return { 'w', "w" }; Card t = discard.back(); discard.pop_back(); deck = discard; discard.clear(); discard.push_back(t); shuffleDeck(); } Card c = deck.back(); deck.pop_back(); return c; }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = lv == 1 ? 1.0f + rnd01() * 0.8f : lv == 3 ? 0.5f + rnd01() * 0.5f : 0.8f + rnd01() * 0.6f; }
    int turnPlayer() const override { return over ? -1 : turn; }
    int nextP(int from, int steps = 1) const { int p = from; for (int s = 0; s < steps; s++) { do { p = (p + dir + N) % N; } while (!active[p]); } return p; }
    int nActive() const { int n = 0; for (bool a : active) if (a) n++; return n; }
    const Card& top() const { return discard.back(); }
    bool playable(const Card& c) const { return c.c == 'w' || c.c == color || c.k == top().k; }
    bool multiOK(const Card& c) const { return multi && isNum(c.k); }   // varias a la vez: solo cartas con número
    void take(int p, int k) { for (int i = 0; i < k; i++) { Card c = draw1(); hands[p].push_back(c); } }
    // jugar una lista de índices (la última carta decide el color)
    bool play(int p, std::vector<int> idx, char chosen) {
        if (over || p != turn || idx.empty()) return false; std::vector<Card> cs; for (int i : idx) cs.push_back(hands[p][i]);
        if (drew && idx[0] != (int)hands[p].size() - 1) return false;
        if (!playable(cs[0])) return false; if (cs.size() > 1) { if (!multiOK(cs[0])) return false; for (auto& c : cs) if (c.k != cs[0].k) return false; }
        std::sort(idx.begin(), idx.end(), std::greater<int>()); for (int i : idx) hands[p].erase(hands[p].begin() + i);
        for (auto& c : cs) discard.push_back(c); const Card& lc = cs.back(); color = lc.c == 'w' ? chosen : lc.c; drew = false; int m = (int)cs.size(); app->audio.play("place");
        lastMsg = pl[p].name + " jugó " + (m > 1 ? std::to_string(m) + "× " : "") + label(lc) + (lc.c == 'w' ? std::string(" y eligió ") + CNAME[std::string("rygb").find(color)] : "");
        if (hands[p].empty()) { over = true; winner = p; return true; }
        int nxt = nextP(p); const std::string& k = cs[0].k;
        if (k == "rev") { if (nActive() == 2) nxt = nextP(nextP(p)); else { if (m % 2 == 1) dir = -dir; nxt = nextP(p); } }
        else if (k == "skip") nxt = nextP(p, 1 + std::min(m, std::max(0, nActive() - 2)));
        else if (k == "p2") { take(nxt, 2 * m); lastMsg += " · " + pl[nxt].name + " roba " + std::to_string(2 * m); nxt = nextP(nxt); }
        else if (k == "p4") { take(nxt, 4 * m); lastMsg += " · " + pl[nxt].name + " roba " + std::to_string(4 * m); nxt = nextP(nxt); }
        turn = nxt; std::fill(sel.begin(), sel.end(), false); resetAi(); return true;
    }
    bool anyPlayable(int p) const { for (auto& c : hands[p]) if (playable(c)) return true; return false; }
    void drawAction(int p) {
        if (over || p != turn || drew) return;
        if (drawUntil) { int guard = 0; do { take(p, 1); guard++; } while (!playable(hands[p].back()) && guard < 60); drew = true; app->audio.play("flip"); lastMsg = pl[p].name + " robó hasta poder jugar"; if (!playable(hands[p].back())) pass(p); return; }
        take(p, 1); drew = true; app->audio.play("flip"); lastMsg = pl[p].name + " robó una carta"; if (!playable(hands[p].back())) pass(p);
    }
    void pass(int p) { if (p != turn) return; drew = false; turn = nextP(p); std::fill(sel.begin(), sel.end(), false); resetAi(); }
    static std::string label(const Card& c) { std::string n = c.k == "skip" ? "🚫" : c.k == "rev" ? "⇄" : c.k == "p2" ? "+2" : c.k == "w" ? "Comodín" : c.k == "p4" ? "+4" : c.k; if (c.c == 'w') return n; return n + " " + (c.c == 'r' ? "rojo" : c.c == 'y' ? "amarillo" : c.c == 'g' ? "verde" : "azul"); }
    char bestColor(const std::vector<Card>& hand) const { int cnt[4] = {}; for (auto& c : hand) { auto p = std::string("rygb").find(c.c); if (p != std::string::npos) cnt[p]++; } int b = 0; for (int i = 1; i < 4; i++) if (cnt[i] > cnt[b]) b = i; return COLS[b]; }
    void resign(int p) override { active[p] = false; resigned = p; for (auto& c : hands[p]) discard.insert(discard.begin(), c); hands[p].clear(); int a = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { a++; w = i; } if (a <= 1) { over = true; winner = w; } else if (turn == p) { turn = nextP(p); drew = false; resetAi(); } }
    void aiMove() {
        int p = turn, lv = std::max(1, std::min(3, pl[p].level)); auto& h = hands[p];
        std::vector<int> pl2; for (size_t i = 0; i < h.size(); i++) if (playable(h[i]) && (!drew || i + 1 == h.size())) pl2.push_back((int)i);
        if (pl2.empty()) { if (!drew) drawAction(p); else pass(p); return; }
        if (drew && !drawUntil && lv == 1 && rnd01() < 0.25f) { pass(p); return; }
        int cnt[5] = {}; for (auto& c : h) { auto q = std::string("rygbw").find(c.c); cnt[q]++; } int nx = nextP(p); bool danger = lv >= 2 && hands[nx].size() <= 2;
        int bi = pl2[0]; float bs = -1e9f;
        for (int i : pl2) { const Card& c = h[i]; float sc = 0; if (c.c == 'w') sc -= 6; else sc += cnt[std::string("rygbw").find(c.c)] * 0.4f;
            if (c.k == "p2" || c.k == "skip" || c.k == "rev") sc += danger ? 6 : 1.5f; if (c.k == "p4") sc += danger ? 8 : 0; if (isNum(c.k)) sc += (c.k[0] - '0') * 0.05f + 1; if (h.size() <= 2) sc += 4;
            sc += rnd01() * (lv == 1 ? 4.f : lv == 2 ? 1.2f : 0.3f); if (sc > bs) { bs = sc; bi = i; } }
        std::vector<int> list{ bi }; const Card c0 = h[bi];
        if (multi && !drew && isNum(c0.k)) { for (size_t j = 0; j < h.size(); j++) if ((int)j != bi && h[j].k == c0.k && rnd01() < (lv == 1 ? 0.5f : 1.f)) list.push_back((int)j); }
        std::vector<Card> rest; for (size_t j = 0; j < h.size(); j++) if (std::find(list.begin(), list.end(), (int)j) == list.end()) rest.push_back(h[j]);
        play(p, list, bestColor(rest));
    }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 1.2f && !finished) { std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ std::max(0, 20 - (int)hands[i].size()) * (i == winner ? 1 : 0), i == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, winner >= 0 ? "Ganó " + pl[winner].name : "Fin"); } return; }
        if (!pl[turn].ai) {
            Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
            if (pd && !pickColor) { if (pd->down[B_X]) { std::vector<int> idx; for (size_t i = 0; i < hands[turn].size(); i++) if (sel[i]) idx.push_back((int)i); tryPlay(idx); } if (pd->down[B_Y]) { if (!drew) drawAction(turn); else pass(turn); } }
            return;
        }
        aiT += dt; if (aiT < aiWait) return; aiMove();
    }
    void tryPlay(std::vector<int> idx) {
        if (idx.empty()) return; if (!playable(hands[turn][idx[0]])) { app->toast("Esa carta no se puede jugar"); return; }
        for (int i : idx) if (hands[turn][i].c == 'w') { pendingList = idx; pickColor = true; return; }
        play(turn, idx, color);
    }
    void drawCard(Gfx& g, Rect r, const Card& c, bool glow, bool dim) {
        g.rrect({ r.x + 2, r.y + 5, r.w, r.h }, 10, rgb(0, 100)); if (glow) g.rrect({ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 13, pal::gold);
        g.rrect(r, 10, rgb(0xffffff)); g.rrect(r.inset(4), 8, c.c == 'w' ? rgb(0x222228) : colRGB(c.c));
        if (c.c == 'w') { for (int i = 0; i < 4; i++) g.circle(r.cx() + (i % 2 ? 1 : -1) * r.w * 0.17f, r.cy() + (i / 2 ? 1 : -1) * r.w * 0.17f, r.w * 0.15f, colRGB(COLS[i])); }
        std::string t = c.k == "skip" ? "🚫" : c.k == "rev" ? "⇄" : c.k == "p2" ? "+2" : c.k == "w" ? "" : c.k == "p4" ? "+4" : c.k;
        if (c.k == "skip") g.emoji("🚫", r.cx(), r.cy(), r.w * 0.55f); else if (!t.empty()) g.textShadow(t, r.cx(), r.cy(), r.w * (t.size() > 1 ? 0.42f : 0.55f), pal::white, F_TITLE, AL_C);
        if (dim) g.rrect(r, 10, rgb(0, 130));
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : mine ? pl[turn].name + ": juega una carta" + (drew ? " o pasa" : " o roba") : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(180.f, 940.f / N);
        for (int i = 0; i < N; i++) { Rect r{ 290 + i * (pw + 2), 66, pw - 4, 52 }; bool on = i == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 20, r.cy(), 22); g.text(pl[i].name, r.x + 38, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 50); g.text(active[i] ? std::to_string(hands[i].size()) + " cartas" + (hands[i].size() == 1 ? " ¡UNO!" : "") : "fuera", r.x + 38, r.y + 36, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 50); }
        // mesa
        drawCard(g, { 560, 150, 96, 136 }, top(), false, false); Rect cb{ 560, 296, 96, 20 }; g.rrect(cb, 10, colRGB(color)); g.text("color: " + std::string(CNAME[std::string("rygb").find(color)]), cb.cx(), cb.cy(), 13, pal::white, F_BOLD, AL_C);
        cards::drawBack(g, { 430, 150, 96, 136 }); g.text(std::to_string(deck.size()) + " en el mazo", 478, 306, 14, pal::cream, F_BOLD, AL_C);
        g.text(std::string(dir > 0 ? "sentido →" : "sentido ←"), 800, 175, 18, pal::cream, F_BOLD, AL_C); g.text(lastMsg, 940, 230, 17, pal::cream, F_BODY, AL_C, 560);
        auto& h = hands[turn];
        if (mine) {
            Rect rb{ 720, 150, 200, 46 };
            if (pickColor) { g.rrect({ 440, 330, 700, 120 }, 14, rgb(0x120c08, 230)); g.text("Elige el color", 790, 352, 20, pal::gold, F_BOLD, AL_C);
                for (int i = 0; i < 4; i++) { Rect r{ 470.f + i * 165, 372, 150, 60 }; uint32_t id = hid("col", i); bool hit = ui.hit(id, r, true); g.rrect(r, 12, colRGB(COLS[i])); g.text(CNAME[i], r.cx(), r.cy(), 20, pal::white, F_BOLD, AL_C); if (ui.focused(id)) ui.drawFocus(r, 12);
                    if (hit) { play(turn, pendingList, COLS[i]); pickColor = false; pendingList.clear(); return; } } ui.setFocusIfNone(hid("col", 0)); return; }
            if (ui.button(drew ? "Pasar" : "Robar carta", { 690, 196, 220, 48 }, drew ? 2 : 3, true, drew ? "" : "🃏")) { if (drew) pass(turn); else drawAction(turn); return; }
            int ns = 0; for (size_t i = 0; i < h.size(); i++) if (sel[i]) ns++;
            if (multi && ui.button("Jugar " + std::to_string(ns) + " marcadas", { 920, 196, 250, 48 }, 3, ns > 0)) { std::vector<int> idx; for (size_t i = 0; i < h.size(); i++) if (sel[i]) idx.push_back((int)i); tryPlay(idx); return; } (void)rb;
        }
        if (peek) {
            int n = (int)h.size(); int perRow = 12; float cw = 76, ch = 108, stepX = std::min(cw + 10, 950.f / perRow); float y0 = n > perRow ? 350 : 400;
            for (int i = 0; i < n; i++) { int rr = i / perRow, cc = i % perRow; Rect r{ 290 + cc * stepX, y0 + rr * 96.f - (sel[i] ? 16 : 0), cw, ch }; uint32_t id = hid("cc", i); bool ok = playable(h[i]) && (!drew || i + 1 == n);
                bool hit = ui.hit(id, { r.x, r.y, stepX - 2, ch }, mine); drawCard(g, r, h[i], sel[i], !ok); if (ui.focused(id)) ui.drawFocus(r, 10);
                if (hit) { int first = -1; for (size_t j = 0; j < h.size(); j++) if (sel[j]) { first = (int)j; break; }
                    bool twins = false; if (multi && isNum(h[i].k)) for (size_t j = 0; j < h.size(); j++) if ((int)j != i && h[j].k == h[i].k) twins = true;
                    bool joins = first >= 0 && isNum(h[i].k) && h[i].k == h[first].k;   // se suma a las ya marcadas (mismo número)
                    if (first >= 0 && !joins && !sel[i]) { std::fill(sel.begin(), sel.end(), false); first = -1; }   // otra carta: empieza de nuevo
                    if (!ok && !(joins && multi)) app->toast("Esa carta no se puede jugar");
                    else if (multi && (joins || sel[i] || twins)) { sel[i] = !sel[i]; app->audio.play("click"); } else tryPlay({ i }); if (over || (int)hands.size() == 0) return; } }
            if (onPhone(turn)) coverHand(g, ui, "cc", n, turn);
            if (mine && n && !ui.focus) ui.setFocusIfNone(hid("cc", 0));
        } else if (mine) g.text("Mantén L para ver tus cartas", 765, 440, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) { const Card& c = hands[p][i]; size_t ci = std::string("rygb").find(c.c); std::string col = c.c == 'w' ? "🌈" : ci == 0 ? "🔴" : ci == 1 ? "🟡" : ci == 2 ? "🟢" : "🔵"; s += mark("cc", (int)i, me) + col + " " + label(c) + (me && i < sel.size() && sel[i] ? "  ✔" : "") + "\n"; } return s; }
    std::vector<Hint> hints() const override { std::vector<Hint> v{ { "DPAD/STICK", "Moverse" }, { "A", multi ? "Marcar / jugar carta" : "Jugar la carta" } }; if (multi) v.push_back({ "X", "Jugar las marcadas" }); v.push_back({ "Y", "Robar / pasar" }); v.push_back({ "L", "Ver tus cartas" }); return v; }
    std::string help() const override { return "Juega una carta del mismo color, número o símbolo que la de arriba. 🚫 salta al siguiente, ⇄ cambia el sentido (con 2 jugadores salta), +2 y +4 hacen que el siguiente robe y pierda el turno, y el comodín 🌈 deja elegir el color. Si no puedes jugar, roba una carta (si sirve, puedes jugarla). Gana quien se queda sin cartas."; }
};
}
Game* makeCartasColores() { return new Colors(); }
