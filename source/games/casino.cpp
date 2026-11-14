// Juegos de casino de un jugador (fichas simbólicas, sin dinero real): Tragamonedas, Video póker, Baccarat, Pai Gow Poker, Blackjack Casino y Rummy tico.
#include "casino.h"

namespace {
using namespace cz;
const std::string kBlank = "";

// ===================================================================== TRAGAMONEDAS
const char* STRIP[30] = { "🍒", "🍋", "🍒", "🍊", "🍋", "🍇", "🍒", "🍋", "🔔", "🍊", "🍒", "🍋", "⭐", "🍇", "🍊", "🍒", "🍋", "7️⃣", "🍊", "🔔", "🍒", "🍇", "💎", "🍋", "🍊", "🔔", "🍒", "⭐", "🍋", "🍊" };
const int SL = 30; const int LINES[5][3] = { {1,1,1}, {0,0,0}, {2,2,2}, {0,1,2}, {2,1,0} };
int payOf(const std::string& s) { if (s == "🍒") return 10; if (s == "🍋") return 15; if (s == "🍊") return 25; if (s == "🍇") return 50; if (s == "🔔") return 90; if (s == "⭐") return 180; if (s == "7️⃣") return 500; return 0; }
struct Slots : Casino {
    int nLines = 3, per = 10; bool spinning = false, showed = false; float st = 0, tick = 0; int stops[3] = { 0, 0, 0 }; std::string shown[3][3]; std::vector<int> hit; const int PER[5] = { 5, 10, 25, 50, 100 };
    void start() override { initWallet(); int o = opts.get("lines", 1); nLines = o == 0 ? 1 : o == 2 ? 5 : 3; for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) shown[k][r] = STRIP[(k * 7 + r) % SL]; say("¡Elige tu apuesta y gira!"); }
    void fin(std::string out[3][3]) { for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) out[k][r] = STRIP[(stops[k] - 1 + r + SL) % SL]; }
    void spin() {
        int total = per * nLines; if (spinning) return; if (w < total) { say("No alcanzan las fichas: baja la apuesta", -1); return; }
        spinning = true; showed = false; st = 0; addW(-total); app->save.jackpot += (int)(total * 0.04f); hit.clear(); for (int k = 0; k < 3; k++) stops[k] = rndInt(SL); say(""); app->audio.play("roll");
    }
    void done() {
        std::string f[3][3]; fin(f); int pay = 0, jp = 0; hit.clear();
        for (int i = 0; i < nLines; i++) { std::string x[3]; for (int k = 0; k < 3; k++) x[k] = f[k][LINES[i][k]];
            if (x[0] == x[1] && x[1] == x[2]) { if (x[0] == "💎") { jp++; hit.push_back(i); } else { pay += payOf(x[0]) * per; hit.push_back(i); } }
            else if (x[0] == "🍒" && x[1] == "🍒") { pay += 2 * per; hit.push_back(i); } else if (x[0] == "🍒") { pay += per / 2; hit.push_back(i); } }
        std::string t; if (jp) { int j = app->save.jackpot; pay += j; app->save.jackpot = 5000; t = "🏆 ¡¡JACKPOT!! +" + std::to_string(j) + " "; }
        for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) shown[k][r] = f[k][r]; spinning = false; if (pay > 0) addW(pay); commit();
        say(pay > 0 ? t + "¡Ganas " + std::to_string(pay) + "!" : "Sin premio. ¡Otra vez!", pay > 0 ? 1 : -1); app->audio.play(jp || pay > per * nLines ? "win" : pay > 0 ? "place" : "lose");
    }
    void update(float dt, Input& in, UI&) override {
        if (spinning) { st += dt; tick += dt; static const float stopAt[3] = { 0.65f, 1.05f, 1.45f }; if (tick > 0.07f) { tick = 0; std::string f[3][3]; fin(f); for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) shown[k][r] = st >= stopAt[k] ? f[k][r] : STRIP[rndInt(SL)]; } if (st >= 1.47f) done(); return; }
        if (pl[0].pad >= 0 && pl[0].pad < Input::MAXP && in.pads[pl[0].pad].rollDown() && w >= per * nLines) spin();
    }
    void draw(Gfx& g, UI& ui) override {
        if (topBar(g, ui)) return; if (broke(g, ui, PER[0] * nLines)) return;
        g.rrect({ 440, 66, 640, 70 }, 16, rgb(0x2a0a3c)); g.text("🏆 JACKPOT PROGRESIVO", 760, 84, 16, pal::cream, F_BOLD, AL_C); g.textShadow(std::to_string(app->save.jackpot), 760, 114, 32, pal::gold, F_TITLE, AL_C);
        g.rrectGrad({ 440, 150, 640, 300 }, 22, rgb(0x3b0a52), rgb(0x16052a)); g.rrectLine({ 440, 150, 640, 300 }, 22, 4, rgb(0xc9a24a));
        for (int k = 0; k < 3; k++) { Rect col{ 470.f + k * 200, 166, 180, 268 }; g.rrect(col, 14, rgb(0xfffaf0)); for (int r = 0; r < 3; r++) { bool mark = false; if (!spinning) for (int li : hit) if (LINES[li][k] == r) mark = true; Rect c{ col.x + 4, col.y + 6 + r * 86.f, col.w - 8, 82 }; if (mark) g.rrect(c, 10, rgb(0xffd76b, 170)); g.emoji(shown[k][r], c.cx(), c.cy(), 64); } }
        drawMsg(g, 478); g.rrect({ 340, 504, 190, 36 }, 18, rgb(0x120c08, 200)); g.text("Apuesta por línea:", 520, 520, 18, pal::cream, F_BOLD, AL_R); for (int i = 0; i < 5; i++) if (ui.button(std::to_string(PER[i]), { 540.f + i * 76, 498, 70, 44 }, PER[i] == per ? 3 : 2, !spinning)) per = PER[i];
        g.rrect({ 600, 552, 320, 28 }, 14, rgb(0x120c08, 200)); g.text("Apuesta total: " + std::to_string(per * nLines) + " (" + std::to_string(nLines) + (nLines == 1 ? " línea)" : " líneas)"), 760, 566, 16, pal::cream, F_BODY, AL_C);
        if (ui.button("GIRAR", { 600, 590, 320, 56 }, 3, !spinning)) spin(); if (!spinning) ui.setFocusIfNone(hid("GIRAR", 600 * 7 + 590));
        g.text("3 iguales: 🍒10 🍋15 🍊25 🍇50 🔔90 ⭐180 7️⃣500 · 🍒🍒 ×2 · 🍒 a la izquierda ×½ · 💎💎💎 = JACKPOT", 765, 665, 14, rgb(0xd8b46a), F_BODY, AL_C, 960);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Girar" }, { "DPAD/STICK", "Moverse" }, { "A", "Elegir" } }; }
    std::string help() const override { return "Elige cuánto apostar por línea y gira (ZL/ZR o el botón). 3 símbolos iguales en una línea pagan según el símbolo. Tres diamantes en una línea ganan el jackpot progresivo, que crece con cada apuesta. Son fichas simbólicas, sin dinero real; tu banca se guarda."; }
};

