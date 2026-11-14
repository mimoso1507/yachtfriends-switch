// Backgammon estándar (sin cubo). Blancas = jugador 1 (abajo, casa a la derecha), rojas = jugador 2. Obligatorio usar todos los dados posibles.
// ZL/ZR tira. La IA evalúa todas las jugadas posibles del turno (pips, bloqueos, fichas solas, golpes, barra).
#include "../app.h"
#include <algorithm>
#include <set>

namespace {
struct M { int from, to, die; };           // from -1 = barra; to -2 = sacar
struct BS { int b[24]; int bar[2]; int off[2]; };
int sgn(int p) { return p ? -1 : 1; }
int ownAt(const BS& s, int p, int i) { int v = s.b[i] * sgn(p); return v > 0 ? v : 0; }
int distOff(int p, int i) { return p ? 24 - i : i + 1; }
bool allHome(const BS& s, int p) { if (s.bar[p] > 0) return false; for (int i = 0; i < 24; i++) if (ownAt(s, p, i) > 0 && distOff(p, i) > 6) return false; return true; }
void singles(const BS& s, int p, int die, std::vector<M>& res) {
    if (s.bar[p] > 0) { int t = p ? die - 1 : 24 - die; if (ownAt(s, 1 - p, t) < 2) res.push_back({ -1, t, die }); return; }
    bool home = allHome(s, p); int dir = p ? 1 : -1, far = 0; for (int i = 0; i < 24; i++) if (ownAt(s, p, i) > 0) far = std::max(far, distOff(p, i));
    for (int i = 0; i < 24; i++) { if (ownAt(s, p, i) <= 0) continue; int t = i + dir * die;
        if (t < 0 || t > 23) { if (home) { int d = distOff(p, i); if (die == d || (die > d && far <= d)) res.push_back({ i, -2, die }); } } else if (ownAt(s, 1 - p, t) < 2) res.push_back({ i, t, die }); }
}
BS applyM(BS s, int p, const M& m) {
    int sg = sgn(p); if (m.from < 0) s.bar[p]--; else s.b[m.from] -= sg;
    if (m.to == -2) s.off[p]++; else { if (s.b[m.to] * sg < 0) { s.b[m.to] = 0; s.bar[1 - p]++; } s.b[m.to] += sg; }
    return s;
}
int maxUse(const BS& s, int p, std::vector<int> dice) {
    if (dice.empty()) return 0; int best = 0; std::set<int> tried;
    for (size_t k = 0; k < dice.size(); k++) { if (tried.count(dice[k])) continue; tried.insert(dice[k]); std::vector<M> ms; singles(s, p, dice[k], ms); if (ms.empty()) continue;
        std::vector<int> rest = dice; rest.erase(rest.begin() + k); for (auto& m : ms) { best = std::max(best, 1 + maxUse(applyM(s, p, m), p, rest)); if (best == (int)dice.size()) return best; } }
    return best;
}
std::vector<M> legalFirst(const BS& s, int p, const std::vector<int>& dice) {
    std::vector<M> out; int mx = maxUse(s, p, dice); if (mx == 0) return out; std::set<int> tried;
    for (size_t k = 0; k < dice.size(); k++) { if (tried.count(dice[k])) continue; tried.insert(dice[k]); std::vector<M> ms; singles(s, p, dice[k], ms); std::vector<int> rest = dice; rest.erase(rest.begin() + k);
        for (auto& m : ms) if (1 + maxUse(applyM(s, p, m), p, rest) == mx) out.push_back(m); }
    if (mx == 1 && dice.size() == 2 && dice[0] != dice[1]) { int hi = std::max(dice[0], dice[1]); bool hasHi = false; for (auto& m : out) if (m.die == hi) hasHi = true; if (hasHi) { std::vector<M> f; for (auto& m : out) if (m.die == hi) f.push_back(m); out = f; } }
    return out;
}
int pip(const BS& s, int p) { int t = s.bar[p] * 25; for (int i = 0; i < 24; i++) t += ownAt(s, p, i) * distOff(p, i); return t; }
float evalPos(const BS& s, int p, int lv) {
    int o = 1 - p; float sc = (float)(pip(s, o) - pip(s, p)); sc += 10.f * (s.off[p] - s.off[o]); sc += 14.f * (s.bar[o] - s.bar[p]);
    int run = 0; for (int i = 0; i < 24; i++) { int mine = ownAt(s, p, i); if (mine >= 2) { sc += 4; run++; sc += run >= 3 ? 2.f * run : 0; } else run = 0;
        if (mine == 1) { float risk = 3; if (lv >= 3) { risk = 0; int dir = p ? 1 : -1; for (int j = 0; j < 24; j++) if (ownAt(s, o, j) > 0) { int d = (i - j) * (o ? 1 : -1); (void)dir; if (d >= 1 && d <= 6) risk += 2.5f; } if (s.bar[o]) risk += 4; if (risk > 14) risk = 14; } sc -= risk; } }
    return sc;
}
struct Seq { std::vector<M> mv; BS after; };
void enumSeq(const BS& s, int p, std::vector<int> dice, std::vector<M> cur, std::vector<Seq>& out, int cap) {
    if ((int)out.size() > cap) return; auto firsts = legalFirst(s, p, dice); if (firsts.empty()) { out.push_back({ cur, s }); return; }
    for (auto& m : firsts) { std::vector<int> rest = dice; rest.erase(std::find(rest.begin(), rest.end(), m.die)); auto c2 = cur; c2.push_back(m); enumSeq(applyM(s, p, m), p, rest, c2, out, cap); }
}

struct Backgammon : Game {
    BS s; int turn = 0, sel = -2, wins[2] = { 0, 0 }, target = 1, winner = -1, resigned = -1, gameNo = 1; bool over = false, rolled = false, gameOver = false; std::vector<int> dice; std::vector<M> aiPlan; std::string msg; float aiT = 0, aiWait = 0.8f, endT = 0, waitT = 0, dieAnim = 0; int d1 = 1, d2 = 1;
    void start() override { target = opts.get("match", 0) == 1 ? 2 : 1; newGame(); }
    void newGame() {
        for (int i = 0; i < 24; i++) s.b[i] = 0; s.bar[0] = s.bar[1] = s.off[0] = s.off[1] = 0; s.b[23] = 2; s.b[12] = 5; s.b[7] = 3; s.b[5] = 5; s.b[0] = -2; s.b[11] = -5; s.b[16] = -3; s.b[18] = -5;
        turn = rndInt(2); rolled = false; dice.clear(); sel = -2; gameOver = false; msg = pl[turn].name + " empieza"; resetAi();
    }
    void resetAi() { aiT = 0; aiWait = 0.7f + rnd01() * 0.5f; waitT = 0; aiPlan.clear(); }
    int turnPlayer() const override { return over ? -1 : turn; }
    void doRoll() { d1 = 1 + rndInt(6); d2 = 1 + rndInt(6); dice = d1 == d2 ? std::vector<int>(4, d1) : std::vector<int>{ d1, d2 }; rolled = true; dieAnim = 0.5f; app->audio.play("roll"); msg = pl[turn].name + " sacó " + std::to_string(d1) + " y " + std::to_string(d2); sel = -2; waitT = 0; aiPlan.clear(); }
    void endTurn() { turn = 1 - turn; rolled = false; dice.clear(); sel = -2; resetAi(); }
    void doMove(const M& m) {
        bool hit = m.to >= 0 && s.b[m.to] * sgn(turn) < 0; s = applyM(s, turn, m); dice.erase(std::find(dice.begin(), dice.end(), m.die)); sel = -2; app->audio.play(hit || m.to == -2 ? "win" : "place");
        if (s.off[turn] == 15) { winner = turn; wins[turn]++; gameOver = true; endT = 0; msg = pl[turn].name + " ganó la partida"; if (wins[turn] >= target) over = true; return; }
        if (dice.empty() || legalFirst(s, turn, dice).empty()) { if (!dice.empty()) msg = "Sin más jugadas"; endTurn(); }
    }
    void resign(int p) override { over = true; resigned = p; winner = 1 - p; wins[winner] = std::max(wins[winner], target); }
    void aiTurn() {
        if (!rolled) { doRoll(); aiT = 0; aiWait = 0.6f; return; }
        if (aiPlan.empty()) { std::vector<Seq> seqs; enumSeq(s, turn, dice, {}, seqs, 3000); if (seqs.empty()) { endTurn(); return; } int lv = std::max(1, std::min(3, pl[turn].level)); int bi = 0;
            if (lv == 1 && rnd01() < 0.5f) bi = rndInt((int)seqs.size()); else { float bs = -1e9f; for (int i = 0; i < (int)seqs.size(); i++) { float v = evalPos(seqs[i].after, turn, lv) + rnd01() * (lv == 1 ? 25.f : lv == 2 ? 4.f : 0.5f); if (v > bs) { bs = v; bi = i; } } }
            aiPlan = seqs[bi].mv; if (aiPlan.empty()) { msg = pl[turn].name + " no puede mover"; endTurn(); return; } std::reverse(aiPlan.begin(), aiPlan.end()); }
        M m = aiPlan.back(); aiPlan.pop_back(); doMove(m); aiT = 0; aiWait = 0.45f;
    }
    void update(float dt, Input& in, UI&) override {
        dieAnim = std::max(0.f, dieAnim - dt);
        if (gameOver) { endT += dt; if (endT > 2.2f) { if (over) { if (!finished) { std::vector<PResult> r = { { wins[0], winner == 0 ? 'w' : 'l' }, { wins[1], winner == 1 ? 'w' : 'l' } }; if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + pl[winner].name + (target > 1 ? " (" + std::to_string(wins[winner]) + " a " + std::to_string(wins[1 - winner]) + ")" : "")); } } else { gameNo++; newGame(); } } return; }
        if (over) return;
        if (pl[turn].ai) { aiT += dt; if (aiT >= aiWait) aiTurn(); return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
        if (!rolled) { if (pd && pd->rollDown()) doRoll(); }
        else { if (pd && pd->down[B_B]) sel = -2; if (legalFirst(s, turn, dice).empty()) { waitT += dt; if (waitT > 1.2f) { msg = pl[turn].name + " no puede mover"; endTurn(); } } }
    }
    // geometría del tablero
    Rect pointRect(int i) const {
        float X0 = 300, pw = 60, half = 6 * pw, bar = 44; int slot; bool top; if (i >= 12) { top = true; slot = i - 12; } else { top = false; slot = 11 - i; }
        float x = X0 + (slot < 6 ? slot * pw : half + bar + (slot - 6) * pw); return top ? Rect{ x, 90, pw, 235 } : Rect{ x, 395, pw, 235 };
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !gameOver && !pl[turn].ai && rolled; std::vector<M> lf; if (mine) lf = legalFirst(s, turn, dice);
        g.rrect({ 284, 70, 840, 580 }, 14, rgb(0x4a2c12)); g.rect({ 296, 82, 772 - 0, 556 }, rgb(0x2d5e3a)); g.rect({ 300 + 360, 82, 44, 556 }, rgb(0x4a2c12));
        std::vector<int> srcs, tgts; bool barSrc = false; for (auto& m : lf) { if (m.from == -1) barSrc = true; else if (std::find(srcs.begin(), srcs.end(), m.from) == srcs.end()) srcs.push_back(m.from); }
        bool offT = false; if (sel != -2) for (auto& m : lf) if (m.from == (sel == 24 ? -1 : sel)) { if (m.to == -2) offT = true; else tgts.push_back(m.to); }
        int firstId = -1;
        for (int i = 0; i < 24; i++) {
            Rect r = pointRect(i); bool top = i >= 12; bool dark = (i % 2) == 0; Col c = dark ? rgb(0xb8452f) : rgb(0xe8cfa0);
            if (top) g.tri(r.x + 3, r.y, r.x + r.w - 3, r.y, r.cx(), r.y + r.h, c); else g.tri(r.x + 3, r.y + r.h, r.x + r.w - 3, r.y + r.h, r.cx(), r.y, c);
            bool isT = std::find(tgts.begin(), tgts.end(), i) != tgts.end(); bool canSrc = std::find(srcs.begin(), srcs.end(), i) != srcs.end() && !barSrc; uint32_t id = hid("bg", i);
            bool hit = ui.hit(id, r, mine && (isT || canSrc)); if (firstId < 0 && (canSrc || (barSrc && false))) firstId = i; if (isT) g.rect(r, rgb(0x7ad06a, 70));
            int v = s.b[i], n = std::abs(v); for (int k = 0; k < std::min(n, 5); k++) { float cy = top ? r.y + 24 + k * 40 : r.y + r.h - 24 - k * 40; g.circle(r.cx() + 1, cy + 3, 19, rgb(0, 90)); g.circleGrad(r.cx() - 4, cy - 4, 19, v > 0 ? rgb(0xfffaf0) : rgb(0xe0584a), v > 0 ? rgb(0xd8c9a2) : rgb(0x8a1f14)); g.ring(r.cx(), cy, 19, 2, v > 0 ? rgb(0x8a7a50) : rgb(0x5a1008)); if (sel == i && k == std::min(n, 5) - 1) g.ring(r.cx(), cy, 23, 4, pal::gold); }
            if (n > 5) g.text(std::to_string(n), r.cx(), top ? r.y + 24 + 4 * 40 : r.y + r.h - 24 - 4 * 40, 18, v > 0 ? rgb(0x222222) : rgb(0xffffff), F_BOLD, AL_C);
            if (isT) g.ring(r.cx(), top ? r.y + r.h - 20 : r.y + 20, 10, 3, rgb(0x7ad06a)); if (ui.focused(id)) ui.drawFocus(r, 4); if (hit) { if (isT) { M mm{}; for (auto& m : lf) if (m.from == (sel == 24 ? -1 : sel) && m.to == i) mm = m; doMove(mm); return; } else if (canSrc) sel = (sel == i ? -2 : i); }
        }
        // barra
        for (int p = 0; p < 2; p++) for (int k = 0; k < s.bar[p]; k++) { float cy = p == 0 ? 440 + k * 36 : 280 - k * 36; g.circleGrad(682 - 4, cy - 4, 18, p == 0 ? rgb(0xfffaf0) : rgb(0xe0584a), p == 0 ? rgb(0xd8c9a2) : rgb(0x8a1f14)); }
        if (barSrc) { Rect br{ 660, 330, 44, 70 }; uint32_t id = hid("bg", 24); bool hit = ui.hit(id, br, mine); g.rrectLine(br, 6, 3, pal::gold); if (sel == 24) g.rect(br, rgb(0xffe27a, 80)); if (ui.focused(id)) ui.drawFocus(br, 6); if (hit) sel = sel == 24 ? -2 : 24; if (firstId < 0) firstId = -100; }
        // bandeja de salida
        { Rect tr{ 1076, 82, 44, 556 }; g.rrect(tr, 8, rgb(0x2a1a0c)); uint32_t id = hid("bg", 25); bool hit = ui.hit(id, tr, mine && offT); if (offT) g.rrectLine(tr, 8, 3, rgb(0x7ad06a)); if (ui.focused(id)) ui.drawFocus(tr, 8); if (hit) { M mm{}; for (auto& m : lf) if (m.from == (sel == 24 ? -1 : sel) && m.to == -2) mm = m; doMove(mm); return; }
          g.text(std::to_string(s.off[0]), tr.cx(), 600, 18, rgb(0xfffaf0), F_BOLD, AL_C); g.text(std::to_string(s.off[1]), tr.cx(), 120, 18, rgb(0xe0584a), F_BOLD, AL_C); }
        if (firstId >= 0) ui.setFocusIfNone(hid("bg", firstId)); else if (firstId == -100) ui.setFocusIfNone(hid("bg", 24));
        // paneles
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : gameOver ? msg : mine ? pl[turn].name + ": mueve" : !rolled && !pl[turn].ai ? pl[turn].name + ": tira los dados" : "Turno de " + pl[turn].name; g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        for (int p = 0; p < 2; p++) { Rect r{ 1136, 90.f + p * 62, 134, 54 }; bool on = p == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.circle(r.x + 14, r.cy(), 8, p == 0 ? rgb(0xfffaf0) : rgb(0xe0584a)); g.text(pl[p].name, r.x + 28, r.y + 17, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, 100); g.text("Pips " + std::to_string(pip(s, p)) + (target > 1 ? " · " + std::to_string(wins[p]) : ""), r.x + 28, r.y + 38, 12, rgb(0xd8b46a), F_BODY, AL_L, 100); }
        // dados
        float dx = 1203; for (int k = 0; k < 2; k++) { int face = dieAnim > 0 ? 1 + rndInt(6) : (k ? d2 : d1); bool used = rolled && dieAnim <= 0 && (d1 == d2 ? (int)dice.size() < 4 - 2 * k - (k ? 0 : 0) && false : std::find(dice.begin(), dice.end(), k ? d2 : d1) == dice.end()); float y = 270 + k * 70; g.rrect({ dx - 28, y - 28, 56, 56 }, 10, used ? rgb(0x8a8470) : rgb(0xfaf3dc)); g.rrectLine({ dx - 28, y - 28, 56, 56 }, 10, 2, rgb(0x6a4a2a));
            static const int L[7][9] = { {0}, {4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8} }; if (rolled || dieAnim > 0) for (int q = 0; q < face; q++) { int ix = L[face][q]; g.circle(dx + ((ix % 3) - 1) * 14.f, y + ((ix / 3) - 1) * 14.f, 4.5f, rgb(0x1a1a1a)); } }
        if (rolled && d1 == d2) g.text("quedan " + std::to_string(dice.size()), dx, 400, 13, pal::cream, F_BODY, AL_C);
        if (!over && !gameOver && !pl[turn].ai && !rolled) { if (ui.button("Tirar dados", { 1136, 430, 134, 50 }, 3)) { doRoll(); return; } ui.setFocusIfNone(hid("Tirar dados", 1136 * 7 + 430)); }
        g.text(msg, 1203, 520, 13, pal::cream, F_BODY, AL_C, 130);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Tirar dados" }, { "DPAD/STICK", "Moverse" }, { "A", "Elegir / mover" }, { "B", "Soltar" } }; }
    std::string help() const override { return "Mueve tus 15 fichas hasta tu casa y sácalas todas. Las blancas van hacia la derecha-abajo, las rojas hacia la derecha-arriba. Tira los dados (ZL/ZR) y mueve cada dado con una ficha. Una ficha sola puede ser comida y vuelve a entrar desde la barra. No puedes caer en puntos con 2 o más fichas rivales. Debes usar todos los dados que puedas."; }
};
}
Game* makeBackgammon() { return new Backgammon(); }
