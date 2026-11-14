// Puntos y cajas (2-4 jugadores). IA: cierra cajas, evita regalar la tercera raya y si no queda otra regala la cadena más corta.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
struct Bd { int gw, gh; std::vector<char> h, v; };
struct Ln { bool horiz; int r, c; };
int boxCount(const Bd& B, int bx, int by) { int gw = B.gw, w1 = gw + 1; return (B.h[by * gw + bx] ? 1 : 0) + (B.h[(by + 1) * gw + bx] ? 1 : 0) + (B.v[by * w1 + bx] ? 1 : 0) + (B.v[by * w1 + bx + 1] ? 1 : 0); }
std::vector<std::pair<int, int>> adj(const Bd& B, const Ln& l) {
    std::vector<std::pair<int, int>> o;
    if (l.horiz) { if (l.r > 0) o.push_back({ l.c, l.r - 1 }); if (l.r < B.gh) o.push_back({ l.c, l.r }); }
    else { if (l.c > 0) o.push_back({ l.c - 1, l.r }); if (l.c < B.gw) o.push_back({ l.c, l.r }); }
    return o;
}
std::vector<Ln> freeLines(const Bd& B) {
    std::vector<Ln> o;
    for (int i = 0; i < (int)B.h.size(); i++) if (!B.h[i]) o.push_back({ true, i / B.gw, i % B.gw });
    for (int i = 0; i < (int)B.v.size(); i++) if (!B.v[i]) o.push_back({ false, i / (B.gw + 1), i % (B.gw + 1) });
    return o;
}
void setLine(Bd& B, const Ln& l, char val) { if (l.horiz) B.h[l.r * B.gw + l.c] = val; else B.v[l.r * (B.gw + 1) + l.c] = val; }
int gain(const Bd& B, const Ln& l) { int n = 0; for (auto& b : adj(B, l)) if (boxCount(B, b.first, b.second) == 3) n++; return n; }
int giveaway(const Bd& B, const Ln& l) {
    Bd C = B; setLine(C, l, 1); int n = 0, guard = 0;
    for (;;) {
        bool found = false; Ln f{};
        for (int by = 0; by < C.gh && !found; by++) for (int bx = 0; bx < C.gw && !found; bx++) {
            if (boxCount(C, bx, by) != 3) continue; int gw = C.gw, w1 = gw + 1; found = true;
            if (!C.h[by * gw + bx]) f = { true, by, bx }; else if (!C.h[(by + 1) * gw + bx]) f = { true, by + 1, bx };
            else if (!C.v[by * w1 + bx]) f = { false, by, bx }; else f = { false, by, bx + 1 };
        }
        if (!found || guard++ > 200) break;
        n += std::max(1, gain(C, f)); setLine(C, f, 1);
    }
    return n;
}
struct Lvl { float err, miss, blind, d0, d1; };
const Lvl LVL[4] = { {}, { 0.3f, 0.3f, 0.4f, 0.9f, 1.3f }, { 0.07f, 0, 0, 0.6f, 0.7f }, { 0.01f, 0, 0, 0.5f, 0.6f } };
bool choose(const Bd& B, int lv, Ln& out) {
    const Lvl& P = LVL[std::max(1, std::min(3, lv))];
    auto lines = freeLines(B); if (lines.empty()) return false;
    auto pick = [](const std::vector<Ln>& a) { return a[rndInt((int)a.size())]; };
    if (rnd01() < P.err) { out = pick(lines); return true; }
    std::vector<Ln> takes; if (rnd01() >= P.miss) for (auto& l : lines) if (gain(B, l) > 0) takes.push_back(l);
    if (!takes.empty()) { int mx = 0; for (auto& l : takes) mx = std::max(mx, gain(B, l)); std::vector<Ln> m; for (auto& l : takes) if (gain(B, l) == mx) m.push_back(l); out = pick(m); return true; }
    if (rnd01() < P.blind) { out = pick(lines); return true; }
    std::vector<Ln> safe; for (auto& l : lines) { bool ok = true; for (auto& b : adj(B, l)) if (boxCount(B, b.first, b.second) >= 2) ok = false; if (ok) safe.push_back(l); }
    if (!safe.empty()) { out = pick(safe); return true; }
    int mn = 1 << 30; std::vector<std::pair<Ln, int>> cost; for (auto& l : lines) { int g = giveaway(B, l); cost.push_back({ l, g }); mn = std::min(mn, g); }
    std::vector<Ln> m; for (auto& x : cost) if (x.second == mn) m.push_back(x.first); out = pick(m); return true;
}

