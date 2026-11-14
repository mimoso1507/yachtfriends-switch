// Negocios: tablero de 40 casillas para comprar calles, cobrar alquileres y quebrar a los demás (2 a 8 jugadores).
// Salida, alquileres, cartas y movimiento son automáticos; subasta si nadie compra.
#include "party.h"
#include "negocios_data.h"

namespace {
using namespace nd;
const int N = 40, SAL = 200, JAIL = 10, FINE = 50;
enum Ph { ROLL, MOVE, BUY, AUC, END, DEBT, OVER };
const uint32_t PC[8] = { 0xe53935, 0x1e88e5, 0xfdd835, 0x43a047, 0x8e24aa, 0xfb8c00, 0x00acc1, 0xec407a };

struct Negocios : Party {
    int n = 0, turn = 0, round = 0, limit = 0, dbl = 0, pend = -1, d1 = 1, d2 = 1, cont = 0; Ph phase = ROLL; bool extra = false, over = false, rolling = false, manage = false; int mi = 0;
    std::vector<int> money, pos, jail, jt, gooj, owner, houses, mort; std::vector<bool> alive; std::vector<int> deckS, deckC;
    struct Auc { int bid = 0, lead = -1, cur = 0; std::vector<int> pass; } auc; struct Debt { int to = -1, amt = 0, per = 0; } debt; bool hasDebt = false;
    std::vector<std::string> logv; std::string cardTxt; float aiT = 0, rollT = 0, endT = 0, shake = 0;
    std::vector<int> OWN, STATIONS, UTILS;
    // tratos: ofrecer una propiedad (o ninguna) por otra (o ninguna) más una diferencia en dinero
    bool tradeOn = false, tradeAsk = false; int tf = 0, tp = 0, gi = 0, ti = 0, tcash = 0;
    bool tradable(int k) const { if (k < 0) return true; if (BOARD[k].t == 'p') for (int q : groupSq(BOARD[k].g)) if (houses[q] > 0) return false; return true; }
    std::vector<int> tradeList(int p) const { std::vector<int> v; for (int k : ownedList(p)) if (tradable(k)) v.push_back(k); return v; }
    int partnerAfter(int from, int dir) const { int i = from; for (int g = 0; g < n; g++) { i = (i + dir + n) % n; if (i != turn && alive[i]) return i; } return from; }
    float tval(int q, int k) const { if (k < 0) return 0; float v = (float)BOARD[k].price * (mort[k] ? 0.5f : 1.f); if (BOARD[k].t == 'p') { int g = BOARD[k].g; if (setCount(q, g) + 1 == (int)groupSq(g).size()) v *= 1.7f; else if (setCount(q, g) > 0) v *= 1.15f; } return v; }
    bool tradeValid(int gk, int tk) const { if (tp == turn || !alive[tp]) return false; if (gk >= 0 && (owner[gk] != turn || !tradable(gk))) return false; if (tk >= 0 && (owner[tk] != tp || !tradable(tk))) return false; if (gk < 0 && tk < 0 && tcash == 0) return false; if (tcash > 0 && money[turn] < tcash) return false; if (tcash < 0 && money[tp] < -tcash) return false; return true; }
    void doTrade(int gk, int tk) { if (gk >= 0) owner[gk] = tp; if (tk >= 0) owner[tk] = turn; money[turn] -= tcash; money[tp] += tcash; say(nm(turn) + " cerró un trato con " + nm(tp) + "."); app->audio.play("win"); tradeOn = false; tradeAsk = false; }
    bool aiAccepts(int gk, int tk) const { float recv = tval(tp, gk) + (tcash > 0 ? tcash : 0), give = tval(tp, tk) + (tcash < 0 ? -tcash : 0); return recv >= give * 1.08f && (gk >= 0 || tcash > 0); }
    void propose() { auto ml = tradeList(turn), tl = tradeList(tp); int gk = gi < 0 || gi >= (int)ml.size() ? -1 : ml[gi], tk = ti < 0 || ti >= (int)tl.size() ? -1 : tl[ti]; if (!tradeValid(gk, tk)) { app->toast("Ese trato no es válido"); return; }
        if (isAI(tp)) { if (aiAccepts(gk, tk)) doTrade(gk, tk); else { say(nm(tp) + " rechazó el trato."); tradeOn = false; } } else tradeAsk = true; }

