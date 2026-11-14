// Mente maestra: dos mangas (se invierten los roles). Código de 4 colores, pistas negras/blancas. IA igual que la de PC.
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const int SLOTS = 4;
typedef std::array<int, SLOTS> Code;
struct FB { int black, white; };
FB feedback(Code s, Code g) {
    int black = 0; for (int i = 0; i < SLOTS; i++) if (s[i] == g[i]) { black++; s[i] = g[i] = -1; }
    int white = 0;
    for (int i = 0; i < SLOTS; i++) { if (s[i] < 0) continue; for (int j = 0; j < SLOTS; j++) if (g[j] == s[i]) { white++; g[j] = -2; break; } }
    return { black, white };
}
struct Try { Code g; FB fb; };
Code randCode(int colors) { Code c; for (int& v : c) v = 1 + rndInt(colors); return c; }
Code nextGuess(int colors, const std::vector<Try>& tries, float err, bool smart) {
    if (tries.empty()) { int a = 1 + rndInt(colors), b = 1 + rndInt(colors); if (colors > 1) while (b == a) b = 1 + rndInt(colors); return { a, a, b, b }; }
    if (rnd01() < err) return randCode(colors);
    std::vector<Code> cands; Code g{}; int tot = 1; for (int k = 0; k < SLOTS; k++) tot *= colors;
    for (int n = 0; n < tot; n++) {
        int x = n; for (int k = 0; k < SLOTS; k++) { g[k] = 1 + (x % colors); x /= colors; }
        bool ok = true; for (auto& t : tries) { FB f = feedback(g, t.g); if (f.black != t.fb.black || f.white != t.fb.white) { ok = false; break; } }
        if (ok) cands.push_back(g);
    }
    if (cands.empty()) return randCode(colors);
    if (smart && cands.size() > 2) {
        std::vector<Code> samp; for (int k = 0; k < 14; k++) samp.push_back(cands[rndInt((int)cands.size())]);
        std::vector<Code> ev; if (cands.size() > 250) for (int k = 0; k < 250; k++) ev.push_back(cands[rndInt((int)cands.size())]); else ev = cands;
        Code best = samp[0]; int bv = -1;
        for (auto& c : samp) { bool seen[64] = {}; int n = 0; for (auto& o : ev) { FB f = feedback(o, c); int key = f.black * 5 + f.white; if (!seen[key]) { seen[key] = true; n++; } } if (n > bv) { bv = n; best = c; } }
        return best;
    }
    return cands[rndInt((int)cands.size())];
}
const uint32_t PAL[8] = { 0xe53935, 0xfdd835, 0x43a047, 0xe91ee0, 0x1e88e5, 0xfb8c00, 0xffffff, 0x212121 };
struct Lvl { float err; bool smart; float d0, d1; };
const Lvl LVL[4] = { {}, { 0.4f, false, 1.4f, 1.0f }, { 0.1f, false, 0.9f, 0.9f }, { 0.01f, true, 0.5f, 0.6f } };