// ===================================================================== VIDEO PÓKER
struct VPay { const char* n; int m; };
const VPay VP[9] = { { "Escalera real", 250 }, { "Escalera de color", 50 }, { "Póker", 25 }, { "Full house", 9 }, { "Color", 6 }, { "Escalera", 4 }, { "Trío", 3 }, { "Doble par", 2 }, { "Jacks o mejor", 1 } };
int vpRank(const std::vector<int>& cs) { Ev e = eval5(cs); if (e.cat == 9) return 0; if (e.cat == 8) return 1; if (e.cat == 7) return 2; if (e.cat == 6) return 3; if (e.cat == 5) return 4; if (e.cat == 4) return 5; if (e.cat == 3) return 6; if (e.cat == 2) return 7; if (e.cat == 1 && e.hi >= 11) return 8; return -1; }
struct VideoPoker : Casino {
    int coin = 10, coins = 5, phase = 0, hitRow = -1; std::vector<int> deck, hand; bool held[5] = { false, false, false, false, false }; const int CO[4] = { 5, 10, 25, 100 }; std::string handName;
    void start() override { initWallet(); deck = shoe(1); hand.assign(deck.begin(), deck.begin() + 5); say("Elige tu apuesta y reparte"); }
    void go() {
        if (phase == 0) { int b = coin * coins; if (w < b) { say("No alcanzan las fichas: baja la apuesta", -1); return; } addW(-b); deck = shoe(1); hand.assign(deck.begin(), deck.begin() + 5); deck.erase(deck.begin(), deck.begin() + 5); for (bool& h : held) h = false; phase = 1; hitRow = -1; say("Toca las cartas que quieres retener"); handName = ""; app->audio.play("click"); }
        else { for (int i = 0; i < 5; i++) if (!held[i]) { hand[i] = deck.front(); deck.erase(deck.begin()); } phase = 0; int r = vpRank(hand); int pay = 0; if (r >= 0) pay = (r == 0 && coins == 5 ? 4000 : VP[r].m * coins) * coin; hitRow = r; for (bool& h : held) h = false; if (pay > 0) addW(pay); commit(); handName = eval5(hand).name;
            say(r >= 0 ? std::string("¡") + VP[r].n + "! Ganas " + std::to_string(pay) : "Sin premio", r >= 0 ? 1 : -1); app->audio.play(r >= 0 ? "win" : "lose"); }
    }
    void update(float, Input& in, UI&) override { if (pl[0].pad >= 0 && pl[0].pad < Input::MAXP && in.pads[pl[0].pad].rollDown()) go(); }
    void draw(Gfx& g, UI& ui) override {
        if (topBar(g, ui)) return; if (phase == 0 && broke(g, ui, 5)) return;
        g.rrectGrad({ 290, 64, 400, 300 }, 14, rgb(0x10286b), rgb(0x071240)); g.rrectLine({ 290, 64, 400, 300 }, 14, 3, rgb(0xc9a24a));
        for (int i = 0; i < 9; i++) { float y = 82 + i * 30.f; if (i == hitRow) g.rrect({ 296, y - 14, 388, 28 }, 6, rgb(0xffd76b, 190)); Col c = i == hitRow ? rgb(0x222222) : rgb(0xffffff); g.text(VP[i].n, 306, y, 15, c, F_BODY, AL_L); for (int n = 1; n <= 5; n++) g.text(std::to_string(i == 0 && n == 5 ? 4000 : VP[i].m * n), 440.f + n * 48, y, 13, n == coins ? (i == hitRow ? rgb(0x222222) : pal::gold) : c, F_BODY, AL_R); }
        g.rrectGrad({ 706, 64, 540, 300 }, 14, rgb(0x1f8a4e), rgb(0x0b4526)); for (int i = 0; i < 5; i++) { Rect r{ 724.f + i * 102, 140.f - (held[i] ? 14 : 0), 92, 130 }; uint32_t id = hid("vp", i); bool hit = ui.hit(id, r, phase == 1); drawCard(g, r, hand[i], false, held[i]); if (held[i]) g.text("RETENER", r.cx(), r.y + r.h + 18, 13, pal::gold, F_BOLD, AL_C); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { held[i] = !held[i]; app->audio.play("click"); } }
        if (phase == 1) ui.setFocusIfNone(hid("vp", 0)); g.text(handName, 976, 90, 18, pal::cream, F_BOLD, AL_C); drawMsg(g, 396);
        g.rrect({ 330, 424, 100, 32 }, 16, rgb(0x120c08, 200)); g.text("Moneda:", 420, 440, 17, pal::cream, F_BOLD, AL_R); for (int i = 0; i < 4; i++) if (ui.button(std::to_string(CO[i]), { 440.f + i * 76, 420, 70, 40 }, CO[i] == coin ? 3 : 2, phase == 0)) coin = CO[i];
        g.rrect({ 320, 484, 110, 32 }, 16, rgb(0x120c08, 200)); g.text("Monedas:", 420, 500, 17, pal::cream, F_BOLD, AL_R); for (int n = 1; n <= 5; n++) if (ui.button(std::to_string(n), { 440.f + (n - 1) * 62, 480, 56, 40 }, n == coins ? 3 : 2, phase == 0)) coins = n;
        g.rrect({ 950, 424, 250, 32 }, 16, rgb(0x120c08, 200)); g.text("Apuesta: " + std::to_string(coin * coins) + " fichas", 960, 440, 18, pal::gold, F_BOLD, AL_L);
        if (ui.button(phase == 0 ? "Repartir" : "Cambiar", { 620, 545, 320, 56 }, 3)) go(); if (phase == 0) ui.setFocusIfNone(hid("Repartir", 620 * 7 + 545));
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Repartir / cambiar" }, { "DPAD/STICK", "Moverse" }, { "A", "Retener carta" } }; }
    std::string help() const override { return "Video póker Jacks or Better. Elige moneda y cantidad (1-5), reparte, retén las cartas que quieras (A) y cambia el resto una sola vez. Paga desde un par de J en adelante; con 5 monedas la escalera real paga 4000 por moneda. Fichas simbólicas; tu banca se guarda."; }
};

// ===================================================================== BACCARAT
int bv(int c) { int r = c % 13 + 1; return r >= 10 ? 0 : r; } int btot(const std::vector<int>& cs) { int t = 0; for (int c : cs) t += bv(c); return t % 10; }
struct Baccarat : Casino {
    int bets[3] = { 0, 0, 0 }; std::vector<int> sh, P, B, seqWho, seqIdx; std::vector<char> hist; bool busy = false; float t = 0; int pt = 0, bt = 0, stakedAt = 0;
    void start() override { initWallet(); say("Toca Jugador, Banca o Empate para apostar"); }
    int staked() const { return bets[0] + bets[1] + bets[2]; }
    int drawC() { if (sh.size() < 24) sh = shoe(8); int c = sh.back(); sh.pop_back(); return c; }
    void deal() {
        int st = staked(); if (!st) { say("Toca Jugador, Banca o Empate para apostar"); return; } if (st > w) return; busy = true; addW(-st); stakedAt = st; t = 0; app->audio.play("click");
        P = { drawC(), drawC() }; B = { drawC(), drawC() }; pt = btot(P); bt = btot(B);
        if (!(pt >= 8 || bt >= 8)) { int p3 = -1; if (pt <= 5) { p3 = drawC(); P.push_back(p3); } if (p3 < 0) { if (bt <= 5) B.push_back(drawC()); } else { int v = bv(p3); bool hit = bt <= 2 || (bt == 3 && v != 8) || (bt == 4 && v >= 2 && v <= 7) || (bt == 5 && v >= 4 && v <= 7) || (bt == 6 && (v == 6 || v == 7)); if (hit) B.push_back(drawC()); } }
        pt = btot(P); bt = btot(B); seqWho.clear(); seqIdx.clear(); int L = (int)std::max(P.size(), B.size()); for (int i = 0; i < L; i++) { if (i < (int)P.size()) { seqWho.push_back(0); seqIdx.push_back(i); } if (i < (int)B.size()) { seqWho.push_back(1); seqIdx.push_back(i); } } say("");
    }
    void settle() {
        char out = pt > bt ? 'P' : bt > pt ? 'B' : 'T'; int ret = 0; if (out == 'P') ret = bets[0] * 2; else if (out == 'B') ret = (int)(bets[1] * 1.95f); else ret = bets[2] * 9 + bets[0] + bets[1]; addW(ret); commit(); int net = ret - stakedAt; hist.push_back(out); busy = false;
        say(std::string(out == 'P' ? "Gana el Jugador " : out == 'B' ? "Gana la Banca " : "Empate ") + std::to_string(pt) + " a " + std::to_string(bt) + (net > 0 ? " · ¡ganas " + std::to_string(net) + "!" : net < 0 ? " · pierdes " + std::to_string(-net) : " · quedas igual"), net > 0 ? 1 : net < 0 ? -1 : 0); app->audio.play(net > 0 ? "win" : net < 0 ? "lose" : "click"); for (int& b : bets) b = 0;
    }
    void update(float dt, Input& in, UI&) override { if (busy) { int before = (int)(t / 0.5f); t += dt; if ((int)(t / 0.5f) != before && (int)(t / 0.5f) <= (int)seqWho.size()) app->audio.play("click"); if (t > 0.5f * (seqWho.size() + 1) + 0.4f) settle(); } else if (pl[0].pad >= 0 && pl[0].pad < Input::MAXP && in.pads[pl[0].pad].rollDown()) deal(); }
    void draw(Gfx& g, UI& ui) override {
        if (topBar(g, ui)) return; if (!busy && staked() == 0 && broke(g, ui, 5)) return;
        g.rrectGrad({ 290, 64, 950, 230 }, 18, rgb(0x1f8a4e), rgb(0x0b4526)); int shownN = busy ? std::min((int)seqWho.size(), (int)(t / 0.5f)) : (int)seqWho.size(); int cp = 0, cb = 0; std::vector<int> sp, sb;
        for (int k = 0; k < shownN; k++) { if (seqWho[k] == 0) sp.push_back(P[seqIdx[k]]); else sb.push_back(B[seqIdx[k]]); }
        g.text("Jugador " + (sp.empty() ? std::string() : std::to_string(btot(sp))), 360, 84, 18, pal::cream, F_BOLD, AL_C); g.text("Banca " + (sb.empty() ? std::string() : std::to_string(btot(sb))), 360, 184, 18, pal::cream, F_BOLD, AL_C);
        for (size_t i = 0; i < sp.size(); i++) drawCard(g, { 460.f + i * 90, 72, 70, 98 }, sp[i]); for (size_t i = 0; i < sb.size(); i++) drawCard(g, { 460.f + i * 90, 176, 70, 98 }, sb[i]); (void)cp; (void)cb;
        drawMsg(g, 322); for (size_t i = 0; i < hist.size() && i < 14; i++) { char h = hist[hist.size() - 1 - i]; g.circle(1220 - i * 26.f, 322, 11, h == 'P' ? rgb(0x1f5fa8) : h == 'B' ? rgb(0xc0392b) : rgb(0x1e8449)); g.text(std::string(1, h), 1220 - i * 26.f, 322, 12, rgb(0xffffff), F_BOLD, AL_C); }
        const char* NM[3] = { "Jugador (1:1)", "Banca (0,95:1)", "Empate (8:1)" }; Col CL[3] = { rgb(0x1f5fa8), rgb(0xc0392b), rgb(0x1e8449) };
        for (int s = 0; s < 3; s++) { Rect r{ 300.f + s * 315, 350, 300, 100 }; uint32_t id = hid("bc", s); bool hit = ui.hit(id, r, !busy && staked() + CH[chipSel] <= w); g.rrect(r, 16, bets[s] ? pal::gold : rgb(0xffffff, 120)); g.rrectGrad(r.inset(3), 14, CL[s], rgb(0x101010)); g.text(NM[s], r.cx(), r.y + 34, 22, rgb(0xffffff), F_BOLD, AL_C); g.text(bets[s] ? std::to_string(bets[s]) : "—", r.cx(), r.y + 72, 24, pal::gold, F_TITLE, AL_C); if (ui.focused(id)) ui.drawFocus(r, 16); if (hit) { bets[s] += CH[chipSel]; app->audio.play("click"); } }
        if (!busy) ui.setFocusIfNone(hid("bc", 0)); g.rrect({ 360, 484, 100, 32 }, 16, rgb(0x120c08, 200)); g.text("Ficha:", 450, 500, 18, pal::cream, F_BOLD, AL_R); chipPick(g, ui, 470, 480);
        if (ui.button("Repartir", { 480, 555, 280, 52 }, 3, !busy && staked() > 0)) deal(); if (ui.button("Borrar apuestas", { 780, 555, 260, 52 }, 2, !busy)) for (int& b : bets) b = 0;
        g.text("Gana la mano más cercana a 9. Banca paga 0,95:1 (5% de comisión). Empate paga 8:1 y devuelve Jugador y Banca. Mazo de 8 barajas.", 765, 650, 14, rgb(0xd8b46a), F_BODY, AL_C, 960);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Repartir" }, { "DPAD/STICK", "Moverse" }, { "A", "Apostar" } }; }
    std::string help() const override { return "Apuesta a Jugador, Banca o Empate tocando la casilla con la ficha elegida (puedes apostar a varias). Gana la mano que más se acerque a 9. Jugador paga 1:1, Banca 0,95:1 y Empate 8:1. Fichas simbólicas; tu banca se guarda."; }
};

// ===================================================================== PAI GOW POKER
struct PaiGow : Casino {
    int phase = 0; std::vector<int> mine, bank; std::vector<int> low; Split bs; std::string sInfo;
    void start() override { initWallet(); say("Haz tu apuesta y reparte"); }
    void deal() { if (bet < 5 || bet > w) { say("Pon al menos 5 fichas"); return; } lastBet = bet; addW(-bet); auto s = shoe(1, true); mine.assign(s.begin(), s.begin() + 7); bank.assign(s.begin() + 7, s.begin() + 14); low.clear(); phase = 1; say(""); app->audio.play("click"); }
    void auto_() { Split sp = paiSplit(mine); low.clear(); for (int c : sp.low) low.push_back((int)(std::find(mine.begin(), mine.end(), c) - mine.begin())); }
    void confirm() {
        if (low.size() != 2) { say("Elige 2 cartas para la mano baja", -1); return; } std::vector<int> hi, lo; for (int i = 0; i < 7; i++) if (std::find(low.begin(), low.end(), i) == low.end()) hi.push_back(mine[i]); for (int i : low) lo.push_back(mine[i]); Ev H = evalJ(hi); Ev2 L = eval2(lo);
        if (!paiOk(H, L)) { say("Mano inválida: la de 5 cartas debe ser mayor que la de 2", -1); app->audio.play("lose"); return; }
        bs = paiSplit(bank); phase = 2; int wHi = H.key > bs.hi.key, wLo = L.key > bs.lo.key, wn = wHi + wLo, ret = 0; std::string txt; int kind = 0;
        if (wn == 2) { int prof = (int)(bet * 0.95f); ret = bet + prof; txt = "¡Ganas las dos manos! +" + std::to_string(prof); kind = 1; } else if (wn == 1) { ret = bet; txt = "Una y una: empate, recuperas tu apuesta"; } else { txt = "Pierdes " + std::to_string(bet) + (H.key == bs.hi.key || L.key == bs.lo.key ? " (las copias son del banquero)" : ""); kind = -1; }
        addW(ret); commit(); say(txt, kind); sInfo = "Banquero: alta " + bs.hi.name + " · baja " + (bs.lo.pair ? "Par" : "Carta alta"); app->audio.play(wn == 2 ? "win" : wn == 1 ? "click" : "lose");
    }
    void update(float, Input& in, UI&) override { if (phase == 0 && pl[0].pad >= 0 && pl[0].pad < Input::MAXP && in.pads[pl[0].pad].rollDown()) deal(); }
    void draw(Gfx& g, UI& ui) override {
        if (topBar(g, ui)) return; if (phase == 0 && broke(g, ui, 5)) return;
        g.rrectGrad({ 290, 64, 950, 440 }, 18, rgb(0x1f8a4e), rgb(0x0b4526));
        g.text(phase == 2 ? sInfo : "Banquero", 765, 84, 17, pal::cream, F_BOLD, AL_C); if (phase >= 1) { if (phase == 2) { for (size_t i = 0; i < bs.high.size(); i++) drawCard(g, { 520.f + i * 84, 98, 70, 98 }, bs.high[i]); for (size_t i = 0; i < bs.low.size(); i++) drawCard(g, { 520.f + (i + 5) * 84 + 24, 98, 70, 98 }, bs.low[i]); } else { for (int i = 0; i < 7; i++) drawCard(g, { 520.f + (i < 5 ? i * 84 : i * 84 + 24), 98, 70, 98 }, 0, true); } }
        if (phase >= 1) { std::vector<int> hi; for (int i = 0; i < 7; i++) if (std::find(low.begin(), low.end(), i) == low.end()) hi.push_back(mine[i]);
            g.text(phase == 1 ? "Tus 7 cartas: elige 2 para la mano baja" : "Tus manos", 765, 240, 17, pal::cream, F_BOLD, AL_C);
            for (int i = 0; i < 7; i++) { bool isLow = std::find(low.begin(), low.end(), i) != low.end(); Rect r{ 400.f + i * 96, 256.f - (isLow ? 20 : 0), 84, 118 }; uint32_t id = hid("pg", i); bool hit = ui.hit(id, r, phase == 1); drawCard(g, r, mine[i], false, isLow); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) { auto it = std::find(low.begin(), low.end(), i); if (it != low.end()) low.erase(it); else { if (low.size() >= 2) low.erase(low.begin()); low.push_back(i); } app->audio.play("click"); } }
            if (phase == 1) ui.setFocusIfNone(hid("pg", 0)); if (low.size() == 2) { Ev h = evalJ(hi); Ev2 l = eval2({ mine[low[0]], mine[low[1]] }); g.text("Alta: " + h.name + " · Baja: " + (l.pair ? "Par" : "Carta alta") + (paiOk(h, l) ? "" : "  ⚠ la alta debe ser mayor"), 765, 410, 16, pal::gold, F_BOLD, AL_C); } }
        drawMsg(g, 440);
        if (phase == 0) { betRow(g, ui, 520, false); if (ui.button("Repartir", { 540, 580, 300, 52 }, 3, bet >= 5)) deal(); }
        else if (phase == 1) { if (ui.button("Arreglo automático", { 440, 520, 300, 52 }, 2)) auto_(); if (ui.button("Confirmar manos", { 760, 520, 300, 52 }, 3)) confirm(); }
        else if (ui.button("Nueva mano", { 540, 520, 300, 52 }, 3)) { phase = 0; mine.clear(); low.clear(); bet = std::min(lastBet, w); msg = ""; } if (phase == 2) ui.setFocusIfNone(hid("Nueva mano", 540 * 7 + 520));
        g.text("Mano alta de 5 y baja de 2 (la alta debe ser mayor). Ganas si superas al banquero en las dos; una y una es empate; las copias son del banquero. Comodín: as o completa escalera/color. Comisión 5%.", 765, 670, 13, rgb(0xd8b46a), F_BODY, AL_C, 980);
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Repartir" }, { "DPAD/STICK", "Moverse" }, { "A", "Elegir carta baja" } }; }
    std::string help() const override { return "Recibes 7 cartas (más un comodín) y las divides en una mano alta de 5 y una baja de 2; la alta debe valer más que la baja. Ganas si superas al banquero en las dos manos; si ganas una sola es empate; las copias son del banquero. Pagas 5% de comisión sobre lo ganado. Fichas simbólicas; tu banca se guarda."; }
};

// ===================================================================== BLACKJACK CASINO / RUMMY TICO
int bjVal(int c) { int r = rankOf(c); return r == 1 ? 11 : std::min(10, r); }
int bjTot(const std::vector<int>& cs) { int t = 0, a = 0; for (int c : cs) { t += bjVal(c); if (rankOf(c) == 1) a++; } while (t > 21 && a > 0) { t -= 10; a--; } return t; }
struct BJHand { std::vector<int> cards; int bet = 0; bool done = false, fromSplit = false, splitAce = false, surrender = false; int paid = 0; std::string note; };
struct Blackjack : Casino {
    bool rummy; Blackjack(bool r) : rummy(r) {}
    std::vector<int> sh, dealer; std::vector<BJHand> hands; int cur = 0, phase = 0; bool hideHole = true; float dT = 0;
    void start() override { initWallet(); say(rummy ? "Rummy tico: 4 barajas, blackjack 1:1, bonus 7-7-7 5:1, otro trío o escalera de color 3:1" : "Blackjack: 6 barajas, blackjack 3:2, dobla en cualquier mano"); }
    int drawC() { if (sh.size() < 30) sh = shoe(rummy ? 4 : 6); int c = sh.back(); sh.pop_back(); return c; }
    bool isBJ(const BJHand& h) const { return h.cards.size() == 2 && !h.fromSplit && bjTot(h.cards) == 21; }
    void deal() {
        if (bet < 5 || bet > w) { say("Pon al menos 5 fichas"); return; } lastBet = bet; addW(-bet); phase = 1; hideHole = true; hands.assign(1, BJHand()); hands[0].cards = { drawC(), drawC() }; hands[0].bet = bet; dealer = { drawC(), drawC() }; cur = 0; say(""); app->audio.play("click");
        if (!(rummy && bjVal(dealer[0]) >= 10) && bjTot(dealer) == 21 && dealer.size() == 2) { for (auto& h : hands) h.done = true; hideHole = false; settle(); return; }
        if (isBJ(hands[0])) { hands[0].done = true; dealerPlay(); }
    }
    void nextHand() { cur = -1; for (size_t i = 0; i < hands.size(); i++) if (!hands[i].done) { cur = (int)i; break; } if (cur < 0) { cur = 0; dealerPlay(); return; } auto& h = hands[cur]; if (h.cards.size() == 1) { h.cards.push_back(drawC()); if (h.splitAce && !rummy) { h.done = true; nextHand(); return; } } if (bjTot(h.cards) == 21) { h.done = true; nextHand(); } }
    int bonus(const BJHand& h) const { if (!rummy || h.cards.size() != 3 || h.fromSplit) return 0; int r[3], s[3]; for (int i = 0; i < 3; i++) { r[i] = rankOf(h.cards[i]); s[i] = suitOf(h.cards[i]); } if (r[0] == 7 && r[1] == 7 && r[2] == 7) return 5; if (r[0] == r[1] && r[1] == r[2]) return 3; int v[3] = { r[0], r[1], r[2] }; std::sort(v, v + 3); if (s[0] == s[1] && s[1] == s[2] && ((v[1] == v[0] + 1 && v[2] == v[1] + 1) || (v[0] == 1 && v[1] == 12 && v[2] == 13))) return 3; return 0; }
    void act(int a) {
        if (phase != 1) return; BJHand& h = hands[cur];
        if (a == 0) { h.cards.push_back(drawC()); app->audio.play("click"); int t = bjTot(h.cards), b = bonus(h); if (b) { h.done = true; h.paid = b; h.note = "🎉 ¡Bonus " + std::to_string(b) + ":1!"; } else if (t >= 21) h.done = true; if (h.done) nextHand(); }
        else if (a == 1) { h.done = true; nextHand(); }
        else if (a == 2) { if (h.cards.size() != 2 || w < h.bet) return; addW(-h.bet); h.bet *= 2; h.cards.push_back(drawC()); h.done = true; app->audio.play("place"); nextHand(); }
        else if (a == 3) { int maxH = rummy ? 4 : 2; if (h.cards.size() != 2 || w < h.bet || (int)hands.size() >= maxH) return; addW(-h.bet); int c2 = h.cards.back(); h.cards.pop_back(); h.fromSplit = true; h.splitAce = rankOf(c2) == 1; BJHand nh; nh.cards = { c2 }; nh.bet = h.bet; nh.fromSplit = true; nh.splitAce = rankOf(c2) == 1; int bet0 = h.bet; (void)bet0; hands.insert(hands.begin() + cur + 1, nh); hands[cur].cards.push_back(drawC()); if (hands[cur].splitAce && !rummy) hands[cur].done = true; app->audio.play("click"); if (hands[cur].done) nextHand(); else if (bjTot(hands[cur].cards) == 21) { hands[cur].done = true; nextHand(); } }
        else if (a == 4) { addW(h.bet / 2); h.done = true; h.surrender = true; hideHole = false; phase = 3; commit(); say("Te rindes: recuperas la mitad", -1); }
    }
    void dealerPlay() { hideHole = false; phase = 2; dT = 0; }
    void settle() {
        phase = 3; int dt = bjTot(dealer); bool dbj = dealer.size() == 2 && dt == 21; int net = 0;
        for (auto& h : hands) { int t = bjTot(h.cards), ret = 0; if (h.surrender) continue; if (h.paid) ret = h.bet * (1 + h.paid); else if (isBJ(h)) ret = dbj ? h.bet : (int)(h.bet + h.bet * (rummy ? 1.0f : 1.5f)); else if (t > 21) ret = 0; else if (dbj) ret = 0; else if (dt > 21 || t > dt) ret = h.bet * 2; else if (t == dt) ret = h.bet;
            addW(ret); net += ret - h.bet; int res = ret - h.bet; if (h.note.empty()) h.note = res > 0 ? "✅ +" + std::to_string(res) : res < 0 ? "❌ " + std::to_string(res) : "🤝 empate"; }
        commit(); say(net > 0 ? "¡Ganas " + std::to_string(net) + "!" : net < 0 ? "Pierdes " + std::to_string(-net) : "Empate", net > 0 ? 1 : net < 0 ? -1 : 0); app->audio.play(net > 0 ? "win" : net < 0 ? "lose" : "click");
    }
    void update(float dt, Input& in, UI&) override {
        if (phase == 2) { dT += dt; if (dT > 0.65f) { dT = 0; bool live = false; for (auto& h : hands) if (!h.paid && bjTot(h.cards) <= 21 && !isBJ(h)) live = true; if (live && bjTot(dealer) < 17) { dealer.push_back(drawC()); app->audio.play("click"); } else settle(); } }
        else if (phase == 0 && pl[0].pad >= 0 && pl[0].pad < Input::MAXP && in.pads[pl[0].pad].rollDown()) deal();
    }
    void draw(Gfx& g, UI& ui) override {
        if (topBar(g, ui)) return; if (phase == 0 && broke(g, ui, 5)) return;
        g.rrectGrad({ 290, 64, 950, 440 }, 18, rgb(0x1f8a4e), rgb(0x0b4526)); int dtot = bjTot(hideHole ? std::vector<int>(dealer.begin(), dealer.begin() + std::min<size_t>(1, dealer.size())) : dealer);
        g.text(std::string("Crupier") + (dealer.empty() ? "" : "  " + std::to_string(dtot)), 765, 84, 18, pal::cream, F_BOLD, AL_C); for (size_t i = 0; i < dealer.size(); i++) drawCard(g, { 765.f - dealer.size() * 45 + i * 90, 98, 78, 108 }, dealer[i], hideHole && i == 1);
        float hw = 940.f / std::max<size_t>(1, hands.size()); for (size_t i = 0; i < hands.size(); i++) { auto& h = hands[i]; float cx = 290 + hw * (i + 0.5f); bool on = phase == 1 && (int)i == cur; g.text("Tu mano" + (hands.size() > 1 ? " " + std::to_string(i + 1) : std::string()) + " · apuesta " + std::to_string(h.bet) + " · " + std::to_string(bjTot(h.cards)), cx, 240, 16, on ? pal::gold : pal::cream, F_BOLD, AL_C, hw - 10);
            float cw = std::min(70.f, (hw - 20) / std::max<size_t>(1, h.cards.size())); for (size_t k = 0; k < h.cards.size(); k++) drawCard(g, { cx - h.cards.size() * cw / 2 + k * cw - 0, 258, 66, 92 }, h.cards[k], false, on && k == h.cards.size() - 1); if (!h.note.empty()) g.text(h.note, cx, 372, 16, pal::gold, F_BOLD, AL_C, hw - 10); }
        drawMsg(g, 430);
        if (phase == 0) { betRow(g, ui, 520, false); if (ui.button("Repartir", { 540, 580, 300, 52 }, 3, bet >= 5)) deal(); }
        else if (phase == 1) { auto& h = hands[cur]; bool canD = h.cards.size() == 2 && w >= h.bet, canS = h.cards.size() == 2 && bjVal(h.cards[0]) == bjVal(h.cards[1]) && (int)hands.size() < (rummy ? 4 : 2) && w >= h.bet && (rummy || !h.fromSplit); bool canSur = rummy && hands.size() == 1 && h.cards.size() == 2 && !h.fromSplit && bjVal(dealer[0]) >= 10;
            if (ui.button("Pedir", { 330, 520, 170, 52 }, 3)) act(0); if (ui.button("Plantarse", { 510, 520, 170, 52 }, 2)) act(1); if (ui.button("Doblar", { 690, 520, 170, 52 }, 2, canD)) act(2); if (ui.button("Dividir", { 870, 520, 170, 52 }, 2, canS)) act(3); if (canSur && ui.button("Rendirse (½)", { 1050, 520, 170, 52 }, 1)) act(4); ui.setFocusIfNone(hid("Pedir", 330 * 7 + 520)); }
        else if (phase == 3) { if (ui.button("Nueva mano", { 540, 520, 300, 52 }, 3)) { phase = 0; dealer.clear(); hands.clear(); hideHole = true; msg = ""; bet = std::min(lastBet, w); } ui.setFocusIfNone(hid("Nueva mano", 540 * 7 + 520)); }
    }
    std::vector<Hint> hints() const override { return { { "ROLL", "Repartir" }, { "DPAD/STICK", "Moverse" }, { "A", "Elegir acción" } }; }
    std::string help() const override { return rummy ? "Rummy tico (variante de blackjack): 4 barajas, el crupier se planta en 17, blackjack paga 1:1, puedes dividir hasta 4 manos y doblar en cualquiera. Rendición temprana si el crupier muestra 10 o as. Bonus con 3 cartas: 7-7-7 paga 5:1, otro trío o escalera de color 3:1. Fichas simbólicas." : "Blackjack de casino: llega a 21 sin pasarte y supera al crupier, que se planta en 17. Blackjack paga 3:2; puedes doblar y dividir pares (hasta 2 manos). Fichas simbólicas; tu banca se guarda."; }
};
}
Game* makeSlots() { return new Slots(); }
Game* makeVideoPoker() { return new VideoPoker(); }
Game* makeBaccarat() { return new Baccarat(); }
Game* makePaiGow() { return new PaiGow(); }
Game* makeBlackjackCasino() { return new Blackjack(false); }
Game* makeRummyTico() { return new Blackjack(true); }
