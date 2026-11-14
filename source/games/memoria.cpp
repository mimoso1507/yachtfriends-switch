// Memoria: 1 a 8 jugadores, tableros de 6x6 (18 parejas), varias páginas. Si acertás seguís; si fallás pasa el turno.
// IA: recuerda solo lo que se destapó, con memoria imperfecta (igual que la versión de PC).
#include "../app.h"
#include <algorithm>
#include <cmath>

namespace {
const char* const ANIM[22] = { "🐶", "🐱", "🐭", "🐹", "🐰", "🦊", "🐻", "🐼", "🐨", "🐯", "🦁", "🐮", "🐷", "🐸", "🐵", "🐔", "🐧", "🐦", "🐢", "🐙", "🦋", "🐞" };
const char* const FRUT[22] = { "🍎", "🍌", "🍇", "🍉", "🍓", "🍒", "🍑", "🍍", "🥝", "🍋", "🍊", "🍐", "🥥", "🥭", "🍅", "🥕", "🌽", "🍔", "🍕", "🍩", "🍪", "🍦" };
const char* const ESPA[22] = { "🚗", "🚕", "🚌", "🚑", "🚒", "🚜", "✈️", "🚁", "🚀", "⛵", "🚲", "🏍️", "🚂", "🛸", "⚽", "🏀", "🎾", "🎲", "🎸", "🎨", "⭐", "🌙" };
struct Col2 { const char* name; uint32_t c; };
const Col2 COLS[22] = { { "Rojo", 0xe02424 }, { "Naranja", 0xf97316 }, { "Amarillo", 0xfde047 }, { "Lima", 0xa3e635 }, { "Verde", 0x16a34a }, { "Pino", 0x14532d }, { "Turquesa", 0x14b8a6 }, { "Celeste", 0x38bdf8 }, { "Azul", 0x2563eb }, { "Marino", 0x1e3a8a }, { "Violeta", 0x7c3aed }, { "Magenta", 0xd946ef }, { "Rosa", 0xf9a8d4 }, { "Bordó", 0x7f1d1d }, { "Marrón", 0x78350f }, { "Beige", 0xe7d3a7 }, { "Durazno", 0xfdba74 }, { "Negro", 0x171717 }, { "Gris", 0x9ca3af }, { "Blanco", 0xf5f5f5 }, { "Lila", 0xc4b5fd }, { "Mostaza", 0xca8a04 } };
struct Lvl { float forget, ignore, d0, d1; };
const Lvl LVL[4] = { {}, { 0.55f, 0.4f, 0.9f, 1.3f }, { 0.18f, 0.08f, 0.65f, 0.75f }, { 0, 0, 0.5f, 0.6f } };

struct Memo : Game {
    int pages = 3, theme = 0, idx = 0, turn = 0, N = 0; std::vector<int> scores; std::vector<int> cards, st, themes; int known[36]; std::vector<int> flipped;
    bool pending = false, between = false, over = false; float pendT = 0, betT = 0, aiT = 0, aiWait = 1, endT = 0; int moves = 0, resigned = -1; std::vector<bool> active;
    void start() override { pages = std::max(1, std::min(5, opts.get("pages", 2) + 1)); theme = opts.get("theme", 0); N = (int)pl.size(); scores.assign(N, 0); active.assign(N, true); newPage(); }
    void newPage() {
        int th = theme == 0 ? (idx % 4) : theme - 1;   // 0 mezcla; 1 animales 2 frutas 3 espacio 4 colores
        themeNow = th; std::vector<int> pool(22); for (int i = 0; i < 22; i++) pool[i] = i; for (int i = 21; i > 0; i--) std::swap(pool[i], pool[rndInt(i + 1)]);
        cards.clear(); for (int i = 0; i < 18; i++) { cards.push_back(pool[i]); cards.push_back(pool[i]); } for (int i = 35; i > 0; i--) std::swap(cards[i], cards[rndInt(i + 1)]);
        st.assign(36, 0); for (int& k : known) k = -1; flipped.clear(); pending = false; between = false; resetAi();
    }
    int themeNow = 0;
    void resetAi() { aiT = 0; const Lvl& P = LVL[std::max(1, std::min(3, pl[turn].level))]; aiWait = P.d0 + rnd01() * P.d1; }
    int turnPlayer() const override { return over || between ? -1 : turn; }
    void advance() { for (int i = 1; i <= N; i++) { int q = (turn + i) % N; if (active[q]) { turn = q; return; } } }
    void flip(int i) {
        if (over || pending || between || st[i] != 0) return;
        st[i] = 1; flipped.push_back(i); known[i] = cards[i]; app->audio.play("click");
        if (flipped.size() == 2) {
            moves++; int a = flipped[0], b = flipped[1];
            if (cards[a] == cards[b]) {
                st[a] = st[b] = 2; scores[turn]++; flipped.clear(); app->audio.play("win"); known[a] = known[b] = -1;
                bool all = true; for (int v : st) if (v != 2) all = false;
                if (all) { if (idx + 1 < pages) { between = true; betT = 1.8f; } else over = true; }
            } else { pending = true; pendT = 1.0f; }
        }
        resetAi();
    }
    void resign(int p) override { active[p] = false; resigned = p; bool any = false; for (bool a : active) if (a) any = true;
        if (!any) { over = true; return; } if (turn == p) { for (int i : flipped) st[i] = 0; flipped.clear(); pending = false; advance(); resetAi(); } }
    void update(float dt, Input&, UI&) override {
        if (over) { endT += dt; if (endT > 1.6f && !finished) {
            int best = *std::max_element(scores.begin(), scores.end()); int cnt = (int)std::count(scores.begin(), scores.end(), best);
            std::vector<PResult> r; std::string sm = "Parejas:"; for (int i = 0; i < N; i++) { r.push_back({ scores[i], scores[i] == best ? (cnt > 1 ? 'd' : 'w') : 'l' }); sm += " " + std::to_string(scores[i]); }
            if (resigned >= 0) r[resigned].r = 'x'; finish(r, sm); } return; }
        if (between) { betT -= dt; if (betT <= 0) { idx++; newPage(); } return; }
        if (pending) { pendT -= dt; if (pendT <= 0) { for (int i : flipped) st[i] = 0; flipped.clear(); pending = false; advance(); resetAi(); } return; }
        if (!pl[turn].ai) return;
        aiT += dt; if (aiT < aiWait) return;
        const Lvl& P = LVL[std::max(1, std::min(3, pl[turn].level))];
        for (int k = 0; k < 36; k++) if (known[k] >= 0 && st[k] != 1 && rnd01() < P.forget) known[k] = -1;   // olvida
        std::vector<int> hidden; for (int i = 0; i < 36; i++) if (st[i] == 0) hidden.push_back(i);
        if (hidden.empty()) return;
        auto fresh = [&]() { std::vector<int> f; for (int i : hidden) if (known[i] < 0) f.push_back(i); return f.empty() ? hidden[rndInt((int)hidden.size())] : f[rndInt((int)f.size())]; };
        int pick;
        if (flipped.size() == 1) {
            int a = flipped[0], sym = cards[a]; pick = -1;
            for (int i : hidden) if (known[i] == sym) { pick = i; break; }
            if (pick < 0 || rnd01() < P.ignore) pick = fresh();
        } else {
            pick = -1; std::map<int, std::vector<int>> by; for (int i : hidden) if (known[i] >= 0) by[known[i]].push_back(i);
            for (auto& kv : by) if (kv.second.size() >= 2) { pick = kv.second[0]; break; }
            if (pick < 0 || rnd01() < P.ignore) pick = fresh();
        }
        flip(pick);
    }
    void face(Gfx& g, Rect r, int sym) {
        if (themeNow == 3) { g.rrect(r.inset(6), 10, rgb(COLS[sym].c)); g.rrectLine(r.inset(6), 10, 2, rgb(0, 90)); uint32_t c = COLS[sym].c; bool light = ((c >> 16) & 255) * 0.3 + ((c >> 8) & 255) * 0.59 + (c & 255) * 0.11 > 150; g.text(COLS[sym].name, r.cx(), r.cy(), 13, light ? rgb(0x222222) : rgb(0xffffff), F_BOLD, AL_C, r.w - 10); return; }
        const char* e = themeNow == 0 ? ANIM[sym] : themeNow == 1 ? FRUT[sym] : ESPA[sym]; g.emoji(e, r.cx(), r.cy(), r.w * 0.62f);
    }
    void draw(Gfx& g, UI& ui) override {
        float ts = 86, gap = 8, W = 6 * ts + 5 * gap, x0 = 760 - W / 2, y0 = 84;
        std::string msg = over ? "Fin de la partida" : between ? "¡Página completa!" : pl[turn].ai ? "Piensa " + pl[turn].name + "…" : "Turno de " + pl[turn].name;
        g.rrect({ 410, 14, 700, 46 }, 23, rgb(0x120c08, 200)); g.text(msg + "   ·   Página " + std::to_string(idx + 1) + " de " + std::to_string(pages), 760, 37, 20, pal::gold, F_BOLD, AL_C, 680);
        g.rrect({ x0 - 14, y0 - 14, W + 28, W + 28 }, 18, rgb(0x2f8a4a)); g.rrectLine({ x0 - 14, y0 - 14, W + 28, W + 28 }, 18, 4, rgb(0x6b4224));
        bool mine = !over && !between && !pending && !pl[turn].ai; int firstOk = -1;
        for (int i = 0; i < 36; i++) {
            Rect r{ x0 + (i % 6) * (ts + gap), y0 + (i / 6) * (ts + gap), ts, ts }; uint32_t id = hid("me", i);
            bool ok = mine && st[i] == 0; if (ok && firstOk < 0) firstOk = i; bool hit = ui.hit(id, r, ok);
            g.rrect({ r.x, r.y + 4, r.w, r.h }, 12, rgb(0, 100));
            if (st[i] == 0) { g.rrectGrad(r, 12, rgb(0x2e5fb0), rgb(0x1b3a78)); g.rrectLine(r.inset(5), 8, 2, rgb(0xffffff, 80)); g.text("?", r.cx(), r.cy(), 30, rgb(0xffffff, 110), F_TITLE, AL_C); }
            else { g.rrectGrad(r, 12, st[i] == 2 ? rgb(0xd9f2c6) : rgb(0xfff4d6), st[i] == 2 ? rgb(0xa9d78e) : rgb(0xecd6a0)); face(g, r, cards[i]); }
            if (ui.focused(id)) ui.drawFocus(r, 12); if (hit) flip(i);
        }
        if (firstOk >= 0) ui.setFocusIfNone(hid("me", firstOk));
        // marcador
        float y = 70 + 46 * N + 54; if (y > 400) y = 400; Rect sc{ 16, y, 230, 26.f + 26 * std::min(N, 8) }; if (sc.y + sc.h > 640) sc.y = 640 - sc.h - 150;
        g.rrect(sc, 12, rgb(0x120c08, 200)); g.rrectLine(sc, 12, 1.5f, rgb(0xd8b46a, 110));
        for (int i = 0; i < N && i < 8; i++) { float yy = sc.y + 22 + i * 26; g.circle(sc.x + 16, yy, 6, PCOL[i % 8]); g.text(pl[i].name, sc.x + 30, yy, 14, i == turn && !over ? pal::gold : pal::white, F_BOLD, AL_L, 130); g.text(std::to_string(scores[i]), sc.x + sc.w - 12, yy, 15, pal::gold, F_BOLD, AL_R); }
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Elegir ficha" }, { "A", "Dar vuelta" } }; }
    std::string help() const override { return "Dá vuelta dos fichas por turno. Si son iguales, son tuyas y seguís jugando; si no, se vuelven a tapar y juega el siguiente. Cada página es un tablero de 18 parejas. Gana quien junte más parejas en total."; }
};
}
Game* makeMemoria() { return new Memo(); }
