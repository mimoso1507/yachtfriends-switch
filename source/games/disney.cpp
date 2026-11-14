// ¿Quién es? (Disney y Pixar): pistas que llegan de a una; cuanto antes acertás, más puntos (150/100/60).
#include "party.h"
#include "disney_data.h"

namespace {
struct Disney : Party {
    struct Q { std::string name, film; int year; bool live; std::vector<std::string> clues, o; int a; };
    std::vector<Q> qs; int idx = 0, phase = 0; float timer = 0, secs = 20, revT = 0, endT = 0; bool over = false;
    std::vector<int> ans, gain, score; std::vector<float> aiAt; std::vector<int> aiPick; std::vector<bool> out;
    static const char* BTN(int i) { static const char* B[4] = { "A", "B", "X", "Y" }; return B[i]; }
    void start() override {
        int n = (int)pl.size(); int rn = opts.get("rounds", 1); int rounds = rn == 0 ? 5 : rn == 1 ? 10 : rn == 2 ? 15 : 20;
        int tm = opts.get("secs", 1); secs = tm == 0 ? 15.f : tm == 2 ? 36.f : 24.f; int era = opts.get("era", 0); bool live = opts.get("live", 1) != 0;
        struct C { std::string name, film; int year; bool live; std::vector<std::string> clues; }; std::vector<C> all, pool;
        for (int i = 0; i < dd::NP; i++) { auto a = split(dd::P[i], '|'); if (a.size() < 7) continue; C c{ a[0], a[1], atoi(a[2].c_str()), a[3] == "l", { a[4], a[5], a[6] } }; all.push_back(c);
            if ((live || !c.live) && (era == 1 ? c.year >= 1990 : era == 2 ? c.year < 1990 : true)) pool.push_back(c); }
        auto sh = [](std::vector<C>& v) { for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[rndInt(i + 1)]); }; sh(pool);
        auto& others = pool.size() >= 4 ? pool : all; int total = std::min(rounds, (int)pool.size());
        for (int k = 0; k < total; k++) { Q q; q.name = pool[k].name; q.film = pool[k].film; q.year = pool[k].year; q.live = pool[k].live; q.clues = pool[k].clues; std::vector<std::string> o = { q.name };
            std::vector<int> ix; for (int i = 0; i < (int)others.size(); i++) if (others[i].name != q.name && others[i].film != q.film) ix.push_back(i); for (int i = (int)ix.size() - 1; i > 0; i--) std::swap(ix[i], ix[rndInt(i + 1)]);
            for (int i = 0; i < 3 && i < (int)ix.size(); i++) o.push_back(others[ix[i]].name); while (o.size() < 4) o.push_back("Nadie"); for (int i = 3; i > 0; i--) std::swap(o[i], o[rndInt(i + 1)]); q.o = o; q.a = (int)(std::find(o.begin(), o.end(), q.name) - o.begin()); qs.push_back(q); }
        score.assign(n, 0); out.assign(n, false); begin();
    }
    int stage() const { return phase == 0 ? std::min(2, (int)((secs - timer) / (secs / 3.f))) : 2; }
    void begin() {
        int n = (int)pl.size(); phase = 0; timer = secs; ans.assign(n, -1); gain.assign(n, 0); aiAt.assign(n, 0); aiPick.assign(n, 0);
        for (int i = 0; i < n; i++) if (pl[i].ai) { aiAt[i] = 1.5f + rndInt(100) / 100.f * (secs * 0.85f); int stg = std::min(2, (int)(aiAt[i] / (secs / 3.f))); float skill = 0.3f + 0.08f * pl[i].level + 0.17f * stg; bool ok = rndInt(100) < (int)(skill * 100); aiPick[i] = ok ? qs[idx].a : (qs[idx].a + 1 + rndInt(3)) % 4; }
    }
    int turnPlayer() const override { return -1; }
    void resign(int p) override { if (p >= 0 && p < (int)out.size()) out[p] = true; bool any = false; for (size_t i = 0; i < out.size(); i++) if (!out[i]) any = true; if (!any) over = true; }
    void answer(int p, int c) { if (phase != 0 || out[p] || ans[p] >= 0) return; ans[p] = c; { static const int PT[3] = { 150, 100, 60 }; gain[p] = c == qs[idx].a ? PT[stage()] : 0; } app->audio.play("click"); }
    bool allIn() const { for (size_t i = 0; i < pl.size(); i++) if (!out[i] && ans[i] < 0) return false; return true; }
    void reveal() { phase = 1; revT = 0; for (size_t i = 0; i < pl.size(); i++) if (!out[i]) score[i] += gain[i]; app->audio.play("place"); }
    void update(float dt, Input& in, UI&) override {
        int n = (int)pl.size();
        if (over) { endT += dt; if (endT > 1.5f && !finished) { int best = -1; for (int i = 0; i < n; i++) if (!out[i]) best = std::max(best, score[i]); int nw = 0; for (int i = 0; i < n; i++) if (!out[i] && score[i] == best) nw++;
                std::vector<PResult> r; std::string s = "Puntos: "; for (int i = 0; i < n; i++) { r.push_back({ score[i], out[i] ? 'x' : score[i] == best ? (nw > 1 ? 'd' : 'w') : 'l' }); s += pl[i].name + " " + std::to_string(score[i]) + (i + 1 < n ? " · " : ""); } finish(r, s); } return; }
        if (phase == 0) {
            timer -= dt;
            for (int i = 0; i < n; i++) { if (out[i] || ans[i] >= 0) continue;
                if (pl[i].ai) { if (secs - timer >= aiAt[i]) answer(i, aiPick[i]); continue; }
                Pad* p = padOf(in, i); if (!p) continue; for (int b = 0; b < 4; b++) if (p->down[b]) { answer(i, b); break; } }
            if (in.touchDown) { for (int k = 0; k < 4; k++) if (optRect(k).in(in.tx, in.ty)) { for (int i = 0; i < n; i++) if (!pl[i].ai && !out[i] && ans[i] < 0) { answer(i, k); break; } break; } }
            if (timer <= 0 || allIn()) reveal();
        } else {
            revT += dt; bool skip = false; for (int i = 0; i < n; i++) if (!pl[i].ai) { Pad* p = padOf(in, i); if (p && p->down[B_PLUS]) skip = true; }
            if (revT > 3.2f || skip) { if (idx + 1 >= (int)qs.size()) { over = true; } else { idx++; begin(); } }
        }
    }
    Rect optRect(int k) const { return { 310.f + (k % 2) * 465, 330.f + (k / 2) * 98, 450, 86 }; }
    static void cbox(Gfx& g, const std::string& t, Rect r, float sz, Col c) { float w = g.textW(t, sz, F_BOLD); int ln = std::max(1, (int)std::ceil(w / (r.w * 0.93f))); float h = ln * sz * 1.3f; r.y += std::max(0.f, (r.h - h) / 2); g.textBox(t, r, sz, c, F_BOLD, AL_C); }
    static std::string plain(const char* s) { std::string t = s; auto p = t.find(' '); return p == std::string::npos ? t : t.substr(p + 1); }
    void draw(Gfx& g, UI&) override {
        int n = (int)pl.size(); auto& q = qs[std::min(idx, (int)qs.size() - 1)];
        banner(g, over ? "Fin del juego" : "¿Quién es? · Personaje " + std::to_string(idx + 1) + " de " + std::to_string(qs.size()));
        float f = phase == 0 ? std::max(0.f, timer) / secs : 0; g.rrect({ 310, 62, 915, 14 }, 7, rgb(0x120c08, 220)); if (f > 0.01f) g.rrect({ 310, 62, 915 * f, 14 }, 7, f < 0.25f ? rgb(0xe53935) : pal::gold);
        g.rrect({ 310, 86, 915, 224 }, 20, pal::panelEdge); g.rrectGrad({ 313, 89, 909, 218 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c)); { int st = stage(); for (int i = 0; i <= st; i++) { std::string t = (i + 1 == 1 ? "1. " : i == 1 ? "2. " : "3. ") + q.clues[i]; cbox(g, t, { 340, 98.f + i * 62, 855, 60 }, 22, i == st ? pal::cream : rgb(0xc8b88a)); }
            if (phase == 1) g.text(q.name + " — " + q.film + " (" + std::to_string(q.year) + ")" + (q.live ? " · live action" : ""), 767, 290, 17, pal::gold, F_BOLD, AL_C, 860); else g.text("Vale " + std::string(st == 0 ? "150" : st == 1 ? "100" : "60") + " puntos", 767, 290, 15, pal::gold, F_BOLD, AL_C); }
        for (int k = 0; k < 4; k++) { Rect r = optRect(k); bool rev = phase == 1; Col c0 = rgb(0x3a2a1c), c1 = rgb(0x1e140c);
            if (rev && k == q.a) { c0 = rgb(0x2e8b3c); c1 = rgb(0x1b5e24); }
            g.rrect(r, 16, pal::panelEdge); g.rrectGrad(r.inset(3), 14, c0, c1);
            g.circle(r.x + 40, r.cy(), 26, rgb(0xd8b46a)); g.text(BTN(k), r.x + 40, r.cy(), 30, rgb(0x2a1e18), F_TITLE, AL_C);
            cbox(g, q.o[k], { r.x + 80, r.y + 4, r.w - 92, r.h - 8 }, 24, pal::white);
            if (rev) { std::string who; for (int i = 0; i < n; i++) if (!out[i] && ans[i] == k) who += pl[i].emoji; if (!who.empty()) g.text(who, r.x + r.w - 14, r.y + 14, 20, pal::white, F_BODY, AL_R); } }
        int cols = n <= 4 ? n : 4; float w = 915.f / cols;
        for (int i = 0; i < n; i++) { Rect r{ 310 + (i % cols) * w, 540.f + (i / cols) * 40, w - 6, 36 }; g.rrect(r, 10, rgb(0x120c08, 215));
            std::string st = phase == 0 ? (out[i] ? "-" : ans[i] >= 0 ? "✅" : "...") : (gain[i] > 0 ? "+" + std::to_string(gain[i]) : "x");
            g.text(pl[i].emoji + " " + pl[i].name, r.x + 8, r.cy(), 14, pal::cream, F_BOLD, AL_L, w - 120); g.text(st + "  " + std::to_string(score[i]), r.x + r.w - 8, r.cy(), 16, pal::gold, F_BOLD, AL_R); }
    }
    std::vector<Hint> hints() const override { return { { "A", "Opción A" }, { "B", "Opción B" }, { "X", "Opción C" }, { "Y", "Opción D" }, { "TOUCH", "Tocar" } }; }
    std::string help() const override { return "Aparece una pista de un personaje de Disney o Pixar y cada tanto llega otra más fácil. Elegí entre 4 nombres con A, B, X o Y (o tocando): cuanto antes acertás, más puntos (150, 100 o 60). Gana quien más puntos junte."; }
};
}
Game* makeDisney() { return new Disney(); }