    // ---------- reglas ----------
    static std::string cur(int v) { return "₡" + std::to_string(v); }
    void say(const std::string& t) { logv.push_back(t); if (logv.size() > 6) logv.erase(logv.begin()); }
    std::string nm(int p) const { return pl[p].name; }
    bool ownable(int k) const { char t = BOARD[k].t; return t == 'p' || t == 't' || t == 'u'; }
    int hcost(int k) const { return GHC[BOARD[k].g]; }
    std::vector<int> groupSq(int g) const { std::vector<int> v; for (int k = 0; k < N; k++) if (BOARD[k].t == 'p' && BOARD[k].g == g) v.push_back(k); return v; }
    bool ownsAll(int p, int g) const { for (int k : groupSq(g)) if (owner[k] != p) return false; return true; }
    int aliveCount() const { int c = 0; for (int i = 0; i < n; i++) if (alive[i]) c++; return c; }
    int wealth(int p) const { int w = money[p]; for (int k : OWN) if (owner[k] == p) { w += mort[k] ? BOARD[k].price / 2 : BOARD[k].price; if (BOARD[k].t == 'p') w += houses[k] * hcost(k); } return w; }
    int maxRaise(int p) const { int r = 0; for (int k : OWN) if (owner[k] == p) { if (!mort[k]) r += BOARD[k].price / 2; if (BOARD[k].t == 'p') r += houses[k] * hcost(k) / 2; } return r; }
    bool canBuild(int p, int k) const {
        if (k < 0 || k >= N || BOARD[k].t != 'p' || owner[k] != p || !alive[p] || !ownsAll(p, BOARD[k].g)) return false; auto gs = groupSq(BOARD[k].g);
        for (int q : gs) if (mort[q]) return false; if (houses[k] >= 5 || money[p] < hcost(k)) return false; int mn = 9; for (int q : gs) mn = std::min(mn, houses[q]); return houses[k] <= mn; }
    bool canSell(int p, int k) const { if (k < 0 || k >= N || BOARD[k].t != 'p' || owner[k] != p || houses[k] <= 0) return false; int mx = 0; for (int q : groupSq(BOARD[k].g)) mx = std::max(mx, houses[q]); return houses[k] >= mx; }
    bool canMortgage(int p, int k) const { if (k < 0 || k >= N || !ownable(k) || owner[k] != p || mort[k]) return false; if (BOARD[k].t == 'p') for (int q : groupSq(BOARD[k].g)) if (houses[q] > 0) return false; return true; }
    int unmortCost(int k) const { int h = BOARD[k].price / 2; return h + (h + 9) / 10; }
    bool canUnmort(int p, int k) const { return k >= 0 && k < N && ownable(k) && owner[k] == p && mort[k] && money[p] >= unmortCost(k); }
    bool mayManage(int p) const { return turn == p && alive[p] && !over && (phase == ROLL || phase == END || phase == DEBT); }
    int rentOf(int k, int dice, int mult) const {
        const Sq& b = BOARD[k]; int o = owner[k]; if (o < 0 || mort[k]) return 0;
        if (b.t == 'p') { int h = houses[k]; return h > 0 ? b.rent[h] : (ownsAll(o, b.g) ? b.rent[0] * 2 : b.rent[0]); }
        if (b.t == 't') { int c = 0; for (int q : STATIONS) if (owner[q] == o) c++; static const int R[4] = { 25, 50, 100, 200 }; return R[std::max(0, c - 1)] * mult; }
        if (b.t == 'u') { int c = 0; for (int q : UTILS) if (owner[q] == o) c++; return (c >= 2 ? 10 : 4) * dice; }
        return 0; }
    std::vector<int> others(int p) const { std::vector<int> v; for (int i = 0; i < n; i++) if (i != p && alive[i]) v.push_back(i); return v; }

