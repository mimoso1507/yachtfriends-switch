// Mancala (Kalah). IA alfa-beta de profundidad limitada + algo de azar, igual que la versión de PC.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const int STORE[2] = { 6, 13 };
const int PITS[2][6] = { { 0, 1, 2, 3, 4, 5 }, { 7, 8, 9, 10, 11, 12 } };
typedef std::array<int, 14> Bd;
struct Sim { Bd b; int next; bool over; };
bool hasSeeds(const Bd& b, int q) { for (int k : PITS[q]) if (b[k] > 0) return true; return false; }
Sim sim(const Bd& board, int p, int pit) {
    Bd b = board; int seeds = b[pit]; b[pit] = 0; int i = pit;
    while (seeds > 0) { i = (i + 1) % 14; if (i == STORE[1 - p]) continue; b[i]++; seeds--; }
    bool extra = false;
    if (i == STORE[p]) extra = true;
    else if (std::find(PITS[p], PITS[p] + 6, i) != PITS[p] + 6 && b[i] == 1) { int o = 12 - i; if (b[o] > 0) { b[STORE[p]] += b[o] + 1; b[o] = 0; b[i] = 0; } }
    bool over = !hasSeeds(b, 0) || !hasSeeds(b, 1);
    if (over) for (int q = 0; q < 2; q++) for (int k : PITS[q]) { b[STORE[q]] += b[k]; b[k] = 0; }
    return { b, extra ? p : 1 - p, over };
}
std::vector<int> legal(const Bd& b, int p) { std::vector<int> r; for (int k : PITS[p]) if (b[k] > 0) r.push_back(k); return r; }
double evalB(const Bd& b, int me) { double a = 0, c = 0; for (int k : PITS[me]) a += b[k]; for (int k : PITS[1 - me]) c += b[k]; return (b[STORE[me]] - b[STORE[1 - me]]) + 0.05 * (a - c); }
double finalScore(const Bd& b, int me) { int d = b[STORE[me]] - b[STORE[1 - me]]; return d > 0 ? 100 + d : d < 0 ? -100 + d : 0; }
double search(const Bd& b, int p, int depth, double alpha, double beta, int me, int& budget) {
    auto ms = legal(b, p);
    if (ms.empty() || depth == 0 || budget-- <= 0) return evalB(b, me);
    std::sort(ms.begin(), ms.end(), [p](int x, int y) { return p ? x < y : x > y; });
    if (p == me) {
        double v = -1e9;
        for (int m : ms) { Sim r = sim(b, p, m); double s = r.over ? finalScore(r.b, me) : search(r.b, r.next, depth - 1, alpha, beta, me, budget); v = std::max(v, s); alpha = std::max(alpha, v); if (alpha >= beta) break; }
        return v;
    }
    double v = 1e9;
    for (int m : ms) { Sim r = sim(b, p, m); double s = r.over ? finalScore(r.b, me) : search(r.b, r.next, depth - 1, alpha, beta, me, budget); v = std::min(v, s); beta = std::min(beta, v); if (alpha >= beta) break; }
    return v;
}
struct Lvl { float err; int depth, nodes; float d0, d1; };
const Lvl LVL[4] = { {}, { 0.42f, 2, 600, 0.9f, 1.3f }, { 0.12f, 5, 2500, 0.7f, 0.8f }, { 0.01f, 7, 3500, 0.5f, 0.6f } };
int choose(const Bd& board, int me, int lv) {
    const Lvl& P = LVL[std::max(1, std::min(3, lv))];
    auto ms = legal(board, me); if (ms.empty()) return -1;
    if (rnd01() < P.err) return ms[rndInt((int)ms.size())];
    double bs = -1e18; int bm = ms[0];
    for (int m : ms) { Sim r = sim(board, me, m); int budget = P.nodes; double s = (r.over ? finalScore(r.b, me) : search(r.b, r.next, P.depth - 1, -1e9, 1e9, me, budget)) + rnd01() * 0.01; if (s > bs) { bs = s; bm = m; } }
    return bm;
}

