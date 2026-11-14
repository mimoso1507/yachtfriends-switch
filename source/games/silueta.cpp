// ¿Quién es? Silueta: se ve la sombra de un personaje y todos eligen su nombre a la vez (100 + hasta 50 por rapidez).
#include "party.h"
#include <set>
#include "silueta_data.h"
std::string emojiFile(const std::string& e);

namespace {
struct Silueta : Party {
    struct Ch { std::string name, film, art, g; char t; bool f; }; struct Q { std::string name, film, art; std::vector<std::string> o; int a; };
    std::vector<Q> qs; int idx = 0, phase = 0; float timer = 0, secs = 20, revT = 0, endT = 0; bool over = false;
    std::vector<int> ans, gain, score; std::vector<float> aiAt; std::vector<int> aiPick; std::vector<bool> out;
    static const char* BTN(int i) { static const char* B[4] = { "A", "B", "X", "Y" }; return B[i]; }
    void start() override {
        int n = (int)pl.size(); int rn = opts.get("rounds", 0); int rounds = rn == 0 ? 10 : rn == 1 ? 15 : 20;
        int tm = opts.get("secs", 1); secs = tm == 0 ? 10.f : tm == 2 ? 25.f : 15.f; bool all = opts.get("dif", 0) == 1;
        std::vector<Ch> CH, pool; for (int i = 0; i < sl::NP; i++) { auto a = split(sl::P[i], '|'); if (a.size() < 3) continue; Ch c; c.name = a[0]; c.film = a[1]; c.art = a[2]; c.g = a.size() > 3 && !a[3].empty() ? a[3] : a[2]; c.t = a.size() > 4 && !a[4].empty() ? a[4][0] : 'a'; c.f = a.size() > 5 && a[5] == "1"; CH.push_back(c); }
        for (auto& c : CH) if (all || c.f) pool.push_back(c);
        auto sh = [](std::vector<Ch>& v) { for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[rndInt(i + 1)]); }; sh(pool);
        std::vector<Ch> picked; int total = std::min(rounds, (int)pool.size());
        while ((int)picked.size() < total && !pool.empty()) { int k = 0; for (int i = 0; i < (int)pool.size(); i++) { bool bad = false; for (int j = std::max(0, (int)picked.size() - 2); j < (int)picked.size(); j++) if (picked[j].g == pool[i].g) bad = true; if (!bad) { k = i; break; } } picked.push_back(pool[k]); pool.erase(pool.begin() + k); }
        for (auto& c : picked) { std::vector<Ch> cand; for (auto& x : CH) if (x.g != c.g) cand.push_back(x); sh(cand); std::vector<Ch> out{ c }; std::set<std::string> used{ c.g };
            auto take = [&](bool same, size_t lim) { for (auto& x : cand) { if (out.size() >= lim) break; if ((!same || x.t == c.t) && !used.count(x.g)) { used.insert(x.g); out.push_back(x); } } }; take(true, 3); take(false, 4);
            for (int i = (int)out.size() - 1; i > 0; i--) std::swap(out[i], out[rndInt(i + 1)]); Q q; q.name = c.name; q.film = c.film; q.art = c.art; q.a = 0; for (size_t i = 0; i < out.size(); i++) { q.o.push_back(out[i].name); if (out[i].name == c.name) q.a = (int)i; } while (q.o.size() < 4) q.o.push_back("Nadie"); qs.push_back(q); }
        score.assign(n, 0); out.assign(n, false); begin();
    }
    void begin() {
        int n = (int)pl.size(); phase = 0; timer = secs; ans.assign(n, -1); gain.assign(n, 0); aiAt.assign(n, 0); aiPick.assign(n, 0);
        for (int i = 0; i < n; i++) if (pl[i].ai) { float skill = 0.5f + 0.1f * pl[i].level; bool ok = rndInt(100) < (int)(skill * 100); aiAt[i] = 1.5f + rndInt(100) / 100.f * (secs * 0.6f); aiPick[i] = ok ? qs[idx].a : (qs[idx].a + 1 + rndInt(3)) % 4; }
    }
    int turnPlayer() const override { return -1; }
    void resign(int p) override { if (p >= 0 && p < (int)out.size()) out[p] = true; bool any = false; for (size_t i = 0; i < out.size(); i++) if (!out[i]) any = true; if (!any) over = true; }
    void answer(int p, int c) { if (phase != 0 || out[p] || ans[p] >= 0) return; ans[p] = c; gain[p] = c == qs[idx].a ? 100 + (int)(50 * std::max(0.f, std::min(1.f, timer / secs))) : 0; app->audio.play("click"); }
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
        banner(g, over ? "Fin del juego" : "¿Quién es? · Silueta " + std::to_string(idx + 1) + " de " + std::to_string(qs.size()));
        float f = phase == 0 ? std::max(0.f, timer) / secs : 0; g.rrect({ 310, 62, 915, 14 }, 7, rgb(0x120c08, 220)); if (f > 0.01f) g.rrect({ 310, 62, 915 * f, 14 }, 7, f < 0.25f ? rgb(0xe53935) : pal::gold);
        g.rrect({ 310, 86, 915, 224 }, 20, pal::panelEdge); g.rrectGrad({ 313, 89, 909, 218 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c)); { bool rev = phase == 1; Rect ar{ 767.f - 90, 94, 180, 180 }; SDL_Texture* t = g.tex("sil/" + q.art + ".png"); if (!t) t = g.tex(emojiFile(q.art));
            if (t) { if (!rev) SDL_SetTextureColorMod(t, 0, 0, 0); g.rrect(ar, 14, rgb(0xf0e4c0)); g.image(t, ar.inset(8)); SDL_SetTextureColorMod(t, 255, 255, 255); } else g.text("?", ar.cx(), ar.cy(), 80, pal::gold, F_TITLE, AL_C);
            g.text(rev ? q.name + " (" + q.film + ")" : "¿Quién es este personaje?", 767, 288, 17, pal::gold, F_BOLD, AL_C, 860); }
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
    std::string help() const override { return "Se muestra la silueta negra de un personaje de Disney o Pixar y todos eligen su nombre a la vez con A, B, X o Y (o tocando). Acertar da 100 puntos más hasta 50 por rapidez. Gana quien más puntos junte."; }
};
}
Game* makeSilueta() { return new Silueta(); }
