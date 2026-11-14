// Zilch / Farkle: meta 10000, mínimo 500 para plantarse la primera vez, dados libres al puntuar con los 6. IA igual a la de PC.
#include "dice.h"
#include <algorithm>
#include <cmath>

namespace {
const int ENTRY = 500, TARGET = 10000;
int scoreSel(const std::vector<int>& sel) {
    if (sel.empty()) return 0; int c[7] = {}; for (int v : sel) c[v]++; int kinds = 0, pairs = 0; for (int v = 1; v <= 6; v++) { if (c[v]) kinds++; if (c[v] == 2) pairs++; }
    if (sel.size() == 6) { if (kinds == 6) return 1500; if (kinds == 3 && pairs == 3) return 750; }
    int score = 0, used = 0;
    for (int v = 1; v <= 6; v++) if (c[v] >= 3) { int base = v == 1 ? 1000 : v * 100; score += base * (1 << (c[v] - 3)); used += c[v]; }
    int o = c[1] < 3 ? c[1] : 0, f = c[5] < 3 ? c[5] : 0; score += o * 100 + f * 50; used += o + f;
    return used != (int)sel.size() ? 0 : score;
}
bool hasAny(const std::vector<int>& d) {
    for (int v : d) if (v == 1 || v == 5) return true; int c[7] = {}; for (int v : d) c[v]++; for (int v = 1; v <= 6; v++) if (c[v] >= 3) return true; return scoreSel(d) > 0;
}
struct Opt { std::vector<int> idx; int pts, left; double v; };
std::vector<Opt> options(const std::vector<int>& dice) {
    std::vector<Opt> out; int n = (int)dice.size();
    for (int m = 1; m < (1 << n); m++) { Opt o; std::vector<int> vals; for (int i = 0; i < n; i++) if (m & (1 << i)) { o.idx.push_back(i); vals.push_back(dice[i]); }
        o.pts = scoreSel(vals); if (o.pts > 0) { o.left = n - (int)o.idx.size() == 0 ? 6 : n - (int)o.idx.size(); o.v = 0; out.push_back(o); } }
    return out;
}
int FUT(int l) { switch (l) { case 6: return 350; case 5: return 200; case 4: return 80; case 3: return 0; case 2: return -60; default: return -110; } }
bool bestSel(const std::vector<int>& dice, int lv, Opt& res) {
    auto o = options(dice); if (o.empty()) return false;
    for (auto& x : o) x.v = x.pts + FUT(x.left);
    std::sort(o.begin(), o.end(), [](const Opt& a, const Opt& b) { return a.v > b.v; });
    const float ERR[4] = { 0, 0.4f, 0.1f, 0.01f };
    if (o.size() > 1 && rnd01() < ERR[lv]) { res = lv == 1 ? o[rndInt((int)o.size())] : o[1]; return true; }
    res = o[0]; return true;
}
bool shouldBank(int total, int left, const std::vector<int>& totals, bool lastRound, int me, int lv) {
    int mine = totals[me]; bool already = mine > 0;
    if (!already && total < ENTRY) return false;
    if (mine + total >= TARGET && !lastRound) return true;
    if (lastRound) { int top = 0; for (size_t i = 0; i < totals.size(); i++) if ((int)i != me) top = std::max(top, totals[i]); return mine + total > top; }
    if (left == 6) return false;
    int thr = left == 5 ? 2200 : left == 4 ? 1100 : left == 3 ? 450 : 300;
    int lead = 0; for (size_t i = 0; i < totals.size(); i++) if ((int)i != me) lead = std::max(lead, totals[i]);
    float k = 1; if (lead - mine > 2500) k = 1.35f; else if (mine - lead > 2500) k = 0.75f;
    float t2 = thr * k * (lv == 1 ? 0.5f + rnd01() : lv == 3 ? 0.95f + rnd01() * 0.1f : 0.85f + rnd01() * 0.3f);
    bool bank = total >= t2; if (lv == 1 && rnd01() < 0.2f) bank = !bank; return bank;
}
struct Step { int kind; int i; float wait; };   // 0 toggle, 1 go, 2 bank, 3 pass

struct Zilch : Game {
    int N = 0, turn = 0, turnScore = 0, lastTrigger = -1, finalTurn = -1; std::vector<int> totals, dice; std::vector<bool> sel; bool bust = false, over = false; float rollAnim = 0, endT = 0, bustT = 0; int resigned = -1;
    std::vector<Step> plan; float planT = 0; std::vector<int> shown;
    void start() override { N = (int)pl.size(); totals.assign(N, 0); startTurn(); }
    void roll(int k) { dice.clear(); for (int i = 0; i < k; i++) dice.push_back(1 + rndInt(6)); sel.assign(k, false); rollAnim = 0.5f; shown = dice; app->audio.play("dice"); bust = !hasAny(dice); bustT = 0; plan.clear(); planT = 0; if (bust) app->audio.play("lose"); }
    void startTurn() { turnScore = 0; roll(6); }
    int turnPlayer() const override { return over ? -1 : turn; }
    std::vector<int> selVals() const { std::vector<int> v; for (size_t i = 0; i < dice.size(); i++) if (sel[i]) v.push_back(dice[i]); return v; }
    int selScore() const { return scoreSel(selVals()); }
    int selCount() const { int n = 0; for (bool b : sel) if (b) n++; return n; }
    void toggle(int i) { if (over || bust || rollAnim > 0) return; sel[i] = !sel[i]; app->audio.play("click"); }
    bool canGo() const { return !over && !bust && rollAnim <= 0 && selCount() > 0 && selScore() > 0; }
    bool canBank() const { if (over || bust || rollAnim > 0) return false; int add = selScore(); if (selCount() && !add) return false; int f = turnScore + add; if (f <= 0) return false; return totals[turn] > 0 || f >= ENTRY; }
    void go() { if (!canGo()) return; turnScore += selScore(); int rem = (int)dice.size() - selCount(); roll(rem == 0 ? 6 : rem); }
    void bank() {
        if (!canBank()) return; int f = turnScore + selScore(); totals[turn] += f;
        if (totals[turn] >= TARGET && lastTrigger < 0) { lastTrigger = turn; finalTurn = (turn - 1 + N) % N; }
        endTurn();
    }
    void endTurn() { if (lastTrigger >= 0 && turn == finalTurn) { over = true; return; } turn = (turn + 1) % N; startTurn(); }
    void pass() { if (over || !bust) return; endTurn(); }
    void resign(int p) override { over = true; resigned = p; }
    void update(float dt, Input& in, UI&) override {
        rollAnim = std::max(0.f, rollAnim - dt);
        if (rollAnim > 0) for (auto& v : shown) v = 1 + rndInt(6); else shown = dice;
        if (over) { endT += dt; if (endT > 1.4f && !finished) {
            int best = *std::max_element(totals.begin(), totals.end()); int cnt = (int)std::count(totals.begin(), totals.end(), best);
            std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ totals[i], totals[i] == best ? (cnt > 1 ? 'd' : 'w') : 'l' });
            if (resigned >= 0) r[resigned].r = 'x'; std::string sm = "Puntos:"; for (int t : totals) sm += " " + std::to_string(t); finish(r, sm); } return; }
        if (!pl[turn].ai) {
            Pad* p = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
            if (p) { if (p->rollDown()) { if (bust) pass(); else go(); } if (p->down[B_Y]) bank(); }
            return;
        }
        if (rollAnim > 0) return;
        int lv = std::max(1, std::min(3, pl[turn].level)); float sp = lv == 1 ? 1.6f : lv == 3 ? 0.6f : 1.f;
        if (bust) { bustT += dt; if (bustT > (1.3f + rnd01() * 0.8f) * sp) pass(); return; }
        if (plan.empty()) {
            Opt o; if (!bestSel(dice, lv, o)) { plan.push_back({ totals[turn] > 0 || turnScore >= ENTRY ? 2 : 3, 0, 1.2f * sp }); }
            else {
                int total = turnScore + o.pts; bool bk = shouldBank(total, o.left, totals, lastTrigger >= 0, turn, lv);
                for (int i : o.idx) plan.push_back({ 0, i, (0.45f + rnd01() * 0.35f) * sp }); plan.push_back({ bk ? 2 : 1, 0, (0.9f + rnd01() * 0.7f) * sp });
            }
            planT = 0;
        }
        planT += dt; if (planT < plan[0].wait) return; Step s = plan[0]; plan.erase(plan.begin()); planT = 0;
        if (s.kind == 0) toggle(s.i); else if (s.kind == 1) go(); else if (s.kind == 2) bank(); else pass();
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai;
        Rect tray{ 290, 110, 560, 250 }; g.rrect({ tray.x, tray.y + 6, tray.w, tray.h }, 22, rgb(0, 110)); g.rrectGrad(tray, 22, rgb(0x6b4224), rgb(0x40240f)); g.rrectGrad(tray.inset(14), 14, rgb(0x2f8a4a), rgb(0x1d5a30));
        std::string banner = over ? "Fin de la partida" : bust ? (mine ? "¡Zilch! No hay jugadas: perdés los puntos del turno" : pl[turn].name + " se quedó sin jugadas") : mine ? "Elegí dados que puntúen y tirá de nuevo o bancá" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 40, 560, 48 }, 12, rgb(0x120c08, 210)); g.text(banner, 570, 64, 18, mine && !bust ? pal::gold : pal::white, F_BOLD, AL_C, 540);
        int n = (int)dice.size(); float sz = 72, gap = 14, tw = n * sz + (n - 1) * gap, x0 = tray.cx() - tw / 2; int first = -1;
        for (int i = 0; i < n; i++) {
            Rect r{ x0 + i * (sz + gap), tray.cy() - sz / 2 - (sel[i] ? 16 : 0), sz, sz }; uint32_t id = hid("zd", i);
            bool ok = mine && !bust && rollAnim <= 0; if (ok && first < 0) first = i; bool hit = ui.hit(id, { r.x - 4, tray.y + 40, sz + 8, 170 }, ok);
            dice::draw(g, r, shown.size() > (size_t)i ? shown[i] : dice[i], sel[i]);
            if (sel[i]) g.text("GUARDADO", r.cx(), r.y + sz + 16, 11, pal::gold, F_TITLE, AL_C);
            if (ui.focused(id)) ui.drawFocus(r, 14); if (hit) toggle(i);
        }
        if (first >= 0) ui.setFocusIfNone(hid("zd", first));
        std::string ts = "Puntos del turno: " + std::to_string(turnScore) + (selCount() && selScore() ? " (+" + std::to_string(selScore()) + ")" : "") + (lastTrigger >= 0 ? "   ¡ÚLTIMA VUELTA!" : "");
        g.rrect({ 290, 376, 560, 40 }, 20, rgb(0x120c08, 200)); g.text(ts, 570, 396, 18, pal::cream, F_BOLD, AL_C, 540);
        bool showPass = mine && bust;
        if (ui.button("🎲 Tirar de nuevo", { 290, 430, 270, 56 }, 1, mine && canGo() && !showPass)) go();
        if (!showPass) { if (ui.button("💰 Bancar", { 580, 430, 270, 56 }, 3, mine && canBank())) bank(); }
        else if (ui.button("Pasar turno", { 580, 430, 270, 56 }, 2)) pass();
        if (showPass) ui.setFocusIfNone(hid("Pasar turno", (int)(580 * 7 + 430)));
        // marcador y tabla de puntos
        Rect sc{ 290, 508, 560, 24.f + 26 * std::min(N, 8) / 2 + 26 }; (void)sc;
        float y = 510; g.rrect({ 290, y, 560, 140 }, 12, rgb(0x120c08, 200)); g.rrectLine({ 290, y, 560, 140 }, 12, 1.5f, rgb(0xd8b46a, 110));
        for (int i = 0; i < N && i < 8; i++) { float cx = 310 + (i % 2) * 275, cy = y + 22 + (i / 2) * 28; g.circle(cx, cy, 6, PCOL[i % 8]); g.text(pl[i].name, cx + 14, cy, 14, i == turn && !over ? pal::gold : pal::white, F_BOLD, AL_L, 150); g.text(std::to_string(totals[i]), cx + 255, cy, 15, pal::gold, F_BOLD, AL_R); }
        g.text("Meta " + std::to_string(TARGET) + " · primera vez hace falta sumar " + std::to_string(ENTRY) + " para plantarte", 570, y + 128, 12, rgb(0xd8b46a), F_BODY, AL_C, 540);
        Rect T{ 880, 40, 380, 560 }; g.rrect({ T.x + 3, T.y + 6, T.w, T.h }, 14, rgb(0, 100)); g.rrect(T, 14, pal::panelEdge); g.rrectGrad(T.inset(3), 12, rgb(0x4a2e18), rgb(0x2e1a0c));
        g.textShadow("TABLA DE PUNTOS", T.cx(), T.y + 30, 22, pal::gold, F_TITLE, AL_C);
        static const char* RW[][2] = { { "Un 1", "100" }, { "Un 5", "50" }, { "Tres 1", "1000" }, { "Tres 2", "200" }, { "Tres 3", "300" }, { "Tres 4", "400" }, { "Tres 5", "500" }, { "Tres 6", "600" }, { "Escalera 1 a 6", "1500" }, { "Tres pares", "750" } };
        for (int i = 0; i < 10; i++) { float yy = T.y + 70 + i * 38; g.text(RW[i][0], T.x + 28, yy, 18, pal::cream, F_BODY, AL_L); g.text(RW[i][1], T.x + T.w - 28, yy, 18, pal::gold, F_BOLD, AL_R); g.line(T.x + 20, yy + 19, T.x + T.w - 20, yy + 19, 1, rgb(0xb98a4a, 70)); }
        g.textBox("4 iguales = doble · 5 iguales = doble otra vez", { T.x + 24, T.y + 460, T.w - 48, 80 }, 15, rgb(0xd8b46a));
    }
    std::vector<Hint> hints() const override { if (bust) return { { "ROLL", "Pasar turno" } }; return { { "DPAD/STICK", "Elegir dado" }, { "A", "Guardar / soltar dado" }, { "ROLL", "Tirar de nuevo" }, { "Y", "Bancar" } }; }
    std::string help() const override { return "Tirá 6 dados. Guardá los que puntúan (unos, cincos, tríos, escalera, tres pares) y elegí: tirar de nuevo los que quedan o bancar los puntos del turno. Si no hay ningún dado que puntúe, es Zilch y perdés los puntos del turno. Si puntuás con los 6, tirás 6 otra vez. La primera vez necesitás 500 para plantarte. El primero en 10000 dispara la última vuelta."; }
};
}
Game* makeZilch() { return new Zilch(); }