    void start() override {
        n = (int)pl.size(); int mo = opts.get("money", 1); int st = mo == 0 ? 1000 : mo == 2 ? 2000 : 1500; int lm = opts.get("rounds", 2); limit = lm == 0 ? 0 : lm == 1 ? 20 : 40;
        money.assign(n, st); pos.assign(n, 0); jail.assign(n, 0); jt.assign(n, 0); gooj.assign(n, 0); alive.assign(n, true); owner.assign(N, -1); houses.assign(N, 0); mort.assign(N, 0);
        for (int k = 0; k < N; k++) { char t = BOARD[k].t; if (t == 'p' || t == 't' || t == 'u') OWN.push_back(k); if (t == 't') STATIONS.push_back(k); if (t == 'u') UTILS.push_back(k); }
        for (int i = 0; i < N_S; i++) deckS.push_back(i); for (int i = 0; i < N_C; i++) deckC.push_back(i);
        for (int i = N_S - 1; i > 0; i--) std::swap(deckS[i], deckS[rndInt(i + 1)]); for (int i = N_C - 1; i > 0; i--) std::swap(deckC[i], deckC[rndInt(i + 1)]);
        say("¡Empieza la partida! Cada uno tiene " + cur(st) + ".");
    }
    int turnPlayer() const override { return over ? -1 : (phase == AUC ? auc.cur : turn); }
    void transfer(int p, int amt, int to, int per) { money[p] -= amt; if (to == -2) for (int q : others(p)) money[q] += per; else if (to >= 0) money[to] += amt; }
    void charge(int p, int amt, int to, int per = 0) {
        if (amt <= 0) return; if (money[p] >= amt) { transfer(p, amt, to, per); return; }
        if (money[p] + maxRaise(p) >= amt) { phase = DEBT; hasDebt = true; debt = { to, amt, per }; manage = !isAI(p); say(nm(p) + " debe " + cur(amt) + " y tiene que juntar dinero."); return; }
        bankrupt(p, to); }
    bool isAI(int p) const { return pl[p].ai; }
    void settleDebt() { if (!hasDebt || money[turn] < debt.amt) return; transfer(turn, debt.amt, debt.to, debt.per); hasDebt = false; manage = false; phase = MOVE; say(nm(turn) + " pagó " + cur(debt.amt) + "."); if (cont > 0) { int s = cont; cont = 0; moveBy(turn, s); } fin(); }
    void salary(int p) { money[p] += SAL; }
    void moveBy(int p, int s) { int np = pos[p] + s; if (np >= N) { salary(p); say(nm(p) + " pasó por la Salida y cobró " + cur(SAL) + "."); } pos[p] = ((np % N) + N) % N; land(p, 1); }
    void moveTo(int p, int to) { if (to <= pos[p]) { salary(p); say(nm(p) + " cobró " + cur(SAL) + " de la Salida."); } pos[p] = to; land(p, 1); }
    void goJail(int p) { pos[p] = JAIL; jail[p] = 1; jt[p] = 0; extra = false; dbl = 0; say(nm(p) + " va a la cárcel."); app->audio.play("lose"); }
    void startAuction(int k) {
        phase = AUC; pend = k; auc = Auc(); auc.cur = turn; auc.pass.assign(n, 0); say("Subasta de " + std::string(BOARD[k].n) + "."); aucNext(); }
    void land(int p, int mult) {
        int k = pos[p]; const Sq& b = BOARD[k];
        if (ownable(k)) { int o = owner[k];
            if (o < 0) { if (money[p] >= b.price) { phase = BUY; pend = k; return; } startAuction(k); }
            else if (o != p) { int r = rentOf(k, d1 + d2, mult); if (r > 0) { say(nm(p) + " paga " + cur(r) + " de alquiler a " + nm(o) + " (" + b.n + ")."); app->audio.play("place"); charge(p, r, o); } else say(std::string(b.n) + " está hipotecada: no se cobra alquiler."); } }
        else if (b.t == 'x') { say(nm(p) + " paga " + cur(b.amt) + " de " + b.n + "."); charge(p, b.amt, -1); }
        else if (b.t == 'J') goJail(p);
        else if (b.t == 'c') drawCard(p, b.d); }
    void drawCard(int p, char d) {
        auto& dk = d == 'S' ? deckS : deckC; int id = dk.front(); dk.erase(dk.begin()); const Cd& c = d == 'S' ? CARD_S[id] : CARD_C[id]; cardTxt = std::string(d == 'S' ? "🍀 " : "👥 ") + c.x;
        if (c.t == 5) gooj[p]++; else dk.push_back(id); say(nm(p) + ": " + c.x); app->audio.play("click");
        switch (c.t) {
            case 0: moveTo(p, c.to); break;
            case 1: { int t = -1; for (int q : STATIONS) if (q > pos[p]) { t = q; break; } if (t < 0) t = STATIONS[0]; if (t <= pos[p]) salary(p); pos[p] = t; land(p, 2); break; }
            case 2: money[p] += c.a; break;
            case 3: charge(p, c.a, -1); break;
            case 4: goJail(p); break;
            case 6: pos[p] = (pos[p] + N - 3) % N; land(p, 1); break;
            case 7: { int t = 0; for (int k : OWN) if (owner[k] == p && BOARD[k].t == 'p') t += houses[k] == 5 ? c.H : houses[k] * c.h; if (t > 0) charge(p, t, -1); break; }
            case 8: { auto o = others(p); if (!o.empty()) charge(p, c.a * (int)o.size(), -2, c.a); break; }
            case 9: for (int q : others(p)) { int x = std::min(c.a, money[q]); money[q] -= x; money[p] += x; } break; } }
    void fin() { if (phase == MOVE) after(); }
    void after() { pend = -1; phase = (alive[turn] && jail[turn] == 0 && extra) ? ROLL : END; aiT = 0; }
    void doRoll(int p) {
        d1 = 1 + rndInt(6); d2 = 1 + rndInt(6); cardTxt.clear(); bool db = d1 == d2; int s = d1 + d2; phase = MOVE; extra = false; say(nm(p) + " sacó " + std::to_string(d1) + " y " + std::to_string(d2) + ".");
        if (jail[p] > 0) {
            if (db) { jail[p] = 0; jt[p] = 0; dbl = 0; say("¡Doble! " + nm(p) + " sale de la cárcel."); moveBy(p, s); }
            else { jt[p]++; if (jt[p] >= 3) { jail[p] = 0; jt[p] = 0; say(nm(p) + " paga la fianza de " + cur(FINE) + " y sale."); cont = s; charge(p, FINE, -1); if (phase == MOVE) { cont = 0; moveBy(p, s); } } } }
        else if (db) { dbl++; if (dbl >= 3) { say("Tres dobles seguidos: ¡a la cárcel!"); goJail(p); } else { extra = true; moveBy(p, s); } }
        else { dbl = 0; moveBy(p, s); }
        if (phase == MOVE) fin(); chk(); }
    void startRoll() { if (phase != ROLL || over || rolling) return; rolling = true; rollT = 0.7f; app->audio.play("dice"); }
    void buy(int p) { int k = pend; if (phase != BUY || k < 0 || owner[k] >= 0 || money[p] < BOARD[k].price) return; money[p] -= BOARD[k].price; owner[k] = p; say(nm(p) + " compró " + BOARD[k].n + " por " + cur(BOARD[k].price) + "."); app->audio.play("win"); phase = MOVE; fin(); }
    void decline(int p) { if (phase != BUY) return; startAuction(pend); }
    void endTurn(int p) { if (phase != END || p != turn) return; manage = false; advance(); }
    void payFine(int p) { if (phase != ROLL || jail[p] == 0 || money[p] < FINE) return; money[p] -= FINE; jail[p] = 0; jt[p] = 0; say(nm(p) + " paga " + cur(FINE) + " y sale de la cárcel."); }
    void useCard(int p) { if (phase != ROLL || jail[p] == 0 || gooj[p] <= 0) return; gooj[p]--; jail[p] = 0; jt[p] = 0; say(nm(p) + " usa la tarjeta y sale de la cárcel."); }
    void build(int p, int k) { if (!mayManage(p) || phase == DEBT || !canBuild(p, k)) return; money[p] -= hcost(k); houses[k]++; app->audio.play("place"); say(nm(p) + " construye en " + BOARD[k].n + (houses[k] == 5 ? " (hotel)." : " (" + std::to_string(houses[k]) + ").")); }
    void sell(int p, int k) { if (!mayManage(p) || !canSell(p, k)) return; money[p] += hcost(k) / 2; houses[k]--; say(nm(p) + " vende una casa de " + BOARD[k].n + "."); afterRaise(); }
    void mortgage(int p, int k) { if (!mayManage(p) || !canMortgage(p, k)) return; mort[k] = 1; money[p] += BOARD[k].price / 2; say(nm(p) + " hipoteca " + BOARD[k].n + "."); afterRaise(); }
    void unmortgage(int p, int k) { if (!mayManage(p) || phase == DEBT || !canUnmort(p, k)) return; money[p] -= unmortCost(k); mort[k] = 0; say(nm(p) + " levanta la hipoteca de " + BOARD[k].n + "."); }
    void afterRaise() { if (phase == DEBT) settleDebt(); chk(); }
    void declareBankrupt(int p) { if (phase != DEBT || p != turn) return; int to = debt.to; hasDebt = false; cont = 0; manage = false; bankrupt(p, to); }
    // subasta
    void aucNext() {
        for (int guard = 0; guard < 100; guard++) {
            std::vector<int> rem; for (int i = 0; i < n; i++) if (alive[i] && !auc.pass[i]) rem.push_back(i);
            if (rem.empty() || (auc.lead >= 0 && rem.size() == 1 && rem[0] == auc.lead)) { endAuction(); return; }
            int cand = -1; for (int s = 1; s <= n; s++) { int i = (auc.cur + s) % n; if (alive[i] && !auc.pass[i] && i != auc.lead) { cand = i; break; } } if (cand < 0) { endAuction(); return; }
            if (money[cand] <= auc.bid) { auc.pass[cand] = 1; auc.cur = cand; continue; } auc.cur = cand; aiT = 0; return; }
        endAuction(); }
    void endAuction() {
        int k = pend; if (auc.lead >= 0 && alive[auc.lead]) { money[auc.lead] -= auc.bid; owner[k] = auc.lead; say(nm(auc.lead) + " gana la subasta de " + BOARD[k].n + " por " + cur(auc.bid) + "."); app->audio.play("win"); } else say("Nadie ofertó: " + std::string(BOARD[k].n) + " queda sin dueño.");
        phase = MOVE; pend = -1; fin(); }
    void bid(int p, int amt) { if (phase != AUC || p != auc.cur || amt <= auc.bid || amt > money[p]) return; auc.bid = amt; auc.lead = p; say(nm(p) + " ofrece " + cur(amt) + "."); app->audio.play("click"); aucNext(); }
    void passAuc(int p) { if (phase != AUC || p != auc.cur) return; auc.pass[p] = 1; aucNext(); }
    void advance() {
        if (chk()) return; int i = turn; do { i = (i + 1) % n; } while (!alive[i]); if (i <= turn) round++; turn = i; dbl = 0; extra = false; phase = ROLL; pend = -1; hasDebt = false; cont = 0; manage = false; aiT = 0; mi = 0;
        if (limit && round >= limit) finishG("Se cumplieron las " + std::to_string(limit) + " rondas."); }
    void bankrupt(int p, int to) {
        if (!alive[p]) return; for (int k : OWN) if (BOARD[k].t == 'p' && owner[k] == p && houses[k] > 0) { money[p] += houses[k] * hcost(k) / 2; houses[k] = 0; }
        int cash = std::max(0, money[p]); money[p] = 0; say("💥 " + nm(p) + " quebró.");
        if (to == -2) { auto o = others(p); for (int q : o) money[q] += cash / (int)o.size(); if (!o.empty()) money[o[0]] += cash % (int)o.size(); } else if (to >= 0 && alive[to]) money[to] += cash;
        for (int k : OWN) if (owner[k] == p) { if (to >= 0 && alive[to]) owner[k] = to; else { owner[k] = -1; mort[k] = 0; } }
        alive[p] = false; jail[p] = 0; gooj[p] = 0; if (chk()) return; if (p == turn) { hasDebt = false; cont = 0; phase = MOVE; advance(); } else if (phase == AUC && auc.cur == p) { auc.pass[p] = 1; aucNext(); } }
    bool chk() { if (over) return true; if (aliveCount() <= 1) { finishG(""); return true; } return false; }
    std::vector<bool> win; std::string why;
    void finishG(const std::string& w) { over = true; phase = OVER; why = w; int best = -1; for (int i = 0; i < n; i++) if (alive[i]) best = std::max(best, wealth(i)); win.assign(n, false); for (int i = 0; i < n; i++) win[i] = alive[i] && wealth(i) == best; }
    void resign(int p) override { if (p < 0 || p >= n || !alive[p]) return; bool wasT = p == turn && !over; if (wasT) { hasDebt = false; cont = 0; } bankrupt(p, -1); if (wasT && !over) {} }

