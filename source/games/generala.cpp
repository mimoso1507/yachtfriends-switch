// Generala argentina: 5 dados, 3 tiradas por turno, 10 categorías de puntos fijos, 2 a 8 jugadores.
// IA igual a la de PC: evaluador de esperanza (una y dos tiradas hacia adelante) con errores según el nivel.
#include "dice.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
const double BASE[10] = { 2.7, 5.4, 8.1, 10.8, 13.5, 16.2, 9, 10, 9, 5 };
const int FIRST[10] = { 0, 0, 0, 0, 0, 0, 25, 35, 45, 100 }, PLAIN[10] = { 0, 0, 0, 0, 0, 0, 20, 30, 40, 50 };
const char* const CATN[10] = { "Unos", "Doses", "Treses", "Cuatros", "Cincos", "Seises", "Escalera", "Full", "Póker", "Generala" };
typedef std::array<int, 7> Cnt;
Cnt counts(const std::array<int, 5>& d) { Cnt c{}; for (int v : d) c[v]++; return c; }
int catScore(const Cnt& c, int cat) {
    if (cat < 6) return c[cat + 1] * (cat + 1);
    int mx = 0, twos = 0, threes = 0; for (int v = 1; v <= 6; v++) { mx = std::max(mx, c[v]); if (c[v] == 2) twos++; if (c[v] == 3) threes++; }
    if (cat == 6) return (c[2] && c[3] && c[4] && c[5] && ((c[1] && !c[6]) || (c[6] && !c[1])) && mx == 1) ? PLAIN[6] : 0;
    if (cat == 7) return (twos == 1 && threes == 1) ? PLAIN[7] : 0;
    if (cat == 8) return mx == 4 ? PLAIN[8] : 0;
    return mx == 5 ? PLAIN[9] : 0;
}
const int P6[6] = { 1, 6, 36, 216, 1296, 7776 };
int codeOf(const std::vector<int>& d) { int c = 0; for (int v : d) c += P6[v - 1]; return c; }
Cnt decode(int code) { Cnt c{}; for (int v = 1; v <= 6; v++) { c[v] = code % 6; code /= 6; } return c; }
std::vector<int>& ptsOf(int code) { static std::unordered_map<int, std::vector<int>> M; auto it = M.find(code); if (it != M.end()) return it->second; Cnt c = decode(code); std::vector<int> p; for (int k = 0; k < 10; k++) p.push_back(catScore(c, k)); return M[code] = p; }
struct MSe { int code; double p; };
std::vector<MSe> MS[6]; std::vector<int> ALLCODES; bool msBuilt = false;
void buildMS() {
    if (msBuilt) return; msBuilt = true; const double fact[6] = { 1, 1, 2, 6, 24, 120 };
    for (int k = 0; k <= 5; k++) {
        std::vector<int> cur;
        std::function<void(int, int)> rec = [&](int pos, int mn) {
            if (pos == k) { int cc[7] = {}; for (int x : cur) cc[x]++; double w = fact[k]; for (int x = 1; x <= 6; x++) w /= fact[cc[x]]; MS[k].push_back({ codeOf(cur), w / std::pow(6.0, k) }); return; }
            for (int v = mn; v <= 6; v++) { cur.push_back(v); rec(pos + 1, v); cur.pop_back(); }
        };
        rec(0, 1);
    }
    for (auto& m : MS[5]) ALLCODES.push_back(m.code);
}
struct Eval {
    std::vector<int> open; std::unordered_map<int, double> memo;
    double stopV(int code) { auto& p = ptsOf(code); double b = -99; for (int k : open) b = std::max(b, p[k] - BASE[k]); return b; }
    template <class F> double EVkept(int kc, int r, F fn) { double e = 0; for (auto& m : MS[r]) e += m.p * fn(kc + m.code); return e; }
    double V1(int code) {
        auto it = memo.find(code); if (it != memo.end()) return it->second;
        double best = stopV(code); Cnt c = decode(code);
        std::function<void(int, int, int)> rec = [&](int v, int kcode, int n) {
            if (v > 6) { if (n == 5) return; double e = EVkept(kcode, 5 - n, [this](int x) { return stopV(x); }); best = std::max(best, e); return; }
            for (int x = 0; x <= c[v]; x++) rec(v + 1, kcode + x * P6[v - 1], n + x);
        };
        rec(1, 0, 0); return memo[code] = best;
    }
};
Eval& evalFor(const std::array<int, 10>& sheet) {
    static std::unordered_map<int, Eval> E; int mk = 0; for (int k = 0; k < 10; k++) if (sheet[k] < 0) mk |= 1 << k;
    auto it = E.find(mk); if (it != E.end()) return it->second; Eval e; for (int k = 0; k < 10; k++) if (sheet[k] < 0) e.open.push_back(k); return E[mk] = e;
}
struct Sc { int c; int pts; double v; };
std::vector<Sc> allScores(const std::array<int, 5>& dice, const std::array<int, 10>& sheet, bool servida) {
    Cnt c = counts(dice); std::vector<Sc> out;
    for (int k = 0; k < 10; k++) { if (sheet[k] >= 0) continue; int pts = catScore(c, k); if (pts > 0 && servida && FIRST[k]) pts = FIRST[k]; out.push_back({ k, pts, pts - BASE[k] }); }
    std::sort(out.begin(), out.end(), [](const Sc& a, const Sc& b) { return a.v > b.v; }); return out;
}
int pickScore(const std::vector<Sc>& sc, int lv) {
    const float ERS[4] = { 0, 0.3f, 0.08f, 0 }; if (sc.empty()) return -1;
    if (rnd01() < ERS[lv] && sc.size() > 1) return sc[1 + rndInt((int)std::min<size_t>(lv == 1 ? 3 : 1, sc.size() - 1))].c;
    return sc[0].c;
}
struct Dec { int score; std::array<bool, 5> hold; };   // score >= 0: anotar; si no, guardar "hold"
Dec decide(const std::array<int, 5>& dice, const std::array<int, 10>& sheet, int left, int lv) {
    buildMS(); bool servida = true; for (int v : sheet) if (v >= 0) servida = false; servida = servida && left == 2;
    if (left <= 0) return { pickScore(allScores(dice, sheet, servida), lv), {} };
    Eval& ev = evalFor(sheet); const float ERR[4] = { 0, 0.4f, 0.12f, 0.01f };
    auto sc = allScores(dice, sheet, servida); double stopVal = sc.empty() ? -99 : sc[0].v;
    struct O { std::array<bool, 5> hold; double v; }; std::vector<O> options; std::vector<int> seen;
    for (int m = 0; m < 31; m++) {
        std::vector<int> kept; std::array<bool, 5> hold{}; for (int i = 0; i < 5; i++) { bool on = m & (1 << i); hold[i] = on; if (on) kept.push_back(dice[i]); }
        int kc = codeOf(kept); if (std::find(seen.begin(), seen.end(), kc) != seen.end()) continue; seen.push_back(kc);
        double v = left >= 2 ? ev.EVkept(kc, 5 - (int)kept.size(), [&ev](int x) { return ev.V1(x); }) : ev.EVkept(kc, 5 - (int)kept.size(), [&ev](int x) { return ev.stopV(x); });
        options.push_back({ hold, v });
    }
    std::sort(options.begin(), options.end(), [](const O& a, const O& b) { return a.v > b.v; });
    O choice = options[0];
    if (rnd01() < ERR[lv] && options.size() > 1) choice = lv == 1 ? options[rndInt((int)std::min<size_t>(8, options.size()))] : options[1 + rndInt((int)std::min<size_t>(2, options.size() - 1))];
    if (stopVal >= choice.v - 1e-9) return { pickScore(sc, lv), {} };
    return { -1, choice.hold };
}

