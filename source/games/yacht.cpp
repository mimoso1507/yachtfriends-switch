// Yacht clásico (12 categorías, sin bonus; modalidad Triple x1/x2/x3). Reglas e IA iguales a la versión de PC.
#include "../app.h"
#include <algorithm>
#include <array>

namespace {
typedef std::array<int, 5> Dice;
int sumD(const Dice& d) { int s = 0; for (int v : d) s += v; return s; }
int cv(const Dice& d, int n) { int s = 0; for (int v : d) if (v == n) s += n; return s; }
bool hasAll(const Dice& d, std::initializer_list<int> seq) { for (int x : seq) if (std::find(d.begin(), d.end(), x) == d.end()) return false; return true; }
const char* CATN[12] = { "Unos", "Doses", "Treses", "Cuatros", "Cincos", "Seises", "4 de un tipo", "Full House", "Escalera de 4", "Escalera de 5", "Yacht", "Azar" };
int catScore(int c, const Dice& d) {
    int cnt[7] = {}; for (int v : d) cnt[v]++;
    if (c < 6) return cv(d, c + 1);
    int mx = 0, kinds = 0; for (int i = 1; i <= 6; i++) { mx = std::max(mx, cnt[i]); if (cnt[i]) kinds++; }
    switch (c) {
    case 6: return mx >= 4 ? sumD(d) : 0;
    case 7: return (kinds == 2 && mx == 3) ? sumD(d) : 0;
    case 8: return (hasAll(d, { 1, 2, 3, 4 }) || hasAll(d, { 2, 3, 4, 5 }) || hasAll(d, { 3, 4, 5, 6 })) ? 25 : 0;
    case 9: return (hasAll(d, { 1, 2, 3, 4, 5 }) || hasAll(d, { 2, 3, 4, 5, 6 })) ? 30 : 0;
    case 10: return mx == 5 ? 50 : 0;
    default: return sumD(d);
    }
}

// ---------------- IA: valor esperado exacto sobre los 252 multiconjuntos (igual que a/yacht.js)
const double BASE[12] = { 2.0, 4.1, 6.2, 8.3, 10.4, 12.5, 5, 7.5, 16, 8, 2.3, 22 };
typedef std::array<int, 6> Cnt;
int scoreC(const Cnt& c, int cat) {
    int sum = 0, mx = 0; for (int i = 0; i < 6; i++) { sum += c[i] * (i + 1); mx = std::max(mx, c[i]); }
    if (cat < 6) return c[cat] * (cat + 1);
    switch (cat) {
    case 6: return mx >= 4 ? sum : 0;
    case 7: { std::vector<int> v; for (int x : c) if (x) v.push_back(x); std::sort(v.begin(), v.end()); return v.size() == 2 && v[0] == 2 && v[1] == 3 ? sum : 0; }
    case 8: return ((c[0] && c[1] && c[2] && c[3]) || (c[1] && c[2] && c[3] && c[4]) || (c[2] && c[3] && c[4] && c[5])) ? 25 : 0;
    case 9: return ((c[0] && c[1] && c[2] && c[3] && c[4]) || (c[1] && c[2] && c[3] && c[4] && c[5])) ? 30 : 0;
    case 10: return mx == 5 ? 50 : 0;
    default: return sum;
    }
}
int enc(const Cnt& c) { return c[0] + 6 * c[1] + 36 * c[2] + 216 * c[3] + 1296 * c[4] + 7776 * c[5]; }
struct Trans { Cnt h; std::vector<int> outs; std::vector<double> ps; };
std::vector<Cnt> ST; std::vector<int> IDX; std::vector<std::array<int, 12>> SC; std::vector<std::vector<Trans>> TR;
void vectors(int total, std::vector<Cnt>& out) {
    Cnt cur{}; std::function<void(int, int)> rec = [&](int i, int left) { if (i == 5) { cur[5] = left; out.push_back(cur); return; } for (int x = 0; x <= left; x++) { cur[i] = x; rec(i + 1, left - x); } };
    rec(0, total);
}
void build() {
    if (!ST.empty()) return;
    vectors(5, ST); IDX.assign(46656, -1); for (size_t i = 0; i < ST.size(); i++) IDX[enc(ST[i])] = (int)i;
    for (auto& c : ST) { std::array<int, 12> r; for (int k = 0; k < 12; k++) r[k] = scoreC(c, k); SC.push_back(r); }
    const double fact[6] = { 1, 1, 2, 6, 24, 120 };
    std::vector<std::vector<std::pair<int, double>>> D(6);
    for (int k = 0; k <= 5; k++) { std::vector<Cnt> vs; vectors(k, vs); for (auto& v : vs) { double den = 1; for (int x : v) den *= fact[x]; D[k].push_back({ enc(v), fact[k] / den / pow(6, k) }); } }
    for (auto& c : ST) {
        std::vector<Trans> res; Cnt h{};
        std::function<void(int)> rec = [&](int i) {
            if (i == 6) { int kept = 0; for (int x : h) kept += x; int k = 5 - kept; if (k >= 1) { Trans t; t.h = h; int he = enc(h); for (auto& o : D[k]) { t.outs.push_back(IDX[he + o.first]); t.ps.push_back(o.second); } res.push_back(t); } return; }
            for (int x = 0; x <= c[i]; x++) { h[i] = x; rec(i + 1); } h[i] = 0;
        };
        rec(0); TR.push_back(res);
    }
}
struct Slot { int c, k; };
struct Tables { std::vector<double> T, V1; std::vector<std::vector<std::pair<double, Slot>>> A; };
Tables tables(const std::vector<Slot>& slots) {
    build(); Tables t; size_t n = ST.size(); t.T.assign(n, 0); t.V1.assign(n, 0); t.A.resize(n);
    for (size_t si = 0; si < n; si++) {
        double bv = -1e9; for (auto& sl : slots) { double v = (sl.k + 1) * (SC[si][sl.c] - BASE[sl.c]); t.A[si].push_back({ v, sl }); bv = std::max(bv, v); }
        std::sort(t.A[si].begin(), t.A[si].end(), [](auto& a, auto& b) { return a.first > b.first; }); t.T[si] = bv;
    }
    for (size_t si = 0; si < n; si++) { double b = t.T[si]; for (auto& tr : TR[si]) { double e = 0; for (size_t j = 0; j < tr.outs.size(); j++) e += tr.ps[j] * t.T[tr.outs[j]]; b = std::max(b, e); } t.V1[si] = b; }
    return t;
}
struct AIMove { bool score; int c, k; std::array<bool, 5> held; };

struct Yacht : Game {
    int n = 2, cols = 1; bool triple = false;
    std::vector<std::array<std::array<int, 3>, 12>> sheets;   // -1 = libre
    std::vector<bool> active;
    int turn = 0, rollsLeft = 3; bool rolled = false, over = false; Dice dice{ 1, 1, 1, 1, 1 }; std::array<bool, 5> held{};
    float rollAnim = 0, aiT = 0, endT = -1; Dice shown{ 1, 2, 3, 4, 5 };
    std::map<std::string, Tables> memo;

