// Culebras y escaleras: 2 a 8 jugadores. Primero todos tiran 2 dados para el orden; luego, en cada turno se tira 1 dado y se mueve la ficha.
// Tableros: tres clásicos fijos (como en la versión de PC; la variante aleatoria llega después).
#include "dice.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace {
struct BoardDef { const char* label; std::map<int, int> snakes, ladders; };
const BoardDef BOARDS[3] = {
    { "Equilibrado", { { 16, 6 }, { 47, 26 }, { 49, 11 }, { 56, 53 }, { 62, 19 }, { 64, 60 }, { 87, 24 }, { 93, 73 }, { 95, 75 }, { 98, 78 } }, { { 1, 38 }, { 4, 14 }, { 9, 31 }, { 21, 42 }, { 28, 84 }, { 36, 44 }, { 51, 67 }, { 71, 91 }, { 80, 100 } } },
    { "Muchas serpientes", { { 14, 2 }, { 22, 9 }, { 34, 6 }, { 43, 18 }, { 50, 29 }, { 56, 31 }, { 62, 47 }, { 67, 53 }, { 76, 39 }, { 82, 58 }, { 87, 64 }, { 92, 73 }, { 95, 75 }, { 98, 79 } }, { { 3, 20 }, { 11, 28 }, { 33, 49 }, { 57, 74 }, { 71, 90 } } },
    { "Muchas escaleras", { { 32, 12 }, { 55, 35 }, { 70, 52 }, { 90, 68 }, { 97, 78 } }, { { 2, 23 }, { 8, 31 }, { 15, 41 }, { 20, 38 }, { 24, 46 }, { 40, 59 }, { 47, 63 }, { 54, 72 }, { 60, 79 }, { 65, 83 }, { 73, 88 }, { 80, 97 } } } };
struct P2 { float x, y; };
P2 ctr(int n) { if (n <= 0) return { 0.5f, 9.5f }; int row = (n - 1) / 10, col = (n - 1) % 10; int c = row % 2 == 0 ? col : 9 - col; return { c + 1.5f, (9 - row) + 0.5f }; }
std::vector<P2> snakePts(int a, int b) {
    P2 A = ctr(a), B = ctr(b); float dx = B.x - A.x, dy = B.y - A.y, len = std::max(0.001f, std::hypot(dx, dy)), nx = -dy / len, ny = dx / len;
    int N = std::max(14, (int)std::ceil(len * 5)), waves = std::max(2, (int)std::lround(len / 1.6f)); float amp = std::min(0.5f, 0.18f + 0.04f * len); std::vector<P2> pts;
    for (int i = 0; i <= N; i++) { float t = (float)i / N, w = amp * std::sin(3.14159f * waves * t) * std::min(1.f, std::sin(3.14159f * t) * 2.5f); pts.push_back({ A.x + dx * t + nx * w, A.y + dy * t + ny * w }); }
    return pts;
}
struct Walker { P2 pos{ 0.5f, 9.5f }; std::vector<P2> q; };

