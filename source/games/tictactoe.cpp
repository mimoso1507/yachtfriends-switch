// 3 en raya: partida a 1, 3 o 5 rondas. IA minimax con error según nivel (igual que la versión de PC).
#include "../app.h"
#include <algorithm>

namespace {
const int LINES[8][3] = { { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 }, { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 }, { 0, 4, 8 }, { 2, 4, 6 } };
bool win(const int* b, int v) { for (auto& l : LINES) if (b[l[0]] == v && b[l[1]] == v && b[l[2]] == v) return true; return false; }
int score(int* b, int me, int turn) {
    if (win(b, me)) return 1; if (win(b, 3 - me)) return -1;
    bool full = true; for (int i = 0; i < 9; i++) if (!b[i]) full = false; if (full) return 0;
    int best = turn == me ? -2 : 2;
    for (int i = 0; i < 9; i++) if (!b[i]) { b[i] = turn; int s = score(b, me, 3 - turn); b[i] = 0; best = turn == me ? std::max(best, s) : std::min(best, s); }
    return best;
}
int choose(const int* board, int me, int lv) {
    const float ERR[4] = { 0, 0.42f, 0.12f, 0.01f };
    std::vector<std::pair<int, int>> mv; int b[9]; std::copy(board, board + 9, b);
    for (int i = 0; i < 9; i++) if (!b[i]) { b[i] = me; mv.push_back({ i, score(b, me, 3 - me) }); b[i] = 0; }
    if (mv.empty()) return -1;
    if (rnd01() < ERR[lv]) return mv[rndInt((int)mv.size())].first;
    int m = -9; for (auto& x : mv) m = std::max(m, x.second);
    std::vector<int> best; for (auto& x : mv) if (x.second == m) best.push_back(x.first);
    return best[rndInt((int)best.size())];
}
struct TTT : Game {
    int bestOf = 1, board[9] = {}, turn = 0, startIdx = 0, round = 1, scores[2] = {}, draws = 0, winLine = -1;
    bool roundOver = false, matchOver = false; float aiT = 0, nextT = 0, pop[9] = {};
    void start() override { const int B[3] = { 1, 3, 5 }; bestOf = B[std::max(0, std::min(2, opts.get("bestOf")))]; newRound(); }
    void newRound() { std::fill(board, board + 9, 0); turn = startIdx; winLine = -1; roundOver = false; aiT = 0; }
    int turnPlayer() const override { return (roundOver || matchOver) ? -1 : turn; }
    void move(int i) {
        if (matchOver || roundOver || board[i]) return;
        board[i] = turn + 1; pop[i] = 0.25f; app->audio.play("place");
        for (int l = 0; l < 8; l++) if (board[LINES[l][0]] == turn + 1 && board[LINES[l][1]] == turn + 1 && board[LINES[l][2]] == turn + 1) winLine = l;
        bool full = true; for (int k = 0; k < 9; k++) if (!board[k]) full = false;
        if (winLine >= 0) { roundOver = true; scores[turn]++; }
        else if (full) { roundOver = true; draws++; }
        else turn = 1 - turn;
        if (roundOver) { int need = bestOf / 2 + 1; if (scores[0] >= need || scores[1] >= need || round >= bestOf) matchOver = true; nextT = 0; }
    }
    void resign(int p) override { scores[1 - p] = 99; matchOver = true; roundOver = true; nextT = 0; }
    void update(float dt, Input&, UI&) override {
        for (float& p : pop) p = std::max(0.f, p - dt);
        if (roundOver) {
            nextT += dt;
            if (matchOver && nextT > 1.6f && !finished) {
                int w = scores[0] > scores[1] ? 0 : scores[1] > scores[0] ? 1 : -1;
                std::vector<PResult> r = { { std::min(scores[0], 9), w == 0 ? 'w' : w < 0 ? 'd' : 'l' }, { std::min(scores[1], 9), w == 1 ? 'w' : w < 0 ? 'd' : 'l' } };
                finish(r, w < 0 ? "Empate" : "Ganó " + pl[w].name);
            } else if (!matchOver && nextT > 1.8f) { round++; startIdx = 1 - startIdx; newRound(); }
            return;
        }
        if (!pl[turn].ai) return;
        int lv = pl[turn].level; float d = lv == 1 ? 1.3f : lv == 3 ? 0.7f : 0.9f;
        aiT += dt; if (aiT < d) return; aiT = 0;
        int i = choose(board, turn + 1, lv); if (i >= 0) move(i);
    }
    void draw(Gfx& g, UI& ui) override {
        // marcador
        Rect sc{ 420, 18, 720, 46 }; g.rrect(sc, 10, pal::plateEdge); g.rrectGrad(sc.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
        g.text("❌ " + pl[0].name + "  " + std::to_string(std::min(scores[0], 9)), sc.x + 20, sc.cy(), 18, rgb(0xff8a80), F_BOLD, AL_L, 260);
        g.text(std::to_string(std::min(scores[1], 9)) + "  " + pl[1].name + " ⭕", sc.x + sc.w - 20, sc.cy(), 18, rgb(0x82b1ff), F_BOLD, AL_R, 260);
        g.text("Ronda " + std::to_string(round) + " de " + std::to_string(bestOf), sc.cx(), sc.cy(), 15, pal::cream, F_BODY, AL_C);
        // tablero visto desde arriba, lo más grande posible
        float S = 560, x0 = 780 - S / 2, y0 = 84; Rect bd{ x0, y0, S, S };
        g.rrect({ bd.x + 6, bd.y + 10, bd.w, bd.h }, 26, rgb(0, 110)); g.rrectGrad(bd, 26, rgb(0x9a6a3c), rgb(0x6b4224)); g.rrectGrad(bd.inset(14), 18, rgb(0xf3e3bf), rgb(0xe6cf9c));
        float cs = (S - 28) / 3;
        for (int k = 1; k < 3; k++) { g.line(x0 + 14 + k * cs, y0 + 26, x0 + 14 + k * cs, y0 + S - 26, 8, rgb(0x6b4224)); g.line(x0 + 26, y0 + 14 + k * cs, x0 + S - 26, y0 + 14 + k * cs, 8, rgb(0x6b4224)); }
        bool mine = !roundOver && !matchOver && !pl[turn].ai;
        for (int i = 0; i < 9; i++) {
            Rect c{ x0 + 14 + (i % 3) * cs, y0 + 14 + (i / 3) * cs, cs, cs }; uint32_t id = hid("ttt", i);
            bool hit = ui.hit(id, c, mine && !board[i]);
            float s = 1 - pop[i] * 1.6f, r = cs * 0.3f * s;
            bool onLine = winLine >= 0 && (LINES[winLine][0] == i || LINES[winLine][1] == i || LINES[winLine][2] == i);
            if (onLine) g.rrect(c.inset(10), 18, rgb(0xffe27a, 160));
            if (board[i] == 1) { g.line(c.cx() - r, c.cy() - r, c.cx() + r, c.cy() + r, 22 * s, rgb(0xd9483b)); g.line(c.cx() + r, c.cy() - r, c.cx() - r, c.cy() + r, 22 * s, rgb(0xd9483b)); }
            if (board[i] == 2) g.ring(c.cx(), c.cy(), r * 1.1f, 20 * s, rgb(0x1e6fd9));
            if (ui.focused(id)) ui.drawFocus(c.inset(8), 14);
            if (hit) move(i);
            if (i == 4 && mine) ui.setFocusIfNone(id);
        }
        std::string msg = matchOver ? (scores[0] == scores[1] ? "¡Empate!" : "¡Ganó " + pl[scores[0] > scores[1] ? 0 : 1].name + "!")
                        : roundOver ? (winLine >= 0 ? "Ronda para " + pl[board[LINES[winLine][0]] - 1].name : "Ronda empatada")
                        : (pl[turn].ai ? "Piensa " + pl[turn].name + "…" : "Turno de " + pl[turn].name + (turn == 0 ? " (❌)" : " (⭕)"));
        g.textShadow(msg, 780, 680, 22, pal::gold, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir casilla" }, { "A", "Poner ficha" } }; }
    std::string help() const override { return "Pongan por turnos su ficha (❌ o ⭕) en una casilla libre. Gana la ronda quien hace 3 en línea: fila, columna o diagonal. En partidas de 3 o 5 rondas, empieza cada ronda un jugador distinto."; }
};
}
Game* makeTicTacToe() { return new TTT(); }
