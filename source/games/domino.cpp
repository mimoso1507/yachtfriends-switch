// Dominó doble-6 (28 fichas), 2 a 4 jugadores, 7 fichas cada uno. Empieza quien tiene el doble más alto (o la ficha de más puntos).
// Modo Robo (comer del pozo, con opción "hasta poder") o Bloqueo (se pasa). Gana quien se queda sin fichas; si se bloquea, quien suma menos.
#include "../app.h"
#include <algorithm>

namespace {
typedef std::pair<int, int> T;
struct Opt { int i; char side; };
void pips(Gfx& g, float cx, float cy, float w, int n, Col c) {
    static const int L[7][9] = { {0}, {4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8} };
    static const int CNT[7] = { 0, 1, 2, 3, 4, 5, 6 };
    for (int k = 0; k < CNT[n]; k++) { int ix = L[n][k]; float x = cx + ((ix % 3) - 1) * w * 0.27f, y = cy + ((ix / 3) - 1) * w * 0.27f; g.circle(x, y, w * 0.075f, c); }
}
// ficha con su centro en (cx,cy); horiz: a a la izquierda y b a la derecha; si no, a arriba y b abajo. len = largo.
void drawTile(Gfx& g, float cx, float cy, int a, int b, bool horiz, float len, bool glow = false, bool back = false) {
    float w = len / 2; Rect r = horiz ? Rect{ cx - len / 2, cy - w / 2, len, w } : Rect{ cx - w / 2, cy - len / 2, w, len };
    g.rrect({ r.x + 2, r.y + 3, r.w, r.h }, w * 0.18f, rgb(0, 110));
    g.rrect(r, w * 0.18f, glow ? pal::gold : rgb(0x6a4a2a)); g.rrectGrad(r.inset(glow ? 3 : 1.5f), w * 0.15f, back ? rgb(0x3a6a4a) : rgb(0xfaf3dc), back ? rgb(0x2a4a34) : rgb(0xe4d8b4));
    if (back) return;
    if (horiz) { g.line(cx, cy - w * 0.38f, cx, cy + w * 0.38f, 1.5f, rgb(0x8a7a5a)); pips(g, cx - w / 2, cy, w, a, rgb(0x1a1a1a)); pips(g, cx + w / 2, cy, w, b, rgb(0x1a1a1a)); }
    else { g.line(cx - w * 0.38f, cy, cx + w * 0.38f, cy, 1.5f, rgb(0x8a7a5a)); pips(g, cx, cy - w / 2, w, a, rgb(0x1a1a1a)); pips(g, cx, cy + w / 2, w, b, rgb(0x1a1a1a)); }
}
struct Domino : Game {
    int N = 0, turn = 0, winner = -1, resigned = -1, passStreak = 0, sel = -1, boneyard = 0, le = -1, re = -1; bool block = false, eatUntil = false, over = false, blocked = false, pendSide = false;
    std::vector<std::vector<T>> hands; std::vector<T> pool, line; std::vector<std::array<bool, 7>> lack; std::string msg; float aiT = 0, aiWait = 0.8f, endT = 0, autoT = 0; int lastP = -1;
    void start() override {
        N = (int)pl.size(); block = opts.get("mode", 0) == 1; eatUntil = opts.get("eat", 0) != 0;
        for (int a = 0; a <= 6; a++) for (int b = a; b <= 6; b++) pool.push_back({ a, b });
        for (int i = (int)pool.size() - 1; i > 0; i--) std::swap(pool[i], pool[rndInt(i + 1)]);
        hands.assign(N, {}); lack.assign(N, {}); for (int p = 0; p < N; p++) for (int k = 0; k < 7; k++) { hands[p].push_back(pool.back()); pool.pop_back(); }
        int best = -1; turn = 0; for (int p = 0; p < N; p++) for (auto& t : hands[p]) if (t.first == t.second && t.first > best) { best = t.first; turn = p; }
        if (best < 0) { int bs = -1; for (int p = 0; p < N; p++) for (auto& t : hands[p]) if (t.first + t.second > bs) { bs = t.first + t.second; turn = p; } }
        for (auto& h : hands) std::sort(h.begin(), h.end()); msg = pl[turn].name + " empieza"; resetAi();
    }
    void resetAi() { aiT = 0; aiWait = 0.8f + rnd01() * 0.9f; autoT = 0; }
    int turnPlayer() const override { return over ? -1 : turn; }
    bool fits(const T& t, int e) const { return t.first == e || t.second == e; }
    std::vector<Opt> options(int p) const {
        std::vector<Opt> o; auto& h = hands[p];
        for (int i = 0; i < (int)h.size(); i++) { if (line.empty()) { o.push_back({ i, 'L' }); continue; } if (fits(h[i], le)) o.push_back({ i, 'L' }); if (fits(h[i], re)) o.push_back({ i, 'R' }); }
        return o;
    }
    void nextTurn() { turn = (turn - 1 + N) % N; sel = -1; pendSide = false; resetAi(); }
    int pipSum(int p) const { int s = 0; for (auto& t : hands[p]) s += t.first + t.second; return s; }
    void play(int p, int idx, char side) {
        T t = hands[p][idx]; hands[p].erase(hands[p].begin() + idx);
        if (line.empty()) { line.push_back(t); le = t.first; re = t.second; }
        else if (side == 'L') { if (t.first == le) { line.insert(line.begin(), { t.second, t.first }); le = t.second; } else { line.insert(line.begin(), t); le = t.first; } }
        else { if (t.first == re) { line.push_back(t); re = t.second; } else { line.push_back({ t.second, t.first }); re = t.first; } }
        passStreak = 0; lastP = p; app->audio.play("place"); msg = pl[p].name + " puso " + std::to_string(t.first) + "|" + std::to_string(t.second);
        if (hands[p].empty()) { over = true; winner = p; msg = "¡" + pl[p].name + " se quedó sin fichas!"; return; }
        nextTurn();
    }
    bool canPlay(int p) const { return !options(p).empty(); }
    void draw1(int p) {
        int n = 0; do { hands[p].push_back(pool.back()); pool.pop_back(); n++; } while (eatUntil && !canPlay(p) && !pool.empty());
        std::sort(hands[p].begin(), hands[p].end()); if (!line.empty()) { lack[p][le] = true; lack[p][re] = true; } msg = pl[p].name + " comió " + std::to_string(n); app->audio.play("click"); resetAi();
    }
    void pass(int p) {
        passStreak++; if (!line.empty()) { lack[p][le] = true; lack[p][re] = true; } msg = pl[p].name + " pasó";
        if (passStreak >= N) { over = true; blocked = true; int mn = 999; for (int q = 0; q < N; q++) mn = std::min(mn, pipSum(q)); int cnt = 0; for (int q = 0; q < N; q++) if (pipSum(q) == mn) { cnt++; winner = q; } if (cnt > 1) winner = -1; msg = "Juego bloqueado"; return; }
        nextTurn();
    }
    void resign(int p) override { over = true; resigned = p; winner = -1; int bs = 999; for (int q = 0; q < N; q++) if (q != p && pipSum(q) < bs) { bs = pipSum(q); winner = q; } }
    void aiMove() {
        int p = turn; auto o = options(p);
        if (o.empty()) { if (!block && !pool.empty()) { draw1(p); return; } pass(p); return; }
        int lv = std::max(1, std::min(3, pl[p].level)); float err = lv == 1 ? 0.4f : lv == 2 ? 0.12f : 0.01f, noise = lv == 1 ? 12.f : lv == 2 ? 2.f : 0.3f, look = lv == 1 ? 0.f : lv == 2 ? 1.f : 1.6f, opp = lv == 3 ? 3.f : 0.f;
        if (rnd01() < err) { auto& x = o[rndInt((int)o.size())]; play(p, x.i, x.side); return; }
        float bs = -1e9f; Opt bo = o[0];
        for (auto& x : o) {
            T t = hands[p][x.i]; float sc = t.first + t.second + (t.first == t.second ? 4 : 0);
            if (!line.empty()) {
                int l = le, r = re; if (x.side == 'L') l = t.first == l ? t.second : t.first; else r = t.first == r ? t.second : t.first;
                auto has = [&](int e) { int c = 0; for (int k = 0; k < (int)hands[p].size(); k++) if (k != x.i && fits(hands[p][k], e)) c++; return c; };
                sc += look * 1.5f * (has(l) + has(r)); if (l == r) sc -= look; if (has(l) == 0 && has(r) == 0 && hands[p].size() > 1) sc -= 5 * look;
                if (opp > 0) for (int q = 0; q < N; q++) if (q != p) sc += opp * ((lack[q][l] ? 1 : 0) + (lack[q][r] ? 1 : 0));
            }
            sc += rnd01() * noise; if (sc > bs) { bs = sc; bo = x; }
        }
        play(p, bo.i, bo.side);
    }
    void update(float dt, Input& in, UI&) override {
        if (over) {
            endT += dt; if (endT > 2.f && !finished) {
                std::vector<PResult> r; int win = 0; if (winner >= 0) for (int q = 0; q < N; q++) if (q != winner) win += pipSum(q);
                for (int p = 0; p < N; p++) r.push_back({ p == winner ? win : 0, winner < 0 ? 'd' : p == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x';
                finish(r, winner < 0 ? "Empate" : "Ganó " + pl[winner].name + (blocked ? " (juego bloqueado)" : "") + " · +" + std::to_string(win) + " pts");
            }
            return;
        }
        if (pl[turn].ai) { aiT += dt; if (aiT >= aiWait) aiMove(); return; }
        Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr;
        if (pd && pd->down[B_B] && pendSide) pendSide = false;
        if (!canPlay(turn) && (block || pool.empty())) { autoT += dt; if (autoT > 1.f) pass(turn); }
    }
    void tryTile(int i) {
        auto o = options(turn); std::vector<char> sd; for (auto& x : o) if (x.i == i) sd.push_back(x.side);
        if (sd.empty()) { app->toast("Esa ficha no encaja en ningún extremo"); return; }
        if (sd.size() == 1) { play(turn, i, sd[0]); return; }
        sel = i; pendSide = true;
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = over ? "Fin de la partida" : mine ? pl[turn].name + ": tu turno" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(230.f, 940.f / N);
        for (int p = 0; p < N; p++) { Rect r{ 290 + p * (pw + 2), 66, pw - 4, 46 }; bool on = p == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.text(pl[p].emoji + " " + pl[p].name, r.x + 10, r.y + 15, 14, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 20); g.text(std::to_string(hands[p].size()) + " fichas" + (over ? " · " + std::to_string(pipSum(p)) + " pts" : ""), r.x + 10, r.y + 34, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 20); }
        g.text(std::string(block ? "Bloqueo" : "Robo") + " · pozo: " + std::to_string(pool.size()), 1230, 130, 14, pal::cream, F_BODY, AL_R); g.text(msg, 765, 140, 17, pal::cream, F_BODY, AL_C, 800);
        // mesa
        g.rrectGrad({ 290, 160, 950, 250 }, 24, rgb(0x2f7a4a), rgb(0x1f5a36)); g.rrectLine({ 290, 160, 950, 250 }, 24, 4, rgb(0x6b4224));
        int n = (int)line.size(); if (n == 0) g.text("Mesa vacía: pon la primera ficha", 765, 285, 20, pal::cream, F_BOLD, AL_C);
        int rows = n > 18 ? (n > 36 ? 3 : 2) : 1, per = std::max(1, (n + rows - 1) / rows); float len = std::min(52.f, 900.f / std::max(1, per) - 4), gap = 4;
        for (int rr = 0; rr < rows; rr++) {
            int a = rr * per, b = std::min(n, a + per); if (a >= b) break; float rowW = 0; for (int i = a; i < b; i++) rowW += (line[i].first == line[i].second ? len / 2 : len) + gap; float x = 765 - rowW / 2; float y = rows == 1 ? 285 : 215 + rr * 70.f;
            for (int i = a; i < b; i++) { bool d = line[i].first == line[i].second; float adv = d ? len / 2 : len; drawTile(g, x + adv / 2, y, line[i].first, line[i].second, !d, len, (i == 0 || i == n - 1)); x += adv + gap; }
        }
        if (n > 0) g.text("Extremos: " + std::to_string(le) + " (izq) y " + std::to_string(re) + " (der)", 765, 392, 16, pal::gold, F_BOLD, AL_C);
        auto& h = hands[turn];
        if (mine) {
            auto o = options(turn); bool can = !o.empty();
            if (pendSide && sel >= 0) {
                g.text("¿De qué lado la pones?  (B cancela)", 765, 440, 19, pal::cream, F_BOLD, AL_C);
                if (ui.button("◀ Izquierda (" + std::to_string(le) + ")", { 470, 458, 280, 48 }, 3)) { int s = sel; play(turn, s, 'L'); return; }
                if (ui.button("Derecha (" + std::to_string(re) + ") ▶", { 780, 458, 280, 48 }, 3)) { int s = sel; play(turn, s, 'R'); return; }
                ui.setFocusIfNone(hid("◀ Izquierda (" + std::to_string(le) + ")", 470 * 7 + 458));
            } else if (!can && !block && !pool.empty()) { g.text("No tienes ficha que encaje", 765, 440, 19, pal::cream, F_BOLD, AL_C); if (ui.button("Comer del pozo", { 565, 458, 400, 48 }, 3)) { draw1(turn); return; } ui.setFocusIfNone(hid("Comer del pozo", 565 * 7 + 458)); }
            else if (!can) g.text("No puedes jugar: pasas el turno…", 765, 450, 19, pal::cream, F_BOLD, AL_C);
            else g.text("Elige una ficha para jugar", 765, 440, 19, pal::cream, F_BOLD, AL_C);
        }
        if (peek) {
            int m = (int)h.size(); float len2 = 84, step = std::min(len2 * 0.5f + 16, 900.f / std::max(1, m));
            for (int i = 0; i < m; i++) {
                bool ok = false; for (auto& x : options(turn)) if (x.i == i) ok = true; Rect r{ 765 - (m * step) / 2 + i * step, 530.f - (i == sel ? 18 : 0), len2 / 2 + 6, len2 + 6 }; uint32_t id = hid("dh", i); bool hit = ui.hit(id, r, mine && !pendSide);
                drawTile(g, r.cx(), r.cy(), h[i].first, h[i].second, false, len2, i == sel || ok); if (!ok) g.rrect(r, 8, rgb(0, 90)); if (ui.focused(id)) ui.drawFocus(r, 8); if (hit) tryTile(i);
            }
            if (onPhone(turn)) coverHand(g, ui, "dh", m, turn);
            if (mine && m && !pendSide && (pool.empty() || canPlay(turn) || block)) ui.setFocusIfNone(hid("dh", 0));
        } else if (mine) g.text("Mantén L para ver tus fichas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; std::string s = me ? "Tu turno · tus fichas:\n" : "Tus fichas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("dh", (int)i, me) + "[" + std::to_string(hands[p][i].first) + "|" + std::to_string(hands[p][i].second) + "]" + (me && (int)i == sel ? " ⬆" : "") + ((i % 2) ? "\n" : "    "); return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Jugar ficha" }, { "B", "Cancelar" }, { "L", "Ver tus fichas" } }; }
    std::string help() const override { return "Cada jugador recibe 7 fichas. Empieza quien tiene el doble más alto. Pon una ficha que coincida con un extremo de la fila; si encaja en los dos, eliges el lado. En modo Robo, si no puedes jugar comes del pozo; en Bloqueo pasas. Gana quien se queda sin fichas o, si el juego se bloquea, quien suma menos puntos."; }
};
}
Game* makeDomino() { return new Domino(); }
