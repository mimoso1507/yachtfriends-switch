// 4 en línea clásico 7x6, con el aspecto de la foto de Hoyle: marco marfil, rieles verdes, fichas verdes (estrella) y naranjas (sol).
// IA negamax con poda alfa-beta y los mismos niveles que la versión de PC.
#include "../app.h"
#include <algorithm>
#include <climits>

namespace {
const int R = 6, C = 7, WIN = 100000;
struct Lvl { int dc, lim; float err, noise; int top; float d; };
const Lvl LVL[4] = { {}, { 1, 7000, 0.42f, 6, 99, 1.4f }, { 5, 7000, 0.12f, 0.9f, 3, 1.0f }, { 7, 14000, 0.015f, 0.2f, 2, 0.75f } };
std::vector<std::array<int, 4>> WINS;
const int ORDER_C[7] = { 3, 2, 4, 1, 5, 0, 6 }, WT[5] = { 0, 1, 6, 25, 0 };
void initWins() {
    if (!WINS.empty()) return;
    const int D[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };
    for (auto& d : D) for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) {
        int er = r + 3 * d[0], ec = c + 3 * d[1]; if (er < 0 || er >= R || ec < 0 || ec >= C) continue;
        std::array<int, 4> w; for (int k = 0; k < 4; k++) w[k] = (r + k * d[0]) * C + c + k * d[1]; WINS.push_back(w);
    }
}
bool winAt(const int8_t* b, int r, int c, int p, std::vector<std::pair<int, int>>* line = nullptr) {
    const int D[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };
    for (auto& d : D) {
        std::vector<std::pair<int, int>> cells{ { r, c } };
        int rr = r + d[0], cc = c + d[1]; while (rr >= 0 && rr < R && cc >= 0 && cc < C && b[rr * C + cc] == p) { cells.push_back({ rr, cc }); rr += d[0]; cc += d[1]; }
        rr = r - d[0]; cc = c - d[1]; while (rr >= 0 && rr < R && cc >= 0 && cc < C && b[rr * C + cc] == p) { cells.insert(cells.begin(), { rr, cc }); rr -= d[0]; cc -= d[1]; }
        if (cells.size() >= 4) { if (line) { cells.resize(4); *line = cells; } return true; }
    }
    return false;
}
int evalC(const int8_t* b, int p) {
    int sc = 0;
    for (auto& w : WINS) { int cm = 0, co = 0; for (int k = 0; k < 4; k++) { int v = b[w[k]]; if (v == p) cm++; else if (v) co++; } if (!co && cm) sc += WT[cm]; else if (!cm && co) sc -= WT[co]; }
    for (int r = 0; r < R; r++) { int v = b[r * C + 3]; if (v == p) sc += 3; else if (v) sc -= 3; }
    return sc;
}
int neg(int8_t* b, int8_t* h, int p, int depth, int alpha, int beta, int& n, int lim) {
    n++;
    if (depth <= 0 || n > lim) return evalC(b, p);
    int best = INT_MIN; bool any = false;
    for (int k = 0; k < C; k++) {
        int col = ORDER_C[k], r = h[col]; if (r >= R) continue; any = true;
        b[r * C + col] = p; h[col]++;
        int sc = winAt(b, r, col, p) ? WIN + depth : -neg(b, h, 3 - p, depth - 1, -beta, -alpha, n, lim);
        h[col]--; b[r * C + col] = 0;
        best = std::max(best, sc); alpha = std::max(alpha, best); if (alpha >= beta) break;
    }
    return any ? best : 0;
}
int choose(const int8_t* board, int me, int lv) {
    initWins(); const Lvl& P = LVL[lv];
    int8_t b[42], h[7]; std::copy(board, board + 42, b);
    for (int c = 0; c < C; c++) { int r = 0; while (r < R && b[r * C + c]) r++; h[c] = r; }
    struct M { int v; float s; }; std::vector<M> out;
    for (int k = 0; k < C; k++) {
        int m = ORDER_C[k]; if (h[m] >= R) continue; int r = h[m], n = 0;
        b[r * C + m] = me; h[m]++;
        int sc = winAt(b, r, m, me) ? WIN + 10 : -neg(b, h, 3 - me, P.dc, -INT_MAX, INT_MAX, n, P.lim);
        h[m]--; b[r * C + m] = 0;
        out.push_back({ m, sc + rnd01() * P.noise });
    }
    if (out.empty()) return -1;
    std::sort(out.begin(), out.end(), [](const M& a, const M& b) { return a.s > b.s; });
    if (P.top < 99 && out[0].s >= WIN) return out[0].v;
    if (out.size() > 1 && rnd01() < P.err) {
        if (P.top >= 99) return out[rndInt((int)out.size())].v;
        std::vector<M> ok; for (size_t i = 0; i < out.size() && (int)i < P.top; i++) if (out[i].s > -WIN / 2) ok.push_back(out[i]);
        if (!ok.empty()) return ok[rndInt((int)ok.size())].v;
    }
    return out[0].v;
}

