// Blackjack: cada jugador contra la banca (1 a 8 jugadores, una ronda). IA: estrategia básica con pequeños errores.
#include "cards.h"
#include <algorithm>

namespace {
struct Card { int s, r; };   // r: 0=A, 1..8 = 2..9, 9=10, 10=J, 11=Q, 12=K
int cardVal(const Card& c) { return c.r == 0 ? 11 : c.r >= 9 ? 10 : c.r + 1; }
struct An { int total; bool soft; };
An analyze(const std::vector<Card>& h) { int t = 0, a = 0; for (auto& c : h) { t += cardVal(c); if (c.r == 0) a++; } while (t > 21 && a > 0) { t -= 10; a--; } return { t, a > 0 }; }
int value(const std::vector<Card>& h) { return analyze(h).total; }
bool basicHit(const std::vector<Card>& hand, int up) {
    An h = analyze(hand); int t = h.total; if (t >= 21) return false;
    if (h.soft) { if (t <= 17) return true; if (t == 18) return up >= 9; return false; }
    if (t <= 11) return true; if (t == 12) return !(up >= 4 && up <= 6); if (t <= 16) return !(up >= 2 && up <= 6); return false;
}
bool decide(const std::vector<Card>& hand, const Card* up, int lv) {
    int u = up ? cardVal(*up) : 10; bool hit = basicHit(hand, u); An h = analyze(hand);
    bool dud = (!h.soft && h.total >= 12 && h.total <= 16) || (h.soft && (h.total == 17 || h.total == 18));
    if (lv == 1) { if (h.total < 21 && rnd01() < 0.35f) hit = h.total <= 15 ? rnd01() < 0.5f : rnd01() < 0.25f; }
    else if (dud && rnd01() < (lv == 3 ? 0.01f : 0.08f)) hit = !hit;
    return hit;
}
enum St { PLAYING, STAND, BUST, BJ };
struct BlackJack : Game {
    std::vector<Card> deck, dealer; std::vector<std::vector<Card>> hands; std::vector<int> st, res; int N = 0, turn = 0; bool over = false; float aiT = 0, aiWait = 1, endT = 0, dealT = 0; int resigned = -1;   // res: 0 pierde 1 empate 2 gana 3 blackjack
    void start() override {
        N = (int)pl.size(); deck.clear(); for (int d = 0; d < 4; d++) for (int s = 0; s < 4; s++) for (int r = 0; r < 13; r++) deck.push_back({ s, r });
        for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]);
        hands.assign(N, {}); for (auto& h : hands) { h.push_back(draw()); h.push_back(draw()); } dealer = { draw(), draw() };
        st.assign(N, PLAYING); res.assign(N, 0); for (int i = 0; i < N; i++) if (value(hands[i]) == 21) st[i] = BJ;
        turn = -1; for (int i = 0; i < N; i++) if (st[i] == PLAYING) { turn = i; break; }
        if (turn < 0) { turn = 0; dealerPlay(); } else resetAi();
    }
    Card draw() { Card c = deck.back(); deck.pop_back(); return c; }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = lv == 1 ? 0.9f + rnd01() * 1.3f : lv == 3 ? 0.5f + rnd01() * 0.6f : 0.7f + rnd01() * 0.9f; }
    int turnPlayer() const override { return over ? -1 : turn; }
    void adv() { if (st[turn] == PLAYING) { resetAi(); return; } for (int i = turn + 1; i < N; i++) if (st[i] == PLAYING) { turn = i; resetAi(); return; } dealerPlay(); }
    void hit() { if (over || st[turn] != PLAYING) return; hands[turn].push_back(draw()); app->audio.play("click"); if (value(hands[turn]) > 21) st[turn] = BUST; adv(); }
    void stand() { if (over || st[turn] != PLAYING) return; st[turn] = STAND; app->audio.play("click"); adv(); }
    void dealerPlay() {
        bool any = false; for (int s : st) if (s == STAND || s == BJ) any = true; if (any) while (value(dealer) < 17) dealer.push_back(draw());
        over = true; int dv = value(dealer); bool dbj = dealer.size() == 2 && dv == 21;
        for (int i = 0; i < N; i++) {
            if (st[i] == BUST) { res[i] = 0; continue; } int v = value(hands[i]); bool bj = st[i] == BJ;
            if (dv > 21 || v > dv) res[i] = bj && !dbj ? 3 : 2; else if (v == dv) res[i] = (bj && !dbj) ? 3 : (dbj && !bj ? 0 : 1); else res[i] = 0;
        }
        app->audio.play("flip");
    }
    void resign(int p) override { (void)p; resigned = p; if (!over) { for (int i = 0; i < N; i++) if (st[i] == PLAYING) st[i] = i == p ? BUST : STAND; dealerPlay(); } }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.2f && !finished) {
            std::vector<PResult> r; std::string sm = "Contra la banca:"; for (int i = 0; i < N; i++) { r.push_back({ value(hands[i]), res[i] == 0 ? 'l' : res[i] == 1 ? 'd' : 'w' }); sm += " " + std::string(res[i] == 0 ? "perdió" : res[i] == 1 ? "empató" : "ganó"); }
            if (resigned >= 0) r[resigned].r = 'x'; finish(r, sm); } return; }
        if (!pl[turn].ai) { Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (p) { if (p->down[B_X]) hit(); else if (p->down[B_Y]) stand(); } return; }
        aiT += dt; if (aiT < aiWait) return; if (decide(hands[turn], &dealer[0], std::max(1, std::min(3, pl[turn].level)))) hit(); else stand();
    }
    void hand(Gfx& g, const std::vector<Card>& h, float cx, float y, float cw, int hideFrom = 99) {
        float ch = cw * 1.4f, step = std::min(cw * 0.72f, (cw * 3.4f - cw) / std::max<size_t>(1, h.size() - 1)); float tw = cw + step * (h.size() - 1), x0 = cx - tw / 2;
        for (size_t k = 0; k < h.size(); k++) cards::draw(g, { x0 + k * step, y, cw, ch }, h[k].r == 0 ? 12 : h[k].r - 1, h[k].s, (int)k < hideFrom);
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai;
        std::string banner = over ? "Ronda terminada" : mine ? pl[turn].name + ": pedí carta o plantate" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 16, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 38, 20, pal::gold, F_BOLD, AL_C, 930);
        g.text("🏦 Banca" + std::string(over ? "  " + std::to_string(value(dealer)) : ""), 765, 84, 18, pal::white, F_BOLD, AL_C);
        hand(g, dealer, 765, 100, 80, over ? 99 : 1);
        float slotW = std::min(230.f, 960.f / N); int rows = N > 4 ? 2 : 1, perRow = (N + rows - 1) / rows; slotW = std::min(230.f, 950.f / perRow);
        for (int i = 0; i < N; i++) {
            int rr = i / perRow, cc = i % perRow; float cx = 765 + (cc - (std::min(perRow, N - rr * perRow) - 1) / 2.f) * slotW, y = 270 + rr * 190; bool on = i == turn && !over;
            Rect pr{ cx - slotW / 2 + 4, y - 4, slotW - 8, 176 }; g.rrect(pr, 12, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(pr.inset(2), 10, rgb(0x2a1e18), rgb(0x120c08));
            hand(g, hands[i], cx, y + 36, rows > 1 ? 52 : 62);
            std::string tag = st[i] == BJ ? " ¡Blackjack!" : st[i] == BUST ? " (se pasó)" : "";
            g.text(pl[i].emoji + " " + pl[i].name + tag, cx, y + 14, 14, on ? pal::gold : pal::white, F_BOLD, AL_C, slotW - 14);
            std::string t = std::to_string(value(hands[i])); if (over) t += res[i] == 0 ? "  ✖ Perdió" : res[i] == 1 ? "  = Empate" : res[i] == 3 ? "  ★ Blackjack" : "  ✔ Ganó";
            g.text(t, cx, y + 150, 14, over ? (res[i] == 0 ? rgb(0xff8a80) : rgb(0xa5d6a7)) : pal::gold, F_BOLD, AL_C, slotW - 14);
        }
        if (mine) {
            float by = 270 + (rows - 1) * 190 + 188; if (by > 640) by = 640;
            if (ui.button("Pedir carta", { 420, by, 300, 52 }, 1, true)) hit();
            if (!over && ui.button("Plantarse", { 810, by, 300, 52 }, 3, true)) stand();
            ui.setFocusIfNone(hid("Pedir carta", (int)(420 * 7 + by)));
        }
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" }, { "X", "Pedir carta" }, { "Y", "Plantarse" } }; }
    std::string help() const override { return "Cada jugador juega contra la banca. Sumá lo más cerca posible de 21 sin pasarte. Las figuras valen 10 y el As 11 o 1. Pedí cartas o plantate. La banca pide hasta llegar a 17. Si superás a la banca (o ella se pasa), ganás. 21 con dos cartas es Blackjack."; }
};
}
Game* makeBlackjack() { return new BlackJack(); }
