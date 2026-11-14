// Rummy de fichas (tipo Rummikub), 2-4 jugadores. 106 fichas (4 colores x 1-13 x 2 + 2 comodines), 14 por jugador.
// Jugadas: GRUPO (3-4 del mismo número y colores distintos) o ESCALERA (3+ del mismo color, consecutivas). La primera bajada suma 30 o más con fichas de tu mano.
// Versión Switch: armas jugadas con tu mano y puedes AGREGAR fichas a las de la mesa (y romper jugadas de la mesa para reordenarlas).
#include "../app.h"
#include <algorithm>
#include <set>

namespace {
const int NT = 106, JOKER = 104;
bool isJ(int id) { return id >= JOKER; } int colOf(int id) { return id / 26; } int numOf(int id) { return id % 13 + 1; } int val(int id) { return isJ(id) ? 30 : numOf(id); }
struct Cell { int id, n, c; bool j; };
struct An { bool ok = false; bool group = false; std::vector<Cell> cells; int value = 0; };
An analyze(const std::vector<int>& ids) {
    An a; int L = (int)ids.size(); if (L < 3 || L > 13) return a; std::vector<int> real, jk; std::set<int> seen; for (int id : ids) { if (seen.count(id)) return a; seen.insert(id); (isJ(id) ? jk : real).push_back(id); } if (real.empty()) return a;
    std::vector<int> ns, cs; for (int id : real) { ns.push_back(numOf(id)); cs.push_back(colOf(id)); } bool sameN = std::all_of(ns.begin(), ns.end(), [&](int x) { return x == ns[0]; }), sameC = std::all_of(cs.begin(), cs.end(), [&](int x) { return x == cs[0]; });
    std::set<int> cset(cs.begin(), cs.end()); std::set<int> nset(ns.begin(), ns.end());
    if (L <= 4 && sameN && cset.size() == cs.size()) { An g; g.ok = true; g.group = true; std::vector<int> r2 = real; std::sort(r2.begin(), r2.end(), [](int x, int y) { return colOf(x) < colOf(y); }); for (int id : r2) g.cells.push_back({ id, ns[0], colOf(id), false }); std::vector<int> fr; for (int c = 0; c < 4; c++) if (!cset.count(c)) fr.push_back(c); for (size_t i = 0; i < jk.size(); i++) g.cells.push_back({ jk[i], ns[0], fr[i], true }); g.value = ns[0] * L; a = g; }
    if (sameC && nset.size() == ns.size()) { int mn = *nset.begin(), mx = *nset.rbegin(); int lo = std::max(1, mx - L + 1), hi = std::min(mn, 14 - L);
        if (lo <= hi) { int s = hi; std::map<int,int> byN; for (int id : real) byN[numOf(id)] = id; std::vector<int> jl = jk; An r; r.ok = true; for (int k = 0; k < L; k++) { int n = s + k; if (byN.count(n)) r.cells.push_back({ byN[n], n, cs[0], false }); else { r.cells.push_back({ jl.back(), n, cs[0], true }); jl.pop_back(); } } r.value = L * s + L * (L - 1) / 2; if (!a.ok || r.value > a.value) a = r; } }
    return a;
}
struct Rummy : Game {
    float tscale = 1.f; int N = 0, turn = 0, rounds = 1, roundNo = 1, winner = -1, resigned = -1, startP = -1, stuck = 0; bool over = false, roundEnd = false, played = false, sortNum = false; std::vector<int> deck; std::vector<std::vector<int>> hands; std::vector<std::vector<int>> table; std::vector<bool> melded; std::vector<int> scores; std::vector<bool> sel;
    std::vector<int> staged; std::vector<int> stagedTiles; int stagedValue = 0; std::string msg, info; float aiT = 0, aiWait = 1, endT = 0, showT = 0; std::vector<bool> gone; std::vector<int> pool; std::vector<bool> selP; std::vector<std::vector<int>> tableSnap; std::vector<int> handSnap; bool meldedSnap = false;
    void start() override { N = (int)pl.size(); rounds = opts.get("rounds", 0) == 1 ? 3 : 1; scores.assign(N, 0); gone.assign(N, false); newRound(); }
    void sortHand(std::vector<int>& h) { std::sort(h.begin(), h.end(), [&](int a, int b) { if (isJ(a) != isJ(b)) return !isJ(a); if (sortNum) return numOf(a) != numOf(b) ? numOf(a) < numOf(b) : colOf(a) < colOf(b); return colOf(a) != colOf(b) ? colOf(a) < colOf(b) : numOf(a) < numOf(b); }); }
    void newRound() {
        deck.clear(); for (int i = 0; i < NT; i++) deck.push_back(i); for (int i = NT - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]); table.clear(); melded.assign(N, false); stuck = 0; roundEnd = false; hands.assign(N, {});
        for (int p = 0; p < N; p++) { for (int k = 0; k < 14; k++) { hands[p].push_back(deck.back()); deck.pop_back(); } sortHand(hands[p]); }
        startP = (startP + 1) % N; turn = startP; beginTurn(); msg = "Empieza " + pl[turn].name;
    }
    void beginTurn() { tableSnap = table; handSnap = hands[turn]; meldedSnap = melded[turn]; pool.clear(); selP.clear(); played = false; stagedTiles.clear(); stagedValue = 0; staged.clear(); sel.assign(hands[turn].size(), false); aiT = 0; aiWait = 0.9f + rnd01() * 0.8f; }
    int turnPlayer() const override { return over ? -1 : roundEnd ? -1 : turn; }
    int handVal(int p) const { int s = 0; for (int id : hands[p]) s += val(id); return s; }
    void endRound(int w) {
        roundEnd = true; showT = 0; info = ""; int sum = 0; for (int p = 0; p < N; p++) if (p != w && !gone[p]) { sum += handVal(p); scores[p] -= handVal(p); } scores[w] += sum; winner = w; info = pl[w].name + " ganó la ronda (+" + std::to_string(sum) + ")"; app->audio.play("win");
        if (roundNo >= rounds) { over = true; int bs = -1000000; for (int p = 0; p < N; p++) if (scores[p] > bs) { bs = scores[p]; winner = p; } }
    }
    void nextTurn() { if (gone[turn]) {} do { turn = (turn + 1) % N; } while (gone[turn]); beginTurn(); }
    void undoStage() { // vuelve todo al estado del comienzo del turno (mesa, mano y fichas sueltas)
        if (!played && pool.empty()) return; table = tableSnap; hands[turn] = handSnap; melded[turn] = meldedSnap; pool.clear(); selP.clear(); staged.clear(); stagedTiles.clear(); stagedValue = 0; sortHand(hands[turn]); sel.assign(hands[turn].size(), false); played = false;
    }
    void takeTile(int id) { auto it = std::find(hands[turn].begin(), hands[turn].end(), id); if (it != hands[turn].end()) hands[turn].erase(it); else { auto jt = std::find(pool.begin(), pool.end(), id); if (jt != pool.end()) pool.erase(jt); } }
    bool breakMeld(int mi) { if (!melded[turn] || mi < 0 || mi >= (int)table.size()) return false; for (int id : table[mi]) pool.push_back(id); table.erase(table.begin() + mi); for (auto& v : staged) if (v > mi) v--; played = true; selP.assign(pool.size(), false); app->audio.play("click"); msg = "Fichas sueltas: vuelve a colocarlas todas"; return true; }
    bool layNew(const std::vector<int>& ids) {
        An a = analyze(ids); if (!a.ok) { app->toast("Eso no forma un grupo ni una escalera"); return false; }
        for (int id : ids) takeTile(id); selP.assign(pool.size(), false); std::vector<int> o; for (auto& c : a.cells) o.push_back(c.id); table.push_back(o); played = true;
        if (!melded[turn]) { staged.push_back((int)table.size() - 1); for (int id : ids) stagedTiles.push_back(id); stagedValue += a.value; if (stagedValue >= 30) { melded[turn] = true; staged.clear(); stagedTiles.clear(); msg = pl[turn].name + " hizo su primera bajada"; } else msg = "Llevas " + std::to_string(stagedValue) + " de 30"; }
        else msg = pl[turn].name + " bajó " + std::to_string(ids.size()) + " fichas"; sel.assign(hands[turn].size(), false); app->audio.play("place");
        if (hands[turn].empty() && pool.empty() && melded[turn]) endRound(turn); return true;
    }
    bool addTo(int mi, const std::vector<int>& ids) {
        if (!melded[turn]) { app->toast("Primero tu bajada de 30"); return false; } auto all = table[mi]; for (int id : ids) all.push_back(id); An a = analyze(all); if (!a.ok) { app->toast("Esas fichas no encajan en esa jugada"); return false; }
        for (int id : ids) takeTile(id); selP.assign(pool.size(), false); table[mi].clear(); for (auto& c : a.cells) table[mi].push_back(c.id); played = true; sel.assign(hands[turn].size(), false); msg = pl[turn].name + " agregó fichas"; app->audio.play("place");
        if (hands[turn].empty() && pool.empty()) endRound(turn); return true;
    }
    void endTurn() {
        if (!pool.empty()) { app->toast("Coloca todas las fichas sueltas o pulsa Deshacer"); return; }
        if (!played) { if (!deck.empty()) { hands[turn].push_back(deck.back()); deck.pop_back(); sortHand(hands[turn]); stuck = 0; msg = pl[turn].name + " robó una ficha"; } else { stuck++; msg = pl[turn].name + " pasa"; }
            int act = 0; for (int p = 0; p < N; p++) if (!gone[p]) act++; if (deck.empty() && stuck >= act) { int w = 0, bv = 1 << 30; for (int p = 0; p < N; p++) if (!gone[p] && handVal(p) < bv) { bv = handVal(p); w = p; } endRound(w); return; } }
        else { if (!melded[turn]) { undoStage(); if (!deck.empty()) { hands[turn].push_back(deck.back()); deck.pop_back(); sortHand(hands[turn]); } msg = "No llegó a 30: devuelve las fichas y roba"; } else stuck = 0; }
        nextTurn();
    }
    void resign(int p) override { gone[p] = true; resigned = p; int left = 0, w = -1; for (int q = 0; q < N; q++) if (!gone[q]) { left++; w = q; } if (left <= 1) { over = true; roundEnd = true; winner = w; return; } if (turn == p) { undoStage(); nextTurn(); } }
    // IA
    std::vector<std::vector<int>> findMelds(std::vector<int> h, bool allowJoker) {
        std::vector<std::vector<int>> out; std::vector<int> used(NT, 0); std::vector<int> jokers; for (int id : h) if (isJ(id)) jokers.push_back(id);
        // escaleras
        for (int c = 0; c < 4; c++) { std::vector<int> byN(15, -1), byN2(15, -1); for (int id : h) if (!isJ(id) && colOf(id) == c) { if (byN[numOf(id)] < 0) byN[numOf(id)] = id; else byN2[numOf(id)] = id; }
            int n = 1; while (n <= 13) { if (byN[n] < 0) { n++; continue; } int e = n; while (e + 1 <= 13 && byN[e + 1] >= 0) e++; if (e - n + 1 >= 3) { std::vector<int> r; for (int k = n; k <= e; k++) { r.push_back(byN[k]); used[byN[k]] = 1; } out.push_back(r); } n = e + 1; } }
        // grupos con las no usadas
        for (int n = 1; n <= 13; n++) { std::vector<int> g; std::set<int> cc; for (int id : h) if (!isJ(id) && !used[id] && numOf(id) == n && !cc.count(colOf(id))) { g.push_back(id); cc.insert(colOf(id)); } if (g.size() >= 3) { for (int id : g) used[id] = 1; out.push_back(g); } }
        (void)allowJoker; (void)jokers; return out;
    }
    void aiAct() {
        int p = turn; int lv = std::max(1, std::min(3, pl[p].level)); if (lv == 1 && rnd01() < 0.25f) { endTurn(); return; }
        auto ms = findMelds(hands[p], true); int tot = 0; if (!melded[p]) { for (auto& m : ms) tot += analyze(m).value; if (tot < 30) { ms.clear(); } }
        for (auto& m : ms) if (analyze(m).ok) { layNew(m); if (roundEnd) return; }
        if (melded[p]) { bool again = true; int guard = 0; while (again && guard++ < 20) { again = false; for (int i = 0; i < (int)hands[p].size() && !again; i++) { int id = hands[p][i]; if (isJ(id) && lv < 3) continue; for (int mi = 0; mi < (int)table.size(); mi++) { auto all = table[mi]; all.push_back(id); if (analyze(all).ok) { addTo(mi, { id }); if (roundEnd) return; again = true; break; } } } } }
        endTurn();
    }
    void update(float dt, Input& in, UI&) override {
        if (roundEnd) { showT += dt; if (over) { endT += dt; if (endT > 3.f && !finished) { std::vector<PResult> r; int bs = -1000000; for (int p = 0; p < N; p++) bs = std::max(bs, scores[p]); for (int p = 0; p < N; p++) r.push_back({ scores[p], p == winner ? 'w' : 'l' }); if (resigned >= 0) r[resigned].r = 'x'; finish(r, "Ganó " + pl[winner].name + " con " + std::to_string(scores[winner]) + " puntos"); } return; }
            bool adv = showT > 6.f; for (int i = 0; i < N; i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && showT > 1.f) adv = true; if (adv) { roundNo++; newRound(); } return; }
        if (pl[turn].ai) { aiT += dt; if (aiT >= aiWait) aiAct(); }
    }
    Col tileCol(int c) const { static const uint32_t C[4] = { 0xc8332a, 0x1f5fb0, 0xe58a1f, 0x222222 }; return rgb(C[c]); }
    void tile(Gfx& g, Rect r, int id, bool glow, int asN = -1) {
        g.rrect({ r.x + 1, r.y + 3, r.w, r.h }, 6, rgb(0, 90)); g.rrect(r, 6, glow ? pal::gold : rgb(0x8a7a50)); g.rrectGrad(r.inset(glow ? 3.f : 1.5f), 5, rgb(0xfff8e4), rgb(0xe6d8b4));
        if (isJ(id)) { g.text("☺", r.cx(), r.cy(), r.h * 0.5f, rgb(0x2a8a50), F_BOLD, AL_C); if (asN > 0) g.text(std::to_string(asN), r.cx(), r.y + r.h * 0.85f, r.h * 0.18f, rgb(0x444444), F_BOLD, AL_C); }
        else { g.text(std::to_string(numOf(id)), r.cx(), r.y + r.h * 0.42f, r.h * 0.46f, tileCol(colOf(id)), F_TITLE, AL_C); g.circle(r.cx(), r.y + r.h * 0.8f, r.h * 0.09f, tileCol(colOf(id))); }
    }
    void draw(Gfx& g, UI& ui) override {
        bool mine = !over && !roundEnd && !pl[turn].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(turn) || (pl[turn].pad >= 0 && ui.in->pads[pl[turn].pad].held[B_L]));
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(over ? "¡Ganó " + pl[winner].name + "!" : roundEnd ? "Fin de la ronda" : mine ? pl[turn].name + ": tu turno" : "Turno de " + pl[turn].name, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(230.f, 940.f / N); for (int p = 0; p < N; p++) { Rect r{ 290 + p * (pw + 2), 64, pw - 4, 46 }; bool on = p == turn && !over && !roundEnd; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(pl[p].emoji + " " + pl[p].name, r.x + 10, r.y + 15, 14, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 20); g.text(std::to_string(hands[p].size()) + " fichas" + (rounds > 1 ? " · " + std::to_string(scores[p]) + " pts" : ""), r.x + 10, r.y + 34, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 20); }
        g.text("Pozo: " + std::to_string(deck.size()) + (rounds > 1 ? " · ronda " + std::to_string(roundNo) + "/" + std::to_string(rounds) : ""), 1230, 128, 14, pal::cream, F_BODY, AL_R); g.text(msg, 765, 128, 16, pal::cream, F_BODY, AL_C, 700);
        // mesa
        g.rrectGrad({ 290, 142, 950, 250 }, 18, rgb(0x2f7a4a), rgb(0x1f5a36)); const float TW = 34, TH = 46; int sn = -1; std::vector<int> selIds; if (mine) for (size_t i = 0; i < hands[turn].size(); i++) if (sel[i]) selIds.push_back(hands[turn][i]); if (mine) { if (selP.size() != pool.size()) selP.assign(pool.size(), false); for (size_t i = 0; i < pool.size(); i++) if (selP[i]) selIds.push_back(pool[i]); } bool haveSel = !selIds.empty();
        // atril de la mesa: fijo desde el inicio (3 filas); si las jugadas no caben, se achica poco a poco
        auto rowsFor = [&](float sc) { float x = 304, w0 = TW * sc; int rows = 1; for (auto& m : table) { float w = m.size() * (w0 + 2) + 12; if (x + w > 1226) { x = 304; rows++; } x += w + 6; } return rows; };
        float target = 1.f; for (; target > 0.5f; target -= 0.05f) { float rh = (TH * target + 16); if (rowsFor(target) * rh <= 234.f) break; }
        if (target < tscale) tscale = std::max(target, tscale - 0.012f); else tscale = std::min(1.f, std::max(target, tscale + 0.012f)); if (std::fabs(tscale - target) < 0.012f) tscale = target;
        float sc = tscale, tw = TW * sc, th = TH * sc, rh = th + 16; int shelf = std::max(3, (int)(234.f / rh));
        for (int r = 0; r < shelf; r++) { float yy = 152 + r * rh; g.rrect({ 298, yy - 4, 934, th + 8 }, 8, rgb(0, 38)); for (float xs = 304; xs + tw <= 1226; xs += tw + 2) g.rrect({ xs, yy, tw, th }, 4, rgb(0xffffff, 14)); }
        float x = 304, y = 152;
        for (size_t mi = 0; mi < table.size(); mi++) { auto an = analyze(table[mi]); float w = table[mi].size() * (tw + 2) + 12; if (x + w > 1226) { x = 304; y += rh; } Rect mr{ x - 4, y - 4, w - 4, th + 8 }; bool stg = std::find(staged.begin(), staged.end(), (int)mi) != staged.end(); g.rrect(mr, 8, rgb(0, stg ? 40 : 70));
            uint32_t id = hid("rm", (int)mi); bool hit = ui.hit(id, mr, mine && melded[turn]); for (size_t k = 0; k < an.cells.size(); k++) tile(g, { x + k * (tw + 2), y, tw, th }, an.cells[k].id, stg, an.cells[k].j ? an.cells[k].n : -1);
            if (ui.focused(id)) ui.drawFocus(mr, 8); if (hit) { if (haveSel) { if (addTo((int)mi, selIds)) return; } else if (breakMeld((int)mi)) return; } x += w + 6; }
        (void)sn; if (table.empty()) g.text("Aún no hay jugadas en la mesa", 765, 372, 14, pal::cream, F_BOLD, AL_C);
        if (roundEnd) { g.rrect({ 330, 200, 870, 190 }, 18, pal::panelEdge); g.rrectGrad({ 333, 203, 864, 184 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow(over ? "Fin del juego" : "Fin de la ronda", 765, 236, 26, pal::gold, F_TITLE, AL_C); g.text(info, 765, 280, 18, pal::cream, F_BODY, AL_C, 820); std::string sc; for (int p = 0; p < N; p++) sc += pl[p].name + " " + std::to_string(scores[p]) + "   "; g.text(sc, 765, 320, 16, rgb(0xd8b46a), F_BODY, AL_C, 820); if (!over) g.text("A: siguiente ronda", 765, 360, 14, pal::cream, F_BODY, AL_C); return; }
        if (mine) {
            auto& h = hands[turn]; if (sel.size() != h.size()) sel.assign(h.size(), false); std::string st = melded[turn] ? "" : (stagedValue ? "Llevas " + std::to_string(stagedValue) + " de 30 · " : "Primera bajada: 30 puntos · ");
            g.text(st + (haveSel ? std::to_string(selIds.size()) + " fichas elegidas" : "Elige fichas · sobre una jugada de la mesa sin fichas elegidas la rompes") , 765, 408, 15, pal::cream, F_BOLD, AL_C);
            int cnt = (int)selIds.size(); bool b1 = cnt >= 3, b2 = played;
            if (ui.button("Bajar jugada", { 290, 424, 200, 42 }, 3, b1)) { if (layNew(selIds)) return; }
            if (ui.button(played ? "Terminar turno" : "Robar ficha", { 500, 424, 220, 42 }, played ? 3 : 2)) { endTurn(); return; }
            if (ui.button("Deshacer", { 730, 424, 150, 42 }, 2, played || !pool.empty())) undoStage();
            if (ui.button(sortNum ? "Orden: 123" : "Orden: color", { 890, 424, 180, 42 }, 2)) { sortNum = !sortNum; sortHand(h); sel.assign(h.size(), false); }
            (void)b2;
        }
        if (peek) { auto& h = hands[turn]; int n = (int)h.size(); float hw = std::min(56.f, 960.f / std::max(1, n) - 4), hh = hw * 1.35f; float x1 = 765 - (n * (hw + 4) - 4) / 2;
            for (int i = 0; i < n; i++) { Rect r{ x1 + i * (hw + 4), 530.f - (sel[i] ? 16 : 0), hw, hh }; uint32_t id = hid("rh", i); bool hit = ui.hit(id, r, mine); tile(g, r, h[i], sel[i]); if (ui.focused(id)) ui.drawFocus(r, 6); if (hit) sel[i] = !sel[i]; }
            if (onPhone(turn)) coverHand(g, ui, "rh", n, turn);
            if (n) ui.setFocusIfNone(hid("rh", 0)); }
        if (peek && !pool.empty()) { int n = (int)pool.size(); float hw = std::min(40.f, 900.f / std::max(1, n) - 4), hh = hw * 1.3f; float x1 = 765 - (n * (hw + 4) - 4) / 2; g.text("Fichas sueltas: colócalas todas en jugadas", 765, 463, 14, pal::gold, F_BOLD, AL_C);
            for (int i = 0; i < n; i++) { Rect r{ x1 + i * (hw + 4), 474.f - (selP[i] ? 8 : 0), hw, hh }; uint32_t id = hid("rp", i); bool hit = ui.hit(id, r, mine); tile(g, r, pool[i], selP[i]); if (ui.focused(id)) ui.drawFocus(r, 6); if (hit) selP[i] = !selP[i]; } }
        else if (mine) g.text("Mantén L para ver tus fichas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::string secret(int p) const override { if (p < 0 || p >= (int)hands.size() || pl[p].ai || hands[p].empty()) return ""; bool me = p == turn; static const char* CE[4] = { "🔴", "🔵", "🟠", "⚫" };
        std::string s = me ? "Tu turno · tus fichas:\n" : "Tus fichas:\n"; for (size_t i = 0; i < hands[p].size(); i++) { int id = hands[p][i]; s += mark("rh", (int)i, me) + (isJ(id) ? std::string("☺ comodín") : std::string(CE[colOf(id) & 3]) + " " + std::to_string(numOf(id))) + (me && i < sel.size() && sel[i] ? " ✔" : "") + ((i % 2) ? "\n" : "    "); } return s; }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir ficha / jugada" }, { "L", "Ver tus fichas" } }; }
    std::string help() const override { return "Cada jugador tiene 14 fichas. Una jugada es un GRUPO (3 o 4 fichas del mismo número y colores distintos) o una ESCALERA (3 o más fichas del mismo color y números seguidos). El comodín vale por cualquier ficha. Tu primera bajada debe sumar 30 o más con fichas de tu mano. Después puedes sumar fichas a las jugadas de la mesa o romper una (sin fichas elegidas, A sobre ella) para reordenar: todas las fichas sueltas deben quedar colocadas antes de terminar el turno; Deshacer vuelve al inicio del turno. Si no puedes jugar, robas una ficha. Gana quien se queda sin fichas."; }
};
}
Game* makeRummy() { return new Rummy(); }
