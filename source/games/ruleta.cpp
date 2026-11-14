// Ruleta europea (0-36) o americana (0 y 00), 1-8 jugadores, fichas simbólicas. Apuestan por turnos (la IA apuesta sola), gira la rueda y se pagan las apuestas.
// Pleno 35:1, docena y columna 2:1, rojo/negro, par/impar y 1-18/19-36 1:1. Gana quien termine con más fichas tras las rondas elegidas.
#include "../app.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace {
const int ORDER_EU[37] = { 0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10, 5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };
const int ORDER_US[38] = { 0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2 };
const int REDS[18] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
const int CHIPS[4] = { 5, 10, 25, 100 }, MINB = 5;
const char* OUTL[12] = { "Rojo", "Negro", "Par", "Impar", "1-18", "19-36", "1ª docena", "2ª docena", "3ª docena", "Col 1", "Col 2", "Col 3" };
bool isRed(int n) { for (int r : REDS) if (r == n) return true; return false; }
bool isZero(int n) { return n == 0 || n == 37; }
std::string lab(int n) { return n == 37 ? "00" : std::to_string(n); }
bool wins(int k, int n) {   // k 0..37 pleno; 100.. afuera
    if (k < 100) return k == n; if (isZero(n)) return false;
    switch (k - 100) { case 0: return isRed(n); case 1: return !isRed(n); case 2: return n % 2 == 0; case 3: return n % 2 == 1; case 4: return n <= 18; case 5: return n >= 19; case 6: return n <= 12; case 7: return n >= 13 && n <= 24; case 8: return n >= 25;
        case 9: return n % 3 == 1; case 10: return n % 3 == 2; default: return n % 3 == 0; }
}
int payout(int k, int amt, int n) { if (!wins(k, n)) return 0; if (k < 100) return amt * 36; return amt * ((k - 100) >= 6 ? 3 : 2); }