    // ---------- IA ----------
    int setCount(int p, int g) const { int c = 0; for (int k : groupSq(g)) if (owner[k] == p) c++; return c; }
    bool aiBuy(int p, int k) const { int pr = BOARD[k].price; int after = money[p] - pr; if (BOARD[k].t == 'p') { int g = BOARD[k].g, tot = (int)groupSq(g).size(), mine = setCount(p, g); if (mine + 1 == tot) return after >= 0; bool rival = false; for (int q : groupSq(g)) if (owner[q] >= 0 && owner[q] != p) rival = true; if (rival && mine > 0) return after >= 0; } return after >= (round < 6 ? 120 : 250); }
    int aiValue(int p, int k) const { int pr = BOARD[k].price; double f = 0.85; if (BOARD[k].t == 'p') { int g = BOARD[k].g; if (setCount(p, g) + 1 == (int)groupSq(g).size()) f = 1.6; else if (setCount(p, g) > 0) f = 1.15; } return (int)std::min<double>(money[p] - 60, pr * f); }
    void aiLiquidate(int p) {
        for (int g = 0; g < 200 && phase == DEBT; g++) { int best = -1; for (int k : OWN) if (canSell(p, k) && (best < 0 || houses[k] > houses[best])) best = k; if (best >= 0) { sell(p, best); continue; }
            best = -1; for (int k : OWN) if (canMortgage(p, k) && (best < 0 || BOARD[k].price < BOARD[best].price)) best = k; if (best >= 0) { mortgage(p, best); continue; } break; }
        if (phase == DEBT) { declareBankrupt(p); } }
    void aiManage(int p) {
        for (int g = 0; g < 40; g++) { int best = -1; for (int k : OWN) if (canBuild(p, k) && money[p] - hcost(k) >= 200 && (best < 0 || houses[k] < houses[best] || (houses[k] == houses[best] && BOARD[k].price > BOARD[best].price))) best = k; if (best < 0) break; build(p, best); }
        for (int k : OWN) if (owner[k] == p && mort[k] && money[p] - unmortCost(k) >= 400) unmortgage(p, k); }
    void aiAct(int p) {
        switch (phase) {
            case ROLL: if (jail[p] > 0) { if (gooj[p] > 0) { useCard(p); return; } if (money[p] > 350 && round < 15) { payFine(p); return; } } aiManage(p); startRoll(); break;
            case BUY: if (aiBuy(p, pend)) buy(p); else decline(p); break;
            case AUC: { int k = pend, v = aiValue(p, k); int nb = auc.bid + 10 + (rndInt(3)) * 10; if (nb <= v && nb <= money[p]) bid(p, nb); else passAuc(p); break; }
            case END: aiManage(p); endTurn(p); break;
            case DEBT: aiLiquidate(p); break;
            default: break; } }

