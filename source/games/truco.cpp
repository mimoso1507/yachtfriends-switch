// Truco argentino (sin flor), 2/4/6 jugadores (equipos: pares contra impares). 3 cartas, se juegan hasta 3 bazas. Truco → Retruco → Vale cuatro y Envido / Real envido / Falta envido en la primera baza.
// La IA simula manos posibles de las cartas que no vio (Monte Carlo liviano) y decide qué jugar, cuándo cantar y cuándo aceptar o subir.
#include "spanish.h"
#include <algorithm>
#include <cmath>

namespace {
struct Cd { int s, r; };
int power(const Cd& c) {
    if (c.r == 1) return c.s == 2 ? 14 : c.s == 3 ? 13 : 8; if (c.r == 7) return c.s == 2 ? 12 : c.s == 0 ? 11 : 4;
    switch (c.r) { case 3: return 10; case 2: return 9; case 12: return 7; case 11: return 6; case 10: return 5; case 6: return 3; case 5: return 2; default: return 1; }
}
int envScore(const std::vector<Cd>& h) { auto v = [](const Cd& c) { return c.r <= 7 ? c.r : 0; }; int best = 0; for (auto& c : h) best = std::max(best, v(c)); for (size_t i = 0; i < h.size(); i++) for (size_t j = i + 1; j < h.size(); j++) if (h[i].s == h[j].s) best = std::max(best, 20 + v(h[i]) + v(h[j])); return best; }
int handWinner(const std::vector<int>& r, int manoTeam) {
    int w[2] = { 0, 0 }; for (int x : r) if (x >= 0) w[x]++; if (w[0] >= 2) return 0; if (w[1] >= 2) return 1;
    if (r.size() >= 2) { if (r[0] == -1 && r[1] != -1) return r[1]; if (r[0] != -1 && r[1] == -1) return r[0]; }
    if (r.size() == 3) { if (r[2] != -1) return r[2]; return r[0] != -1 ? r[0] : manoTeam; }
    return -1;
}
const char* TNAME[5] = { "", "", "TRUCO", "RETRUCO", "VALE CUATRO" }; const char* ENAME[3] = { "ENVIDO", "REAL ENVIDO", "FALTA ENVIDO" }; const int EPTS[3] = { 2, 3, 0 };
std::vector<int> g_envAll;
const std::vector<int>& envAll() { if (g_envAll.empty()) { std::vector<Cd> d; for (int s = 0; s < 4; s++) for (int r : sp::RANKS) d.push_back({ s, r }); for (int i = 0; i < 40; i++) for (int j = i + 1; j < 40; j++) for (int k = j + 1; k < 40; k++) g_envAll.push_back(envScore({ d[i], d[j], d[k] })); } return g_envAll; }
float envWinProb(int E, int n, bool strongOpp) {
    auto& all = envAll(); int m = std::max(1, n / 2), T = 120; float win = 0; auto draw = [&]() { return all[rndInt((int)all.size())]; };
    for (int t = 0; t < T; t++) { int mine = E; for (int i = 1; i < m; i++) mine = std::max(mine, draw()); int opp = -1; for (int j = 0; j < m; j++) { int v = draw(); if (j == 0 && strongOpp && rnd01() < 0.7f) v = std::max(v, draw()); opp = std::max(opp, v); } win += mine > opp ? 1.f : mine == opp ? 0.5f : 0.f; }
    return win / T;
}
struct Tp { int p; Cd c; };
struct Pend { int type = 0; int level = 2; std::vector<int> stakes; int by = 0; bool hasUnder = false; int underLevel = 0, underBy = 0; };   // type 1 truco, 2 envido

struct Truco : Game {
    int N = 0, target = 15, dealer = 0, mano = 0, turn = 0, level = 1, holder = -1, starter = 0, handNo = 0, winner = -1, resigned = -1; int scores[2] = { 0, 0 };
    std::vector<std::vector<Cd>> hands, dealt; std::vector<Tp> trick; std::vector<std::vector<Tp>> tricks; std::vector<int> results; bool over = false, envDone = false, handEnd = false, hasPend = false; Pend pend;
    std::string msg, handInfo, envInfo; float aiT = 0, aiWait = 1, endT = 0, showT = 0; int lastCaller = -1;
    int team(int p) const { return p % 2; }
    int real = 0; void start() override { real = (int)pl.size(); if (real == 3 || real == 5) { Player b; b.name = "IA Compañero"; b.emoji = "🤖"; b.ai = true; b.level = 2; pl.push_back(b); } N = (int)pl.size(); target = opts.get("target", 0) == 1 ? 30 : 15; dealer = N - 1; handNo = 0; newHand(); }
    void newHand() {
        handNo++; dealer = (dealer + 1) % N; mano = (dealer + 1) % N; std::vector<Cd> d; for (int s = 0; s < 4; s++) for (int r : sp::RANKS) d.push_back({ s, r }); for (int i = 39; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]);
        hands.assign(N, {}); for (int p = 0; p < N; p++) for (int k = 0; k < 3; k++) { hands[p].push_back(d.back()); d.pop_back(); } dealt = hands; trick.clear(); tricks.clear(); results.clear(); starter = turn = mano; level = 1; holder = -1; hasPend = false; envDone = false; handEnd = false; envInfo.clear(); handInfo.clear(); msg = "Reparte " + pl[dealer].name; aiT = 0; aiWait = 0.9f + rnd01() * 0.8f; showT = 0;
    }
    int turnPlayer() const override { return over ? -1 : actor(); }
    int responder() const { return hasPend ? (pend.by + 1) % N : -1; }
    std::string secret(int p) const override { if (p < 0 || p >= N || pl[p].ai || p >= (int)hands.size() || hands[p].empty()) return ""; bool me = p == actor(); std::string s = me ? "Tu turno · tus cartas:\n" : "Tus cartas:\n"; for (size_t i = 0; i < hands[p].size(); i++) s += mark("tc", (int)i, me) + std::to_string(hands[p][i].r) + " de " + sp::suitName(hands[p][i].s) + "\n"; return s; }
    int actor() const { return handEnd ? -1 : hasPend ? responder() : turn; }
    bool canEnvido() const { return !handEnd && !hasPend && results.empty() && !envDone && level == 1; }
    int fal() const { return std::max(1, target - std::max(scores[0], scores[1])); }
    int stakePts(const std::vector<int>& st, size_t upto) const { int s = 0; for (size_t i = 0; i < upto && i < st.size(); i++) s += st[i] == 2 ? fal() : EPTS[st[i]]; return s; }
    std::vector<int> envRaises(const std::vector<int>& st) const { std::vector<int> o; int last = st.back(); if (last == 2) return o; if (last == 0) { if (std::count(st.begin(), st.end(), 0) < 2) o.push_back(0); o.push_back(1); o.push_back(2); } else o.push_back(2); return o; }
    void say(int p, const std::string& s) { msg = pl[p].name + ": " + s; app->audio.play("click"); aiT = 0; aiWait = 0.9f + rnd01() * 0.7f; }
    void endHand(int t, int pts, const std::string& why) { scores[t] += pts; handEnd = true; hasPend = false; handInfo = "Se lleva " + std::to_string(pts) + " " + (pts == 1 ? "punto" : "puntos") + " el equipo " + (t == 0 ? "A" : "B") + " (" + why + ")"; showT = 0; app->audio.play("win"); if (scores[t] >= target) { over = true; winner = t; } }
    void play(int p, int ci) {
        Cd c = hands[p][ci]; hands[p].erase(hands[p].begin() + ci); trick.push_back({ p, c }); app->audio.play("place"); msg = pl[p].name + " jugó"; aiT = 0; aiWait = 0.8f + rnd01() * 0.8f;
        if ((int)trick.size() < N) { turn = (p + 1) % N; return; }
        int best = -1, win = -2; bool tie = false; for (auto& t : trick) { int pw = power(t.c); if (pw > best) { best = pw; win = t.p; tie = false; } else if (pw == best && team(t.p) != team(win)) tie = true; }
        results.push_back(tie ? -1 : team(win)); tricks.push_back(trick); trick.clear(); int hw = handWinner(results, team(mano));
        if (hw >= 0) { endHand(hw, level, "las bazas"); return; } turn = tie ? starter : win; starter = turn; msg = tie ? "Parda" : "Baza para " + pl[win].name;
    }
    void call(int p, int kind) {   // kind 10 truco, 0..2 envido
        if (kind == 10) { pend = Pend(); pend.type = 1; pend.level = level + 1; pend.by = p; hasPend = true; say(p, TNAME[level + 1]); }
        else { pend = Pend(); pend.type = 2; pend.stakes = { kind }; pend.by = p; hasPend = true; say(p, ENAME[kind]); }
    }
    void respond(int p, int r) {   // r: 0 quiero, 1 no, 2 subir (truco), 10+k subir envido k, 20+k envido sobre truco
        if (!hasPend) return; Pend pd = pend;
        if (pd.type == 1) {
            if (r == 0) { level = pd.level; holder = team(p); hasPend = false; say(p, "¡Quiero!"); }
            else if (r == 1) { say(p, "No quiero"); int t = team(pd.by); int pts = level; hasPend = false; endHand(t, pts, "no quiso el " + std::string(TNAME[pd.level])); }
            else if (r == 2) { level = pd.level; holder = team(p); Pend np; np.type = 1; np.level = pd.level + 1; np.by = p; pend = np; say(p, TNAME[np.level]); }
            else if (r >= 20) { Pend np; np.type = 2; np.stakes = { r - 20 }; np.by = p; np.hasUnder = true; np.underLevel = pd.level; np.underBy = pd.by; pend = np; say(p, ENAME[r - 20]); }
            return;
        }
        if (r == 0 || r == 1) { resolveEnvido(p, r == 0); return; }
        if (r >= 10) { Pend np = pd; np.stakes.push_back(r - 10); np.by = p; pend = np; say(p, ENAME[r - 10]); }
    }
    void resolveEnvido(int p, bool wanted) {
        Pend pd = pend; int ct = team(pd.by), wt; int pts; envDone = true; std::vector<int> sc(N); for (int i = 0; i < N; i++) sc[i] = envScore(dealt[i]);
        if (!wanted) { wt = ct; pts = pd.stakes.size() > 1 ? stakePts(pd.stakes, pd.stakes.size() - 1) : 1; envInfo = "Envido: no quiso. +" + std::to_string(pts) + " para el equipo " + (wt == 0 ? "A" : "B"); say(p, "No quiero"); }
        else { int bestP = -1, bv = -1; for (int k = 0; k < N; k++) { int i = (mano + k) % N; if (sc[i] > bv) { bv = sc[i]; bestP = i; } } wt = team(bestP); pts = stakePts(pd.stakes, pd.stakes.size());
            envInfo = "Envido: "; for (int i = 0; i < N; i++) envInfo += pl[i].name + " " + std::to_string(sc[i]) + (i + 1 < N ? " · " : ""); envInfo += " → +" + std::to_string(pts) + " para el equipo " + (wt == 0 ? "A" : "B"); say(p, "¡Quiero!"); }
        scores[wt] += pts; hasPend = false; if (pd.hasUnder) { Pend tp; tp.type = 1; tp.level = pd.underLevel; tp.by = pd.underBy; pend = tp; hasPend = true; }
        if (scores[wt] >= target) { over = true; winner = wt; hasPend = false; handEnd = true; handInfo = envInfo; }
    }
    void fold(int p) { say(p, "Me voy al mazo"); endHand(1 - team(p), level, pl[p].name + " se fue al mazo"); }
    void resign(int p) override { over = true; resigned = p; winner = 1 - team(p); handEnd = true; }
    // --------- IA ---------
    struct World { std::vector<std::vector<Cd>> h; float w; };
    int pickSim(const std::vector<Cd>& h, const std::vector<Tp>& tr, int p, bool first) const {
        int L = (int)h.size(); if (L == 1) return 0; int lo = 0, hi = 0, plo = 99, phi = -1; for (int i = 0; i < L; i++) { int x = power(h[i]); if (x < plo) { plo = x; lo = i; } if (x > phi) { phi = x; hi = i; } }
        if (tr.empty()) { if (!first) return hi; if (L == 2) return hi; int md = 0; for (int i = 0; i < 3; i++) if (i != lo && i != hi) md = i; return md; }
        int bp = -1, bw = -1; for (auto& t : tr) { int x = power(t.c); if (x > bp) { bp = x; bw = t.p; } } if (team(bw) == team(p)) return lo;
        int bi = -1, bpw = 99; for (int i = 0; i < L; i++) { int x = power(h[i]); if (x > bp && x < bpw) { bpw = x; bi = i; } } return bi >= 0 ? bi : lo;
    }
    int simRest(std::vector<std::vector<Cd>> hs, std::vector<Tp> tr, int t, int st, std::vector<int> res, int manoTeam) const {
        for (int g = 0; g < 6; g++) { while ((int)tr.size() < N) { auto& h = hs[t]; if (h.empty()) return manoTeam; int ci = pickSim(h, tr, t, res.empty()); tr.push_back({ t, h[ci] }); h.erase(h.begin() + ci); t = (t + 1) % N; }
            int best = -1, win = -2; bool tie = false; for (auto& x : tr) { int pw = power(x.c); if (pw > best) { best = pw; win = x.p; tie = false; } else if (pw == best && team(x.p) != team(win)) tie = true; }
            res.push_back(tie ? -1 : team(win)); int hw = handWinner(res, manoTeam); if (hw >= 0) return hw; t = tie ? st : win; st = t; tr.clear(); }
        return manoTeam;
    }
    std::vector<World> worlds(int idx, int K) const {
        std::vector<Cd> unseen; auto known = [&](const Cd& c) { for (auto& x : hands[idx]) if (x.s == c.s && x.r == c.r) return true; for (auto& t : tricks) for (auto& y : t) if (y.c.s == c.s && y.c.r == c.r) return true; for (auto& y : trick) if (y.c.s == c.s && y.c.r == c.r) return true; return false; };
        for (int s = 0; s < 4; s++) for (int r : sp::RANKS) { Cd c{ s, r }; if (!known(c)) unseen.push_back(c); }
        int tot = 0; for (int i = 0; i < N; i++) if (i != idx) tot += (int)hands[i].size(); std::vector<World> out; if (tot > (int)unseen.size()) return out;
        for (int k = 0; k < K; k++) { for (int i = 0; i < tot; i++) std::swap(unseen[i], unseen[i + rndInt((int)unseen.size() - i)]); World w; w.w = 1; int pos = 0; w.h.assign(N, {}); for (int i = 0; i < N; i++) if (i != idx) for (size_t j = 0; j < hands[i].size(); j++) w.h[i].push_back(unseen[pos++]); out.push_back(w); }
        return out;
    }
    float evalCard(int idx, const std::vector<World>& ws, int ci) const {
        if (ws.empty()) return 0.5f; float sum = 0; for (auto& w : ws) { auto hs = w.h; hs[idx] = hands[idx]; auto tr = trick; int t = turn; int st = trick.empty() ? turn : trick[0].p; auto res = results; if (ci >= 0) { tr.push_back({ idx, hs[idx][ci] }); hs[idx].erase(hs[idx].begin() + ci); t = (idx + 1) % N; }
            sum += simRest(hs, tr, t, st, res, team(mano)) == team(idx) ? 1.f : 0.f; } return sum / ws.size();
    }
    void aiAct() {
        int p = actor(); int lv = std::max(1, std::min(3, pl[p].level)); int K = lv == 1 ? 25 : lv == 2 ? 60 : 100; float err = lv == 1 ? 0.35f : lv == 2 ? 0.08f : 0.02f;
        if (hasPend && p == responder()) {
            int E = envScore(dealt[p]);
            if (pend.type == 2) {
                float pE = envWinProb(E, N, true); auto rs = envRaises(pend.stakes); int S = std::max(1, stakePts(pend.stakes, pend.stakes.size())), R = pend.stakes.size() > 1 ? stakePts(pend.stakes, pend.stakes.size() - 1) : 1;
                if (!rs.empty() && pE >= 0.78f && rnd01() < 0.7f) { respond(p, 10 + rs.back()); return; }
                float thr = 0.5f - (float)R / (2 * S) + 0.08f + (S >= 8 ? 0.08f : 0.f) + (rnd01() - 0.5f) * 0.1f * (lv == 1 ? 4 : 1); if (scores[1 - team(p)] + R >= target) thr -= 0.12f;
                respond(p, rnd01() < err * 0.5f ? (rnd01() < 0.5f ? 0 : 1) : (pE >= thr ? 0 : 1)); return;
            }
            if (canEnvidoUnder() && E >= 28 && rnd01() < 0.6f) { respond(p, 20 + (E >= 31 && rnd01() < 0.5f ? 1 : 0)); return; }
            auto ws = worlds(p, K); float pw = evalCard(p, ws, -1); int L = pend.level; float thr = 1.f / (2 * L) + 0.13f + (rnd01() - 0.5f) * (lv == 1 ? 0.5f : 0.1f);
            if (L < 4 && pw > 0.82f && rnd01() < 0.6f) { respond(p, 2); return; } respond(p, pw >= thr ? 0 : 1); return;
        }
        // mi turno
        if (canEnvido()) { int E = envScore(dealt[p]); int k = -1; float r = rnd01(); if (E >= 32 && r < 0.3f) k = 2; else if (E >= 30) k = rnd01() < 0.55f ? 1 : 0; else if (E >= 27) k = rnd01() < 0.85f ? 0 : 1; else if (E >= 25) k = rnd01() < 0.45f ? 0 : -1; else if (E >= 23) k = rnd01() < 0.12f ? 0 : -1; else if (rnd01() < 0.04f) k = 0;
            if (lv == 1 && rnd01() < 0.4f) k = -1; if (k >= 0) { call(p, k); return; } }
        auto ws = worlds(p, K); int n = (int)hands[p].size(); std::vector<float> pc(n); int bi = 0; float bs = -1; for (int i = 0; i < n; i++) { pc[i] = evalCard(p, ws, i) + rnd01() * 0.01f; if (pc[i] > bs) { bs = pc[i]; bi = i; } }
        if (level < 4 && (holder == -1 || holder == team(p)) && !(level >= 2 && holder == team(p) && false)) { float need = level == 1 ? 0.66f : level == 2 ? 0.78f : 0.86f; if (lv == 1) need += 0.1f; if (bs > need && rnd01() < (lv == 1 ? 0.3f : 0.65f)) { call(p, 10); return; } if (level == 1 && lv >= 2 && bs > 0.4f && rnd01() < 0.04f) { call(p, 10); return; } }
        if (lv >= 2 && bs < 0.07f && level >= 2) { fold(p); return; }
        int ci = rnd01() < err ? rndInt(n) : bi; play(p, ci);
    }
    bool canEnvidoUnder() const { return hasPend && pend.type == 1 && pend.level == 2 && results.empty() && !envDone; }
    void update(float dt, Input& in, UI&) override {
        if (over && handEnd) { endT += dt; if (endT > 3.f && !finished) { std::vector<PResult> r; for (int p = 0; p < N; p++) r.push_back({ scores[team(p)], team(p) == winner ? 'w' : 'l' }); if (resigned >= 0 && resigned < real) r[resigned].r = 'x'; finish(r, "Ganó el equipo " + std::string(winner == 0 ? "A" : "B") + " (" + std::to_string(scores[0]) + " a " + std::to_string(scores[1]) + ")"); } return; }
        if (handEnd) { showT += dt; bool adv = showT > 5.5f; for (int i = 0; i < N; i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && showT > 1.f) adv = true; if (adv) newHand(); return; }
        int a = actor(); if (a >= 0 && pl[a].ai) { aiT += dt; if (aiT >= aiWait) aiAct(); }
    }
    std::string seatLbl(int p) const { return pl[p].emoji + " " + pl[p].name; }
    void draw(Gfx& g, UI& ui) override {
        int a = actor(); bool mine = !over && !handEnd && a >= 0 && !pl[a].ai; int humans = 0; for (auto& p : pl) if (!p.ai) humans++; bool peek = mine && (humans <= 1 || onPhone(a) || (pl[a].pad >= 0 && ui.in->pads[pl[a].pad].held[B_L]));
        std::string banner = handEnd ? (over ? "¡Ganó el equipo " + std::string(winner == 0 ? "A" : "B") + "!" : "Fin de la mano") : hasPend ? (pend.type == 1 ? std::string(TNAME[pend.level]) : std::string(ENAME[pend.stakes.back()])) + " de " + pl[pend.by].name + ": responde " + pl[responder()].name : mine ? pl[a].name + ": tu turno" : "Turno de " + pl[turn].name;
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        for (int t = 0; t < 2; t++) { Rect r{ 290.f + t * 480, 66, 470, 52 }; g.rrect(r, 10, rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); std::string nm = "Equipo " + std::string(t ? "B: " : "A: "); for (int p = t; p < N; p += 2) nm += pl[p].name + (p + 2 < N ? ", " : ""); g.text(nm, r.x + 12, r.y + 16, 14, pal::white, F_BOLD, AL_L, 360); g.text(std::to_string(scores[t]) + " / " + std::to_string(target), r.x + r.w - 12, r.cy(), 22, pal::gold, F_TITLE, AL_R); }
        g.text("Reparte " + pl[dealer].name + " · mano " + pl[mano].name + " · " + (level == 1 ? "sin truco" : TNAME[level]), 765, 134, 15, pal::cream, F_BODY, AL_C);
        // bazas
        for (size_t k = 0; k < results.size(); k++) g.text("Baza " + std::to_string(k + 1) + ": " + (results[k] < 0 ? "parda" : std::string("equipo ") + (results[k] == 0 ? "A" : "B")), 1230, 160.f + k * 20, 14, pal::cream, F_BODY, AL_R);
        std::vector<Tp> show = trick; if (show.empty() && !tricks.empty()) show = tricks.back(); float cw = 72, chh = 102, gap = 10, x0 = 765 - (show.size() * (cw + gap) - gap) / 2; for (size_t i = 0; i < show.size(); i++) { Rect r{ x0 + i * (cw + gap), 190, cw, chh }; sp::draw(g, r, show[i].c.s, show[i].c.r, true, false, trick.empty()); g.text(pl[show[i].p].name, r.cx(), r.y + chh + 14, 13, pal::cream, F_BOLD, AL_C, cw + gap); }
        if (show.empty()) g.text("Mesa vacía", 765, 240, 18, pal::cream, F_BODY, AL_C);
        std::string cnt; for (int p = 0; p < N; p++) cnt += pl[p].name + ": " + std::to_string(hands[p].size()) + " cartas   "; g.text(cnt, 765, 330, 13, rgb(0xd8b46a), F_BODY, AL_C, 940); g.text(msg, 765, 352, 17, pal::cream, F_BOLD, AL_C, 940);
        if (handEnd) { g.rrect({ 320, 372, 890, 230 }, 18, pal::panelEdge); g.rrectGrad({ 323, 375, 884, 224 }, 16, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow(over ? "¡Fin del juego!" : "Fin de la mano", 765, 402, 24, pal::gold, F_TITLE, AL_C); g.text(handInfo, 765, 440, 17, pal::cream, F_BODY, AL_C, 850); if (!envInfo.empty()) g.text(envInfo, 765, 476, 15, rgb(0xd8b46a), F_BODY, AL_C, 850);
            for (int p = 0; p < N; p++) for (size_t k = 0; k < dealt[p].size(); k++) { Rect r{ 330 + p * (860.f / N) + k * 34, 508, 30, 42 }; sp::draw(g, r, dealt[p][k].s, dealt[p][k].r, true); } g.text("A: seguir", 765, 586, 14, pal::cream, F_BODY, AL_C); return; }
        // acciones y mano
        if (mine) {
            struct Bt { std::string l; int act; int st; }; std::vector<Bt> bs;
            if (hasPend) { bs.push_back({ "¡Quiero!", 0, 3 }); bs.push_back({ "No quiero", 1, 1 }); if (pend.type == 1 && pend.level < 4) bs.push_back({ std::string("¡") + TNAME[pend.level + 1] + "!", 2, 2 }); if (pend.type == 1 && canEnvidoUnder()) for (int k = 0; k < 3; k++) bs.push_back({ ENAME[k], 20 + k, 2 }); if (pend.type == 2) for (int k : envRaises(pend.stakes)) bs.push_back({ ENAME[k], 10 + k, 2 }); }
            else { if (level < 4 && (holder == -1 || holder == team(a))) bs.push_back({ level == 1 ? "¡Truco!" : level == 2 ? "¡Retruco!" : "¡Vale cuatro!", 10, 3 }); if (canEnvido()) for (int k = 0; k < 3; k++) bs.push_back({ ENAME[k], k, 2 }); bs.push_back({ "Irme al mazo", 99, 1 }); }
            int n = (int)bs.size(); float bw = std::min(180.f, 940.f / n); float bx0 = 765 - (n * (bw + 6) - 6) / 2;
            for (int i = 0; i < n; i++) { Rect r{ bx0 + i * (bw + 6), 384, bw, 42 }; if (ui.button(bs[i].l, r, bs[i].st)) { if (bs[i].act == 99) fold(a); else if (hasPend) respond(a, bs[i].act); else call(a, bs[i].act); return; } }
            if (hasPend) { Rect r0{ bx0, 384, bw, 42 }; ui.setFocusIfNone(hid("¡Quiero!", (int)(r0.x * 7 + r0.y))); }
        }
        if (peek) { auto& h = hands[a]; int n = (int)h.size(); float hw = 96, hh = 134, x1 = 765 - (n * (hw + 14) - 14) / 2; bool canPlay = !hasPend && turn == a;
            for (int i = 0; i < n; i++) { Rect r{ x1 + i * (hw + 14), 480, hw, hh }; uint32_t id = hid("tc", i); bool hit = ui.hit(id, r, canPlay); sp::draw(g, r, h[i].s, h[i].r, true); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { play(a, i); return; } }
            if (onPhone(a)) coverHand(g, ui, "tc", n, a);
            if (canPlay && n && !mine == false) ui.setFocusIfNone(hid("tc", 0)); if (!canPlay) g.text("Responde con los botones", 765, 470, 15, pal::cream, F_BODY, AL_C); }
        else if (mine) g.text("Mantén L para ver tus cartas", 765, 560, 20, pal::cream, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Jugar carta / elegir" }, { "L", "Ver tus cartas" } }; }
    std::string help() const override { return "Truco argentino sin flor. Cada uno recibe 3 cartas y se juegan hasta 3 bazas; gana la mano quien gana 2. Puedes cantar TRUCO (2 puntos), el rival responde quiero, no quiero o sube a RETRUCO (3) y VALE CUATRO (4). En la primera baza se puede cantar ENVIDO, REAL ENVIDO o FALTA ENVIDO: gana quien sume más con las cartas del mismo palo (20 + las dos cartas). Las mejores cartas son el ancho de espadas, el ancho de bastos, el 7 de espadas y el 7 de oros. Gana quien llega primero al objetivo."; }
};
}
Game* makeTruco() { return new Truco(); }