struct C4 : Game {
    int8_t board[42] = {}; int heights[7] = {}, turn = 0, count = 0, winner = -1, cursor = 3; bool over = false;
    std::vector<std::pair<int, int>> line; float aiT = 0, endT = -1; SDL_Texture* frameTex = nullptr; float frameS = 0;
    ~C4() { if (frameTex) SDL_DestroyTexture(frameTex); }
    struct Fall { int col, row; float y, vy; int v; bool on; } fall{};
    void start() override { initWins(); }
    int turnPlayer() const override { return over ? -1 : turn; }
    void move(int col) {
        if (over || fall.on || col < 0 || col >= C || heights[col] >= R) return;
        int row = heights[col]; board[row * C + col] = turn + 1; heights[col]++; count++;
        fall = { col, row, -1.f, 0, turn + 1, true };
        if (winAt(board, row, col, turn + 1, &line)) { over = true; winner = turn; }
        else if (count == R * C) over = true;
        else turn = 1 - turn;
    }
    void resign(int p) override { over = true; winner = 1 - p; endT = 0.5f; }
    void update(float dt, Input&, UI&) override {
        if (fall.on) {
            fall.vy += dt * 26; fall.y += fall.vy * dt; float target = R - 1 - fall.row;
            if (fall.y >= target) { fall.on = false; app->audio.play("place"); if (over) endT = 0; }
        }
        if (over) {
            if (endT >= 0 && (endT += dt) > 1.6f && !finished) {
                std::vector<PResult> r(2); for (int i = 0; i < 2; i++) { r[i].pts = winner == i ? 1 : 0; r[i].r = winner < 0 ? 'd' : winner == i ? 'w' : 'l'; }
                finish(r, winner < 0 ? "Tablero lleno: empate" : "4 en línea de " + pl[winner].name);
            }
            return;
        }
        if (!pl[turn].ai || fall.on) return;
        aiT += dt; if (aiT < LVL[pl[turn].level].d) return; aiT = 0;
        int c = choose(board, turn + 1, pl[turn].level); if (c >= 0) move(c);
    }
    static void coin(Gfx& g, float cx, float cy, float r, int v, bool hl) {
        Col a = v == 1 ? rgb(0x7dd36a) : rgb(0xffb04a), b = v == 1 ? rgb(0x2e8a2a) : rgb(0xe0661a);
        g.circle(cx + 2, cy + 4, r, rgb(0, 90)); g.circleGrad(cx - r * 0.2f, cy - r * 0.2f, r, a, b); g.ring(cx, cy, r, r * 0.12f, mix(b, rgb(0), 0.25f));
        if (v == 1) { // estrella
            for (int k = 0; k < 5; k++) { float a1 = -1.5708f + k * 1.2566f, a2 = a1 + 0.6283f, a3 = a1 - 0.6283f; float R1 = r * 0.55f, R2 = r * 0.22f;
                g.tri(cx + cosf(a1) * R1, cy + sinf(a1) * R1, cx + cosf(a2) * R2, cy + sinf(a2) * R2, cx + cosf(a3) * R2, cy + sinf(a3) * R2, rgb(0xeaffd8)); }
            g.circle(cx, cy, r * 0.23f, rgb(0xeaffd8));
        } else { // sol
            g.circle(cx, cy, r * 0.28f, rgb(0xfff2c0)); for (int k = 0; k < 8; k++) { float a1 = k * 0.7854f; g.line(cx + cosf(a1) * r * 0.36f, cy + sinf(a1) * r * 0.36f, cx + cosf(a1) * r * 0.56f, cy + sinf(a1) * r * 0.56f, r * 0.09f, rgb(0xfff2c0)); }
        }
        if (hl) g.ring(cx, cy, r + 5, 5, pal::focus);
    }
    void draw(Gfx& g, UI& ui) override {
        float cell = 84, W = C * cell + 24, H = R * cell + 24, x0 = 790 - W / 2, y0 = 112;
        bool mine = !over && !fall.on && !pl[turn].ai;
        // rieles verdes y base
        g.rrect({ x0 - 30, y0 - 10, 26, H + 40 }, 6, rgb(0x5b8a2c)); g.rrect({ x0 + W + 4, y0 - 10, 26, H + 40 }, 6, rgb(0x5b8a2c));
        g.rrectGrad({ x0 - 44, y0 + H + 18, W + 88, 26 }, 8, rgb(0x8bc34a), rgb(0x5b8a2c));
        g.rrect({ x0 + 6, y0 + 10, W, H }, 18, rgb(0, 70));
        // fichas detrás del marco
        auto cxOf = [&](int c) { return x0 + 12 + c * cell + cell / 2; }; auto cyOf = [&](float rr) { return y0 + 12 + (R - 1 - rr) * cell + cell / 2; };
        for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) {
            int v = board[r * C + c]; if (!v || (fall.on && fall.col == c && fall.row == r)) continue;
            bool on = false; for (auto& p : line) if (p.first == r && p.second == c) on = true;
            coin(g, cxOf(c), cyOf((float)r), cell * 0.4f, v, on && over);
        }
        if (fall.on) coin(g, cxOf(fall.col), y0 + 12 + fall.y * cell + cell / 2, cell * 0.4f, fall.v, false);
        // marco marfil con agujeros de verdad (textura con huecos transparentes, se rehace si cambia la resolución)
        if (!frameTex || frameS != g.S) {
            if (frameTex) SDL_DestroyTexture(frameTex);
            frameS = g.S; frameTex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)ceilf(W * g.S), (int)ceilf(H * g.S));
            SDL_SetTextureBlendMode(frameTex, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(g.ren, frameTex); SDL_SetRenderDrawColor(g.ren, 0, 0, 0, 0); SDL_RenderClear(g.ren);
            g.rrectGrad({ 0, 0, W, H }, 18, rgb(0xfbf9f0), rgb(0xe6e1cf));
            SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_NONE);
            for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) g.circle(12 + c * cell + cell / 2, 12 + r * cell + cell / 2, cell * 0.4f, Col{ 0, 0, 0, 0 });
            SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
            for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) { float cx = 12 + c * cell + cell / 2, cy = 12 + r * cell + cell / 2; g.ring(cx, cy, cell * 0.4f + 4, 5, rgb(0xcfc8b0)); g.ring(cx, cy, cell * 0.4f + 1, 2, rgb(0x9e977f)); }
            g.rrectLine({ 0, 0, W, H }, 18, 3, rgb(0xd6cfb6));
            SDL_SetRenderTarget(g.ren, nullptr);
        }
        g.image(frameTex, { x0, y0, W, H });
        // columnas: tocar o elegir con el mando
        for (int c = 0; c < C; c++) {
            Rect col{ x0 + 12 + c * cell, y0 - 80, cell, H + 80 }; uint32_t id = hid("c4col", c);
            bool hit = ui.hit(id, col, mine && heights[c] < R);
            if (ui.focused(id) || (!ui.padNav && mine && col.in(ui.in->tx, ui.in->ty) && ui.in->touchHeld)) cursor = c;
            if (hit) { move(c); }
        }
        if (mine) { ui.setFocusIfNone(hid("c4col", cursor)); coin(g, cxOf(cursor), y0 - 44, cell * 0.34f, turn + 1, false); g.tri(cxOf(cursor) - 10, y0 - 8, cxOf(cursor) + 10, y0 - 8, cxOf(cursor), y0 + 4, pal::gold); }
        std::string msg = over ? (winner < 0 ? "¡Empate!" : "¡4 en línea de " + pl[winner].name + "!") : pl[turn].ai ? "Piensa " + pl[turn].name + "…" : "Turno de " + pl[turn].name;
        g.textShadow(msg, 790, 22, 22, pal::gold, F_BOLD, AL_C);
        coin(g, 790 - g.textW(msg, 22, F_BOLD) / 2 - 26, 22, 13, (over && winner >= 0 ? winner : turn) + 1, false);
    }
    std::vector<Hint> hints() const override { return { { "LR/STICK", "Elegir columna" }, { "A", "Soltar ficha" } }; }
    std::string help() const override { return "Por turnos, soltá una ficha en una columna: cae hasta el fondo. Gana quien junte 4 fichas seguidas en horizontal, vertical o diagonal. Con el mando: izquierda/derecha para elegir la columna y A para soltar."; }
};
}
Game* makeConnect4() { return new C4(); }