struct MM : Game {
    int colors = 6, maxTries = 10, manga = 0, maker = 0, score[2] = {}; bool waiting = true, mangaOver = false, won = false;
    Code secret{}, cur{}; std::vector<Try> tries; float aiT = 0, aiWait = 1, endT = 0; int resigned = -1;
    void start() override { colors = opts.get("colors", 1) == 0 ? 4 : opts.get("colors", 1) == 2 ? 8 : 6; int t = opts.get("tries", 1); maxTries = t == 0 ? 8 : t == 2 ? 12 : 10; startManga(); }
    void startManga() { waiting = true; mangaOver = false; won = false; tries.clear(); cur = {}; endT = 0; resetAi(); }
    void resetAi() { aiT = 0; int who = waiting ? maker : 1 - maker; const Lvl& P = LVL[std::max(1, std::min(3, pl[who].level))]; aiWait = P.d0 + rnd01() * P.d1; }
    int turnPlayer() const override { return mangaOver ? -1 : waiting ? maker : 1 - maker; }
    int curCount() const { int n = 0; for (int v : cur) if (v) n++; return n; }
    void place(int color) { for (int& v : cur) if (!v) { v = color; app->audio.play("click"); return; } }
    void removeLast() { for (int i = SLOTS - 1; i >= 0; i--) if (cur[i]) { cur[i] = 0; return; } }
    void confirm() {
        if (curCount() < SLOTS) return;
        if (waiting) { secret = cur; waiting = false; cur = {}; resetAi(); app->audio.play("place"); return; }
        guess(cur); cur = {};
    }
    void guess(Code g) {
        FB fb = feedback(secret, g); tries.push_back({ g, fb }); app->audio.play("place");
        if (fb.black == SLOTS) { mangaOver = true; won = true; } else if ((int)tries.size() >= maxTries) mangaOver = true;
        if (mangaOver) { int br = 1 - maker; score[br] += won ? std::max(1, maxTries - (int)tries.size() + 1) : 0; endT = 0; } else resetAi();
    }
    void resign(int p) override { resigned = p; mangaOver = true; manga = 1; }
    void update(float dt, Input& in, UI& ui) override {
        if (mangaOver) {
            endT += dt;
            if (endT > 2.2f) {
                if (manga == 0 && resigned < 0) { manga = 1; maker = 1 - maker; startManga(); }
                else if (!finished) {
                    int a = score[0], b = score[1]; int w = resigned >= 0 ? 1 - resigned : a > b ? 0 : b > a ? 1 : -1;
                    std::vector<PResult> r = { { a, w == 0 ? 'w' : w < 0 ? 'd' : 'l' }, { b, w == 1 ? 'w' : w < 0 ? 'd' : 'l' } };
                    if (resigned >= 0) r[resigned].r = 'x';
                    finish(r, "Puntaje: " + std::to_string(a) + " - " + std::to_string(b));
                }
            }
            return;
        }
        int who = turnPlayer();
        if (!pl[who].ai) {
            Pad* p = (pl[who].pad >= 0 && pl[who].pad < Input::MAXP) ? &in.pads[pl[who].pad] : nullptr;
            if (p) { if (p->down[B_B]) removeLast(); if (p->down[B_X]) confirm(); }
            return;
        }
        aiT += dt; if (aiT < aiWait) return;
        const Lvl& P = LVL[std::max(1, std::min(3, pl[who].level))];
        if (waiting) { secret = randCode(colors); waiting = false; resetAi(); }
        else guess(nextGuess(colors, tries, P.err, P.smart));
        (void)ui;
    }
    void peg(Gfx& g, float cx, float cy, float r, int v, bool hole = true) {
        if (!v) { if (hole) { g.circle(cx, cy, r, rgb(0x1c1008)); g.ring(cx, cy, r, 1.5f, rgb(0x5a3a1c)); } return; }
        g.circle(cx + 1, cy + 2.5f, r, rgb(0, 110)); g.circleGrad(cx - r * 0.2f, cy - r * 0.25f, r, mix(rgb(PAL[v - 1]), rgb(0xffffff), 0.35f), rgb(PAL[v - 1]));
        g.ring(cx, cy, r, 1.5f, rgb(0, 90));
    }
    void draw(Gfx& g, UI& ui) override {
        bool human = !mangaOver && !pl[turnPlayer()].ai;
        // tablero
        Rect P{ 290, 22, 520, 636 }; g.rrect({ P.x + 4, P.y + 8, P.w, P.h }, 18, rgb(0, 110)); g.rrect(P, 18, pal::panelEdge); g.rrectGrad(P.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
        // código secreto
        bool reveal = mangaOver || (waiting && human);
        g.text("Código", P.x + 22, P.y + 36, 16, rgb(0xd8b46a), F_BOLD, AL_L);
        const Code& show = waiting ? cur : secret;
        for (int k = 0; k < SLOTS; k++) { float cx = P.x + 180 + k * 52, cy = P.y + 36; if (reveal && show[k]) peg(g, cx, cy, 19, show[k]); else { g.circle(cx, cy, 19, rgb(0x120c08)); g.text(waiting ? "" : "?", cx, cy, 20, pal::gold, F_TITLE, AL_C); } }
        float rowH = std::min(50.f, (P.h - 90) / maxTries), y0 = P.y + 76;
        for (int t = 0; t < maxTries; t++) {
            float cy = y0 + t * rowH + rowH / 2; bool isCur = !waiting && !mangaOver && t == (int)tries.size();
            if (isCur) g.rrect({ P.x + 12, cy - rowH / 2 + 1, P.w - 24, rowH - 2 }, 10, rgb(0xffe27a, 40));
            g.text(std::to_string(t + 1), P.x + 28, cy, 14, rgb(0xd8b46a), F_BODY, AL_C);
            for (int k = 0; k < SLOTS; k++) { float cx = P.x + 80 + k * 52; int v = t < (int)tries.size() ? tries[t].g[k] : isCur ? cur[k] : 0; peg(g, cx, cy, std::min(17.f, rowH * 0.36f), v); }
            if (t < (int)tries.size()) {
                int bl = tries[t].fb.black, wh = tries[t].fb.white;
                for (int k = 0; k < SLOTS; k++) { float cx = P.x + 330 + (k % 2) * 22, cy2 = cy + ((k / 2) ? 10 : -10); Col c = k < bl ? rgb(0x111111) : k < bl + wh ? rgb(0xffffff) : rgb(0x3a2414); g.circle(cx, cy2, 7, c); g.ring(cx, cy2, 7, 1, rgb(0x888888)); }
                g.text(std::to_string(bl) + " ● " + std::to_string(wh) + " ○", P.x + 400, cy, 15, pal::cream, F_BODY, AL_L);
            }
        }
        // panel derecho
        Rect R{ 830, 22, 420, 636 }; g.rrect({ R.x + 4, R.y + 8, R.w, R.h }, 18, rgb(0, 110)); g.rrect(R, 18, pal::panelEdge); g.rrectGrad(R.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
        g.textShadow("MANGA " + std::to_string(manga + 1) + " DE 2", R.cx(), R.y + 40, 24, pal::gold, F_TITLE, AL_C);
        for (int p = 0; p < 2; p++) g.text(pl[p].emoji + " " + pl[p].name + ": " + std::to_string(score[p]) + (maker == p ? "  (arma)" : "  (adivina)"), R.x + 24, R.y + 84 + p * 28, 17, p == turnPlayer() ? pal::gold : pal::white, F_BOLD, AL_L, R.w - 40);
        std::string msg = mangaOver ? (won ? "¡Descifrado en " + std::to_string(tries.size()) + "!" : "No lo descifraron") : waiting ? (pl[maker].ai ? pl[maker].name + " arma su código…" : pl[maker].name + ": armá tu código (que el otro no mire)") : pl[1 - maker].ai ? "Piensa " + pl[1 - maker].name + "…" : pl[1 - maker].name + ": adiviná el código";
        g.textBox(msg, { R.x + 24, R.y + 150, R.w - 48, 60 }, 18, pal::gold, F_BOLD, AL_C);
        // intento en curso (se toca para quitar el color)
        for (int k = 0; k < SLOTS; k++) {
            float cx = R.cx() - 1.5f * 62 + k * 62, cy = R.y + 250; Rect r{ cx - 28, cy - 28, 56, 56 }; uint32_t id = hid("mmslot", k);
            bool hit = ui.hit(id, r, human && cur[k]); g.rrect(r, 14, rgb(0x120c08)); peg(g, cx, cy, 20, cur[k]);
            if (ui.focused(id)) ui.drawFocus(r, 14); if (hit) { cur[k] = 0; app->audio.play("click"); }
        }
        // paleta
        int first = -1;
        for (int c = 1; c <= colors; c++) {
            int i = c - 1; Rect r{ R.x + 42 + (i % 4) * 84.f, R.y + 316 + (i / 4) * 84.f, 72, 72 }; uint32_t id = hid("mmcol", c);
            bool hit = ui.hit(id, r, human && curCount() < SLOTS); if (first < 0) first = c;
            g.rrectGrad(r, 16, rgb(0xfff4d6), rgb(0xead39c)); peg(g, r.cx(), r.cy(), 24, c);
            if (ui.focused(id)) ui.drawFocus(r, 16); if (hit) place(c);
        }
        if (human && first >= 0) ui.setFocusIfNone(hid("mmcol", first));
        if (ui.button("Borrar", { R.x + 24, R.y + 500, 176, 52 }, 2, human && curCount() > 0)) removeLast();
        if (ui.button(waiting ? "Fijar código" : "Probar", { R.x + 220, R.y + 500, 176, 52 }, 1, human && curCount() == SLOTS)) confirm();
        g.text(waiting && human ? "Elegí 4 colores (se pueden repetir)" : "● bien color y lugar · ○ color en otro lugar", R.cx(), R.y + 590, 14, rgb(0xd8b46a), F_BODY, AL_C, R.w - 30);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Poner color" }, { "B", "Borrar último" }, { "X", "Probar / fijar" } }; }
    std::string help() const override { return "Un jugador arma un código secreto de 4 colores (pueden repetirse) y el otro intenta adivinarlo. Después de cada intento aparecen pistas: ● negra = color y lugar correctos, ○ blanca = color correcto en otro lugar. Se juegan 2 mangas, cambiando los roles. Quien adivina gana más puntos cuantos menos intentos use."; }
};
}
Game* makeMenteMaestra() { return new MM(); }
