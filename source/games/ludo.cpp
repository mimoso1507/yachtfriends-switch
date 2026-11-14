// Ludo 2-4 jugadores, 4 fichas cada uno. Con un 5 se sale de la base; un 6 da otro tiro (tres 6 seguidos pierden el turno).
// Comer (caer en ficha rival fuera de casillas seguras) da 8 movidas gratis; llegar a la meta da 6. Dos fichas propias juntas forman una barricada.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const int TRACK[52][2] = { {1,6},{2,6},{3,6},{4,6},{5,6},{6,5},{6,4},{6,3},{6,2},{6,1},{6,0},{7,0},{8,0},{8,1},{8,2},{8,3},{8,4},{8,5},
    {9,6},{10,6},{11,6},{12,6},{13,6},{14,6},{14,7},{14,8},{13,8},{12,8},{11,8},{10,8},{9,8},{8,9},{8,10},{8,11},{8,12},{8,13},{8,14},
    {7,14},{6,14},{6,13},{6,12},{6,11},{6,10},{6,9},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8},{0,7},{0,6} };
const int HOME[4][6][2] = { { {1,7},{2,7},{3,7},{4,7},{5,7},{6,7} }, { {7,1},{7,2},{7,3},{7,4},{7,5},{7,6} }, { {13,7},{12,7},{11,7},{10,7},{9,7},{8,7} }, { {7,13},{7,12},{7,11},{7,10},{7,9},{7,8} } };
const int BASE_O[4][2] = { {0,0},{9,0},{9,9},{0,9} }, BASE_S[4][2] = { {2,2},{4,2},{2,4},{4,4} };
const uint32_t COL[4] = { 0xe8524a, 0x3ddc97, 0xffd76b, 0x5aa9ff };
const char* CNAME[4] = { "rojo", "verde", "amarillo", "azul" };
const int FIN = 56, EXIT = 5;
bool isSafe(int a) { return a % 13 == 0 || a % 13 == 8; }