struct Mancala : Game {
    Bd b{}; int turn = 0, lastPit = -1, resigned = -1; bool over = false, extra = false; float aiT = 0, aiWait = 0.6f, endT = 0, flash = 0;
    void start() override { b.fill(4); b[6] = b[13] = 0; }
    int turnPlayer() const override { return over ? -1 : turn; }
    void move(int pit) {
        if (over || b[pit] == 0) return;
        Sim r = sim(b, turn, pit); extra = r.next == turn && !r.over;
        int before = b[STORE[turn]] + 0; b = r.b; lastPit = pit; flash = 0.5f; app->audio.play(b[STORE[turn]] > before + 1 ? "win" : "place");
        if (r.over) over = true; else turn = r.next;
        aiT = 0; if (!over) { const Lvl& P = LVL[std::max(1, std::min(3, pl[turn].level))]; aiWait = P.d0 + rnd01() * P.d1; }
    }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input&, UI&) override {
        flash = std::max(0.f, flash - dt);
        if (over) {
            endT += dt;
            if (endT > 1.4f && !finished) {
                int a = b[6], c = b[13]; int w = resigned >= 0 ? 1 - resigned : a > c ? 0 : c > a ? 1 : -1;
                std::vector<PResult> r = { { a, w == 0 ? 'w' : w < 0 ? 'd' : 'l' }, { c, w == 1 ? 'w' : w < 0 ? 'd' : 'l' } };
                if (resigned >= 0) r[resigned].r = 'x';
                finish(r, w < 0 ? "Empate " + std::to_string(a) + " a " + std::to_string(c) : "Ganó " + pl[w].name + " (" + std::to_string(std::max(a, c)) + " a " + std::to_string(std::min(a, c)) + ")");
            }
            return;
        }
        if (!pl[turn].ai) return;
        aiT += dt; if (aiT < aiWait) return;
        int i = choose(b, turn, pl[turn].level); if (i >= 0) move(i); else over = true;
    }
    void seeds(Gfx& g, Rect r, int n) {
        static const uint32_t C[] = { 0xe0584a, 0x4aa3e0, 0xf2c14a, 0x7ad06a, 0xb978e0, 0xf08a3c };
        for (int k = 0; k < n && k < 24; k++) {
            float a = k * 2.4f, rad = (k == 0 ? 0 : 0.14f + 0.2f * sqrtf((float)k / 24.f)) * std::min(r.w, r.h);
            float x = r.cx() + cosf(a) * rad, y = r.cy() + sinf(a) * rad * 0.9f, s = std::min(r.w, r.h) * 0.075f;
            g.circle(x + 1, y + 2, s, rgb(0, 100)); g.circle(x, y, s, rgb(C[k % 6])); g.circle(x - s * 0.3f, y - s * 0.3f, s * 0.35f, rgb(0xffffff, 120));
        }
    }
    void draw(Gfx& g, UI& ui) override {
        Rect B{ 276, 150, 980, 380 };
        g.rrect({ B.x + 5, B.y + 10, B.w, B.h }, 40, rgb(0, 110)); g.rrectGrad(B, 40, rgb(0x9a6a3c), rgb(0x5a3a1c)); g.rrectGrad(B.inset(10), 32, rgb(0x7a4a26), rgb(0x4a2c12));
        float pw = 104, gap = 10, x0 = B.x + 20 + 120 + 12;
        // almacenes: 13 (jugador 2) a la izquierda, 6 (jugador 1) a la derecha
        Rect st[2] = { { B.x + B.w - 20 - 120, B.y + 24, 120, B.h - 48 }, { B.x + 20, B.y + 24, 120, B.h - 48 } };
        for (int p = 0; p < 2; p++) {
            g.rrectGrad(st[p], 50, rgb(0x2a160a), rgb(0x3c2210)); seeds(g, { st[p].x, st[p].y + 40, st[p].w, st[p].h - 80 }, b[STORE[p]]);
            g.text(std::to_string(b[STORE[p]]), st[p].cx(), p == 0 ? st[p].y + st[p].h - 24 : st[p].y + 24, 26, pal::gold, F_TITLE, AL_C);
        }
        bool mine = !over && !pl[turn].ai; int firstOk = -1;
        for (int p = 0; p < 2; p++) for (int k = 0; k < 6; k++) {
            int pit = PITS[p][k]; float x = x0 + (p == 0 ? k : 5 - k) * (pw + gap), y = p == 0 ? B.y + B.h / 2 + 8 : B.y + 26;
            Rect r{ x, y, pw, B.h / 2 - 34 }; uint32_t id = hid("mc", pit);
            bool ok = mine && p == turn && b[pit] > 0; if (ok && firstOk < 0) firstOk = pit;
            bool hit = ui.hit(id, r, ok);
            g.rrectGrad(r, 44, rgb(0x2a160a), rgb(0x3c2210));
            if (pit == lastPit && flash > 0) g.rrectLine(r, 44, 4, rgb(0xffe27a, (uint8_t)(255 * flash * 2)));
            if (ok && !ui.padNav) g.rrectLine(r, 44, 2, rgb(0xffe27a, 150));
            seeds(g, r, b[pit]);
            g.text(std::to_string(b[pit]), r.cx(), p == 0 ? r.y + r.h - 14 : r.y + 14, 18, pal::cream, F_BOLD, AL_C);
            if (ui.focused(id)) ui.drawFocus(r, 44);
            if (hit) move(pit);
        }
        if (firstOk >= 0) ui.setFocusIfNone(hid("mc", firstOk));
        for (int p = 0; p < 2; p++) {
            float y = p == 0 ? B.y + B.h + 34 : B.y - 22; bool on = p == turn && !over;
            g.rrect({ B.cx() - 250, y - 16, 500, 32 }, 16, rgb(0x120c08, 200));
            g.text(pl[p].emoji + " " + pl[p].name + (p == 0 ? "  (abajo, almacén a la derecha)" : "  (arriba, almacén a la izquierda)"), B.cx(), y, 18, on ? pal::gold : pal::white, F_BOLD, AL_C);
        }
        std::string msg = over ? "Fin de la partida" : pl[turn].ai ? "Piensa " + pl[turn].name + "…" : extra && flash > 0 ? "¡Turno extra!" : "Turno de " + pl[turn].name;
        g.textShadow(msg, B.cx(), 96, 24, pal::gold, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "LR/STICK", "Elegir casilla" }, { "A", "Sembrar" } }; }
    std::string help() const override { return "Elegí una de tus casillas y sembrá sus semillas de a una, en sentido contrario a las agujas del reloj, incluyendo tu almacén pero no el del rival. Si la última semilla cae en tu almacén, jugás otra vez. Si cae en una casilla tuya vacía y enfrente hay semillas, te las llevás todas. Cuando un lado queda vacío, termina: gana quien tenga más en su almacén."; }
};
}
Game* makeMancala() { return new Mancala(); }
