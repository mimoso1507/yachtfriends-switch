// Batalla naval 8x8, flota 4-3-2-2-1. Si aciertas, sigues disparando. Cada jugador coloca su flota (Y gira, A coloca, o "Al azar"); contra otra persona
// se tapa la pantalla entre turnos. La IA busca por paridad y, al acertar, remata el barco (nivel 3 usa densidad de probabilidad).
#include "../app.h"
#include <algorithm>

namespace {
const int SZ = 8; const int FLEET[5] = { 4, 3, 2, 2, 1 };
struct Naval : Game {
    int grid[2][64], shots[2][64]; std::vector<int> left[2]; bool ready[2] = { false, false }, over = false, horiz = true, cover = false; int turn = 0, winner = -1, resigned = -1, placer = 0, lastShot = -1, lastHit = -1; float aiT = 0, aiWait = 0.7f, endT = 0, flash = 0; std::string msg; int focusCell = -1;
    void start() override {
        for (int p = 0; p < 2; p++) { for (int i = 0; i < 64; i++) grid[p][i] = shots[p][i] = 0; left[p] = std::vector<int>(FLEET, FLEET + 5); }
        for (int p = 0; p < 2; p++) if (pl[p].ai) { randomFleet(p); ready[p] = true; }
        placer = pl[0].ai ? 1 : 0; if (ready[0] && ready[1]) beginFire(); else cover = !pl[0].ai && !pl[1].ai && false;
        msg = "Coloca tu flota";
    }
    int turnPlayer() const override { return over ? -1 : ready[0] && ready[1] ? turn : placer; }
    bool cellsOk(int p, int r, int c, int len, bool h, std::vector<int>& out) const { out.clear(); for (int i = 0; i < len; i++) { int rr = h ? r : r + i, cc = h ? c + i : c; if (rr < 0 || rr >= SZ || cc < 0 || cc >= SZ || grid[p][rr * SZ + cc]) return false; out.push_back(rr * SZ + cc); } return true; }
    bool place(int p, int r, int c, int len, bool h) { std::vector<int> o; if (!cellsOk(p, r, c, len, h, o)) return false; auto it = std::find(left[p].begin(), left[p].end(), len); if (it == left[p].end()) return false; for (int i : o) grid[p][i] = 1; left[p].erase(it); return true; }
    void randomFleet(int p) { for (int i = 0; i < 64; i++) grid[p][i] = 0; left[p] = std::vector<int>(FLEET, FLEET + 5); for (int len : FLEET) for (int tries = 0; tries < 500; tries++) if (place(p, rndInt(SZ), rndInt(SZ), len, rndInt(2) == 0)) break; }
    void beginFire() { turn = rndInt(2); msg = pl[turn].name + " dispara primero"; aiT = 0; coverIfHotseat(); }
    void coverIfHotseat() { cover = !pl[0].ai && !pl[1].ai; }
    int sunkLen(int p, int cell) const {   // largo del barco de p conectado a 'cell' si está totalmente hundido, si no 0
        std::vector<int> st = { cell }, seen = { cell }; bool all = true;
        while (!st.empty()) { int i = st.back(); st.pop_back(); if (shots[1 - p][i] != 2) all = false; int r = i / SZ, c = i % SZ; const int D[4][2] = { {0,1},{0,-1},{1,0},{-1,0} };
            for (auto& d : D) { int rr = r + d[0], cc = c + d[1]; if (rr < 0 || rr >= SZ || cc < 0 || cc >= SZ) continue; int j = rr * SZ + cc; if (grid[p][j] && std::find(seen.begin(), seen.end(), j) == seen.end()) { seen.push_back(j); st.push_back(j); } } }
        return all ? (int)seen.size() : 0;
    }
    bool fire(int p, int t) {
        int o = 1 - p; if (over || shots[p][t]) return false; bool hit = grid[o][t] == 1; shots[p][t] = hit ? 2 : 1; lastShot = t; lastHit = hit; flash = 0.6f;
        bool done = true; for (int i = 0; i < 64; i++) if (grid[o][i] && shots[p][i] != 2) done = false;
        if (hit) { int sl = sunkLen(o, t); msg = pl[p].name + (sl ? ": ¡hundido (" + std::to_string(sl) + ")!" : ": ¡tocado!"); app->audio.play("win"); } else { msg = pl[p].name + ": agua"; app->audio.play("place"); }
        if (done) { over = true; winner = p; return true; }
        aiT = 0; aiWait = 0.5f + rnd01() * 0.6f; if (!hit) { turn = o; coverIfHotseat(); } return true;
    }
    void resign(int p) override { over = true; resigned = p; winner = 1 - p; }
    int aiShot(int p) {
        int lv = std::max(1, std::min(3, pl[p].level)); std::vector<int> free; for (int i = 0; i < 64; i++) if (!shots[p][i]) free.push_back(i);
        if (lv == 1 && rnd01() < 0.7f) return free[rndInt((int)free.size())];
        // objetivos: vecinos de aciertos sin hundir
        std::vector<int> tg; int o = 1 - p; for (int i = 0; i < 64; i++) if (shots[p][i] == 2 && !sunkLen(o, i)) { int r = i / SZ, c = i % SZ; const int D[4][2] = { {0,1},{0,-1},{1,0},{-1,0} };
            for (auto& d : D) { int rr = r + d[0], cc = c + d[1]; if (rr < 0 || rr >= SZ || cc < 0 || cc >= SZ) continue; int j = rr * SZ + cc; if (!shots[p][j]) { int w = 1; int pr = r - d[0], pc = c - d[1]; if (pr >= 0 && pr < SZ && pc >= 0 && pc < SZ && shots[p][pr * SZ + pc] == 2) w = 4; for (int k = 0; k < w; k++) tg.push_back(j); } } }
        if (!tg.empty() && (lv > 1 || rnd01() < 0.6f)) return tg[rndInt((int)tg.size())];
        if (lv == 3) { std::vector<int> dens(64, 0); std::vector<int> rem; for (int len : FLEET) rem.push_back(len);
            for (int len : rem) for (int r = 0; r < SZ; r++) for (int c = 0; c < SZ; c++) for (int h = 0; h < 2; h++) { bool ok = true; std::vector<int> cs; for (int i = 0; i < len; i++) { int rr = h ? r : r + i, cc = h ? c + i : c; if (rr >= SZ || cc >= SZ || shots[p][rr * SZ + cc] == 1) { ok = false; break; } cs.push_back(rr * SZ + cc); } if (ok) for (int x : cs) if (!shots[p][x]) dens[x]++; }
            int best = -1, bi = free[0]; for (int i : free) { int v = dens[i] * 10 + (int)(rnd01() * 5); if (v > best) { best = v; bi = i; } } return bi; }
        std::vector<int> par; for (int i : free) if (((i / SZ) + (i % SZ)) % 2 == 0) par.push_back(i); if (!par.empty()) return par[rndInt((int)par.size())]; return free[rndInt((int)free.size())];
    }
    void update(float dt, Input& in, UI&) override {
        flash = std::max(0.f, flash - dt);
        if (over) { endT += dt; if (endT > 2.f && !finished) { int a = 0, b = 0; for (int i = 0; i < 64; i++) { if (grid[0][i] && shots[1][i] != 2) a++; if (grid[1][i] && shots[0][i] != 2) b++; } std::vector<PResult> r = { { a, winner == 0 ? 'w' : 'l' }, { b, winner == 1 ? 'w' : 'l' } }; if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + pl[winner].name); } return; }
        for (int p = 0; p < 2; p++) if (pl[p].ai && !ready[p]) { randomFleet(p); ready[p] = true; if (!ready[1 - p]) placer = 1 - p; else beginFire(); }
        if (!(ready[0] && ready[1])) return;
        if (cover) { for (int p = 0; p < 2; p++) if (!pl[p].ai && pl[p].pad >= 0 && in.pads[pl[p].pad].down[B_A]) cover = false; return; }
        if (pl[turn].ai) { aiT += dt; if (aiT >= aiWait) fire(turn, aiShot(turn)); }
        else { Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; (void)pd; }
    }
    void boardDraw(Gfx& g, UI& ui, Rect R, int p, bool ownView, bool interactive, uint32_t salt) {
        float cs = R.w / SZ; g.rrect({ R.x - 6, R.y - 6, R.w + 12, R.h + 12 }, 10, rgb(0x5a3a1c)); g.rect(R, rgb(0x2a6a9a)); int o = ownView ? 1 - p : p;
        for (int i = 0; i < 64; i++) {
            Rect c{ R.x + (i % SZ) * cs, R.y + (i / SZ) * cs, cs, cs }; g.rrectLine(c, 0, 1, rgb(0xffffff, 60)); uint32_t id = hid("nv", (int)(salt + i));
            bool canFire = interactive && !ownView && !shots[p][i]; bool hit = ui.hit(id, c, interactive && (ownView ? false : canFire));
            if (ownView) { if (grid[p][i]) g.rrect(c.inset(3), 6, rgb(0x8a95a3)); int sv = shots[o][i]; if (sv == 1) g.circle(c.cx(), c.cy(), cs * 0.12f, rgb(0xffffff, 200)); if (sv == 2) { g.rrect(c.inset(3), 6, rgb(0xc8553a)); g.line(c.x + 5, c.y + 5, c.x + cs - 5, c.y + cs - 5, 3, rgb(0xffd54f)); g.line(c.x + cs - 5, c.y + 5, c.x + 5, c.y + cs - 5, 3, rgb(0xffd54f)); } }
            else { int sv = shots[p][i]; if (sv == 1) g.circle(c.cx(), c.cy(), cs * 0.14f, rgb(0xffffff, 220)); if (sv == 2) { g.circle(c.cx(), c.cy(), cs * 0.34f, rgb(0xe84a2a)); g.circle(c.cx(), c.cy(), cs * 0.18f, rgb(0xffd54f)); }
                if (i == lastShot && flash > 0 && turn == p) g.rrectLine(c, 3, 3, pal::gold); }
            if (ui.focused(id)) { ui.drawFocus(c, 4); focusCell = i; } if (hit && interactive && !ownView) { fire(p, i); return; }
        }
        if (interactive && !ownView) { int first = -1; for (int i = 0; i < 64; i++) if (!shots[p][i]) { first = i; break; } if (first >= 0) ui.setFocusIfNone(hid("nv", (int)(salt + first))); }
    }
    void draw(Gfx& g, UI& ui) override {
        bool firing = ready[0] && ready[1]; bool mineTurn = !over && (firing ? !pl[turn].ai : true);
        if (!firing) {   // colocación
            int p = placer; g.textShadow(pl[p].name + ": coloca tu flota", 765, 40, 24, pal::gold, F_BOLD, AL_C);
            Rect R{ 400, 100, 480, 480 }; float cs = R.w / SZ; g.rrect({ R.x - 8, R.y - 8, R.w + 16, R.h + 16 }, 10, rgb(0x5a3a1c)); g.rect(R, rgb(0x2a6a9a)); int len = left[p].empty() ? 0 : left[p][0]; focusCell = -1;
            for (int i = 0; i < 64; i++) { Rect c{ R.x + (i % SZ) * cs, R.y + (i / SZ) * cs, cs, cs }; g.rrectLine(c, 0, 1, rgb(0xffffff, 60)); if (grid[p][i]) g.rrect(c.inset(3), 6, rgb(0x8a95a3)); uint32_t id = hid("np", i); bool hit = ui.hit(id, c, len > 0);
                if (ui.focused(id)) { ui.drawFocus(c, 4); focusCell = i; } if (hit && len > 0) { if (place(p, i / SZ, i % SZ, len, horiz)) app->audio.play("place"); else app->toast("No cabe ahí"); return; } }
            if (focusCell >= 0 && len > 0) { std::vector<int> o; bool ok = cellsOk(p, focusCell / SZ, focusCell % SZ, len, horiz, o); if (!o.empty() || !ok) { for (int i = 0; i < len; i++) { int rr = focusCell / SZ + (horiz ? 0 : i), cc = focusCell % SZ + (horiz ? i : 0); if (rr < SZ && cc < SZ) { Rect c{ R.x + cc * cs, R.y + rr * cs, cs, cs }; g.rrect(c.inset(3), 6, ok ? rgb(0x7ad06a, 180) : rgb(0xf26a55, 180)); } } } }
            if (len > 0) { ui.setFocusIfNone(hid("np", 0)); std::string fl; for (int l : left[p]) fl += std::to_string(l) + " "; g.text("Falta colocar: " + fl + "(ahora el de " + std::to_string(len) + ")", 765, 600, 18, pal::cream, F_BOLD, AL_C); }
            if (ui.button(horiz ? "Girar (horizontal)" : "Girar (vertical)", { 910, 150, 250, 50 }, 2)) horiz = !horiz;
            if (ui.button("Al azar", { 910, 215, 250, 50 }, 2)) randomFleet(p);
            if (ui.button("Quitar todo", { 910, 280, 250, 50 }, 2)) { for (int i = 0; i < 64; i++) grid[p][i] = 0; left[p] = std::vector<int>(FLEET, FLEET + 5); }
            if (ui.button("¡Listo!", { 910, 345, 250, 50 }, 3, left[p].empty())) { ready[p] = true; if (!ready[1 - p]) { placer = 1 - p; msg = ""; } else beginFire(); app->audio.play("click"); return; }
            Pad* pd = (pl[p].pad >= 0 && pl[p].pad < Input::MAXP) ? &ui.in->pads[pl[p].pad] : nullptr; if (pd && pd->down[B_Y]) horiz = !horiz; if (pd && pd->down[B_X]) randomFleet(p);
            return;
        }
        if (cover && !over) { g.rrect({ 340, 200, 600, 260 }, 20, pal::panelEdge); g.rrectGrad({ 343, 203, 594, 254 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow("Pasa el mando a " + pl[turn].name, 640, 290, 28, pal::gold, F_TITLE, AL_C);
            if (ui.button("Listo, soy yo", { 490, 350, 300, 56 }, 3)) cover = false; ui.setFocusIfNone(hid("Listo, soy yo", 490 * 7 + 350)); return; }
        int v = turn;   // vista: tablero propio (pequeño) y del rival (grande)
        std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : mineTurn ? pl[turn].name + ": ¡dispara!" : "Dispara " + pl[turn].name + "…"; g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        // si el turno es de la IA, mostramos al humano (el primero que no es IA) sus dos tableros
        int viewer = !pl[v].ai ? v : (!pl[0].ai ? 0 : !pl[1].ai ? 1 : v); focusCell = -1;
        g.text("Tu flota", 400, 100, 18, pal::cream, F_BOLD, AL_C); boardDraw(g, ui, { 300, 130, 200, 200 }, viewer, true, false, 5000);
        g.text("Aguas de " + pl[1 - viewer].name, 900, 100, 18, pal::cream, F_BOLD, AL_C); boardDraw(g, ui, { 660, 130, 480, 480 }, viewer, false, !over && !pl[turn].ai && turn == viewer, 1000);
        int sunk = 0; for (int i = 0; i < 64; i++) if (grid[1 - viewer][i] && shots[viewer][i] == 2) sunk++; int mineLeft = 0; for (int i = 0; i < 64; i++) if (grid[viewer][i] && shots[1 - viewer][i] != 2) mineLeft++;
        g.text("Casillas de barco rival tocadas: " + std::to_string(sunk) + "/12", 400, 360, 16, pal::cream, F_BODY, AL_C, 220); g.text("Te quedan: " + std::to_string(mineLeft) + "/12", 400, 390, 16, pal::cream, F_BODY, AL_C, 220); g.text(msg, 400, 440, 17, pal::gold, F_BOLD, AL_C, 220);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Mover mira" }, { "A", "Colocar / disparar" }, { "Y", "Girar barco" }, { "X", "Flota al azar" } }; }
    std::string help() const override { return "Cada uno tiene una flota de 5 barcos (4, 3, 2, 2 y 1 casillas) en un mar de 8x8. Los colocas (Y gira, A coloca) y luego se turnan para disparar al tablero rival. Si aciertas, vuelves a disparar. Gana quien hunda toda la flota rival."; }
};
}
Game* makeNaval() { return new Naval(); }
