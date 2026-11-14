// Biblioteca común del casino: banca acumulada (se guarda en la SD), cartas con comodín, evaluador de póker, barra superior y apuestas con fichas.
#pragma once
#include "cards.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace cz {
inline int rankOf(int c) { return c % 13 + 1; } inline int suitOf(int c) { return c / 13; }
inline std::vector<int> shoe(int decks, bool joker = false) { std::vector<int> d; for (int k = 0; k < decks; k++) for (int c = 0; c < 52; c++) d.push_back(c); if (joker) d.push_back(52); for (int i = (int)d.size() - 1; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]); return d; }
inline void drawCard(Gfx& g, Rect r, int c, bool back = false, bool glow = false) {
    if (back) { cards::drawBack(g, r); return; }
    if (c == 52) { g.rrect({ r.x + 2, r.y + 5, r.w, r.h }, 10, rgb(0, 100)); if (glow) g.rrect({ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 13, pal::gold); g.rrect(r, 10, rgb(0xb9a888)); g.rrectGrad(r.inset(2), 8, rgb(0xffffff), rgb(0xf1ead8)); g.text("JOKER", r.cx(), r.cy(), r.w * 0.2f, rgb(0x6a1b9a), F_BOLD, AL_C); g.text("☺", r.cx(), r.cy() - r.h * 0.2f, r.w * 0.4f, rgb(0x6a1b9a), F_BOLD, AL_C); return; }
    int r1 = rankOf(c); cards::draw(g, r, r1 == 1 ? 12 : r1 - 2, suitOf(c), true, glow);
}
const char* CATN[11] = { "Carta alta", "Par", "Doble par", "Trío", "Escalera", "Color", "Full house", "Póker", "Escalera de color", "Escalera real", "Cinco ases" };
struct Ev { int cat = 0; double key = 0; int hi = 0; std::string name; };
inline Ev eval5(const std::vector<int>& cs) {
    std::vector<int> vals; for (int c : cs) { int r = rankOf(c); vals.push_back(r == 1 ? 14 : r); } std::sort(vals.begin(), vals.end(), std::greater<int>()); std::map<int,int> cnt; for (int v : vals) cnt[v]++;
    std::vector<int> groups; for (auto& kv : cnt) groups.push_back(kv.first); std::sort(groups.begin(), groups.end(), [&](int a, int b) { return cnt[a] != cnt[b] ? cnt[a] > cnt[b] : a > b; });
    bool flush = true; for (int c : cs) if (suitOf(c) != suitOf(cs[0])) flush = false; double sh = 0;
    if (groups.size() == 5) { if (vals[0] - vals[4] == 4) sh = vals[0]; else if (vals[0] == 14 && vals[1] == 5 && vals[2] == 4 && vals[3] == 3 && vals[4] == 2) sh = 13.5; }
    int top = cnt[groups[0]], sec = groups.size() > 1 ? cnt[groups[1]] : 0, cat;
    if (top == 5) cat = 10; else if (sh > 0 && flush) cat = sh == 14 ? 9 : 8; else if (top == 4) cat = 7; else if (top == 3 && sec == 2) cat = 6; else if (flush) cat = 5; else if (sh > 0) cat = 4; else if (top == 3) cat = 3; else if (top == 2 && sec == 2) cat = 2; else if (top == 2) cat = 1; else cat = 0;
    double key = cat * 1e10; if (sh > 0) key += sh * 1e4; else for (size_t i = 0; i < groups.size(); i++) key += groups[i] * std::pow(100.0, 4 - (int)i);
    return { cat, key, groups[0], CATN[cat] };
}
inline Ev evalJ(const std::vector<int>& cs) {
    auto it = std::find(cs.begin(), cs.end(), 52); if (it == cs.end()) return eval5(cs); std::vector<int> others; for (int c : cs) if (c != 52) others.push_back(c); Ev best; bool has = false;
    for (int s = 0; s < 4; s++) { auto o = others; o.push_back(13 * s); Ev e = eval5(o); if (!has || e.key > best.key) { best = e; has = true; } }
    for (int c = 0; c < 52; c++) { if (std::find(others.begin(), others.end(), c) != others.end()) continue; auto o = others; o.push_back(c); Ev e = eval5(o); if ((e.cat == 4 || e.cat == 5 || e.cat == 8 || e.cat == 9) && e.key > best.key) best = e; }
    return best;
}
struct Ev2 { double key; bool pair; int v[2]; };
inline Ev2 eval2(const std::vector<int>& cs) { int v[2]; for (int i = 0; i < 2; i++) v[i] = cs[i] == 52 ? 14 : (rankOf(cs[i]) == 1 ? 14 : rankOf(cs[i])); if (v[0] < v[1]) std::swap(v[0], v[1]); return v[0] == v[1] ? Ev2{ 1000.0 + v[0], true, { v[0], v[1] } } : Ev2{ v[0] * 15.0 + v[1], false, { v[0], v[1] } }; }
inline bool paiOk(const Ev& hi, const Ev2& lo) { if (lo.pair) return hi.cat >= 2 || (hi.cat == 1 && hi.hi >= lo.v[0]); if (hi.cat >= 1) return true; double h = hi.key; int t0 = (int)(std::floor(h / 1e8)) % 100, t1 = (int)(std::floor(h / 1e6)) % 100; return t0 > lo.v[0] || (t0 == lo.v[0] && t1 >= lo.v[1]); }
struct Split { std::vector<int> low, high; Ev hi; Ev2 lo; };
inline Split paiSplit(const std::vector<int>& c7) {
    std::vector<Split> all; for (int i = 0; i < 7; i++) for (int j = i + 1; j < 7; j++) { std::vector<int> low = { c7[i], c7[j] }, high; for (int k = 0; k < 7; k++) if (k != i && k != j) high.push_back(c7[k]); Ev hi = evalJ(high); Ev2 lo = eval2(low); if (paiOk(hi, lo)) all.push_back({ low, high, hi, lo }); }
    int mc = 0; for (auto& s : all) mc = std::max(mc, s.hi.cat); std::vector<Split> pool; for (auto& s : all) if (s.hi.cat == mc) pool.push_back(s); std::sort(pool.begin(), pool.end(), [](const Split& a, const Split& b) { return a.lo.key != b.lo.key ? a.lo.key > b.lo.key : a.hi.key > b.hi.key; }); return pool[0];
}

struct Casino : Game {
    int w = 0, startW = 0, chipSel = 1, bet = 0, lastBet = 0; bool bankMode = true, closing = false; std::string msg; int msgKind = 0; float endT = 0; const int CH[5] = { 5, 10, 25, 100, 500 };
    void initWallet() { bankMode = opts.get("mode", 0) == 0; w = bankMode ? app->save.bank : 1000; if (w < 0) w = 1000; startW = w; }
    void addW(int n) { w += n; if (bankMode) app->save.bank = w; }
    void commit() { if (bankMode) app->save.store(); }
    int turnPlayer() const override { return closing ? -1 : 0; }
    void resign(int) override { cash(); }
    void cash() { if (closing) return; closing = true; commit(); int d = w - startW; finish({ { w, d > 0 ? 'w' : d < 0 ? 'l' : 'd' } }, "Te retiras con " + std::to_string(w) + " fichas (" + (d > 0 ? "+" : "") + std::to_string(d) + ")"); }
    bool topBar(Gfx& g, UI& ui) {   // true si se pulsó Retirarse
        g.rrect({ 290, 12, 760, 40 }, 20, rgb(0x120c08, 215)); int d = w - startW; g.text("💰 " + std::to_string(w) + " fichas", 312, 32, 20, pal::gold, F_BOLD, AL_L); g.text((d > 0 ? "+" : "") + std::to_string(d), 560, 32, 17, d > 0 ? rgb(0x5be08a) : d < 0 ? rgb(0xff7b7b) : pal::cream, F_BOLD, AL_L);
        g.text(bankMode ? "banca acumulada" : "partida nueva", 1030, 32, 13, pal::cream, F_BODY, AL_R);
        if (ui.button("Retirarme", { 1070, 12, 170, 40 }, 1)) { cash(); return true; } return false;
    }
    bool broke(Gfx& g, UI& ui, int mn) {   // sin fichas suficientes: botón de recarga
        if (w >= mn) return false; g.rrect({ 330, 250, 800, 140 }, 16, rgb(0x3a1010, 235)); g.text("Te quedaste sin fichas suficientes", 730, 290, 22, rgb(0xff9a9a), F_BOLD, AL_C);
        if (ui.button("Recargar 1000 fichas", { 530, 320, 400, 48 }, 3)) { w = 1000; if (bankMode) app->save.bank = w; commit(); } ui.setFocusIfNone(hid("Recargar 1000 fichas", 530 * 7 + 320)); return true;
    }
    void chipPick(Gfx&, UI& ui, float x, float y) { for (int i = 0; i < 5; i++) if (ui.button(std::to_string(CH[i]), { x + i * 76, y, 70, 40 }, i == chipSel ? 3 : 2)) chipSel = i; }
    void betRow(Gfx& g, UI& ui, float y, bool locked) {   // apuesta acumulable
        g.rrect({ 400, y, 170, 40 }, 20, rgb(0x120c08, 200)); g.text("Apuesta: " + std::to_string(bet), 560, y + 20, 20, pal::gold, F_BOLD, AL_R);
        for (int i = 0; i < 5; i++) if (ui.button(std::to_string(CH[i]), { 580.f + i * 66, y, 60, 40 }, 2, !locked && bet + CH[i] <= w)) { bet += CH[i]; app->audio.play("click"); }
        if (ui.button("Borrar", { 920, y, 100, 40 }, 2, !locked)) bet = 0; if (ui.button("Todo", { 1026, y, 80, 40 }, 2, !locked)) bet = w; if (ui.button("Repetir", { 1112, y, 110, 40 }, 2, !locked && lastBet > 0)) bet = std::min(lastBet, w);
    }
    void say(const std::string& s, int kind = 0) { msg = s; msgKind = kind; }
    void drawMsg(Gfx& g, float y) { if (!msg.empty()) g.rrect({ 400, y - 18, 730, 36 }, 18, rgb(0x120c08, 200)); g.text(msg, 765, y, 20, msgKind > 0 ? rgb(0x5be08a) : msgKind < 0 ? rgb(0xff9a9a) : pal::cream, F_BOLD, AL_C, 940); }
    void update(float dt, Input&, UI&) override { (void)dt; }
};
}
