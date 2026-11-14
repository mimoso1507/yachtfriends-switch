// Escoba de 15: 2 a 4 jugadores, baraja española. Se juega una carta y, si con cartas de la mesa suma 15, te las llevas (capturar es obligatorio).
// Valores: 1-7 su número, sota 8, caballo 9, rey 10. Limpiar la mesa = escoba. Por mano: escobas + más cartas + más oros + más sietes + 7 de oros.
#include "spanish.h"
#include <algorithm>

namespace {
struct Card { int s, r; };
int val(const Card& c) { return c.r <= 7 ? c.r : c.r - 2; }
bool cmpCard(const Card& a, const Card& b) { return a.s != b.s ? a.s < b.s : a.r < b.r; }
struct Escoba : Game {
    int N = 0, target = 21, dealer = 0, turn = 0, lastCapture = -1, winner = -1, resigned = -1; std::vector<Card> deck, table; std::vector<std::vector<Card>> hands, pile; std::vector<int> scores, escobas;
    std::vector<bool> active, selT; int selH = -1; bool over = false, handEnd = false; float aiT = 0, aiWait = 1, endT = 0, showT = 0; std::string msg; std::vector<std::string> report;
    void start() override { N = (int)pl.size(); target = opts.get("target", 1) == 0 ? 11 : opts.get("target", 1) == 2 ? 31 : 21; scores.assign(N, 0); active.assign(N, true); dealer = N - 1; newHand(); }
    void newHand() {
        deck.clear(); for (int s = 0; s < 4; s++) for (int r : sp::RANKS) deck.push_back({ s, r }); for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]);
        hands.assign(N, {}); pile.assign(N, {}); escobas.assign(N, 0); table.clear(); lastCapture = -1; handEnd = false; selH = -1; selT.assign(40, false);
        dealer = nextP(dealer); turn = nextP(dealer); for (int i = 0; i < 4; i++) { table.push_back(deck.back()); deck.pop_back(); } dealHands(); resetAi(); }
    void dealHands() { for (int p = 0; p < N; p++) if (active[p]) for (int i = 0; i < 3 && !deck.empty(); i++) { hands[p].push_back(deck.back()); deck.pop_back(); } for (auto& h : hands) std::sort(h.begin(), h.end(), cmpCard); }
    int nextP(int p) const { for (int i = 1; i <= N; i++) { int q = (p + i) % N; if (active[q]) return q; } return p; }
    void resetAi() { aiT = 0; aiWait = 0.8f + rnd01() * 0.9f; }
    int turnPlayer() const override { return over || handEnd ? -1 : turn; }
    static void combos(const std::vector<Card>& t, int need, std::vector<std::vector<int>>& out) {
        int n = (int)t.size(); for (int m = 1; m < (1 << n); m++) { int s = 0; for (int i = 0; i < n; i++) if (m >> i & 1) s += val(t[i]); if (s == need) { std::vector<int> v; for (int i = 0; i < n; i++) if (m >> i & 1) v.push_back(i); out.push_back(v); } }
    }
    void playCard(int hi, std::vector<int> ti) {
        int p = turn; Card c = hands[p][hi]; hands[p].erase(hands[p].begin() + hi); std::sort(ti.begin(), ti.end(), std::greater<int>());
        if (ti.empty()) { table.push_back(c); msg = pl[p].name + " dejó " + name(c); app->audio.play("place"); }
        else { pile[p].push_back(c); for (int i : ti) { pile[p].push_back(table[i]); table.erase(table.begin() + i); } lastCapture = p; msg = pl[p].name + " se llevó cartas"; app->audio.play("win");
            if (table.empty()) { bool lastPlay = deck.empty() && std::all_of(hands.begin(), hands.end(), [](const std::vector<Card>& h) { return h.empty(); }); if (!lastPlay) { escobas[p]++; msg += " · ¡ESCOBA!"; } } }
        selH = -1; std::fill(selT.begin(), selT.end(), false);
        bool allEmpty = true; for (auto& h : hands) if (!h.empty()) allEmpty = false;
        if (allEmpty) { if (!deck.empty()) dealHands(); else { endHand(); return; } }
        turn = nextP(p); while (hands[turn].empty() && active[turn]) turn = nextP(turn); resetAi();
    }
    static std::string name(const Card& c) { return std::to_string(c.r) + " de " + sp::suitName(c.s); }
    void endHand() {
        if (lastCapture >= 0) for (auto& c : table) pile[lastCapture].push_back(c); table.clear();
        std::vector<int> add(N, 0); report.clear(); auto best = [&](auto f, const char* lbl) { int mx = -1, who = -1; bool tie = false; for (int p = 0; p < N; p++) { if (!active[p]) continue; int v = f(p); if (v > mx) { mx = v; who = p; tie = false; } else if (v == mx) tie = true; } if (!tie && who >= 0 && mx > 0) { add[who]++; report.push_back(std::string(lbl) + ": " + pl[who].name); } };
        for (int p = 0; p < N; p++) { add[p] += escobas[p]; if (escobas[p]) report.push_back(std::to_string(escobas[p]) + (escobas[p] == 1 ? " escoba de " : " escobas de ") + pl[p].name); }
        best([&](int p) { return (int)pile[p].size(); }, "Más cartas"); best([&](int p) { int n = 0; for (auto& c : pile[p]) if (c.s == 0) n++; return n; }, "Más oros");
        best([&](int p) { int n = 0; for (auto& c : pile[p]) if (c.r == 7) n++; return n; }, "Más sietes");
        for (int p = 0; p < N; p++) for (auto& c : pile[p]) if (c.s == 0 && c.r == 7) { add[p]++; report.push_back("Siete de oros: " + pl[p].name); }
        for (int p = 0; p < N; p++) scores[p] += add[p]; handEnd = true; showT = 0;
        int mx = *std::max_element(scores.begin(), scores.end()); if (mx >= target) { int cnt = (int)std::count(scores.begin(), scores.end(), mx); if (cnt == 1) { over = true; winner = (int)(std::find(scores.begin(), scores.end(), mx) - scores.begin()); } }
    }
    void resign(int p) override { active[p] = false; resigned = p; int a = 0, w = -1; for (int i = 0; i < N; i++) if (active[i]) { a++; w = i; } if (a <= 1) { over = true; winner = w; handEnd = true; return; } if (turn == p) { turn = nextP(p); resetAi(); } }
    void aiMove() {
        int p = turn, lv = std::max(1, std::min(3, pl[p].level)); auto& h = hands[p]; if (h.empty()) { turn = nextP(p); return; }
        float bs = -1e9f; int bh = 0; std::vector<int> bt;
        for (int i = 0; i < (int)h.size(); i++) { std::vector<std::vector<int>> cs; combos(table, 15 - val(h[i]), cs);
            if (cs.empty()) { float sc = -2 - (h[i].s == 0 ? 3 : 0) - (h[i].r == 7 ? 4 : 0) - val(h[i]) * 0.1f; int sum = val(h[i]); for (auto& c : table) sum += val(c); if (lv >= 2 && sum <= 15) sc -= 4 + (sum == 15 ? 6 : 0); if (lv >= 2 && sum < 10) sc -= 2; sc += rnd01() * (lv == 1 ? 4.f : 0.5f); if (sc > bs) { bs = sc; bh = i; bt.clear(); } continue; }
            for (auto& v : cs) { float sc = 1 + (v.size() + 1) * 0.6f; bool clear = v.size() == table.size(); if (clear) sc += 5; if (h[i].s == 0) sc += 1.2f; if (h[i].r == 7) sc += 2; if (h[i].s == 0 && h[i].r == 7) sc += 8; for (int k : v) { if (table[k].s == 0) sc += 1.2f; if (table[k].r == 7) sc += 2; if (table[k].s == 0 && table[k].r == 7) sc += 8; }
                sc += rnd01() * (lv == 1 ? 4.f : 0.5f); if (sc > bs) { bs = sc; bh = i; bt = v; } } }
        playCard(bh, bt);
    }
    void update(float dt, Input& in, UI&) override {
        if (handEnd) { showT += dt; bool adv = showT > 6.f; for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && showT > 1.2f) adv = true;
            if (over) { endT += dt; if (endT > 3.f && !finished) { std::vector<PResult> r; for (int i = 0; i < N; i++) r.push_back({ scores[i], i == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Puntos: " + [&] { std::string s; for (int v : scores) s += std::to_string(v) + " "; return s; }()); } return; }
            if (adv) newHand(); return; }
        if (!pl[turn].ai) { Pad* pd = (pl[turn].pad >= 0 && pl[turn].pad < Input::MAXP) ? &in.pads[pl[turn].pad] : nullptr; if (pd && pd->down[B_X]) tryPlay(); return; }
        aiT += dt; if (aiT >= aiWait) aiMove();
    }
    void tryPlay() {
        if (selH < 0) { app->toast("Elige una carta de tu mano"); return; } auto& h = hands[turn]; if (selH >= (int)h.size()) return; std::vector<int> ti; int sum = val(h[selH]); for (size_t i = 0; i < table.size(); i++) if (selT[i]) { ti.push_back((int)i); sum += val(table[i]); }
        std::vector<std::vector<int>> cs; combos(table, 15 - val(h[selH]), cs);
        if (ti.empty()) { if (!cs.empty()) { app->toast("Hay captura posible: es obligatoria"); return; } playCard(selH, {}); return; }
        if (sum != 15) { app->toast("Las cartas deben sumar 15"); return; } playCard(selH, ti);
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !handEnd && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        std::string banner = over ? "¡Ganó " + pl[winner].name + "!" : handEnd ? "Fin de la mano" : mine ? pl[turn].name + ": elige tu carta y las de la mesa que sumen 15" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 19, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(200.f, 940.f / N);
        for (int i = 0; i < N; i++) { Rect r{ 290 + i * (pw + 2), 66, pw - 4, 52 }; bool on = i == turn && !over && !handEnd; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 20, r.cy(), 22); g.text(pl[i].name, r.x + 38, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 50); g.text(std::to_string(scores[i]) + " pts · " + std::to_string(pile[i].size()) + " cartas · " + std::to_string(escobas[i]) + " esc.", r.x + 38, r.y + 36, 12, rgb(0xd8b46a), F_BODY, AL_L, pw - 46); }
        g.text("Meta: " + std::to_string(target) + " puntos · mazo: " + std::to_string(deck.size()), 1230, 130, 14, pal::cream, F_BODY, AL_R); g.text(msg, 765, 150, 17, pal::cream, F_BODY, AL_C, 930);
        if (handEnd) { g.rrect({ 330, 190, 870, 380 }, 18, pal::panelEdge); g.rrectGrad({ 333, 193, 864, 374 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow("Puntos de la mano", 765, 226, 26, pal::gold, F_TITLE, AL_C);
            float y = 262; for (auto& s : report) { g.text(s, 765, y, 19, pal::cream, F_BODY, AL_C, 800); y += 28; } g.text("A para seguir", 765, 545, 16, rgb(0xd8b46a), F_BODY, AL_C); return; }
        // mesa
        int nt = (int)table.size(); float cw = 76, ch = 108, step = std::min(cw + 10, 900.f / std::max(1, nt));
        for (int i = 0; i < nt; i++) { Rect r{ 330 + (i % 10) * step, 180 + (i / 10) * 118.f - (selT[i] ? 12 : 0), cw, ch }; uint32_t id = hid("tb", i); bool hit = ui.hit(id, { r.x, r.y, step - 2, ch }, mine && selH >= 0); sp::draw(g, r, table[i].s, table[i].r, true, selT[i]); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { selT[i] = !selT[i]; app->audio.play("click"); } }
        if (nt == 0) g.text("Mesa limpia", 765, 230, 20, pal::cream, F_BOLD, AL_C);
        if (mine) { int sum = selH >= 0 ? val(hands[turn][selH]) : 0; for (int i = 0; i < nt; i++) if (selT[i]) sum += val(table[i]);
            g.text(selH >= 0 ? "Suma: " + std::to_string(sum) : "Elige una carta de tu mano", 765, 420, 22, sum == 15 ? rgb(0x7ee87e) : pal::cream, F_BOLD, AL_C);
            if (ui.button("Jugar / capturar", { 565, 440, 400, 50 }, 3, selH >= 0)) { tryPlay(); return; } }
        auto& h = hands[turn];
        if (peek) { int n = (int)h.size(); float hw = 90, hh = 126; float x0 = 765 - (n * (hw + 12) - 12) / 2;
            for (int i = 0; i < n; i++) { Rect r{ x0 + i * (hw + 12), 520.f - (i == selH ? 20 : 0), hw, hh }; uint32_t id = hid("hd", i); bool hit = ui.hit(id, r, mine); sp::draw(g, r, h[i].s, h[i].r, true, i == selH); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { selH = i == selH ? -1 : i; std::fill(selT.begin(), selT.end(), false); app->audio.play("click"); } }
            if (onPhone(turn)) coverHand(g, ui, "hd", n, turn);
            if (mine && n) ui.setFocusIfNone(hid("hd", 0)); }
        else if (mine) g.text("Mantén L para ver tus cartas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("hd", (int)i, me) + std::to_string(hands[p][i].r) + " de " + sp::suitName(hands[p][i].s) + (me && (int)i == selH ? "  ⬆" : "") + "\n"; return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir carta" }, { "X", "Jugar / capturar" }, { "L", "Ver tus cartas" } }; }
    std::string help() const override { return "Se juega con baraja española. En tu turno elige una carta de tu mano y las cartas de la mesa que, sumadas a ella, den exactamente 15 (1-7 su valor, sota 8, caballo 9, rey 10); te las llevas. Si se puede capturar, es obligatorio. Limpiar la mesa es una escoba (1 punto). Al final de la mano: más cartas, más oros, más sietes y el siete de oros suman 1 punto cada uno."; }
};
}
Game* makeEscoba() { return new Escoba(); }
