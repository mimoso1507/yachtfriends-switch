// Damas chinas: tablero estrella de 121 casillas, 2/3/4/6 jugadores con 10 fichas. Se mueve de a un paso o saltando fichas en cadena.
#include "../app.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <queue>

namespace {
const int DIRS[6][3] = { { 1, -1, 0 }, { 1, 0, -1 }, { 0, 1, -1 }, { -1, 1, 0 }, { -1, 0, 1 }, { 0, -1, 1 } };
const int CORN[6][3] = { { 8, -4, -4 }, { 4, 4, -8 }, { -4, 8, -4 }, { -8, 4, 4 }, { -4, -4, 8 }, { 4, -8, 4 } };
const uint32_t COL[6] = { 0xe8524a, 0xf5c542, 0x3ddc97, 0x5aa9ff, 0xb36ae2, 0xff9a3c }, DARKC[6] = { 0x9a2b25, 0xa07d10, 0x1f8f60, 0x2a6fc0, 0x6a3a96, 0xb5601a };
bool inCorner(int i, int x, int y, int z) { switch (i) { case 0: return x >= 5 && y >= -4 && z >= -4; case 1: return z <= -5 && x <= 4 && y <= 4; case 2: return y >= 5 && x >= -4 && z >= -4; case 3: return x <= -5 && y <= 4 && z <= 4; case 4: return z >= 5 && x >= -4 && y >= -4; default: return y <= -5 && x <= 4 && z <= 4; } }
struct Geo {
    std::vector<std::array<int, 3>> cells; std::map<std::pair<int,int>, int> idx; std::vector<std::array<int, 6>> nb, jp; std::vector<int> corner[6]; int apex[6];
    Geo() {
        for (int x = -8; x <= 8; x++) for (int y = -8; y <= 8; y++) { int z = -x - y; if (std::abs(z) > 8) continue; bool in = std::abs(x) <= 4 && std::abs(y) <= 4 && std::abs(z) <= 4; for (int i = 0; i < 6; i++) if (inCorner(i, x, y, z)) in = true; if (in) { idx[{ x, y }] = (int)cells.size(); cells.push_back({ x, y, z }); } }
        nb.resize(cells.size()); jp.resize(cells.size());
        for (size_t k = 0; k < cells.size(); k++) for (int d = 0; d < 6; d++) { auto a = idx.find({ cells[k][0] + DIRS[d][0], cells[k][1] + DIRS[d][1] }); nb[k][d] = a == idx.end() ? -1 : a->second; auto b = idx.find({ cells[k][0] + 2 * DIRS[d][0], cells[k][1] + 2 * DIRS[d][1] }); jp[k][d] = b == idx.end() ? -1 : b->second; }
        for (int i = 0; i < 6; i++) { for (size_t k = 0; k < cells.size(); k++) if (inCorner(i, cells[k][0], cells[k][1], cells[k][2])) corner[i].push_back((int)k); apex[i] = idx[{ CORN[i][0], CORN[i][1] }]; }
    }
    int dist(int a, int b) const { return std::max({ std::abs(cells[a][0] - cells[b][0]), std::abs(cells[a][1] - cells[b][1]), std::abs(cells[a][2] - cells[b][2]) }); }
};
const Geo& G() { static Geo g; return g; }
// destinos desde 'from': mapa destino -> camino
std::map<int, std::vector<int>> reach(const std::vector<int>& b, int from) {
    const Geo& g = G(); std::map<int, std::vector<int>> out; for (int d = 0; d < 6; d++) { int n = g.nb[from][d]; if (n >= 0 && b[n] < 0) out[n] = { from, n }; }
    std::set<int> seen{ from }; std::queue<std::pair<int, std::vector<int>>> q; q.push({ from, { from } });
    while (!q.empty()) { auto cur = q.front(); q.pop(); for (int d = 0; d < 6; d++) { int m = g.nb[cur.first][d], j = g.jp[cur.first][d]; if (m < 0 || j < 0 || b[m] < 0 || b[j] >= 0 || seen.count(j)) continue; seen.insert(j); auto np = cur.second; np.push_back(j); if (!out.count(j)) out[j] = np; q.push({ j, np }); } }
    return out;
}
struct Damas : Game {
    int N = 0, turn = 0, winner = -1, sel = -1, moves = 0; bool over = false; int seats[6] = {}; std::vector<int> board; std::map<int, std::vector<int>> reachSel; std::vector<bool> active; float aiT = 0, aiWait = 0.6f, endT = 0, anim = 0, animDur = 0; std::vector<int> animPath; int animTo = -1;
    void start() override {
        const Geo& g = G(); N = (int)pl.size(); static const int S2[2] = { 0, 3 }, S3[3] = { 0, 2, 4 }, S4[4] = { 1, 2, 4, 5 }; board.assign(g.cells.size(), -1); active.assign(N, true);
        for (int p = 0; p < N; p++) { seats[p] = N == 2 ? S2[p] : N == 3 ? S3[p] : N == 4 ? S4[p] : p; for (int k : g.corner[seats[p]]) board[k] = p; }
        turn = rndInt(N); aiT = 0; }
    int turnPlayer() const override { return over ? -1 : turn; }
    const std::vector<int>& goal(int p) const { return G().corner[(seats[p] + 3) % 6]; }
    bool isWin(int p) const { for (int k : goal(p)) if (board[k] != p) return false; return true; }
    int inGoal(int p) const { int c = 0; for (int k : goal(p)) if (board[k] == p) c++; return c; }
    bool canMove(int p) const { for (size_t k = 0; k < board.size(); k++) if (board[k] == p && !reach(board, (int)k).empty()) return true; return false; }
    void next() { int gd = 0; do { turn = (turn + 1) % N; gd++; } while ((!active[turn] || !canMove(turn)) && gd < 20); aiT = 0; sel = -1; reachSel.clear(); }
    bool move(int p, int from, int to) {
        if (over || p != turn || board[from] != p) return false; auto r = reach(board, from); if (!r.count(to)) return false; animPath = r[to]; animTo = to; animDur = 0.17f * (float)(animPath.size() - 1); anim = animDur;
        board[from] = -1; board[to] = p; moves++; app->audio.play("place"); if (isWin(p)) { over = true; winner = p; app->audio.play("win"); return true; } next(); return true; }
    void resign(int p) override { if (p < 0 || p >= N || !active[p]) return; active[p] = false; for (auto& v : board) if (v == p) v = -1; int al = 0, last = -1; for (int i = 0; i < N; i++) if (active[i]) { al++; last = i; } if (al <= 1) { over = true; winner = last; return; } if (turn == p) next(); }
    // IA
    bool aiMove() {
        const Geo& g = G(); int p = turn, lv = std::max(1, std::min(3, pl[p].level)); float err = lv == 1 ? 0.3f : lv == 2 ? 0.06f : 0.f, noise = lv == 1 ? 2.2f : lv == 2 ? 0.8f : 0.25f;
        int tgt = (seats[p] + 3) % 6, ap = g.apex[tgt]; std::set<int> gl(g.corner[tgt].begin(), g.corner[tgt].end()); std::vector<int> pcs; for (size_t k = 0; k < board.size(); k++) if (board[k] == p) pcs.push_back((int)k);
        float avg = 0; for (int k : pcs) avg += g.dist(k, ap); avg /= (float)pcs.size(); std::vector<std::pair<int,int>> all; int bf = -1, bt = -1; float bs = -1e9f;
        for (int f : pcs) { auto r = reach(board, f); for (auto& kv : r) { int t = kv.first; float sc = (float)(g.dist(f, ap) - g.dist(t, ap)); sc += 0.35f * std::max(0.f, g.dist(f, ap) - avg) * (g.dist(f, ap) - g.dist(t, ap) > 0 ? 1.f : 0.f); sc += 0.15f * (float)(kv.second.size() - 2); if (gl.count(f) && !gl.count(t)) sc -= 40; if (gl.count(t) && !gl.count(f)) sc += 3; sc += rnd01() * noise; all.push_back({ f, t }); if (sc > bs) { bs = sc; bf = f; bt = t; } } }
        if (all.empty()) return false; if (rnd01() < err) { auto m = all[rndInt((int)all.size())]; bf = m.first; bt = m.second; } return move(p, bf, bt); }
    void update(float dt, Input& in, UI&) override {
        anim = std::max(0.f, anim - dt);
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) r.push_back({ inGoal(p), p == winner ? 'w' : 'l' }); finish(r, "Ganó " + (winner >= 0 ? pl[winner].name : std::string("nadie"))); } return; }
        if (anim > 0) return;
        if (pl[turn].ai) { aiT += dt; if (aiT > aiWait) { aiT = 0; if (!aiMove()) next(); } return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (pd && pd->down[B_B] && sel >= 0) { sel = -1; reachSel.clear(); }
    }
    void xy(int k, float& x, float& y) const { const Geo& g = G(); const float S = 23.f; x = 620 + S * 1.7320508f * (g.cells[k][0] + g.cells[k][2] / 2.f); y = 356 + S * 1.5f * g.cells[k][2]; }
    void draw(Gfx& g, UI& ui) override {
        const Geo& ge = G(); bool mine = !over && !pl[turn].ai && anim <= 0;
        g.rrect({ 300, 56, 640, 600 }, 14, rgb(0x6b4224)); g.rrect({ 306, 62, 628, 588 }, 12, rgb(0xf4ecd8));
        for (size_t k = 0; k < ge.cells.size(); k++) { float x, y; xy((int)k, x, y); g.circle(x, y, 19.5f, rgb(0xe4d6b0)); }
        for (size_t k = 0; k < ge.cells.size(); k++) { float x, y; xy((int)k, x, y); int tint = -1; for (int i = 0; i < 6; i++) if (inCorner(i, ge.cells[k][0], ge.cells[k][1], ge.cells[k][2])) tint = i; g.circle(x, y, 16, tint >= 0 ? rgb(COL[tint], 75) : rgb(0x7a5a34)); g.ring(x, y, 16, 1.5f, rgb(0x5a3f20)); }
        // casillas enfocables
        std::vector<int> movable; if (mine) for (size_t k = 0; k < board.size(); k++) if (board[k] == turn && !reach(board, (int)k).empty()) movable.push_back((int)k);
        std::string banner = over ? "¡Ganó " + (winner >= 0 ? pl[winner].name : std::string("nadie")) + "!" : !pl[turn].ai ? pl[turn].name + (sel < 0 ? ": elige una ficha" : ": elige adónde moverla (B cancela)") : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        int firstId = -1;
        for (size_t k = 0; k < board.size(); k++) { float x, y; xy((int)k, x, y); int v = board[k]; float lift = 0;
            bool isMov = mine && sel < 0 && std::find(movable.begin(), movable.end(), (int)k) != movable.end(), isDest = mine && sel >= 0 && reachSel.count((int)k), isSel = mine && sel == (int)k;
            if (anim > 0 && (int)k == animTo && v >= 0) { float t = 1 - anim / animDur, f = t * (animPath.size() - 1); int i = std::min((int)animPath.size() - 2, (int)f); float ax, ay, bx, by; xy(animPath[i], ax, ay); xy(animPath[i + 1], bx, by); float u = f - i; x = ax + (bx - ax) * u; y = ay + (by - ay) * u; lift = std::sin(u * 3.14159f) * 9; }
            if (isDest) { g.circle(x, y, 9, rgb(0xffffff, 220)); g.ring(x, y, 9, 2, rgb(0x3a2f1a)); }
            if (v >= 0) { int seat = seats[v]; g.circle(x, y + 13, 10, rgb(0, 60)); g.circle(x, y - lift, 15.5f, rgb(DARKC[seat])); g.circle(x, y - lift, 13.5f, rgb(COL[seat])); g.circle(x - 5, y - lift - 6, 5, rgb(0xffffff, 130)); if (isSel) g.ring(x, y, 19, 3, rgb(0xffffff)); else if (isMov) g.ring(x, y, 18, 2, rgb(0xffffff, 190)); }
            if (isMov || isDest || isSel) { uint32_t id = hid("cc", (int)k); Rect hr{ x - 19, y - 19, 38, 38 }; bool hit = ui.hit(id, hr, true); if (ui.focused(id)) ui.drawFocus(hr, 19); if (isMov && firstId < 0) firstId = (int)k;
                if (hit) { if (isSel) { sel = -1; reachSel.clear(); } else if (isDest) { int f = sel; sel = -1; if (move(turn, f, (int)k)) { reachSel.clear(); return; } } else { sel = (int)k; reachSel = reach(board, sel); int ap = ge.apex[(seats[turn] + 3) % 6], bd = -1, bdist = 99; for (auto& kv : reachSel) { int dd = ge.dist(kv.first, ap); if (dd < bdist) { bdist = dd; bd = kv.first; } } if (bd >= 0) ui.forceFocus(hid("cc", bd)); } } } }
        if (firstId >= 0 && sel < 0) ui.setFocusIfNone(hid("cc", firstId));
        for (int p = 0; p < N; p++) { Rect r{ 960, 70.f + p * 58, 280, 52 }; bool on = p == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.circle(r.x + 22, r.cy(), 12, rgb(COL[seats[p]])); g.text(pl[p].emoji + " " + pl[p].name, r.x + 42, r.y + 18, 15, on ? pal::gold : pal::white, F_BOLD, AL_L, 230); g.text("en la meta " + std::to_string(inGoal(p)) + "/10", r.x + 42, r.y + 37, 13, rgb(0xd8b46a), F_BODY, AL_L, 230); }
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir / mover" }, { "B", "Cancelar" }, { "TOUCH", "Tocar" } }; }
    std::string help() const override { return "Cada jugador tiene 10 fichas en una punta de la estrella y gana quien primero las lleva todas a la punta opuesta. En tu turno mueves UNA ficha: un paso a una casilla vecina libre, o saltando sobre una ficha (tuya o ajena) a la casilla libre de atrás, y puedes encadenar varios saltos seguidos. Elige la ficha con A y luego el destino."; }
};
}
Game* makeDamasChinas() { return new Damas(); }