    void start() override {
        n = (int)pl.size(); triple = opts.get("variant") == 1; cols = triple ? 3 : 1;
        sheets.assign(n, {}); for (auto& s : sheets) for (auto& r : s) r = { -1, -1, -1 };
        active.assign(n, true); turn = 0; startTurn();
    }
    void startTurn() { rollsLeft = 3; held = {}; dice = { 1, 1, 1, 1, 1 }; rolled = false; aiT = 0; }
    bool open(int p) { for (auto& r : sheets[p]) for (int k = 0; k < cols; k++) if (r[k] < 0) return true; return false; }
    int total(int p) { int t = 0; for (auto& r : sheets[p]) for (int k = 0; k < cols; k++) if (r[k] > 0) t += r[k]; return t; }
    int turnPlayer() const override { return over ? -1 : turn; }
    void roll() {
        if (over || rollsLeft <= 0 || rollAnim > 0) return;
        for (int i = 0; i < 5; i++) if (!(held[i] && rolled)) dice[i] = 1 + rndInt(6);
        rollsLeft--; rolled = true; rollAnim = 0.55f; app->audio.play("dice");
    }
    void hold(int i) { if (over || !rolled || rollsLeft <= 0 || rollAnim > 0) return; held[i] = !held[i]; app->audio.play("click"); }
    void score(int c, int k) {
        if (over || !rolled || rollAnim > 0 || sheets[turn][c][k] >= 0) return;
        sheets[turn][c][k] = catScore(c, dice) * (k + 1); app->audio.play("place"); advance();
    }
    void advance() {
        for (int s = 1; s <= n; s++) { int q = (turn + s) % n; if (active[q] && open(q)) { turn = q; startTurn(); return; } }
        over = true; endT = 0;
    }
    void resign(int p) override { active[p] = false; int left = 0; for (bool a : active) left += a; if (left < 2) { over = true; endT = 0; } else if (turn == p) advance(); }
    void finishUp() {
        int best = -1; for (int i = 0; i < n; i++) if (active[i]) best = std::max(best, total(i));
        int w = 0; for (int i = 0; i < n; i++) if (active[i] && total(i) == best) w++;
        std::vector<PResult> r; for (int i = 0; i < n; i++) r.push_back({ total(i), !active[i] ? 'x' : total(i) == best ? (w > 1 ? 'd' : 'w') : 'l' });
        finish(r, "Mejor puntaje: " + std::to_string(best));
    }
    AIMove decide(int lv) {
        std::vector<Slot> slots; for (int c = 0; c < 12; c++) for (int k = 0; k < cols; k++) if (sheets[turn][c][k] < 0) slots.push_back({ c, k });
        std::string key; for (auto& s : slots) key += std::to_string(s.c) + ":" + std::to_string(s.k) + ",";
        auto it = memo.find(key); if (it == memo.end()) { if (memo.size() > 30) memo.clear(); it = memo.emplace(key, tables(slots)).first; }
        Tables& tb = it->second; Cnt c{}; for (int v : dice) c[v - 1]++; int si = IDX[enc(c)];
        struct Opt { double v; AIMove m; }; std::vector<Opt> opts2;
        for (size_t i = 0; i < tb.A[si].size() && i < 2; i++) { AIMove m{ true, tb.A[si][i].second.c, tb.A[si][i].second.k, {} }; opts2.push_back({ tb.A[si][i].first, m }); }
        if (rollsLeft > 0) {
            auto& Vn = rollsLeft >= 2 ? tb.V1 : tb.T;
            for (auto& t : TR[si]) {
                double e = 0; for (size_t j = 0; j < t.outs.size(); j++) e += t.ps[j] * Vn[t.outs[j]];
                AIMove m{ false, 0, 0, {} }; Cnt left = t.h; for (int i = 0; i < 5; i++) { if (left[dice[i] - 1] > 0) { left[dice[i] - 1]--; m.held[i] = true; } }
                opts2.push_back({ e, m });
            }
        }
        std::sort(opts2.begin(), opts2.end(), [](const Opt& a, const Opt& b) { return a.v > b.v; });
        const double ERR[4] = { 0, 0.40, 0.12, 0.01 };
        int k = 0; if (opts2.size() > 1 && rnd01() < ERR[lv]) k = lv == 1 ? rndInt(std::min<int>(6, (int)opts2.size())) : 1 + rndInt(std::min<int>(2, (int)opts2.size() - 1));
        return opts2[k].m;
    }
    void update(float dt, Input&, UI&) override {
        if (rollAnim > 0) { rollAnim -= dt; for (int i = 0; i < 5; i++) shown[i] = (held[i] && rollsLeft < 2) ? dice[i] : 1 + rndInt(6); if (rollAnim <= 0) shown = dice; }
        if (over) { if (endT >= 0 && (endT += dt) > 1.2f && !finished) finishUp(); return; }
        if (!pl[turn].ai || rollAnim > 0) return;
        int lv = pl[turn].level; float delay = lv == 1 ? 1.4f : lv == 3 ? 0.7f : 0.95f;
        aiT += dt; if (aiT < delay) return; aiT = 0;
        if (!rolled) { roll(); return; }
        AIMove m = decide(lv);
        if (m.score) { score(m.c, m.k); return; }
        for (int i = 0; i < 5; i++) if (held[i] != m.held[i]) { held[i] = m.held[i]; app->audio.play("click"); return; }
        if (rollsLeft > 0) roll();
    }
    static void drawDie(Gfx& g, Rect r, int v, bool held, float tilt) {
        g.rrect({ r.x + 3, r.y + 6, r.w, r.h }, r.w * 0.18f, rgb(0, 90));
        g.rrectGrad(r, r.w * 0.18f, rgb(0xffffff), rgb(0xe4ddd0));
        if (held) g.rrectLine(r.inset(-4), r.w * 0.2f, 4, pal::gold);
        static const int PAT[7][9] = { {}, { 0,0,0,0,1,0,0,0,0 }, { 1,0,0,0,0,0,0,0,1 }, { 1,0,0,0,1,0,0,0,1 }, { 1,0,1,0,0,0,1,0,1 }, { 1,0,1,0,1,0,1,0,1 }, { 1,0,1,1,0,1,1,0,1 } };
        (void)tilt;
        for (int k = 0; k < 9; k++) if (PAT[v][k]) { float px = r.x + r.w * (0.25f + 0.25f * (k % 3)), py = r.y + r.h * (0.25f + 0.25f * (k / 3)); g.circleGrad(px, py, r.w * 0.085f, rgb(v == 1 ? 0xc0392b : 0x2b1d14), rgb(v == 1 ? 0x8e1f12 : 0x000000)); }
    }
    static bool rollKey(UI& ui) { if (ui.activePad < 0) return false; const Pad& p = ui.in->pads[ui.activePad]; return p.rollDown(); }
    static bool sideKey(UI& ui) { if (ui.activePad < 0) return false; const Pad& p = ui.in->pads[ui.activePad]; return p.sideDown(); }
    bool focusOnCell = false; uint32_t firstCell = 0; std::string rollId;
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai; bool fc = false; uint32_t fcell = 0;
        // bandeja de dados (paño verde con borde de madera)
        Rect tray{ 280, 70, 470, 290 };
        g.rrect({ tray.x, tray.y + 6, tray.w, tray.h }, 22, rgb(0, 110)); g.rrectGrad(tray, 22, rgb(0x6b4224), rgb(0x40240f));
        g.rrectGrad(tray.inset(14), 14, rgb(0x2f8a4a), rgb(0x1d5a30));
        std::string banner = over ? "Fin de la partida" : mine ? (pl[turn].name + ": ¡tu turno!") : ("Turno de " + pl[turn].name);
        Rect bn{ 280, 18, 470, 42 }; g.rrect(bn, 10, pal::plateEdge); g.rrectGrad(bn.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(banner, bn.cx(), bn.cy(), 19, mine ? pal::gold : pal::white, F_BOLD, AL_C, 440);
        for (int i = 0; i < 5; i++) {
            float sz = 74, x = tray.x + 34 + i * 82, y = tray.y + 108 - (held[i] && rolled ? 26 : 0) + (rollAnim > 0 && !(held[i] && rollsLeft < 2) ? sinf(rollAnim * 40 + i) * 8 : 0);
            Rect r{ x, y, sz, sz }; uint32_t id = hid("die", i);
            bool c = ui.hit(id, { x - 4, tray.y + 60, sz + 8, 170 }, mine && rolled && rollsLeft > 0);
            if (c) hold(i);
            if (!rolled && rollAnim <= 0) drawDie(g, r, i + 1, false, 0); else drawDie(g, r, rollAnim > 0 ? shown[i] : dice[i], held[i] && rolled, 0);
            if (held[i] && rolled) g.text("GUARDADO", r.cx(), r.y + sz + 18, 11, pal::gold, F_TITLE, AL_C);
            if (ui.focused(id)) ui.drawFocus(r, 14);
        }
        // tiradas que quedan
        for (int k = 0; k < 3; k++) g.circle(tray.cx() - 24 + k * 24, tray.y + tray.h + 20, 7, k < rollsLeft ? pal::gold : rgb(0x3a2a1a));
        std::string rl = rolled ? "Tirar de nuevo (" + std::to_string(rollsLeft) + ")" : "Tirar";
        if (ui.button(rl, { 330, 410, 370, 60 }, 1, mine && rollsLeft > 0 && rollAnim <= 0, "🎲")) roll();
        if (mine) ui.setFocusIfNone(hid(rl + "🎲", (int)(330 * 7 + 410)));
        if (mine && rollsLeft > 0 && rollAnim <= 0 && rollKey(ui)) roll();   // ZL/ZR tira (el que tiene cada Joy-Con)
        g.text(mine && rolled ? "Tocá los dados para guardarlos y elegí una categoría en el bloc" : "", 515, 494, 14, rgb(0xf5e6c8, 220), F_BODY, AL_C);
        // bloc de notas
        Rect pad{ 780, 18, 480, 640 };
        g.rrect({ pad.x + 4, pad.y + 8, pad.w, pad.h }, 6, rgb(0, 100)); g.rrectGrad(pad, 6, rgb(0xfff7c2), rgb(0xf6e9a4));
        for (int k = 0; k < 6; k++) g.circle(pad.x + 40 + k * 80, pad.y + 8, 6, rgb(0x6b5a40));
        float catW = 150, colsW = pad.w - catW - 20, cw = colsW / (n * cols), rowH = 39, y0 = pad.y + 64;
        g.text("Categoría", pad.x + 14, pad.y + 40, 15, rgb(0x4a3a2a), F_BOLD);
        for (int p = 0; p < n; p++) {
            float x = pad.x + catW + p * cols * cw; g.emoji(pl[p].emoji, x + cols * cw / 2 - (n <= 4 ? 30 : 0), pad.y + 40, 20, active[p] ? 255 : 90);
            if (n <= 4) g.text(pl[p].name, x + cols * cw / 2 - 16, pad.y + 40, 13, p == turn && !over ? rgb(0xa0281a) : rgb(0x3a2a1a), F_BOLD, AL_L, cols * cw / 2 + 6);
        }
        for (int c = 0; c <= 12; c++) {
            float y = y0 + c * rowH;
            g.line(pad.x + 10, y + rowH, pad.x + pad.w - 10, y + rowH, 1.5f, rgb(0x7fa6c8, 160));
            g.text(c < 12 ? CATN[c] : "TOTAL", pad.x + 16, y + rowH / 2, c < 12 ? 17 : 18, rgb(0x2a2a50), c < 12 ? F_BODY : F_BOLD, AL_L, catW - 12);
            for (int p = 0; p < n; p++) for (int k = 0; k < cols; k++) {
                Rect cell{ pad.x + catW + (p * cols + k) * cw, y + 3, cw, rowH - 6 };
                if (c == 12) { if (k == 0) g.text(std::to_string(total(p)), pad.x + catW + p * cols * cw + cols * cw / 2, y + rowH / 2, 18, rgb(0xa0281a), F_BOLD, AL_C); continue; }
                int v = sheets[p][c][k]; bool tap = v < 0 && p == turn && mine && rolled && rollAnim <= 0;
                uint32_t id = hid("cell", c * 32 + p * 3 + k);
                if (tap) {
                    bool hit = ui.hit(id, cell);
                    g.rrect(cell.inset(2), 6, rgb(0xffd54f, 150));
                    g.text(std::to_string(catScore(c, dice) * (k + 1)), cell.cx(), cell.cy(), 16, rgb(0x7a4a00), F_BOLD, AL_C);
                    if (!fcell) fcell = id; if (ui.focused(id)) { ui.drawFocus(cell, 6); fc = true; }
                    if (hit) { score(c, k); return; }
                } else g.text(v < 0 ? "·" : std::to_string(v), cell.cx(), cell.cy(), 16, rgb(0x1a1a40), F_BODY, AL_C);
            }
        }
        focusOnCell = fc; if (mine && sideKey(ui)) { ui.padNav = true; if (fc) ui.forceFocus(hid(rl + "🎲", (int)(330 * 7 + 410))); else if (fcell) ui.forceFocus(fcell); app->audio.play("click"); }   // L/R: cambia entre los dados y el bloc
        if (triple) for (int p = 0; p < n; p++) for (int k = 0; k < 3; k++) g.text("x" + std::to_string(k + 1), pad.x + catW + (p * 3 + k) * cw + cw / 2, y0 - 8, 11, rgb(0x6a5a40), F_BODY, AL_C);
    }
    std::vector<Hint> hints() const override {
        if (over || pl[turn].ai) return {};
        if (!rolled) return { { "ROLL", "Tirar los dados" }, { "A", "Elegir" } };
        std::vector<Hint> h{ { "DPAD/STICK", "Moverse" }, { "A", "Guardar dado / anotar" }, { "LS", "Cambiar de lado: dados / bloc" } };
        if (rollsLeft > 0) h.push_back({ "ROLL", "Tirar de nuevo" });
        return h;
    }
    std::string help() const override {
        return "Tirá los 5 dados hasta 3 veces por turno. Entre tirada y tirada podés guardar los dados que quieras (tocalos o elegilos con A; ZL/ZR tira los dados (el que tiene tu Joy-Con); L/R cambia entre los dados y el bloc). "
               "Después anotá el resultado en una categoría libre del bloc. Escaleras: 25 y 30 puntos. Yacht (5 iguales): 50. Gana el mayor total.";
    }
};
}
Game* makeYacht() { return new Yacht(); }