    // ---------- control ----------
    int mine(int p) const { return p; }
    std::vector<int> ownedList(int p) const { std::vector<int> v; for (int k : OWN) if (owner[k] == p) v.push_back(k); return v; }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r; std::string s = why.empty() ? "" : why + " "; for (int i = 0; i < n; i++) { r.push_back({ wealth(i), win[i] ? 'w' : 'l' }); if (win[i]) s += "Ganó " + nm(i) + " con " + cur(wealth(i)) + "."; } finish(r, s); } return; }
        if (rolling) { rollT -= dt; shake += dt; if (rollT <= 0) { rolling = false; doRoll(turn); aiT = 0; } return; }
        int p = turnPlayer(); if (p < 0) return; aiT += dt;
        if (isAI(p)) { if (aiT > (phase == AUC ? 0.5f : 0.75f)) { aiT = 0; aiAct(p); } return; }
        Pad* pd = padOf(in, p); auto touch = [&](Rect r) { return in.touchDown && r.in(in.tx, in.ty); };
        bool A = pd && pd->down[B_A], B = pd && pd->down[B_B], X = pd && pd->down[B_X], Y = pd && pd->down[B_Y], R = pd && pd->down[B_R], Z = pd && pd->rollDown();
        if (tradeAsk) { Pad* pq = padOf(in, tp); auto ml = tradeList(turn), tl = tradeList(tp); int gk = gi < 0 || gi >= (int)ml.size() ? -1 : ml[gi], tk = ti < 0 || ti >= (int)tl.size() ? -1 : tl[ti];
            if ((pq && pq->down[B_A]) || touch({ 520, 440, 150, 44 })) { if (tradeValid(gk, tk)) doTrade(gk, tk); else tradeAsk = false; } else if ((pq && pq->down[B_B]) || touch({ 690, 440, 150, 44 })) { say(nm(tp) + " rechazó el trato."); tradeOn = false; tradeAsk = false; } return; }
        if (tradeOn && mayManage(p)) { auto ml = tradeList(p), tl = tradeList(tp); int up = pd && pd->down[B_UP] ? -1 : pd && pd->down[B_DOWN] ? 1 : 0, lr = pd && pd->down[B_LEFT] ? -1 : pd && pd->down[B_RIGHT] ? 1 : 0;
            for (int f = 0; f < 4; f++) if (touch({ 330, 300.f + f * 40, 430, 36 })) tf = f;
            if (up) tf = (tf + up + 4) % 4;
            if (lr || touch({ 790, 300.f + tf * 40, 60, 36 })) { if (!lr) lr = 1; if (tf == 0) { tp = partnerAfter(tp, lr); gi = -1; ti = -1; } else if (tf == 1) { gi += lr; if (gi < -1) gi = (int)ml.size() - 1; if (gi >= (int)ml.size()) gi = -1; } else if (tf == 2) { ti += lr; if (ti < -1) ti = (int)tl.size() - 1; if (ti >= (int)tl.size()) ti = -1; } else tcash = std::max(-money[tp], std::min(money[p], tcash + 25 * lr)); }
            if (A || touch(mBtn(0))) propose(); if (B || touch(mBtn(4))) tradeOn = false; return; }
        if (manage && mayManage(p)) {
            auto lst = ownedList(p); if (lst.empty()) { manage = false; return; } mi = std::max(0, std::min(mi, (int)lst.size() - 1));
            if (pd) { if (pd->down[B_LEFT]) mi = std::max(0, mi - 1); if (pd->down[B_RIGHT]) mi = std::min((int)lst.size() - 1, mi + 1); if (pd->down[B_UP]) mi = std::max(0, mi - 7); if (pd->down[B_DOWN]) mi = std::min((int)lst.size() - 1, mi + 7); }
            for (size_t i = 0; i < lst.size(); i++) if (touch(mcell((int)i))) mi = (int)i;
            int k = lst[mi]; bool bB = A || touch(mBtn(0)), sS = Y || touch(mBtn(1)), mM = X || touch(mBtn(2)), q = R || touch(mBtn(3));
            if (bB) build(p, k); if (sS) sell(p, k); if (mM) { if (mort[k]) unmortgage(p, k); else mortgage(p, k); } if (q && phase == DEBT) declareBankrupt(p); else if (q && n > 1 && aliveCount() > 1) { tradeOn = true; tf = 0; tp = partnerAfter(p, 1); gi = -1; ti = -1; tcash = 0; }
            if ((B || touch(mBtn(4))) && phase != DEBT) manage = false; return; }
        switch (phase) {
            case ROLL: if (A || Z || touch(btn(0))) startRoll(); else if (jail[p] > 0 && (Y || touch(btn(1)))) payFine(p); else if (jail[p] > 0 && gooj[p] > 0 && (B || touch(btn(2)))) useCard(p); else if (X || touch(btn(3))) { if (!ownedList(p).empty()) manage = true; } break;
            case BUY: if (A || touch(btn(0))) buy(p); else if (B || touch(btn(1))) decline(p); break;
            case AUC: if (A || touch(btn(0))) bid(p, auc.bid + 10); else if (X || touch(btn(1))) bid(p, auc.bid + 50); else if (Y || touch(btn(2))) bid(p, auc.bid + 100); else if (B || touch(btn(3))) passAuc(p); break;
            case END: if (A || touch(btn(0))) endTurn(p); else if (X || touch(btn(3))) { if (!ownedList(p).empty()) manage = true; } break;
            case DEBT: if (X || touch(btn(3))) manage = true; if (R || touch(btn(1))) declareBankrupt(p); break;
            default: break; }
    }

    // ---------- dibujo ----------
    const float C = 54, BX = 300, BY = 62;
    Rect cell(int k) const { int c, r; if (k <= 10) { c = 10 - k; r = 10; } else if (k <= 20) { c = 0; r = 10 - (k - 10); } else if (k <= 30) { c = k - 20; r = 0; } else { c = 10; r = k - 30; } return { BX + c * C, BY + r * C, C, C }; }
    Rect btn(int i) const { return { 410.f + (i % 2) * 190, 470.f + (i / 2) * 50, 180, 44 }; }
    Rect mcell(int i) const { return { 342.f + (i % 7) * 66, 290.f + (i / 7) * 36, 62, 32 }; }
    Rect mBtn(int i) const { return { 330.f + i * 98, 480, 92, 40 }; }
    void tbtn(Gfx& g, Rect r, const std::string& lab, int st, bool on = true) { Col c0 = st == 1 ? rgb(0xc0392b) : st == 3 ? rgb(0xe0b84c) : rgb(0xf0dca8), c1 = st == 1 ? rgb(0x7a1c12) : st == 3 ? rgb(0x9a7220) : rgb(0xb8975a); if (!on) { c0 = rgb(0x6a5a4a); c1 = rgb(0x3a3028); } g.rrect(r, 12, rgb(0x2a1a0c)); g.rrectGrad(r.inset(2), 10, c0, c1); g.text(lab, r.cx(), r.cy(), 16, st == 1 ? pal::white : rgb(0x2a1a0c), F_BOLD, AL_C, r.w - 8); }
    void drawCell(Gfx& g, int k) {
        Rect r = cell(k); const Sq& b = BOARD[k]; g.rect(r, rgb(0xe8dcb8)); g.rect({ r.x, r.y, r.w, 1 }, rgb(0x6a5a3a)); g.rect({ r.x, r.y, 1, r.h }, rgb(0x6a5a3a));
        if (b.t == 'p') { g.rect({ r.x + 1, r.y + 1, r.w - 2, 14 }, rgb(GCOL[b.g])); g.text(b.s, r.cx(), r.y + 28, 12, rgb(0x2a1a0c), F_BOLD, AL_C, 50); g.text(std::to_string(b.price), r.cx(), r.y + 43, 11, rgb(0x5a4a3a), F_BODY, AL_C);
            if (houses[k] >= 5) g.rect({ r.x + r.w / 2 - 5, r.y + 3, 10, 10 }, rgb(0xd32f2f)); else for (int h = 0; h < houses[k]; h++) g.rect({ r.x + 4 + h * 12.f, r.y + 3, 9, 9 }, rgb(0x2e7d32)); }
        else { g.emoji(b.t == 't' || b.t == 'u' || b.t == 'x' || b.t == 'j' || b.t == 'f' || b.t == 'J' || b.t == 'g' || b.t == 'c' ? b.e : "", r.cx(), r.y + 19, 24); if (b.t == 'g') g.text("SALIDA", r.cx(), r.y + 43, 11, rgb(0x2a1a0c), F_BOLD, AL_C); else if (b.t == 't' || b.t == 'u') g.text(std::to_string(b.price), r.cx(), r.y + 43, 11, rgb(0x5a4a3a), F_BODY, AL_C); else if (b.t == 'x') g.text(std::to_string(b.amt), r.cx(), r.y + 43, 11, rgb(0x5a4a3a), F_BODY, AL_C); else if (b.t == 'c') g.text(b.d == 'S' ? "Suerte" : "Comun.", r.cx(), r.y + 43, 10, rgb(0x5a4a3a), F_BODY, AL_C, 50); else if (b.t == 'j') g.text("Cárcel", r.cx(), r.y + 43, 10, rgb(0x5a4a3a), F_BODY, AL_C); else if (b.t == 'f') g.text("Libre", r.cx(), r.y + 43, 10, rgb(0x5a4a3a), F_BODY, AL_C); else if (b.t == 'J') g.text("A la cárcel", r.cx(), r.y + 43, 10, rgb(0x5a4a3a), F_BODY, AL_C, 52); }
        if (mort[k]) { g.rect(r, rgb(0x000000, 120)); }
        if (owner[k] >= 0) g.circle(r.x + r.w - 8, r.y + r.h - 8, 5, rgb(PC[owner[k] % 8]));
        if (k == pos[turn] && !over) g.rrect({ r.x, r.y, r.w, 3 }, 0, pal::gold);
    }
    void die(Gfx& g, float cx, float cy, int v) { g.rrect({ cx - 24, cy - 24, 48, 48 }, 9, rgb(0x888888)); g.rrect({ cx - 22, cy - 22, 44, 44 }, 8, pal::white); static const int P[6][9] = { { 0,0,0,0,1,0,0,0,0 }, { 1,0,0,0,0,0,0,0,1 }, { 1,0,0,0,1,0,0,0,1 }, { 1,0,1,0,0,0,1,0,1 }, { 1,0,1,0,1,0,1,0,1 }, { 1,0,1,1,0,1,1,0,1 } }; for (int i = 0; i < 9; i++) if (P[v - 1][i]) g.circle(cx - 12 + (i % 3) * 12, cy - 12 + (i / 3) * 12, 4, rgb(0x222222)); }
    void draw(Gfx& g, UI&) override {
        g.rrect({ BX - 4, BY - 4, C * 11 + 8, C * 11 + 8 }, 6, pal::panelEdge); for (int k = 0; k < N; k++) drawCell(g, k);
        g.rect({ BX + C, BY + C, C * 9, C * 9 }, rgb(0x2e6b4a)); g.gradRect({ BX + C, BY + C, C * 9, C * 9 }, rgb(0x2e7d52), rgb(0x1b4a31));
        // fichas
        for (int p = 0; p < n; p++) if (alive[p]) { Rect r = cell(pos[p]); float ox = (p % 4) * 12 + 5, oy = 16 + (p / 4) * 15 + (jail[p] > 0 ? 6 : 0); g.circle(r.x + ox + 4, r.y + oy + 8, 9, rgb(PC[p % 8])); g.emoji(pl[p].emoji, r.x + ox + 4, r.y + oy + 8, 14); }
        // jugadores
        for (int i = 0; i < n; i++) { Rect r{ 905, 62.f + i * 36, 335, 33 }; bool on = i == turnPlayer() && !over; g.rrect(r, 9, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 7, rgb(0x2a1e18), rgb(0x120c08)); g.circle(r.x + 14, r.cy(), 7, rgb(PC[i % 8]));
            g.text(pl[i].emoji + " " + pl[i].name + (alive[i] ? "" : " (quebró)") + (jail[i] ? " ⛓" : ""), r.x + 28, r.cy(), 14, alive[i] ? pal::white : rgb(0x888888), F_BOLD, AL_L, 190); g.text(cur(money[i]), r.x + r.w - 8, r.cy(), 16, pal::gold, F_BOLD, AL_R); }
        float ly = 62 + n * 36 + 8; g.rrect({ 905, ly, 335, 64.f + 6 * 22 - 20 }, 12, rgb(0x120c08, 215)); for (size_t i = 0; i < logv.size(); i++) g.text(logv[i], 915, ly + 14 + i * 22.f, 12, i + 1 == logv.size() ? pal::white : rgb(0xc8b88a), F_BODY, AL_L, 320);
        banner(g, over ? "Fin de la partida" : "Ronda " + std::to_string(round + 1) + (limit ? "/" + std::to_string(limit) : "") + " · " + pl[turnPlayer() < 0 ? 0 : turnPlayer()].name);
        float cx = BX + C * 5.5f; Rect mid{ BX + C + 8, BY + C + 8, C * 9 - 16, C * 9 - 16 };
        if ((manage || tradeOn || tradeAsk) && mayManage(turn)) { drawManage(g); return; }
        int p = turnPlayer(); if (p < 0) { g.text("¡Fin!", cx, 300, 40, pal::gold, F_TITLE, AL_C); return; } bool human = !isAI(p); Pad* none = nullptr; (void)none;
        die(g, cx - 34, 150 + (rolling ? (int)(std::sin(shake * 40) * 4) : 0), rolling ? 1 + (int)(shake * 25) % 6 : d1); die(g, cx + 34, 150 + (rolling ? (int)(std::cos(shake * 40) * 4) : 0), rolling ? 1 + (int)(shake * 31) % 6 : d2);
        const Sq& sq = BOARD[pos[turn]]; g.text(std::string(sq.t == 'p' || sq.t == 't' || sq.t == 'u' ? "" : "") + sq.n, cx, 208, 16, pal::cream, F_BOLD, AL_C, 440);
        if (!cardTxt.empty()) { g.rrect({ BX + 40, 226, C * 9 - 80, 52 }, 12, rgb(0xfff3c9)); g.textBox(cardTxt, { BX + 50, 232, C * 9 - 100, 44 }, 15, rgb(0x2a1a0c), F_BOLD, AL_C); }
        if (phase == BUY || phase == AUC) { int k = pend; Rect r{ cx - 90, 292, 180, 150 }; g.rrect(r, 10, rgb(0xf6ecd0)); if (BOARD[k].t == 'p') g.rect({ r.x + 4, r.y + 4, r.w - 8, 26 }, rgb(GCOL[BOARD[k].g])); g.text(BOARD[k].n, r.cx(), r.y + 17, 14, BOARD[k].t == 'p' ? rgb(0x111111) : rgb(0x2a1a0c), F_BOLD, AL_C, 170);
            g.text("Precio " + cur(BOARD[k].price), r.cx(), r.y + 46, 15, rgb(0x2a1a0c), F_BOLD, AL_C); if (BOARD[k].t == 'p') for (int h = 0; h < 6; h++) g.text((h == 0 ? "Alquiler " : h == 5 ? "Hotel " : std::to_string(h) + " casa" + (h > 1 ? "s " : " ")) + cur(BOARD[k].rent[h]), r.cx(), r.y + 66 + h * 13.f, 11, rgb(0x4a3a2a), F_BODY, AL_C);
            else g.text(BOARD[k].t == 't' ? "Alquiler 25 · 50 · 100 · 200" : "4x o 10x los dados", r.cx(), r.y + 76, 12, rgb(0x4a3a2a), F_BODY, AL_C, 170); }
        if (phase == AUC) g.text("Oferta: " + cur(auc.bid) + (auc.lead >= 0 ? " (" + nm(auc.lead) + ")" : ""), cx, 452, 17, pal::gold, F_BOLD, AL_C);
        if (phase == DEBT) g.text("Debés " + cur(debt.amt) + ": hipotecá o vendé casas", cx, 452, 17, rgb(0xff9a9a), F_BOLD, AL_C, 440);
        if (!human) { g.text(pl[p].name + " está jugando…", cx, 520, 18, pal::cream, F_BOLD, AL_C); return; }
        if (phase == ROLL) { tbtn(g, btn(0), "🎲 Tirar (A)", 3); if (jail[p] > 0) { tbtn(g, btn(1), "Pagar ₡50 (Y)", 0, money[p] >= FINE); if (gooj[p] > 0) tbtn(g, btn(2), "Tarjeta (B)", 0); } if (!ownedList(p).empty()) tbtn(g, btn(3), "Administrar (X)", 0); }
        else if (phase == BUY) { tbtn(g, btn(0), "Comprar (A)", 3); tbtn(g, btn(1), "Subasta (B)", 1); }
        else if (phase == AUC) { if (auc.cur == p) { tbtn(g, btn(0), "+10 (A)", 3); tbtn(g, btn(1), "+50 (X)", 0); tbtn(g, btn(2), "+100 (Y)", 0); tbtn(g, btn(3), "Pasar (B)", 1); } else g.text("Esperando a " + nm(auc.cur), cx, 520, 18, pal::cream, F_BOLD, AL_C); }
        else if (phase == END) { tbtn(g, btn(0), "Terminar turno (A)", 3); if (!ownedList(p).empty()) tbtn(g, btn(3), "Administrar (X)", 0); }
        else if (phase == DEBT) { tbtn(g, btn(3), "Administrar (X)", 3); tbtn(g, btn(1), "Quebrar (R)", 1); }
    }
    void drawTrade(Gfx& g) {
        Rect pn{ BX + C + 8, BY + C + 8, C * 9 - 16, C * 9 - 16 }; g.rrect(pn, 14, rgb(0x120c08, 240)); auto ml = tradeList(turn), tl = tradeList(tp); int gk = gi < 0 || gi >= (int)ml.size() ? -1 : ml[gi], tk = ti < 0 || ti >= (int)tl.size() ? -1 : tl[ti];
        auto nmOf = [&](int k) { return k < 0 ? std::string("Nada") : std::string(BOARD[k].n) + (mort[k] ? " (hipotecada)" : ""); };
        g.text(nm(turn) + ": ofrecer un trato", pn.cx(), 270, 18, pal::gold, F_BOLD, AL_C, 460);
        std::string lab[4] = { "Con: " + nm(tp), "Doy: " + nmOf(gk), "Recibo: " + nmOf(tk), std::string(tcash >= 0 ? "Pago " : "Me pagan ") + cur(std::abs(tcash)) };
        for (int f = 0; f < 4; f++) { Rect r{ 330, 300.f + f * 40, 430, 36 }; g.rrect(r, 8, f == tf && !tradeAsk ? pal::gold : rgb(0x5a3a1c)); g.rrect(r.inset(2), 6, rgb(0x2a1e18)); g.text(lab[f], r.x + 12, r.cy(), 15, pal::white, F_BOLD, AL_L, 410); g.text("◀ ▶", r.x + r.w + 30, r.cy(), 14, pal::cream, F_BOLD, AL_C); }
        if (!tradeAsk) { g.text("Arriba/abajo: campo · izquierda/derecha: cambiar · A: proponer · B: cancelar", pn.cx(), 470, 12, rgb(0xd8b46a), F_BODY, AL_C, 460); tbtn(g, mBtn(0), "Proponer (A)", 3); tbtn(g, mBtn(4), "Cancelar (B)", 1); }
        else { g.text(nm(tp) + ": ¿aceptas el trato?", pn.cx(), 415, 18, pal::cream, F_BOLD, AL_C); tbtn(g, { 520, 440, 150, 44 }, "Aceptar (A)", 3); tbtn(g, { 690, 440, 150, 44 }, "Rechazar (B)", 1); }
    }
    void drawManage(Gfx& g) {
        if (tradeOn || tradeAsk) { drawTrade(g); return; }
        int p = turn; auto lst = ownedList(p); Rect pn{ BX + C + 8, BY + C + 8, C * 9 - 16, C * 9 - 16 }; g.rrect(pn, 14, rgb(0x120c08, 235));
        g.text(nm(p) + ": administrar propiedades  (" + cur(money[p]) + ")", pn.cx(), 262, 18, pal::gold, F_BOLD, AL_C, 460); if (phase == DEBT) g.text("Debés " + cur(debt.amt) + ": junta dinero o quebrás", pn.cx(), 284 - 4 + 0, 12, rgb(0xff9a9a), F_BOLD, AL_C);
        for (size_t i = 0; i < lst.size(); i++) { int k = lst[i]; Rect r = mcell((int)i); const Sq& b = BOARD[k]; g.rrect(r, 6, (int)i == mi ? pal::gold : rgb(0x6a5a3a)); g.rrect(r.inset(2), 5, rgb(0xf0e4c0)); g.rect({ r.x + 2, r.y + 2, r.w - 4, 8 }, b.t == 'p' ? rgb(GCOL[b.g]) : rgb(0x777777)); g.text(b.t == 'p' ? b.s : b.t == 't' ? "Term." : "Serv.", r.cx(), r.y + 19, 12, rgb(0x2a1a0c), F_BOLD, AL_C, 58);
            g.text(mort[k] ? "hipot." : b.t == 'p' ? (houses[k] >= 5 ? "hotel" : std::to_string(houses[k]) + " casas") : "", r.cx(), r.y + 28, 9, mort[k] ? rgb(0xb71c1c) : rgb(0x4a3a2a), F_BODY, AL_C); }
        if (!lst.empty()) { int k = lst[std::min(mi, (int)lst.size() - 1)]; const Sq& b = BOARD[k]; g.text(std::string(b.n) + (b.t == 'p' && houses[k] ? "  (" + std::to_string(houses[k]) + " casas)" : "") + (mort[k] ? "  · HIPOTECADA" : ""), pn.cx(), 430, 16, pal::white, F_BOLD, AL_C, 460);
            g.text(std::string("Construir ") + (b.t == 'p' ? cur(hcost(k)) : "—") + " · Vender casa +" + cur(b.t == 'p' ? hcost(k) / 2 : 0) + " · " + (mort[k] ? "Levantar hipoteca " + cur(unmortCost(k)) : "Hipotecar +" + cur(b.price / 2)), pn.cx(), 454, 12, rgb(0xd8b46a), F_BODY, AL_C, 460);
            tbtn(g, mBtn(0), "Construir (A)", 3, canBuild(p, k)); tbtn(g, mBtn(1), "Vender (Y)", 0, canSell(p, k)); tbtn(g, mBtn(2), mort[k] ? "Deship. (X)" : "Hipotecar (X)", 0, mort[k] ? canUnmort(p, k) : canMortgage(p, k)); if (phase == DEBT) tbtn(g, mBtn(3), "Quebrar (R)", 1); else tbtn(g, mBtn(3), "Trato (R)", 0); tbtn(g, mBtn(4), "Cerrar (B)", 1, phase != DEBT); }
    }
    std::vector<Hint> hints() const override { return { { "A", "Aceptar / tirar" }, { "B", "No / pasar" }, { "X", "Administrar" }, { "Y", "Pagar / vender" }, { "ROLL", "Tirar dados" }, { "TOUCH", "Tocar" } }; }
    std::string help() const override { return "Compra calles, estaciones y servicios al caer en ellas (si no las comprás, se subastan). Los demás te pagan alquiler; con todas las calles de un color podés construir casas y hoteles. Cobrás 200 al pasar por la Salida. Dobles repiten turno y tres seguidos van a la cárcel. Si no podés pagar, vendé casas o hipotecá; si no alcanza, quebrás. Gana el último en pie o, al llegar al límite de rondas, el más rico."; }
};
}
Game* makeNegocios() { return new Negocios(); }