struct Snakes : Game {
    int N = 0, boardIdx = 0, phase = 0, orderTurn = 0, turn = 0, dice = 0, pendingDest = -1, winner = -1; std::vector<int> orderRolls, turnOrder, pos; std::vector<Walker> walk; bool over = false;
    int od[2] = { 1, 1 }; float rollAnim = 0, aiT = 0, aiWait = 1, endT = 0, busyT = 0; int resigned = -1; std::string log;
    void start() override { N = (int)pl.size(); int b = opts.get("board", 0); boardIdx = b > 2 ? rndInt(3) : b; pos.assign(N, 0); walk.assign(N, {}); orderRolls.assign(N, -1); resetAi(); }
    void resetAi() { aiT = 0; int lv = std::max(1, std::min(3, pl[cur()].level)); aiWait = (lv == 1 ? 0.9f + rnd01() * 1.3f : lv == 3 ? 0.5f + rnd01() * 0.6f : 0.9f + rnd01() * 0.9f); }
    int cur() const { return phase == 0 ? orderTurn : turnOrder[turn]; }
    int turnPlayer() const override { return over ? -1 : cur(); }
    bool busy() const { for (auto& w : walk) if (!w.q.empty()) return true; return false; }
    void rollOrder() {
        int p = orderTurn; if (phase != 0 || orderRolls[p] >= 0 || rollAnim > 0) return; od[0] = 1 + rndInt(6); od[1] = 1 + rndInt(6); orderRolls[p] = od[0] + od[1]; rollAnim = 0.5f; app->audio.play("dice");
        orderTurn = (orderTurn + 1) % N; bool all = true; for (int v : orderRolls) if (v < 0) all = false;
        if (all) { std::vector<int> idx(N); for (int i = 0; i < N; i++) idx[i] = i; std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return orderRolls[a] > orderRolls[b]; }); turnOrder = idx; turn = 0; phase = 1; dice = 0; pendingDest = -1; log = "Orden: " + pl[idx[0]].name + " empieza"; }
        resetAi();
    }
    void roll() { if (phase != 1 || over || dice || busy() || rollAnim > 0) return; int p = cur(); dice = 1 + rndInt(6); int d = pos[p] + dice; if (d > 100) d = pos[p]; pendingDest = d; rollAnim = 0.45f; app->audio.play("dice"); resetAi(); }
    void place() {
        if (phase != 1 || over || pendingDest < 0 || busy() || rollAnim > 0) return; int p = cur(); int from = pos[p], mid = pendingDest, np = mid; Walker& w = walk[p];
        for (int c = from + 1; c <= mid; c++) w.q.push_back(ctr(c));
        const BoardDef& B = BOARDS[boardIdx]; auto s = B.snakes.find(np); auto l = B.ladders.find(np);
        if (s != B.snakes.end()) { auto pts = snakePts(np, s->second); for (auto& q : pts) w.q.push_back(q); np = s->second; log = pl[p].name + " cayó en una serpiente"; app->audio.play("lose"); }
        else if (l != B.ladders.end()) { P2 A = ctr(np), Bq = ctr(l->second); for (int i = 1; i <= 10; i++) w.q.push_back({ A.x + (Bq.x - A.x) * i / 10, A.y + (Bq.y - A.y) * i / 10 }); np = l->second; log = pl[p].name + " subió por una escalera"; app->audio.play("win"); }
        else app->audio.play("place");
        pos[p] = np; dice = 0; pendingDest = -1;
        if (np == 100) { over = true; winner = p; } else { turn = (turn + 1) % N; }
        resetAi();
    }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input& in, UI&) override {
        rollAnim = std::max(0.f, rollAnim - dt);
        for (auto& w : walk) { float step = 7.f * dt; while (step > 0 && !w.q.empty()) { P2 t = w.q.front(); float dx = t.x - w.pos.x, dy = t.y - w.pos.y, d = std::hypot(dx, dy); if (d <= step) { w.pos = t; w.q.erase(w.q.begin()); step -= d; } else { w.pos.x += dx / d * step; w.pos.y += dy / d * step; step = 0; } } }
        if (over) { if (busy()) return; endT += dt; if (endT > 1.4f && !finished) {
            std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ pos[i], i == winner ? 'w' : 'l' }); if (resigned >= 0) { r[resigned].r = 'x'; for (int i = 0; i < N; i++) if (i != resigned && i == (resigned + 1) % N) r[i].r = 'w'; for (int i = 0; i < N; i++) if (r[i].r == 'w' && i != (resigned + 1) % N) r[i].r = 'l'; }
            finish(r, winner >= 0 ? "Llegó a la meta " + pl[winner].name : "Partida terminada"); } return; }
        if (busy() || rollAnim > 0) return;
        int p = cur();
        if (!pl[p].ai) { Pad* pd = (pl[p].pad >= 0 && pl[p].pad < Input::MAXP) ? &in.pads[pl[p].pad] : nullptr; if (pd && pd->down[B_X]) { if (phase == 0) rollOrder(); else if (!dice) roll(); else place(); } return; }
        aiT += dt; if (aiT < aiWait) return; if (phase == 0) rollOrder(); else if (!dice) roll(); else place();
    }
    void draw(Gfx& g, UI& ui) override {
        float cs = 54, ox = 290, oy = 40; const BoardDef& B = BOARDS[boardIdx];
        auto toS = [&](P2 p) { return P2{ ox + p.x * cs, oy + p.y * cs }; };
        g.rrect({ ox - 12, oy - 12, cs * 11 + 24, cs * 10 + 24 }, 16, rgb(0x6b4224)); g.rect({ ox, oy, cs * 11, cs * 10 }, rgb(0x3a2414));
        // casilla de salida
        g.rect({ ox, oy + 9 * cs, cs, cs }, rgb(0x2f6b22)); g.text("SALIDA", ox + cs / 2, oy + 9.5f * cs, 11, pal::white, F_TITLE, AL_C);
        for (int n = 1; n <= 100; n++) {
            int row = (n - 1) / 10, col = (n - 1) % 10, c = row % 2 == 0 ? col : 9 - col; Rect r{ ox + (c + 1) * cs, oy + (9 - row) * cs, cs, cs };
            g.rect(r.inset(1), ((row + c) % 2) ? rgb(0xf3e3bf) : rgb(0xe3cd96)); g.text(std::to_string(n), r.x + 4, r.y + 10, 11, rgb(0x6b5a40), F_BODY, AL_L);
        }
        g.rect({ ox + (9 + 1) * cs, oy, cs, cs }, rgb(0xffe27a, 80));
        // escaleras
        for (auto& kv : B.ladders) { P2 a = toS(ctr(kv.first)), b = toS(ctr(kv.second)); float dx = b.x - a.x, dy = b.y - a.y, L = std::hypot(dx, dy), nx = -dy / L * 7, ny = dx / L * 7;
            g.line(a.x + nx, a.y + ny, b.x + nx, b.y + ny, 4, rgb(0x8a5a2c)); g.line(a.x - nx, a.y - ny, b.x - nx, b.y - ny, 4, rgb(0x8a5a2c));
            int k = std::max(2, (int)(L / 22)); for (int i = 1; i < k; i++) { float t = (float)i / k; g.line(a.x + dx * t + nx, a.y + dy * t + ny, a.x + dx * t - nx, a.y + dy * t - ny, 3, rgb(0xb98a4a)); } }
        // serpientes
        static const uint32_t SC[5] = { 0x2e8b3d, 0xc2571a, 0x6d3bb5, 0x0f7f8f, 0xa3262f }; int k = 0;
        for (auto& kv : B.snakes) { auto pts = snakePts(kv.first, kv.second); uint32_t col = SC[k++ % 5];
            for (size_t i = 1; i < pts.size(); i++) { P2 a = toS(pts[i - 1]), b = toS(pts[i]); g.line(a.x, a.y, b.x, b.y, 9 - 5.f * i / pts.size(), rgb(col)); }
            P2 h = toS(pts[0]); g.circle(h.x, h.y, 10, rgb(col)); g.circle(h.x - 3, h.y - 3, 2.5f, pal::white); g.circle(h.x + 3, h.y - 3, 2.5f, pal::white); }
        // fichas
        for (int i = 0; i < N; i++) { P2 s = toS(walk[i].pos); float ox2 = ((i % 4) - 1.5f) * 9, oy2 = (i / 4 - 0.5f) * 12; g.circle(s.x + ox2 + 1, s.y + oy2 + 3, 9, rgb(0, 110)); g.circle(s.x + ox2, s.y + oy2, 9, PCOL[i % 8]); g.ring(s.x + ox2, s.y + oy2, 9, 1.5f, rgb(0, 130)); }
        if (pendingDest >= 0) { P2 s = toS(ctr(pendingDest)); float a = 0.6f + 0.4f * std::sin(SDL_GetTicks() / 150.f); g.ring(s.x, s.y, cs * 0.45f, 4, rgb(0xffe27a, (uint8_t)(255 * a))); }
        // panel derecho
        Rect R{ 900, 28, 360, 640 }; g.rrect({ R.x + 3, R.y + 6, R.w, R.h }, 16, rgb(0, 100)); g.rrect(R, 16, pal::panelEdge); g.rrectGrad(R.inset(3), 14, rgb(0x4a2e18), rgb(0x2e1a0c));
        int p = cur(); bool mine = !over && !pl[p].ai && !busy();
        g.textShadow(over ? "¡FIN!" : phase == 0 ? "ORDEN DE JUEGO" : pl[p].name, R.cx(), R.y + 36, 22, pal::gold, F_TITLE, AL_C);
        float dxs = R.cx() - (phase == 0 ? 80 : 36);
        if (phase == 0) { for (int q = 0; q < 2; q++) dice::draw(g, { dxs + q * 90, R.y + 70, 70, 70 }, rollAnim > 0 ? 1 + rndInt(6) : od[q]); }
        else dice::draw(g, { dxs, R.y + 70, 72, 72 }, dice ? (rollAnim > 0 ? 1 + rndInt(6) : dice) : 0);
        std::string label = over ? "" : phase == 0 ? "Tirar 2 dados" : !dice ? "🎲 Tirar" : pendingDest == pos[p] ? "Pasar (te pasás de 100)" : "Mover a la casilla " + std::to_string(pendingDest);
        if (!over && ui.button(label, { R.x + 30, R.y + 170, R.w - 60, 54 }, 1, mine && rollAnim <= 0)) { if (phase == 0) rollOrder(); else if (!dice) roll(); else place(); }
        if (mine) ui.setFocusIfNone(hid(label, (int)((R.x + 30) * 7 + R.y + 170)));
        float y = R.y + 250; for (int i = 0; i < N; i++) { int q = phase == 0 ? i : turnOrder[i % N]; if (phase == 1 && i >= N) break; g.circle(R.x + 28, y + 12, 7, PCOL[q % 8]); g.text(pl[q].name, R.x + 44, y + 12, 15, q == (over ? -1 : p) ? pal::gold : pal::white, F_BOLD, AL_L, 170);
            g.text(phase == 0 ? (orderRolls[q] >= 0 ? std::to_string(orderRolls[q]) : "…") : "casilla " + std::to_string(pos[q]), R.x + R.w - 24, y + 12, 15, rgb(0xd8b46a), F_BODY, AL_R); y += 28; }
        g.textBox(log, { R.x + 24, R.y + R.h - 90, R.w - 48, 70 }, 15, rgb(0xd8b46a));
    }
    std::vector<Hint> hints() const override { return { { "A", "Tirar / mover" }, { "X", "Tirar / mover" } }; }
    std::string help() const override { return "Primero todos tiran 2 dados para fijar el orden. Después, en tu turno tirá un dado y mové tu ficha esa cantidad de casillas. Si caés al pie de una escalera, subís; si caés en la boca de una serpiente, bajás por su cuerpo. Para ganar hay que llegar al 100 con la tirada justa; si te pasás, no avanzás."; }
};
}
Game* makeCulebras() { return new Snakes(); }