struct Ludo : Game {
    int N = 0, turn = 0, roll = 0, bonus = 0, sixes = 0, winner = -1, resigned = -1; bool over = false, extra = false, danger = true, rolled = false;
    int seats[4] = {}; int pos[4][4]; bool active[4]; std::vector<std::pair<int,int>> bq; std::string msg; float aiT = 0, aiWait = 0.8f, endT = 0, dieAnim = 0; int dieShow = 1;
    void start() override {
        N = (int)pl.size(); danger = opts.get("danger", 1) != 0; static const int S2[2] = { 0, 2 }, S3[3] = { 0, 3, 2 }, S4[4] = { 0, 3, 2, 1 };
        for (int p = 0; p < N; p++) { seats[p] = N == 2 ? S2[p] : N == 3 ? S3[p] : S4[p]; active[p] = true; for (int t = 0; t < 4; t++) pos[p][t] = -1; }
        turn = rndInt(N); msg = pl[turn].name + " empieza"; resetAi();
    }
    void resetAi() { aiT = 0; aiWait = 0.7f + rnd01() * 0.7f; }
    int turnPlayer() const override { return over ? -1 : turn; }
    int absP(int p, int ps) const { return (13 * seats[p] + ps) % 52; }
    int cur() const { return roll ? roll : bonus; }
    std::map<int,int> cellTokens(int a, int ep, int et) const { std::map<int,int> c; for (int q = 0; q < N; q++) if (active[q]) for (int t = 0; t < 4; t++) { int y = pos[q][t]; if (y >= 0 && y <= 50 && absP(q, y) == a && !(q == ep && t == et)) c[q]++; } return c; }
    bool barrierAt(int a, int ep, int et) const { for (auto& kv : cellTokens(a, ep, et)) if (kv.second >= 2) return true; return false; }
    bool canMove(int p, int t, int amount, bool isRoll) const {
        int x = pos[p][t]; if (x == FIN) return false;
        if (x < 0) return isRoll && amount == EXIT && !barrierAt(absP(p, 0), p, t);
        int y = x + amount; if (y > FIN) return false;
        for (int s = x + 1; s <= std::min(y, 50); s++) if (barrierAt(absP(p, s), p, t)) return false;
        return true;
    }
    std::vector<int> legal(int p) const { std::vector<int> o; int a = cur(); if (!a) return o; for (int t = 0; t < 4; t++) if (canMove(p, t, a, roll != 0)) o.push_back(t); return o; }
    std::vector<int> legalFor(int p, int a, bool isRoll) const { std::vector<int> o; for (int t = 0; t < 4; t++) if (canMove(p, t, a, isRoll)) o.push_back(t); return o; }
    int nextActive(int from) const { for (int k = 1; k <= 3; k++) { int s = (seats[from] - k + 8) % 4; for (int q = 0; q < N; q++) if (active[q] && seats[q] == s) return q; } return from; }
    void passTurn() { turn = nextActive(turn); roll = 0; bonus = 0; bq.clear(); extra = false; sixes = 0; rolled = false; resetAi(); }
    void doRoll() {
        if (over || cur()) return; int r = 1 + rndInt(6); dieShow = r; dieAnim = 0.5f; app->audio.play("roll"); sixes = r == 6 ? sixes + 1 : 0;
        if (sixes >= 3) { msg = pl[turn].name + " sacó tres 6: pierde el turno"; passTurn(); return; }
        if (legalFor(turn, r, true).empty()) { msg = pl[turn].name + " sacó " + std::to_string(r) + ": sin jugada"; passTurn(); return; }
        roll = r; msg = pl[turn].name + " sacó " + std::to_string(r); resetAi();
    }
    void move(int t) {
        int p = turn, amount = cur(); bool isRoll = roll != 0; if (!amount || !canMove(p, t, amount, isRoll)) return;
        int x = pos[p][t], nx = x < 0 ? 0 : x + amount; pos[p][t] = nx; int caught = 0;
        if (nx <= 50) {
            int a = absP(p, nx); bool startEat = danger && a % 13 == 0 && seats[p] == a / 13;
            if (!isSafe(a) || startEat) for (int q = 0; q < N; q++) if (q != p && active[q]) for (int tt = 0; tt < 4; tt++) { int y = pos[q][tt]; if (y >= 0 && y <= 50 && absP(q, y) == a) { pos[q][tt] = -1; caught++; } }
        }
        for (int i = 0; i < caught; i++) bq.push_back({ 8, 0 }); if (nx == FIN) bq.push_back({ 6, 1 });
        msg = pl[p].name + (x < 0 ? " sacó una ficha" : " movió " + std::to_string(amount)) + (caught ? " · ¡comió! +8" : "") + (nx == FIN ? " · ¡meta! +6" : ""); app->audio.play(caught || nx == FIN ? "win" : "place");
        if (isRoll) extra = amount == 6; roll = 0; bonus = 0;
        bool all = true; for (int k = 0; k < 4; k++) if (pos[p][k] != FIN) all = false; if (all) { over = true; winner = p; bq.clear(); return; }
        while (!bq.empty()) { auto b = bq.front(); bq.erase(bq.begin()); if (!legalFor(turn, b.first, false).empty()) { bonus = b.first; resetAi(); return; } }
        if (extra) { extra = false; roll = 0; resetAi(); } else passTurn();
    }
    void resign(int p) override {
        active[p] = false; for (int t = 0; t < 4; t++) pos[p][t] = -1; resigned = p; int left = 0, w = -1; for (int q = 0; q < N; q++) if (active[q]) { left++; w = q; }
        if (left <= 1) { over = true; winner = w; return; } if (turn == p) passTurn();
    }
    int danger_(int idx, int cell) const {
        int ow = -1; if (cell % 13 == 0) for (int q = 0; q < N; q++) if (seats[q] == cell / 13) ow = q;
        bool foreign = danger && ow >= 0 && ow != idx && active[ow]; if (isSafe(cell) && !foreign) return 0; int d = 0;
        if (isSafe(cell)) { for (int y : pos[ow]) { if (y < 0) d++; else if (y <= 50) { int k = (cell - absP(ow, y) + 52) % 52; if (k >= 1 && k <= 6) d++; } } return d; }
        for (int q = 0; q < N; q++) if (q != idx && active[q]) for (int y : pos[q]) if (y >= 0 && y <= 50) { int k = (cell - absP(q, y) + 52) % 52; if (k >= 1 && k <= 6) d++; }
        return d;
    }
    float scoreMove(int idx, int t, int amount, float dg) const {
        int x = pos[idx][t], nx = x < 0 ? 0 : x + amount; float sc = 0; if (nx == FIN) sc += 70; if (x < 0) sc += 45; if (nx > 50 && nx < FIN) sc += x <= 50 ? 18 : 3;
        auto eatScore = [&](int a) { float s = 0; for (int q = 0; q < N; q++) if (q != idx && active[q]) for (int y : pos[q]) if (y >= 0 && y <= 50 && absP(q, y) == a) s += 100 + y * 1.2f; return s; };
        if (nx <= 50) {
            int a = absP(idx, nx), ow = -1; if (a % 13 == 0) for (int q = 0; q < N; q++) if (seats[q] == a / 13) ow = q; bool foreign = danger && ow >= 0 && ow != idx && active[ow];
            if (danger && ow == idx) sc += eatScore(a);
            if (isSafe(a) && !foreign) sc += 14; else if (foreign) sc += 14 - 22 * dg * danger_(idx, a) - 6 * (dg ? 1 : 0); else { sc += eatScore(a); sc -= 22 * dg * danger_(idx, a); }
            for (int k = 0; k < 4; k++) if (k != t && pos[idx][k] >= 0 && pos[idx][k] <= 50 && absP(idx, pos[idx][k]) == a) sc += 18;
        }
        if (x >= 0 && x <= 50) {
            int a = absP(idx, x); int ow = -1; if (a % 13 == 0) for (int q = 0; q < N; q++) if (seats[q] == a / 13) ow = q;
            if (dg > 0 && danger_(idx, a) > 0) sc += 16 + (danger && a % 13 == 0 && ow != idx ? 14 : 0);
            for (int k = 0; k < 4; k++) if (k != t && pos[idx][k] >= 0 && pos[idx][k] <= 50 && absP(idx, pos[idx][k]) == a) sc -= 10;
        }
        return sc + nx * 0.25f;
    }
    int aiPick() {
        auto lg = legal(turn); if (lg.empty()) return -1; if (lg.size() == 1) return lg[0]; int lv = std::max(1, std::min(3, pl[turn].level));
        float err = lv == 1 ? 0.4f : lv == 2 ? 0.12f : 0.01f, noise = lv == 1 ? 30.f : lv == 2 ? 8.f : 1.f, dg = lv == 1 ? 0.f : lv == 2 ? 1.f : 1.3f;
        if (rnd01() < err) return lg[rndInt((int)lg.size())]; int best = lg[0]; float bs = -1e9f; for (int t : lg) { float v = scoreMove(turn, t, cur(), dg) + rnd01() * noise; if (v > bs) { bs = v; best = t; } } return best;
    }
    void update(float dt, Input& in, UI&) override {
        dieAnim = std::max(0.f, dieAnim - dt);
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) { int s = 0; for (int t = 0; t < 4; t++) if (pos[p][t] == FIN) s++; r.push_back({ s, p == winner ? 'w' : 'l' }); } if (resigned >= 0) r[resigned].r = 'x'; finish(r, winner >= 0 ? "Ganó " + pl[winner].name : "Fin"); } return; }
        if (pl[turn].ai) { aiT += dt; if (aiT < aiWait) return; if (!cur()) doRoll(); else { int t = aiPick(); if (t >= 0) move(t); else passTurn(); } return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
        if (pd && !cur() && pd->rollDown()) doRoll();
    }
    void xy(int p, int t, float& x, float& y) const {
        int ps = pos[p][t], s = seats[p];
        if (ps < 0) { x = BASE_O[s][0] + BASE_S[t][0]; y = BASE_O[s][1] + BASE_S[t][1]; }
        else if (ps <= 50) { int a = (13 * s + ps) % 52; x = TRACK[a][0] + 0.5f; y = TRACK[a][1] + 0.5f; }
        else { x = HOME[s][ps - 51][0] + 0.5f; y = HOME[s][ps - 51][1] + 0.5f; }
    }
    void draw(Gfx& g, UI& ui) override {
        const float U = 38.f, bx = 300, by = 80; auto P = [&](float cx, float cy, float& sx, float& sy) { sx = bx + cx * U; sy = by + cy * U; };
        bool mine = !over && !pl[turn].ai; float sx, sy;
        g.rrect({ bx - 10, by - 10, U * 15 + 20, U * 15 + 20 }, 14, rgb(0x6b4224)); g.rect({ bx, by, U * 15, U * 15 }, rgb(0xf4ecd8));
        for (int s = 0; s < 4; s++) { bool used = false; for (int q = 0; q < N; q++) if (seats[q] == s) used = true; P((float)BASE_O[s][0], (float)BASE_O[s][1], sx, sy); g.rect({ sx, sy, 6 * U, 6 * U }, rgb(COL[s])); g.rect({ sx + U, sy + U, 4 * U, 4 * U }, used ? rgb(0xf4ecd8) : rgb(0xd9d2bf));
            for (int k = 0; k < 4; k++) g.circle(sx + BASE_S[k][0] * U, sy + BASE_S[k][1] * U, U * 0.62f, rgb(COL[s], 90)); }
        for (int i = 0; i < 52; i++) { P((float)TRACK[i][0], (float)TRACK[i][1], sx, sy); g.rect({ sx, sy, U, U }, i % 13 == 0 ? rgb(COL[i / 13]) : rgb(0xffffff)); g.rrectLine({ sx, sy, U, U }, 0, 1, rgb(0x8a7a58)); if (i % 13 == 8) g.text("★", sx + U / 2, sy + U / 2, U * 0.7f, rgb(0xb8a36a), F_BOLD, AL_C); }
        for (int s = 0; s < 4; s++) for (int i = 0; i < 6; i++) { P((float)HOME[s][i][0], (float)HOME[s][i][1], sx, sy); g.rect({ sx, sy, U, U }, rgb(COL[s], i == 5 ? 255 : 200)); g.rrectLine({ sx, sy, U, U }, 0, 1, rgb(0x8a7a58)); }
        float c0x, c0y; P(7.5f, 7.5f, c0x, c0y); float ax[4][2] = { {6,6}, {9,6}, {9,9}, {6,9} }; for (int s = 0; s < 4; s++) { float a1x, a1y, a2x, a2y; P(ax[s][0], ax[s][1], a1x, a1y); P(ax[(s + 1) % 4][0], ax[(s + 1) % 4][1], a2x, a2y); g.tri(a1x, a1y, a2x, a2y, c0x, c0y, rgb(COL[(s + 0) % 4])); }
        auto lg = mine ? legal(turn) : std::vector<int>();
        // fichas
        std::map<int, std::vector<std::pair<int,int>>> cells; for (int p = 0; p < N; p++) if (active[p]) for (int t = 0; t < 4; t++) { float x, y; xy(p, t, x, y); cells[(int)(x * 4) * 1000 + (int)(y * 4)].push_back({ p, t }); }
        int firstId = -1;
        for (auto& kv : cells) { auto& v = kv.second; bool barrier = v.size() >= 2 && pos[v[0].first][v[0].second] >= 0 && pos[v[0].first][v[0].second] <= 50; for (auto& e : v) if (e.first != v[0].first) barrier = false;
            for (size_t j = 0; j < v.size(); j++) { int p = v[j].first, t = v[j].second; float x, y; xy(p, t, x, y); P(x, y, sx, sy); bool multi = v.size() > 1 && pos[p][t] >= 0; if (multi) { sx += (j % 2 ? 1 : -1) * U * 0.16f; sy += (j > 1 ? 1 : -1) * U * 0.16f; }
                float r = U * (multi ? 0.3f : 0.4f); bool mv = mine && p == turn && std::find(lg.begin(), lg.end(), t) != lg.end(); uint32_t id = hid("lt", p * 4 + t);
                Rect hr{ sx - U * 0.5f, sy - U * 0.5f, U, U }; bool hit = ui.hit(id, hr, mv); if (mv && firstId < 0) firstId = p * 4 + t;
                g.circle(sx, sy + r * 0.5f, r * 0.9f, rgb(0, 70)); g.circle(sx, sy, r, rgb(0x3a2f1a)); g.circle(sx, sy, r - 2, rgb(COL[seats[p]])); g.circle(sx - r * 0.25f, sy - r * 0.3f, r * 0.28f, rgb(0xffffff, 140));
                if (mv) g.ring(sx, sy, r + 3, 3, rgb(0xffffff)); if (barrier) g.ring(sx, sy, r + 2, 2, rgb(0x111111)); if (ui.focused(id)) ui.drawFocus(hr, r); if (hit) { move(t); return; } } }
        if (firstId >= 0) ui.setFocusIfNone(hid("lt", firstId));
        // panel derecho
        std::string banner = over ? "¡Ganó " + (winner >= 0 ? pl[winner].name : std::string("nadie")) + "!" : mine ? pl[turn].name + ": " + (cur() ? "mueve una ficha" : "tira el dado") : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        for (int p = 0; p < N; p++) { Rect r{ 930, 90.f + p * 62, 300, 54 }; bool on = p == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.circle(r.x + 22, r.cy(), 12, rgb(COL[seats[p]]));
            int home = 0; for (int t = 0; t < 4; t++) if (pos[p][t] == FIN) home++; g.text(pl[p].emoji + " " + pl[p].name, r.x + 42, r.y + 18, 15, on ? pal::gold : pal::white, F_BOLD, AL_L, 250); g.text(std::string(CNAME[seats[p]]) + " · en meta " + std::to_string(home) + "/4", r.x + 42, r.y + 38, 13, rgb(0xd8b46a), F_BODY, AL_L, 250); }
        // dado
        float dx = 1080, dy = 400; g.rrect({ dx - 40, dy - 40, 80, 80 }, 14, rgb(0xfaf3dc)); g.rrectLine({ dx - 40, dy - 40, 80, 80 }, 14, 3, rgb(0x6a4a2a)); int face = dieAnim > 0 ? 1 + rndInt(6) : (cur() ? cur() > 6 ? 0 : (roll ? roll : dieShow) : dieShow);
        if (bonus && !roll) g.text("+" + std::to_string(bonus), dx, dy, 34, pal::gold, F_TITLE, AL_C); else { static const int L[7][9] = { {0}, {4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8} }; static const int CN[7] = { 0, 1, 2, 3, 4, 5, 6 }; for (int k = 0; k < CN[face]; k++) { int ix = L[face][k]; g.circle(dx + ((ix % 3) - 1) * 20.f, dy + ((ix / 3) - 1) * 20.f, 6, rgb(0x1a1a1a)); } }
        if (mine && !cur()) { if (ui.button("Tirar dado", { 930, 470, 300, 50 }, 3)) { doRoll(); return; } ui.setFocusIfNone(hid("Tirar dado", 930 * 7 + 470)); }
        g.text(msg, 1080, 560, 15, pal::cream, F_BODY, AL_C, 300);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Tirar el dado" }, { "DPAD/STICK", "Elegir ficha" }, { "A", "Mover" } }; }
    std::string help() const override { return "Cada jugador tiene 4 fichas. Con un 5 sale una ficha de la base. Un 6 da otro tiro (tres seguidos pierden el turno). Caer en una ficha rival la manda a su base (salvo en salidas y estrellas ★) y da 8 movidas gratis; llegar a la meta da 6. Dos fichas propias juntas forman una barricada que nadie puede pasar. Para entrar a la meta hace falta el número exacto."; }
};
}
Game* makeLudo() { return new Ludo(); }