struct Ruleta : Game {
    int N = 0, rounds = 10, round = 1, bp = 0, chipSel = 0, spinN = 0; bool zero2 = false, over = false; int phase = 0;   // 0 apuestan, 1 gira, 2 resultado
    std::vector<int> chips; std::vector<std::map<int,int>> bets; std::vector<std::vector<std::pair<int,int>>> log, lastBets; std::vector<int> delta; std::vector<int> hist;
    float t = 0, aiT = 0, rot0 = 0, rotF = 0, rot = 0, endT = 0; std::string msg; int resigned = -1; std::vector<bool> gone;
    const int* order() const { return zero2 ? ORDER_US : ORDER_EU; } int M() const { return zero2 ? 38 : 37; }
    void start() override {
        N = (int)pl.size(); int r = opts.get("rounds", 1); rounds = r == 0 ? 5 : r == 2 ? 20 : 10; zero2 = opts.get("us", 0) != 0; int c = opts.get("chips", 1); int sc = c == 0 ? 500 : c == 2 ? 2000 : 1000;
        chips.assign(N, sc); bets.assign(N, {}); log.assign(N, {}); lastBets.assign(N, {}); delta.assign(N, 0); gone.assign(N, false); beginBetting();
    }
    int turnPlayer() const override { return over || phase != 0 ? -1 : bp; }
    int staked(int p) const { int s = 0; for (auto& kv : bets[p]) s += kv.second; return s; }
    bool broke(int p) const { return chips[p] < MINB; }
    bool bet(int p, int k, int amt) { if (staked(p) + amt > chips[p]) return false; bets[p][k] += amt; log[p].push_back({ k, amt }); return true; }
    void undo(int p) { if (log[p].empty()) return; auto l = log[p].back(); log[p].pop_back(); bets[p][l.first] -= l.second; if (bets[p][l.first] <= 0) bets[p].erase(l.first); }
    void beginBetting() { phase = 0; for (int p = 0; p < N; p++) { bets[p].clear(); log[p].clear(); } bp = -1; nextBettor(); msg = "Ronda " + std::to_string(round) + " de " + std::to_string(rounds); }
    void nextBettor() {
        bp++; while (bp < N && (broke(bp) || gone[bp])) bp++;
        aiT = 0; if (bp >= N) spin();
    }
    void aiBet(int p) {
        int budget = std::max(0, std::min(chips[p], (int)(chips[p] * (0.06 + rnd01() * 0.12)) + MINB)); int left = budget, nb = 1 + rndInt(4); std::map<int,bool> used;
        for (int i = 0; i < nb && left >= MINB; i++) {
            int k = rnd01() < 0.35 ? rndInt(M()) : 100 + rndInt(12); if (used[k]) continue; used[k] = true; float share = i == nb - 1 ? (float)left : (float)left / (nb - i);
            float lim = k < 100 ? std::min(share, std::max((float)MINB, budget * 0.25f)) : share; int amt = 0; for (int c : CHIPS) if (c <= lim) amt = c; if (amt < MINB) amt = MINB; if (amt > left) { amt = 0; for (int c : CHIPS) if (c <= left) amt = c; } if (amt < MINB) break;
            bet(p, k, amt); left -= amt; }
        if (!staked(p) && chips[p] >= MINB) bet(p, 100 + rndInt(6), MINB);
    }
    void spin() {
        phase = 1; t = 0; spinN = rndInt(M()); int idx = 0; for (int i = 0; i < M(); i++) if (order()[i] == spinN) idx = i; float step = 6.2831853f / M(); rotF = -1.5707963f - idx * step - step * 0.0f; rot0 = rotF - 6.2831853f * 5; rot = rot0; app->audio.play("roll"); msg = "¡No va más!";
    }
    void settle() {
        phase = 2; t = 0; hist.push_back(spinN); for (int p = 0; p < N; p++) { int ret = 0; for (auto& kv : bets[p]) ret += payout(kv.first, kv.second, spinN); delta[p] = ret - staked(p); chips[p] += delta[p]; if (!log[p].empty()) lastBets[p] = log[p]; }
        msg = "Salió el " + lab(spinN); app->audio.play("win");
    }
    void resign(int p) override { gone[p] = true; resigned = p; int left = 0; for (int q = 0; q < N; q++) if (!gone[q]) left++; if (left == 0 || N == left + 1) { if (N > 1 && left <= 1) over = true; } if (phase == 0 && bp == p) nextBettor(); }
    void update(float dt, Input& in, UI&) override {
        if (over) {
            endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; int best = -1; for (int p = 0; p < N; p++) if (!gone[p]) best = std::max(best, chips[p]); int nb = 0; for (int p = 0; p < N; p++) if (!gone[p] && chips[p] == best) nb++;
                for (int p = 0; p < N; p++) r.push_back({ chips[p], gone[p] ? 'x' : chips[p] == best ? (nb > 1 ? 'd' : 'w') : 'l' }); std::string w; for (int p = 0; p < N; p++) if (!gone[p] && chips[p] == best) w += (w.empty() ? "" : " y ") + pl[p].name; finish(r, "Ganó " + w + " con " + std::to_string(best) + " fichas"); }
            return;
        }
        if (phase == 0) { if (bp < N && pl[bp].ai) { aiT += dt; if (aiT > 0.7f) { aiBet(bp); nextBettor(); } } else if (bp < N) { Pad* pd = (pl[bp].pad >= 0 && pl[bp].pad < Input::MAXP) ? &in.pads[pl[bp].pad] : nullptr; if (pd) { if (pd->down[B_Y]) undo(bp); if (pd->down[B_B]) chipSel = (chipSel + 1) % 4; if (pd->down[B_X]) nextBettor(); } } }
        else if (phase == 1) { t += dt; float u = std::min(1.f, t / 5.f), e = 1 - powf(1 - u, 3); rot = rot0 + (rotF - rot0) * e; if (u >= 1) settle(); }
        else { t += dt; bool adv = t > 5.f; for (int p = 0; p < N; p++) if (!pl[p].ai && !gone[p] && pl[p].pad >= 0 && in.pads[pl[p].pad].down[B_A] && t > 1.f) adv = true;
            if (adv) { bool any = false; for (int p = 0; p < N; p++) if (!broke(p) && !gone[p]) any = true; if (round >= rounds || !any) over = true; else { round++; beginBetting(); } } }
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && phase == 0 && bp < N && !pl[bp].ai; int V = mine ? bp : -1;
        std::string banner = over ? "Fin de la partida" : phase == 0 ? (mine ? pl[bp].name + ": haz tus apuestas" : "Apuesta " + pl[bp].name + "…") : phase == 1 ? "La rueda gira…" : "Salió el " + lab(spinN);
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        // mesa
        float cw = 48, ch = 44, x0 = 340, y0 = 76; g.rrectGrad({ 296, 66, 636, 360 }, 18, rgb(0x1f6a3c), rgb(0x175530)); g.rrectLine({ 296, 66, 636, 360 }, 18, 4, rgb(0x6b4224));
        auto chipOn = [&](Rect r, int k) { if (V < 0) return; auto it = bets[V].find(k); if (it != bets[V].end()) { g.circle(r.cx(), r.cy(), 15, rgb(0xffd54f)); g.ring(r.cx(), r.cy(), 15, 2, rgb(0x7a4a00)); g.text(std::to_string(it->second), r.cx(), r.cy(), 13, rgb(0x3a2600), F_BOLD, AL_C); } };
        int firstId = -1;
        auto cell = [&](Rect r, int k, const std::string& label, Col c, Col tc) -> bool {
            uint32_t id = hid("rc", k); bool ok = mine && staked(V) + CHIPS[chipSel] <= chips[V]; bool hit = ui.hit(id, r, ok); if (firstId < 0 && ok) firstId = k;
            g.rect(r, c); g.rrectLine(r, 0, 1, rgb(0xe8e0c0, 160)); g.text(label, r.cx(), r.cy(), r.h > 40 ? 18.f : 15.f, tc, F_BOLD, AL_C, r.w - 4);
            if (phase == 2 && wins(k, spinN) && (k >= 100 || k == spinN)) g.rrectLine(r.inset(2), 0, 3, pal::gold);
            chipOn(r, k); if (ui.focused(id)) ui.drawFocus(r, 4); return hit; };
        auto place = [&](int k) { if (!bet(V, k, CHIPS[chipSel])) app->toast("No te alcanzan las fichas"); else app->audio.play("click"); };
        // ceros
        if (zero2) { if (cell({ x0 - 44, y0, 44, ch * 1.5f }, 0, "0", rgb(0x2a8a50), rgb(0xffffff))) place(0); if (cell({ x0 - 44, y0 + ch * 1.5f, 44, ch * 1.5f }, 37, "00", rgb(0x2a8a50), rgb(0xffffff))) place(37); }
        else if (cell({ x0 - 44, y0, 44, ch * 3 }, 0, "0", rgb(0x2a8a50), rgb(0xffffff))) place(0);
        for (int c = 0; c < 12; c++) for (int r = 0; r < 3; r++) { int n = c * 3 + (3 - r); Rect rc{ x0 + c * cw, y0 + r * ch, cw, ch }; if (cell(rc, n, std::to_string(n), isRed(n) ? rgb(0xc8332a) : rgb(0x1a1a1a), rgb(0xffffff))) place(n); }
        for (int r = 0; r < 3; r++) if (cell({ x0 + 12 * cw, y0 + r * ch, 44, ch }, 111 - r, "2:1", rgb(0x175530), pal::cream)) place(111 - r);
        // docenas y afuera
        for (int d = 0; d < 3; d++) if (cell({ x0 + d * 4 * cw, y0 + 3 * ch + 4, 4 * cw, 40 }, 106 + d, OUTL[6 + d], rgb(0x175530), pal::cream)) place(106 + d);
        Col oc[6] = { rgb(0xc8332a), rgb(0x1a1a1a), rgb(0x175530), rgb(0x175530), rgb(0x175530), rgb(0x175530) };
        for (int i = 0; i < 6; i++) if (cell({ x0 + i * 2 * cw, y0 + 3 * ch + 48, 2 * cw, 40 }, 100 + i, OUTL[i], oc[i], rgb(0xffffff))) place(100 + i);
        if (firstId >= 0) ui.setFocusIfNone(hid("rc", firstId));
        // controles de apuesta
        if (mine) {
            g.text(pl[V].name + ": " + std::to_string(chips[V]) + " fichas · apostado " + std::to_string(staked(V)), 614, 376, 17, pal::cream, F_BOLD, AL_C, 600);
            for (int i = 0; i < 4; i++) { Rect r{ 320.f + i * 70, 392, 62, 30 }; if (ui.button(std::to_string(CHIPS[i]), r, i == chipSel ? 3 : 2)) chipSel = i; }
            if (ui.button("Deshacer", { 606, 392, 100, 30 }, 2, !log[V].empty())) undo(V);
            if (ui.button("Repetir", { 712, 392, 100, 30 }, 2, log[V].empty() && !lastBets[V].empty())) { for (auto& l : lastBets[V]) bet(V, l.first, l.second); }
            if (ui.button("¡Listo!", { 818, 392, 100, 30 }, 3)) { nextBettor(); return; }
        } else if (phase == 0 && !over) g.text("Esperando apuestas…", 614, 396, 18, pal::cream, F_BOLD, AL_C);
        // rueda
        float wx = 1085, wy = 190, R = 110; g.circle(wx, wy, R + 8, rgb(0x6b4224)); int m = M(); float step = 6.2831853f / m;
        for (int i = 0; i < m; i++) { int n = order()[i]; float a0 = rot + i * step - step / 2, a1 = a0 + step; Col c = isZero(n) ? rgb(0x2a8a50) : isRed(n) ? rgb(0xc8332a) : rgb(0x1a1a1a); g.tri(wx, wy, wx + cosf(a0) * R, wy + sinf(a0) * R, wx + cosf(a1) * R, wy + sinf(a1) * R, c); }
        g.circle(wx, wy, R * 0.62f, rgb(0xd8b46a)); g.circle(wx, wy, R * 0.55f, rgb(0x3a2410)); for (int i = 0; i < m; i++) { float a = rot + i * step; g.text(lab(order()[i]), wx + cosf(a) * R * 0.82f, wy + sinf(a) * R * 0.82f, 9, rgb(0xffffff), F_BOLD, AL_C); }
        g.tri(wx - 9, wy - R - 16, wx + 9, wy - R - 16, wx, wy - R + 2, pal::gold); if (phase != 1 || t > 4.6f) g.circle(wx, wy - R * 0.9f, 6, rgb(0xffffff)); else g.circle(wx + cosf(-rot * 1.6f - 1.5f) * R * 0.88f, wy + sinf(-rot * 1.6f - 1.5f) * R * 0.88f, 6, rgb(0xffffff));
        if (phase == 2) g.textShadow(lab(spinN), wx, wy, 40, pal::gold, F_TITLE, AL_C);
        // historial
        std::string h; for (int i = (int)hist.size() - 1; i >= 0 && i > (int)hist.size() - 13; i--) h += lab(hist[i]) + " "; g.text(hist.empty() ? "" : "Últimos: " + h, 1085, 330, 14, pal::cream, F_BODY, AL_C, 320);
        // jugadores
        float pw = std::min(230.f, 960.f / N); for (int p = 0; p < N; p++) { Rect r{ 296 + p * (pw + 2), 450, pw - 4, 60 }; bool on = phase == 0 && p == bp; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.text(pl[p].emoji + " " + pl[p].name, r.x + 10, r.y + 16, 14, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 20); g.text(std::to_string(chips[p]) + " fichas", r.x + 10, r.y + 36, 14, rgb(0xd8b46a), F_BODY, AL_L, pw - 20);
            if (phase == 2 && !gone[p]) g.text((delta[p] >= 0 ? "+" : "") + std::to_string(delta[p]), r.x + pw - 14, r.y + 36, 16, delta[p] > 0 ? rgb(0x7ad06a) : delta[p] < 0 ? rgb(0xf26a55) : pal::cream, F_BOLD, AL_R); }
        g.text(msg + (zero2 ? " · americana" : " · europea"), 765, 540, 17, pal::cream, F_BODY, AL_C, 900);
        if (phase == 2) g.text("A: seguir", 765, 566, 15, pal::cream, F_BODY, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Apostar" }, { "B", "Cambiar ficha" }, { "Y", "Deshacer" }, { "X", "¡Listo!" } }; }
    std::string help() const override { return "Apuestan por turnos (la IA apuesta sola). Elige el valor de la ficha (B) y apuesta en un número (35 a 1), docena o columna (2 a 1) o rojo/negro, par/impar, 1-18/19-36 (1 a 1). Con ¡Listo! (X) terminas. Después gira la rueda y se pagan las apuestas. El 0 pierde las apuestas de afuera. Gana quien tenga más fichas al final."; }
};
}
Game* makeRuleta() { return new Ruleta(); }
