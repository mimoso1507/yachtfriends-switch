// Laberinto — carrera de 1 a 4 jugadores en el MISMO laberinto. Gana quien llegue primero a la salida.
// Pantalla: 1 jugador = completa · 2 = dividida con línea vertical · 3 = cuadros 2x2 + 4º de estadísticas · 4 = cuatro cuadros.
// Cada humano usa su propio mando (cruz o palanca); el jugador 1 también puede tocar la pantalla.
#include "../app.h"
#include <algorithm>
#include <deque>

namespace {
const int DIRS[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
struct Rng { uint32_t a; float operator()() { a += 0x6D2B79F5u; uint32_t t = a; t = (t ^ (t >> 15)) * (1 | t); t = (t + ((t ^ (t >> 7)) * (61 | t))) ^ t; return ((t ^ (t >> 14))) / 4294967296.f; } };
struct Maze { int W, H; std::vector<uint8_t> g; int sx, sy, gx, gy; bool wall(int x, int y) const { return x < 0 || y < 0 || x >= W || y >= H || g[y * W + x]; } };
Maze makeMaze(uint32_t seed, int cw, int ch) {
    Rng R{ seed }; Maze m; m.W = cw * 2 + 1; m.H = ch * 2 + 1; m.g.assign(m.W * m.H, 1);
    std::vector<std::pair<int, int>> st{ { 0, 0 } }; m.g[1 * m.W + 1] = 0; std::vector<bool> seen(cw * ch, false); seen[0] = true;
    while (!st.empty()) {
        auto [cx, cy] = st.back(); std::vector<std::array<int, 4>> nb;
        for (auto& d : DIRS) { int nx = cx + d[0], ny = cy + d[1]; if (nx >= 0 && ny >= 0 && nx < cw && ny < ch && !seen[ny * cw + nx]) nb.push_back({ nx, ny, d[0], d[1] }); }
        if (nb.empty()) { st.pop_back(); continue; }
        auto p = nb[(size_t)(R() * nb.size())];
        seen[p[1] * cw + p[0]] = true; m.g[(cy * 2 + 1 + p[3]) * m.W + cx * 2 + 1 + p[2]] = 0; m.g[(p[1] * 2 + 1) * m.W + p[0] * 2 + 1] = 0; st.push_back({ p[0], p[1] });
    }
    int sy = 1 + 2 * (ch / 2); m.g[sy * m.W] = 0; m.g[sy * m.W + m.W - 1] = 0; m.sx = 0; m.sy = sy; m.gx = m.W - 1; m.gy = sy;
    return m;
}
std::vector<int> distMap(const Maze& m) {
    std::vector<int> d(m.W * m.H, -1); std::deque<int> q{ m.gy * m.W + m.gx }; d[m.gy * m.W + m.gx] = 0;
    while (!q.empty()) { int c = q.front(); q.pop_front(); int x = c % m.W, y = c / m.W; for (auto& dd : DIRS) { int nx = x + dd[0], ny = y + dd[1]; if (!m.wall(nx, ny) && d[ny * m.W + nx] < 0) { d[ny * m.W + nx] = d[c] + 1; q.push_back(ny * m.W + nx); } } }
    return d;
}
struct Lab : Game {
    Maze m; std::vector<int> dist; int n = 1, maxD = 1;
    std::vector<int> px, py, steps, done; std::vector<float> lastMove, aiT; float t = 0, endT = -1; bool over = false; int winner = -1;
    void start() override {
        const int SZ[3][2] = { { 12, 8 }, { 18, 11 }, { 26, 16 } }; int s = std::max(0, std::min(2, opts.get("size", 1)));
        m = makeMaze((uint32_t)(SDL_GetTicks() * 2654435761u) ^ (uint32_t)rndInt(1 << 30), SZ[s][0], SZ[s][1]); dist = distMap(m);
        n = (int)pl.size(); px.assign(n, m.sx); py.assign(n, m.sy); steps.assign(n, 0); done.assign(n, -1); lastMove.assign(n, -1); aiT.assign(n, 1.2f + rnd01() * 0.6f);
        maxD = std::max(1, dist[m.sy * m.W + m.sx]);
    }
    bool move(int p, int d) {
        if (over || done[p] >= 0 || t - lastMove[p] < 0.044f) return false;
        int x = px[p] + DIRS[d][0], y = py[p] + DIRS[d][1]; if (m.wall(x, y)) return false;
        lastMove[p] = t; px[p] = x; py[p] = y; steps[p]++;
        if (x == m.gx && y == m.gy) { done[p] = (int)(t * 1000); if (winner < 0) { winner = p; over = true; endT = 0; app->audio.play("win"); } }
        return true;
    }
    void resign(int p) override { done[p] = 1 << 30; int left = 0; for (int i = 0; i < n; i++) if (done[i] < 0) left++; if (left == 0 || (n > 1 && left == 1 && winner < 0)) { for (int i = 0; i < n; i++) if (done[i] < 0) winner = i; over = true; endT = 0; } }
    void update(float dt, Input& in, UI&) override {
        t += dt;
        if (over) {
            if ((endT += dt) > 1.6f && !finished) {
                std::vector<int> ord(n); for (int i = 0; i < n; i++) ord[i] = i;
                std::vector<PResult> r(n); for (int i = 0; i < n; i++) { int d = dist[py[i] * m.W + px[i]]; r[i].pts = std::max(0, 100 - 100 * d / maxD); r[i].r = i == winner ? 'w' : (done[i] == (1 << 30) ? 'x' : 'l'); }
                finish(r, winner >= 0 ? "Ganó " + pl[winner].name + " en " + fmtTime(done[winner]) : "Fin de la carrera");
            }
            return;
        }
        for (int p = 0; p < n; p++) {
            if (done[p] >= 0) continue;
            if (pl[p].ai) {
                const float MS[4] = { 0, 0.23f, 0.15f, 0.095f }, ERR[4] = { 0, 0.14f, 0.05f, 0.01f }; int lv = pl[p].level;
                aiT[p] -= dt; if (aiT[p] > 0) continue; aiT[p] = MS[lv] * (0.85f + rnd01() * 0.3f);
                int best = -1, bd = 1 << 30; std::vector<int> o;
                for (int d = 0; d < 4; d++) { int x = px[p] + DIRS[d][0], y = py[p] + DIRS[d][1]; if (m.wall(x, y)) continue; o.push_back(d); int dd = dist[y * m.W + x]; if (dd >= 0 && dd < bd) { bd = dd; best = d; } }
                if (!o.empty() && best >= 0) { lastMove[p] = -1; move(p, rnd01() < ERR[lv] ? o[rndInt((int)o.size())] : best); }
                continue;
            }
            int pad = pl[p].pad; if (pad < 0) continue; const Pad& P = in.pads[pad];
            int d = -1; if (P.held[B_UP]) d = 0; else if (P.held[B_RIGHT]) d = 1; else if (P.held[B_DOWN]) d = 2; else if (P.held[B_LEFT]) d = 3;
            int db = d == 0 ? B_UP : d == 1 ? B_RIGHT : d == 2 ? B_DOWN : B_LEFT;
            if (d >= 0 && (P.down[db] || t - lastMove[p] > 0.11f)) {
                if (!move(p, d)) {   // en una esquina, probar la otra dirección pulsada
                    for (int e = 0; e < 4; e++) { int b = e == 0 ? B_UP : e == 1 ? B_RIGHT : e == 2 ? B_DOWN : B_LEFT; if (e != d && P.held[b] && move(p, e)) break; }
                }
            }
            // táctil (jugador del mando 1): mantener el dedo hacia donde querés ir
            if (pad == 0 && in.touchHeld && t - lastMove[p] > 0.11f && view[p].w > 0) {
                float cs = cellSize(view[p]); Rect mr = mazeRect(view[p], cs); float cx = mr.x + (px[p] + 0.5f) * cs, cy = mr.y + (py[p] + 0.5f) * cs;
                float dx = in.tx - cx, dy = in.ty - cy; if (fabsf(dx) + fabsf(dy) > cs) { int dd = fabsf(dx) > fabsf(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0); if (!move(p, dd)) { int alt = fabsf(dx) > fabsf(dy) ? (dy > 0 ? 2 : 0) : (dx > 0 ? 1 : 3); move(p, alt); } }
            }
        }
    }
    Rect view[4]{};
    float cellSize(Rect v) const { return std::min((v.w - 16) / m.W, (v.h - 52) / m.H); }
    Rect mazeRect(Rect v, float cs) const { return { v.cx() - cs * m.W / 2, v.y + 44 + (v.h - 52 - cs * m.H) / 2, cs * m.W, cs * m.H }; }
    void drawMaze(Gfx& g, Rect v, int me) {
        g.rrect({ v.x + 3, v.y + 5, v.w, v.h }, 14, rgb(0, 100)); g.rrect(v, 14, rgb(0x2c5a1c)); g.rrectGrad(v.inset(3), 12, rgb(0x3f7a28), rgb(0x2c5a1c));
        // placa con nombre, color y progreso
        Rect pr{ v.x + 10, v.y + 8, v.w - 20, 30 }; g.rrect(pr, 8, rgb(0x1c1410));
        g.circle(pr.x + 14, pr.cy(), 7, PCOL[me]); g.emoji(pl[me].emoji, pr.x + 36, pr.cy(), 20); g.text(pl[me].name, pr.x + 52, pr.cy(), 15, pal::white, F_BOLD, AL_L, pr.w * 0.45f);
        int d = dist[py[me] * m.W + px[me]]; int prog = 100 - 100 * std::max(0, d) / maxD;
        g.text(done[me] >= 0 && done[me] < (1 << 30) ? "🏁 " + fmtTime(done[me]) : std::to_string(prog) + "% · " + std::to_string(steps[me]) + " pasos", pr.x + pr.w - 10, pr.cy(), 13, pal::gold, F_BODY, AL_R);
        float cs = cellSize(v); Rect mr = mazeRect(v, cs);
        g.rect(mr, rgb(0xe9d9a8));
        for (int y = 0; y < m.H; y++) for (int x = 0; x < m.W; x++) if (m.g[y * m.W + x]) {
            Rect c{ mr.x + x * cs, mr.y + y * cs, cs + 0.5f, cs + 0.5f }; g.rect(c, rgb(0x2f6b22));
            if (cs > 7) g.rect({ c.x, c.y, c.w, c.h * 0.35f }, rgb(0x4d9a34));
        }
        g.emoji("🏁", mr.x + (m.gx + 0.5f) * cs, mr.y + (m.gy + 0.5f) * cs, cs * 1.6f);
        for (int p = 0; p < n; p++) if (p != me) g.circle(mr.x + (px[p] + 0.5f) * cs, mr.y + (py[p] + 0.5f) * cs, cs * 0.3f, withA(PCOL[p], 120));
        float cx = mr.x + (px[me] + 0.5f) * cs, cy = mr.y + (py[me] + 0.5f) * cs;
        g.circle(cx + 1, cy + 2, cs * 0.46f, rgb(0, 90)); g.circleGrad(cx, cy, cs * 0.44f, mix(PCOL[me], rgb(0xffffff), 0.35f), PCOL[me]); g.ring(cx, cy, cs * 0.46f, std::max(1.5f, cs * 0.08f), rgb(0xffffff));
    }
    void draw(Gfx& g, UI&) override {
        Rect A{ 262, 14, 1004, 640 };
        if (n == 1) view[0] = A;
        else if (n == 2) { view[0] = { A.x, A.y, A.w / 2 - 6, A.h }; view[1] = { A.x + A.w / 2 + 6, A.y, A.w / 2 - 6, A.h }; }
        else for (int i = 0; i < 4; i++) view[i] = { A.x + (i % 2) * (A.w / 2 + 4), A.y + (i / 2) * (A.h / 2 + 4), A.w / 2 - 4, A.h / 2 - 4 };
        for (int p = 0; p < n; p++) drawMaze(g, view[p], p);
        if (n == 2) g.rect({ A.cx() - 2, A.y, 4, A.h }, pal::gold);
        if (n == 3) {   // 4º cuadro: estadísticas
            Rect s = view[3]; g.rrect({ s.x + 3, s.y + 5, s.w, s.h }, 14, rgb(0, 100)); g.rrect(s, 14, pal::panelEdge); g.rrectGrad(s.inset(3), 12, rgb(0x4a2e18), rgb(0x2e1a0c));
            g.textShadow("📊 ESTADÍSTICAS", s.cx(), s.y + 30, 20, pal::gold, F_TITLE, AL_C); g.text("⏱️ " + fmtTime((int)(t * 1000)), s.cx(), s.y + 62, 18, pal::white, F_BOLD, AL_C);
            std::vector<int> ord{ 0, 1, 2 }; std::sort(ord.begin(), ord.end(), [&](int a, int b) { return dist[py[a] * m.W + px[a]] < dist[py[b] * m.W + px[b]]; });
            for (int k = 0; k < 3; k++) {
                int i = ord[k]; float y = s.y + 100 + k * 64; int d = dist[py[i] * m.W + px[i]]; float prog = 1 - std::max(0, d) / (float)maxD;
                g.text(std::to_string(k + 1) + ". " + pl[i].name, s.x + 30, y, 16, pal::white, F_BOLD, AL_L, s.w * 0.5f); g.text(std::to_string(steps[i]) + " pasos", s.x + s.w - 30, y, 14, pal::cream, F_BODY, AL_R);
                g.rrect({ s.x + 30, y + 16, s.w - 60, 12 }, 6, rgb(0x1c1410)); g.rrect({ s.x + 30, y + 16, (s.w - 60) * prog, 12 }, 6, PCOL[i]);
            }
        }
        if (!over && t < 1.2f) g.textShadow("¡YA!", A.cx(), A.cy(), 80 - t * 20, pal::gold, F_TITLE, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Mover tu ficha" } }; }
    std::string help() const override { return "Todos corren en el mismo laberinto. Llevá tu ficha desde la entrada (izquierda) hasta la bandera de salida (derecha). Cada jugador usa su propio mando: cruz o palanca. En modo portátil también podés mantener el dedo hacia donde querés ir."; }
};
}
Game* makeLaberinto() { return new Lab(); }