struct Dots : Game {
    Bd B; std::vector<int> boxes; int n = 2, turn = 0, scores[4] = {}, lastR = -1, lastC = -1; bool lastH = true, over = false; float aiT = 0, aiWait = 0.8f, endT = 0; int resigned = -1;
    void start() override {
        n = (int)pl.size(); int s = opts.get("size", 1); static const int GW[] = { 4, 5, 6, 7 }, GH[] = { 3, 4, 5, 8 };
        B.gw = GW[std::max(0, std::min(3, s))]; B.gh = GH[std::max(0, std::min(3, s))];
        B.h.assign((B.gh + 1) * B.gw, 0); B.v.assign(B.gh * (B.gw + 1), 0); boxes.assign(B.gw * B.gh, -1); resetAi();
    }
    void resetAi() { aiT = 0; const Lvl& P = LVL[std::max(1, std::min(3, pl[turn].level))]; aiWait = P.d0 + rnd01() * P.d1; }
    int turnPlayer() const override { return over ? -1 : turn; }
    void move(const Ln& l) {
        if (over) return; char& cell = l.horiz ? B.h[l.r * B.gw + l.c] : B.v[l.r * (B.gw + 1) + l.c]; if (cell) return;
        cell = 1; lastH = l.horiz; lastR = l.r; lastC = l.c; int gained = 0;
        for (int by = 0; by < B.gh; by++) for (int bx = 0; bx < B.gw; bx++) if (boxes[by * B.gw + bx] < 0 && boxCount(B, bx, by) == 4) { boxes[by * B.gw + bx] = turn; scores[turn]++; gained++; }
        app->audio.play(gained ? "win" : "place");
        bool all = true; for (int v : boxes) if (v < 0) all = false;
        if (all) over = true; else if (!gained) turn = (turn + 1) % n;
        resetAi();
    }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input&, UI&) override {
        if (over) {
            endT += dt;
            if (endT > 1.4f && !finished) {
                int best = 0; for (int i = 0; i < n; i++) best = std::max(best, scores[i]); int winners = 0; for (int i = 0; i < n; i++) if (scores[i] == best) winners++;
                std::vector<PResult> r; std::string sm = "Cajas:"; for (int i = 0; i < n; i++) { r.push_back({ scores[i], scores[i] == best ? (winners > 1 ? 'd' : 'w') : 'l' }); sm += " " + std::to_string(scores[i]); }
                if (resigned >= 0) { for (int i = 0; i < n; i++) if (r[i].r == 'w') r[i].r = 'l'; r[resigned].r = 'x'; int o = (resigned + 1) % n; r[o].r = 'w'; }
                finish(r, sm);
            }
            return;
        }
        if (!pl[turn].ai) return;
        aiT += dt; if (aiT < aiWait) return;
        Ln l; if (choose(B, pl[turn].level, l)) move(l);
    }
    void draw(Gfx& g, UI& ui) override {
        float cs = std::min(860.f / B.gw, 540.f / B.gh); float W = cs * B.gw, H = cs * B.gh; float x0 = 770 - W / 2, y0 = 340 - H / 2 + 6;
        g.rrect({ x0 - 36, y0 - 36, W + 72, H + 72 }, 20, rgb(0, 100)); g.rrectGrad({ x0 - 40, y0 - 42, W + 80, H + 80 }, 20, rgb(0xfff4d6), rgb(0xecd6a0)); g.rrectLine({ x0 - 40, y0 - 42, W + 80, H + 80 }, 20, 3, rgb(0x8a5a2c));
        for (int by = 0; by < B.gh; by++) for (int bx = 0; bx < B.gw; bx++) { int o = boxes[by * B.gw + bx]; if (o >= 0) { Rect r{ x0 + bx * cs + 4, y0 + by * cs + 4, cs - 8, cs - 8 }; g.rrect(r, 8, withA(PCOL[o % 8], 150)); g.text(pl[o].emoji, r.cx(), r.cy(), cs * 0.4f, pal::white, F_BODY, AL_C); } }
        bool mine = !over && !pl[turn].ai; float th = 28; int first = -1;
        auto lineUi = [&](bool horiz, int r, int c) {
            bool on = horiz ? B.h[r * B.gw + c] : B.v[r * (B.gw + 1) + c];
            Rect hr = horiz ? Rect{ x0 + c * cs + 12, y0 + r * cs - th / 2, cs - 24, th } : Rect{ x0 + c * cs - th / 2, y0 + r * cs + 12, th, cs - 24 };
            uint32_t id = hid(horiz ? "dh" : "dv", r * 64 + c); if (!on && first < 0) first = r * 64 + c + (horiz ? 0 : 100000);
            bool hit = ui.hit(id, hr, mine && !on);
            float x1 = horiz ? x0 + c * cs + 10 : x0 + c * cs, y1 = horiz ? y0 + r * cs : y0 + r * cs + 10, x2 = horiz ? x0 + (c + 1) * cs - 10 : x0 + c * cs, y2 = horiz ? y0 + r * cs : y0 + (r + 1) * cs - 10;
            bool isLast = horiz == lastH && r == lastR && c == lastC;
            if (on) g.line(x1, y1, x2, y2, 8, isLast ? rgb(0xd9483b) : rgb(0x3a2414)); else g.line(x1, y1, x2, y2, 3, rgb(0xb98a4a, 90));
            if (ui.focused(id)) { g.line(x1, y1, x2, y2, 12, rgb(0xffe27a, 200)); }
            if (hit) move({ horiz, r, c });
        };
        for (int r = 0; r <= B.gh; r++) for (int c = 0; c < B.gw; c++) lineUi(true, r, c);
        for (int r = 0; r < B.gh; r++) for (int c = 0; c <= B.gw; c++) lineUi(false, r, c);
        for (int r = 0; r <= B.gh; r++) for (int c = 0; c <= B.gw; c++) { g.circle(x0 + c * cs, y0 + r * cs, 9, rgb(0x3a2414)); }
        if (mine && first >= 0) { bool hz = first < 100000; int k = hz ? first : first - 100000; ui.setFocusIfNone(hid(hz ? "dh" : "dv", k)); }
        // marcador
        float y = 70 + 46 * n + 54; Rect sc{ 16, y, 230, 26.f + 28 * n }; if (sc.y + sc.h > 440) sc.y = 440 - sc.h;
        g.rrect(sc, 12, rgb(0x120c08, 200)); g.rrectLine(sc, 12, 1.5f, rgb(0xd8b46a, 110));
        for (int i = 0; i < n; i++) { float yy = sc.y + 22 + i * 28; g.circle(sc.x + 18, yy, 7, PCOL[i % 8]); g.text(pl[i].name, sc.x + 34, yy, 15, i == turn && !over ? pal::gold : pal::white, F_BOLD, AL_L, 130); g.text(std::to_string(scores[i]) + " cajas", sc.x + sc.w - 12, yy, 15, pal::gold, F_BOLD, AL_R); }
        std::string msg = over ? "Fin de la partida" : pl[turn].ai ? "Piensa " + pl[turn].name + "…" : "Turno de " + pl[turn].name;
        g.textShadow(msg, 770, 20, 22, pal::gold, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir raya" }, { "A", "Trazar la raya" } }; }
    std::string help() const override { return "Por turnos, cada jugador traza una raya entre dos puntos. Quien cierra el cuarto lado de una caja se la queda y juega otra vez. Evitá regalar la tercera raya de una caja. Gana quien cierre más cajas."; }
};
}
Game* makePuntosCajas() { return new Dots(); }
