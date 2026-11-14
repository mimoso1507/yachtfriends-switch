// Bingo: 2 a 8 jugadores. Cada uno tiene su cartón de 5×5 (B 1-15, I 16-30, N 31-45, G 46-60, O 61-75, centro libre).
// Se sacan bolas solas; los números que salen se marcan solos. Gana el primero en cantar ¡BINGO! con la figura elegida.
#include "../app.h"
#include <algorithm>

namespace {
const char* const LET = "BINGO";
struct Bingo : Game {
    int N = 0, mode = 0, winner = -1, resigned = -1; float ballT = 0, ballEvery = 2.5f, endT = 0, aiT = 0; bool over = false;
    std::vector<std::array<std::array<int, 5>, 5>> cards; std::vector<int> called, bag; std::vector<bool> active; std::vector<float> aiDelay; std::string msg; std::vector<int> wrongTry;
    void start() override {
        N = (int)pl.size(); mode = opts.get("mode", 0); ballEvery = opts.get("speed", 1) == 0 ? 3.5f : opts.get("speed", 1) == 2 ? 1.5f : 2.5f; active.assign(N, true); wrongTry.assign(N, 0);
        for (int i = 0; i < N; i++) { std::array<std::array<int, 5>, 5> c{}; for (int col = 0; col < 5; col++) { std::vector<int> v; for (int k = 1; k <= 15; k++) v.push_back(col * 15 + k); for (int a = 14; a > 0; a--) std::swap(v[a], v[rndInt(a + 1)]); for (int r = 0; r < 5; r++) c[r][col] = v[r]; } c[2][2] = 0; cards.push_back(c); aiDelay.push_back(0.5f + rnd01() * 2.0f); }
        for (int k = 1; k <= 75; k++) bag.push_back(k); for (int a = 74; a > 0; a--) std::swap(bag[a], bag[rndInt(a + 1)]); ballT = 1.2f;
    }
    int turnPlayer() const override { return -1; }
    bool isCalled(int v) const { return v == 0 || std::find(called.begin(), called.end(), v) != called.end(); }
    bool complete(int p) const {
        auto& c = cards[p]; auto ok = [&](int r, int k) { return isCalled(c[r][k]); };
        auto all = [&](auto pred) { for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) if (pred(r, k) && !ok(r, k)) return false; return true; };
        switch (mode) {
        case 1: return all([](int, int) { return true; });
        case 2: return all([](int r, int k) { return r == 2 || k == 2; });
        case 3: return all([](int r, int k) { return r == k || r + k == 4; });
        case 4: return all([](int r, int k) { return r == 0 || r == 4 || k == 0 || k == 4; });
        case 5: return all([](int r, int k) { return (r == 0 || r == 4) && (k == 0 || k == 4); });
        default: for (int i = 0; i < 5; i++) { bool row = true, col = true; for (int j = 0; j < 5; j++) { row = row && ok(i, j); col = col && ok(j, i); } if (row || col) return true; }
            { bool d1 = true, d2 = true; for (int i = 0; i < 5; i++) { d1 = d1 && ok(i, i); d2 = d2 && ok(i, 4 - i); } return d1 || d2; }
        }
    }
    void claim(int p) {
        if (over || !active[p]) return;
        if (complete(p)) { over = true; winner = p; msg = "¡BINGO de " + pl[p].name + "!"; app->audio.play("win"); }
        else { wrongTry[p]++; msg = pl[p].name + " cantó y todavía no tiene la figura"; app->audio.play("lose"); }
    }
    void resign(int p) override { active[p] = false; resigned = p; int a = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { a++; w = i; } if (a <= 1) { over = true; winner = w; } }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 1.5f && !finished) { std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ i == winner ? 100 : 0, i == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, winner >= 0 ? "Ganó " + pl[winner].name : "Fin"); } return; }
        for (int i = 0; i < N; i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].connected && (in.pads[pl[i].pad].down[B_A] || in.pads[pl[i].pad].down[B_X])) claim(i);
        for (int i = 0; i < N && !over; i++) if (pl[i].ai && active[i] && complete(i)) { aiDelay[i] -= dt; if (aiDelay[i] <= 0) claim(i); }
        ballT -= dt; if (ballT <= 0 && !over) { if (bag.empty()) { over = true; msg = "Se acabaron las bolas"; return; } called.push_back(bag.back()); bag.pop_back(); ballT = ballEvery; app->audio.play("click"); for (int i = 0; i < N; i++) if (complete(i)) aiDelay[i] = 0.4f + rnd01() * 1.6f * (pl[i].level == 1 ? 2.f : 1.f); }
    }
    void card(Gfx& g, int p, Rect r) {
        bool win = over && p == winner; g.rrect({ r.x + 2, r.y + 4, r.w, r.h }, 10, rgb(0, 100)); g.rrect(r, 10, win ? pal::gold : rgb(0x5a3a1c)); g.rrect(r.inset(3), 8, rgb(0xfff4d6));
        float cw = (r.w - 12) / 5, hh = r.h * 0.17f; Rect hd{ r.x + 6, r.y + 6, r.w - 12, hh }; g.rrect(hd, 6, rgb(0x1e6fd9));
        for (int c = 0; c < 5; c++) g.text(std::string(1, LET[c]), hd.x + c * cw + cw / 2, hd.cy(), hh * 0.7f, pal::white, F_TITLE, AL_C);
        float ch = (r.h - hh - 18) / 5;
        for (int rr = 0; rr < 5; rr++) for (int c = 0; c < 5; c++) { Rect q{ r.x + 6 + c * cw, hd.y + hh + 3 + rr * ch, cw, ch }; int v = cards[p][rr][c]; bool m = isCalled(v); if (m) g.circle(q.cx(), q.cy(), std::min(cw, ch) * 0.42f, v == 0 ? rgb(0xf2c14e) : rgb(0xe53935));
            g.text(v == 0 ? "★" : std::to_string(v), q.cx(), q.cy(), std::min(cw, ch) * (v == 0 ? 0.45f : 0.5f), m ? pal::white : rgb(0x2b1a0c), F_BOLD, AL_C); }
        g.text(pl[p].emoji + " " + pl[p].name, r.cx(), r.y - 10, 14, pal::white, F_BOLD, AL_C, r.w);
    }
    void draw(Gfx& g, UI& ui) override {
        (void)ui; static const char* PN[] = { "cualquier línea", "cartón lleno", "cruz", "X", "borde", "4 esquinas" };
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(over ? msg : std::string("Figura: ") + PN[mode] + " · Pulsa A para cantar ¡BINGO!", 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        // bola actual
        int last = called.empty() ? 0 : called.back();
        if (last) { int c = (last - 1) / 15; g.circle(380, 190, 78, rgb(0, 80)); g.circle(380, 184, 78, rgb(0xffffff)); g.circle(380, 184, 64, c == 0 ? rgb(0x1e6fd9) : c == 1 ? rgb(0xe53935) : c == 2 ? rgb(0xeeeeee) : c == 3 ? rgb(0x2e9e4a) : rgb(0xf9a825));
            g.text(std::string(1, LET[c]), 380, 150, 26, c == 2 ? rgb(0x222222) : pal::white, F_TITLE, AL_C); g.text(std::to_string(last), 380, 198, 54, c == 2 ? rgb(0x222222) : pal::white, F_TITLE, AL_C); }
        else g.text("Sale la primera bola…", 380, 190, 20, pal::cream, F_BOLD, AL_C);
        g.text(std::to_string(called.size()) + " de 75 bolas", 380, 292, 16, pal::cream, F_BOLD, AL_C);
        for (int i = 0; i < 12 && i + 1 < (int)called.size(); i++) { int v = called[called.size() - 2 - i]; float x = 300 + (i % 6) * 36, y = 330 + (i / 6) * 36; g.circle(x, y, 15, rgb(0xfff4d6)); g.text(std::to_string(v), x, y, 14, rgb(0x2b1a0c), F_BOLD, AL_C); }
        // cartones
        int cols = N <= 2 ? N : N <= 4 ? 2 : 4, rows = (N + cols - 1) / cols; float aw = 640, ah = 590, cw = std::min(300.f, (aw - (cols - 1) * 16) / cols), chh = std::min(cw * 1.06f, (ah - (rows - 1) * 40) / rows);
        float x0 = 1240 - (cols * cw + (cols - 1) * 16) - 10, y0 = 90;
        for (int i = 0; i < N; i++) { int rr = i / cols, cc = i % cols; card(g, i, { x0 + cc * (cw + 16), y0 + 18 + rr * (chh + 38), cw, chh }); }
    }
    std::vector<Hint> hints() const override { return { { "A", "¡Cantar BINGO!" } }; }
    std::string help() const override { return "Cada jugador tiene un cartón de 5×5 con números del 1 al 75 (el centro es libre). Salen bolas solas y los números se marcan solos. Cuando completes la figura elegida, pulsa A en tu mando para cantar ¡BINGO!. Gana el primero que canta con la figura completa."; }
};
}
Game* makeBingo() { return new Bingo(); }