struct Generala : Game {
    int N = 0, turn = 0, rollsLeft = 3; std::vector<std::array<int, 10>> sheets; std::vector<bool> first, active; std::array<int, 5> dice{ 1, 1, 1, 1, 1 }; std::array<bool, 5> held{}; std::array<int, 5> shown{ 1, 1, 1, 1, 1 };
    bool rolled = false, over = false; float rollAnim = 0, endT = 0, aiT = 0, aiWait = 1; std::vector<std::pair<int, int>> plan; int resigned = -1;
    void start() override { N = (int)pl.size(); sheets.assign(N, {}); for (auto& s : sheets) s.fill(-1); first.assign(N, true); active.assign(N, true); startTurn(); }
    void startTurn() { rollsLeft = 3; held.fill(false); dice.fill(1); shown = dice; rolled = false; plan.clear(); aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = (lv == 1 ? 1.6f : lv == 3 ? 0.6f : 1.f) * (0.8f + rnd01() * 0.7f); }
    bool openFor(int p) const { for (int v : sheets[p]) if (v < 0) return true; return false; }
    int total(int p) const { int t = 0; for (int v : sheets[p]) if (v > 0) t += v; return t; }
    int turnPlayer() const override { return over ? -1 : turn; }
    void roll() { if (over || rollsLeft <= 0 || rollAnim > 0) return; for (int i = 0; i < 5; i++) if (!(held[i] && rolled)) dice[i] = 1 + rndInt(6); rollsLeft--; rolled = true; rollAnim = 0.45f; app->audio.play("dice"); }
    void hold(int i) { if (over || !rolled || rollsLeft <= 0 || rollAnim > 0) return; held[i] = !held[i]; app->audio.play("click"); }
    void score(int c) {
        if (over || !rolled || rollAnim > 0 || sheets[turn][c] >= 0) return; int val = catScore(counts(dice), c);
        if (val > 0 && FIRST[c] && first[turn] && rollsLeft == 2) val = FIRST[c]; sheets[turn][c] = val; first[turn] = false; app->audio.play(val > 0 ? "place" : "lose"); advance();
    }
    void advance() { for (int s = 1; s <= N; s++) { int q = (turn + s) % N; if (active[q] && openFor(q)) { turn = q; startTurn(); return; } } over = true; }
    void resign(int p) override { active[p] = false; resigned = p; int a = 0; for (bool b : active) if (b) a++; if (a < 2) { over = true; return; } if (turn == p) advance(); }
    void update(float dt, Input& in, UI&) override {
        rollAnim = std::max(0.f, rollAnim - dt); if (rollAnim > 0) { for (int i = 0; i < 5; i++) shown[i] = (held[i] && rollsLeft < 2) ? dice[i] : 1 + rndInt(6); } else shown = dice;
        if (over) { endT += dt; if (endT > 1.4f && !finished) {
            int best = -1; for (int i = 0; i < N; i++) if (active[i]) best = std::max(best, total(i)); int w = 0; for (int i = 0; i < N; i++) if (active[i] && total(i) == best) w++;
            std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ total(i), !active[i] ? 'x' : total(i) == best ? (w > 1 ? 'd' : 'w') : 'l' }); finish(r, "Mejor puntaje: " + std::to_string(best)); } return; }
        if (!pl[turn].ai) { Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (p && p->rollDown() && rollsLeft > 0 && rollAnim <= 0 && !over) roll(); return; }
        if (rollAnim > 0) return;
        aiT += dt; if (aiT < aiWait) return; aiT = 0; int lv = std::max(1, std::min(3, pl[turn].level)); aiWait = (lv == 1 ? 1.6f : lv == 3 ? 0.6f : 1.f) * (0.5f + rnd01() * 0.5f);
        if (!rolled) { roll(); return; }
        if (plan.empty()) {
            Dec d = decide(dice, sheets[turn], rollsLeft, lv);
            if (d.score >= 0) plan.push_back({ 1, d.score }); else if (d.score == -1 && rollsLeft <= 0) { int c = 0; while (c < 10 && sheets[turn][c] >= 0) c++; plan.push_back({ 1, c }); }
            else { for (int i = 0; i < 5; i++) if (held[i] != d.hold[i]) plan.push_back({ 0, i }); plan.push_back({ 2, 0 }); }
        }
        auto s = plan[0]; plan.erase(plan.begin()); if (s.first == 0) hold(s.second); else if (s.first == 1) score(s.second); else roll();
    }
    static bool sideKey(UI& ui) { if (ui.activePad < 0) return false; const Pad& p = ui.in->pads[ui.activePad]; return p.sideDown(); }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai; bool fc = false; uint32_t fcell = 0;
        Rect tray{ 280, 70, 470, 290 }; g.rrect({ tray.x, tray.y + 6, tray.w, tray.h }, 22, rgb(0, 110)); g.rrectGrad(tray, 22, rgb(0x6b4224), rgb(0x40240f)); g.rrectGrad(tray.inset(14), 14, rgb(0x2f8a4a), rgb(0x1d5a30));
        std::string banner = over ? "Fin de la partida" : mine ? pl[turn].name + ": ¡tu turno!" : "Turno de " + pl[turn].name;
        Rect bn{ 280, 18, 470, 42 }; g.rrect(bn, 10, pal::plateEdge); g.rrectGrad(bn.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(banner, bn.cx(), bn.cy(), 19, mine ? pal::gold : pal::white, F_BOLD, AL_C, 440);
        for (int i = 0; i < 5; i++) {
            float sz = 74, x = tray.x + 34 + i * 82, y = tray.y + 108 - (held[i] && rolled ? 26 : 0); Rect r{ x, y, sz, sz }; uint32_t id = hid("gd", i);
            bool hit = ui.hit(id, { x - 4, tray.y + 60, sz + 8, 170 }, mine && rolled && rollsLeft > 0 && rollAnim <= 0); if (hit) hold(i);
            dice::draw(g, r, rolled ? shown[i] : i + 1, held[i] && rolled); if (held[i] && rolled) g.text("GUARDADO", r.cx(), r.y + sz + 18, 11, pal::gold, F_TITLE, AL_C);
            if (ui.focused(id)) ui.drawFocus(r, 14);
        }
        for (int k = 0; k < 3; k++) g.circle(tray.cx() - 24 + k * 24, tray.y + tray.h + 20, 7, k < rollsLeft ? pal::gold : rgb(0x3a2a1a));
        std::string rl = rolled ? "Tirar de nuevo (" + std::to_string(rollsLeft) + ")" : "Tirar";
        if (ui.button(rl, { 330, 410, 370, 60 }, 1, mine && rollsLeft > 0 && rollAnim <= 0, "🎲")) roll();
        if (mine) ui.setFocusIfNone(hid(rl + "🎲", (int)(330 * 7 + 410)));
        if (mine && rolled) { g.rrect({ 280, 484, 470, 30 }, 15, rgb(0x120c08, 190)); } g.text(mine && rolled ? "Tocá los dados para guardarlos y elegí una categoría en el bloc" : "", 515, 499, 14, rgb(0xf5e6c8), F_BODY, AL_C, 450);
        Rect pad{ 780, 18, 480, 640 }; g.rrect({ pad.x + 4, pad.y + 8, pad.w, pad.h }, 6, rgb(0, 100)); g.rrectGrad(pad, 6, rgb(0xfff7c2), rgb(0xf6e9a4));
        for (int k = 0; k < 6; k++) g.circle(pad.x + 40 + k * 80, pad.y + 8, 6, rgb(0x6b5a40));
        float catW = 150, colsW = pad.w - catW - 20, cw = colsW / N, rowH = 45, y0 = pad.y + 64;
        g.text("Categoría", pad.x + 14, pad.y + 40, 15, rgb(0x4a3a2a), F_BOLD);
        for (int p = 0; p < N; p++) { float x = pad.x + catW + p * cw; g.emoji(pl[p].emoji, x + cw / 2 - (N <= 4 ? 30 : 0), pad.y + 40, 20, active[p] ? 255 : 90); if (N <= 4) g.text(pl[p].name, x + cw / 2 - 16, pad.y + 40, 13, p == turn && !over ? rgb(0xa0281a) : rgb(0x3a2a1a), F_BOLD, AL_L, cw / 2 + 6); }
        Cnt cc = counts(dice); bool first1 = first[turn];
        for (int c = 0; c <= 10; c++) {
            float y = y0 + c * rowH; g.line(pad.x + 10, y + rowH, pad.x + pad.w - 10, y + rowH, 1.5f, rgb(0x7fa6c8, 160));
            g.text(c < 10 ? CATN[c] : "TOTAL", pad.x + 16, y + rowH / 2, c < 10 ? 18 : 19, rgb(0x2a2a50), c < 10 ? F_BODY : F_BOLD, AL_L, catW - 12);
            for (int p = 0; p < N; p++) {
                Rect cell{ pad.x + catW + p * cw, y + 3, cw, rowH - 6 };
                if (c == 10) { g.text(std::to_string(total(p)), cell.cx(), y + rowH / 2, 19, rgb(0xa0281a), F_BOLD, AL_C); continue; }
                int v = sheets[p][c]; bool tap = v < 0 && p == turn && mine && rolled && rollAnim <= 0; uint32_t id = hid("gc", c * 16 + p);
                if (tap) { bool hit = ui.hit(id, cell); int pts = catScore(cc, c); if (pts > 0 && FIRST[c] && first1 && rollsLeft == 2) pts = FIRST[c];
                    g.rrect(cell.inset(2), 6, rgb(0xffd54f, 150)); g.text(std::to_string(pts), cell.cx(), cell.cy(), 17, rgb(0x7a4a00), F_BOLD, AL_C); if (!fcell) fcell = id; if (ui.focused(id)) { ui.drawFocus(cell, 6); fc = true; } if (hit) { score(c); return; } }
                else g.text(v < 0 ? "·" : std::to_string(v), cell.cx(), cell.cy(), 17, rgb(0x1a1a40), F_BODY, AL_C);
            }
        }
        if (mine && sideKey(ui)) { ui.padNav = true; if (fc) ui.forceFocus(hid(rl + "🎲", (int)(330 * 7 + 410))); else if (fcell) ui.forceFocus(fcell); app->audio.play("click"); }
    }
    // (L/R) cambia entre los dados y el bloc
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Guardar dado / anotar" }, { "LS", "Cambiar de lado: dados / bloc" }, { "ROLL", "Tirar los dados" } }; }
    std::string help() const override { return "Tirá los 5 dados hasta 3 veces por turno. Entre tiradas guardá los que quieras (A). Después anotá el resultado en una categoría libre del bloc. Escalera 20 (25 servida), Full 30 (35), Póker 40 (45), Generala 50 (100 si sale a la primera tirada de tu primera jugada). Gana el mayor total."; }
};
}
Game* makeGenerala() { return new Generala(); }
