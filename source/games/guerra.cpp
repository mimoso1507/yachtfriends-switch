// Guerra de cartas: un mazo repartido entre 2-4 jugadores; cada uno tira cuando quiere; la carta más alta gana la ronda.
#include "cards.h"
#include <algorithm>

#ifndef DBG_SHOW
#define DBG_SHOW 1.6f
#endif
namespace {
struct Card { int s, v; };
struct War : Game {
    std::vector<std::vector<Card>> decks; std::vector<int> wins; std::vector<Card> table; std::vector<bool> played; std::vector<int> roundPlayers;
    std::vector<Card> lastCards; std::vector<int> lastWinners; std::vector<int> lastPlayers; bool tie = false, over = false; float showT = 0, endT = 0, aiT = 0, aiWait = 1; int resigned = -1;
    void start() override {
        int n = (int)pl.size(); decks.assign(n, {}); wins.assign(n, 0); table.assign(n, { 0, 0 }); played.assign(n, false);
        std::vector<Card> d; for (int s = 0; s < 4; s++) for (int v = 0; v < 13; v++) d.push_back({ s, v });
        for (int i = (int)d.size() - 1; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]);
        for (size_t i = 0; i < d.size(); i++) decks[i % n].push_back(d[i]);
        newRound(); resetAi();
    }
    void resetAi() { aiT = 0; int lv = 2; aiWait = (lv == 1 ? 0.9f : 0.7f) + rnd01() * 1.0f; }
    void newRound() { roundPlayers.clear(); for (size_t i = 0; i < decks.size(); i++) if (!decks[i].empty()) roundPlayers.push_back((int)i); std::fill(played.begin(), played.end(), false); }
    int turnPlayer() const override { return -1; }
    bool canPlay(int p) const { return !over && showT <= 0 && !played[p] && std::find(roundPlayers.begin(), roundPlayers.end(), p) != roundPlayers.end(); }
    void play(int p) {
        if (!canPlay(p)) return; table[p] = decks[p].front(); decks[p].erase(decks[p].begin()); played[p] = true; app->audio.play("click");
        bool all = true; for (int q : roundPlayers) if (!played[q]) all = false;
        if (all) resolve();
    }
    void resolve() {
        int mx = -1; for (int q : roundPlayers) mx = std::max(mx, table[q].v);
        lastWinners.clear(); for (int q : roundPlayers) if (table[q].v == mx) lastWinners.push_back(q);
        tie = lastWinners.size() > 1; if (!tie) wins[lastWinners[0]]++;
        lastCards = table; lastPlayers = roundPlayers; showT = DBG_SHOW; app->audio.play(tie ? "place" : "win");
        roundPlayers.clear(); for (size_t i = 0; i < decks.size(); i++) if (!decks[i].empty()) roundPlayers.push_back((int)i);
        if (roundPlayers.empty()) over = true;
        std::fill(played.begin(), played.end(), false);
    }
    void resign(int p) override { decks[p].clear(); roundPlayers.erase(std::remove(roundPlayers.begin(), roundPlayers.end(), p), roundPlayers.end()); resigned = p;
        bool all = !roundPlayers.empty(); for (int q : roundPlayers) if (!played[q]) all = false; if (all) resolve();
        bool empty = true; for (auto& d : decks) if (!d.empty()) empty = false; if (empty) over = true; }
    void update(float dt, Input& in, UI&) override {
        showT = std::max(0.f, showT - dt);
        if (over) {
            if (showT <= 0) { endT += dt; if (endT > 1.0f && !finished) {
                int best = *std::max_element(wins.begin(), wins.end()); int cnt = (int)std::count(wins.begin(), wins.end(), best);
                std::vector<PResult> r; for (size_t i = 0; i < pl.size(); i++) r.push_back({ wins[i], wins[i] == best ? (cnt > 1 ? 'd' : 'w') : 'l' });
                if (resigned >= 0) { r[resigned].r = 'x'; }
                std::string sm = "Rondas ganadas:"; for (int w : wins) sm += " " + std::to_string(w);
                finish(r, sm); } }
            return;
        }
        for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && in.pads[pl[i].pad].connected) play((int)i);
        aiT += dt; if (aiT >= aiWait) { aiT = 0; aiWait = 0.7f + rnd01() * 1.0f; for (size_t i = 0; i < pl.size(); i++) if (pl[i].ai && canPlay((int)i)) { play((int)i); break; } }
    }
    void draw(Gfx& g, UI& ui) override {
        int n = (int)pl.size(); float cw = 150, ch = 210, gap = 30, tw = n * cw + (n - 1) * gap, x0 = 760 - tw / 2;
        std::string msg = over ? (showT > 0 ? "Última ronda…" : "Fin del mazo") : showT > 0 ? (tie ? "¡Empate! Nadie gana esta ronda" : pl[lastWinners[0]].name + " se lleva la ronda") : "Cada jugador tira su carta cuando quiera";
        g.rrect({ 360, 56, 800, 48 }, 24, rgb(0x120c08, 200)); g.text(msg, 760, 80, 24, pal::gold, F_BOLD, AL_C, 780);
        for (int i = 0; i < n; i++) {
            Rect r{ x0 + i * (cw + gap), 190, cw, ch }; bool show = showT > 0 && std::find(lastPlayers.begin(), lastPlayers.end(), i) != lastPlayers.end();
            bool won = show && std::find(lastWinners.begin(), lastWinners.end(), i) != lastWinners.end() && !tie;
            if (decks[i].empty() && !show && !played[i]) { g.rrect(r, 10, rgb(0, 60)); }
            else cards::draw(g, r, show ? lastCards[i].v : 0, show ? lastCards[i].s : 0, show, won);
            if (played[i] && !show) g.rrect({ r.x + 30, r.y + 20, r.w - 60, r.h - 40 }, 10, rgb(0xffffff, 40));
            g.text(pl[i].emoji + " " + pl[i].name, r.cx(), r.y - 22, 18, pal::white, F_BOLD, AL_C, cw + gap);
            g.rrect({ r.cx() - 76, r.y + ch + 14, 152, 28 }, 14, rgb(0x120c08, 200));
            g.text("Cartas: " + std::to_string(decks[i].size()) + " · Rondas: " + std::to_string(wins[i]), r.cx(), r.y + ch + 28, 14, pal::cream, F_BODY, AL_C, 146);
            bool me = !pl[i].ai && canPlay(i);
            uint32_t id = hid("wr", i); Rect br{ r.x, r.y + ch + 52, cw, 50 };
            bool hit = ui.hit(id, br, me); g.rrectGrad(br, 12, me ? rgb(0xf26a55) : rgb(0x555555), me ? rgb(0xc8331f) : rgb(0x3a3a3a));
            g.text(played[i] ? "Listo" : "Tirar", br.cx(), br.cy(), 20, pal::white, F_TITLE, AL_C);
            if (ui.focused(id)) ui.drawFocus(br, 12); if (hit) play(i);
            if (!pl[i].ai && pl[i].pad >= 0 && !solo()) g.text("🎮" + std::to_string(pl[i].pad + 1), r.cx(), r.y + ch + 118, 14, rgb(0xd8b46a), F_BODY, AL_C);
        }
        // un solo humano: que el foco empiece en su botón
        for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad == 0) ui.setFocusIfNone(hid("wr", (int)i));
    }
    bool solo() const { int h = 0; for (auto& p : pl) if (!p.ai) h++; return h <= 1; }
    std::vector<Hint> hints() const override { return { { "A", "Tirar tu carta (cada uno con su mando)" } }; }
    std::string help() const override { return "Se reparte un mazo entre todos. Cada jugador da vuelta su carta de arriba cuando quiere (A en su mando). Cuando todos tiraron, la carta más alta se lleva la ronda; si hay empate, nadie. Gana quien más rondas ganó cuando se acaba el mazo."; }
};
}
Game* makeGuerra() { return new War(); }
