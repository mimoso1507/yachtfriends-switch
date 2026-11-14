// Juegos de fiesta 1: Verdad o reto, Tabú y Charadas. Lo secreto (palabra a actuar) se ve manteniendo L en la Switch, o en el celular del jugador.
#include "party.h"

namespace {
// ===================================================================== VERDAD O RETO
struct VerdadReto : Party {
    int turn = 0, round = 1, rounds = 2, phase = 0; bool over = false; std::string kind, prompt; std::vector<int> dv, dr; float endT = 0;
    void start() override { rounds = opts.get("rounds", 1) + 1; refill(true); refill(false); }
    void refill(bool v) { auto& d = v ? dv : dr; d.clear(); int n = v ? pd::VERDAD_N : pd::RETO_N; for (int i = 0; i < n; i++) d.push_back(i); for (int i = n - 1; i > 0; i--) std::swap(d[i], d[rndInt(i + 1)]); }
    int turnPlayer() const override { return over ? -1 : turn; }
    void choose(bool verdad) { auto& d = verdad ? dv : dr; if (d.empty()) refill(verdad); int i = d.back(); d.pop_back(); prompt = verdad ? pd::VERDAD[i] : pd::RETO[i]; kind = verdad ? "VERDAD" : "RETO"; phase = 1; app->audio.play("win"); }
    void next() { phase = 0; prompt.clear(); if (turn == (int)pl.size() - 1) { if (round >= rounds) { over = true; return; } round++; } turn = (turn + 1) % (int)pl.size(); app->audio.play("click"); }
    void resign(int) override { over = true; }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 1.5f && !finished) { std::vector<PResult> r; for (size_t i = 0; i < pl.size(); i++) r.push_back({ 0, 'd' }); finish(r, "¡Se terminó la ronda de verdad o reto!"); } return; }
        Pad* pd_ = padOf(in, turn); if (pd_ && phase == 1 && pd_->down[B_A]) next();
    }
    void draw(Gfx& g, UI& ui) override {
        banner(g, over ? "¡Terminó el juego!" : pl[turn].name + ": elige Verdad o Reto" + (rounds > 1 ? "  (vuelta " + std::to_string(round) + "/" + std::to_string(rounds) + ")" : ""));
        g.emoji("🎡", 765, 150, 90); if (phase == 0 && !over) { g.rrect({ 430, 215, 420, 50 }, 16, rgb(0x120c08, 210)); g.text("Elige una opción", 640, 240, 20, pal::cream, F_BOLD, AL_C); }
        if (phase == 0 && !over) { if (ui.button("💬 Verdad", { 430, 440, 200, 70 }, 3)) { choose(true); return; } if (ui.button("🔥 Reto", { 650, 440, 200, 70 }, 1)) { choose(false); return; } ui.setFocusIfNone(hid("💬 Verdad", 430 * 7 + 440)); }
        if (phase == 1) { g.rrect({ 330, 240, 870, 200 }, 20, pal::panelEdge); g.rrectGrad({ 333, 243, 864, 194 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c)); g.textShadow(kind == "RETO" ? "🔥 RETO" : "💬 VERDAD", 765, 280, 30, pal::gold, F_TITLE, AL_C); g.textBox(prompt, { 360, 310, 810, 110 }, 26, pal::cream, F_BOLD, AL_C);
            if (ui.button("Siguiente jugador", { 520, 470, 330, 60 }, 3)) { next(); return; } ui.setFocusIfNone(hid("Siguiente jugador", 520 * 7 + 470)); }
        std::string sc; for (size_t i = 0; i < pl.size(); i++) sc += std::string(i == (size_t)turn ? "▶ " : "") + pl[i].name + "   "; g.rrect({ 330, 580, 870, 40 }, 20, rgb(0x120c08, 210)); g.text(sc, 765, 600, 15, rgb(0xd8b46a), F_BODY, AL_C, 940);
    }
    std::vector<Hint> hints() const override { return { { "A", "Elegir / siguiente" }, { "DPAD/STICK", "Moverse" } }; }
    std::string help() const override { return "Juego de fiesta sin ganador. En tu turno eliges VERDAD (una pregunta para contestar sincero) o RETO (algo para hacer) y lo cumples. Luego sigue el próximo jugador."; }
};

// ===================================================================== TABÚ y CHARADAS
struct ActGame : Party {
    bool tabu; ActGame(bool t) : tabu(t) {}
    struct Item { std::string cat, word; std::vector<std::string> forb; };
    std::vector<Item> deck; Item card; int phase = 0, t = 0, turns = 0, rounds = 1, desc = 0, turnScore = 0, secs = 60; int score[2] = { 0, 0 }; float timer = 0, endT = 0; bool over = false; std::vector<std::vector<int>> teams;
    std::vector<int> lv; std::string flash; float flashT = 0;
    // modo "dibujos" de Charadas: el actuante dibuja con el stick; su equipo marca ¡Acertaron! con A
    struct Sg { float x1, y1, x2, y2; uint32_t col; float th; }; std::vector<Sg> segs; std::vector<size_t> strokes; bool drawM = false, drawing = false; float cx = 620, cy = 330, lastX = -1, lastY = -1; int color = 0, bsz = 1; const Rect CV{ 300, 134, 640, 440 };
    static uint32_t DCOL(int i) { static const uint32_t C[8] = { 0x111111, 0xe53935, 0x1e88e5, 0x43a047, 0xfdd835, 0xfb8c00, 0x8e24aa, 0x8d6e63 }; return C[i]; }
    void addSeg(float a, float b, float c, float d) { segs.push_back({ a, b, c, d, DCOL(color), bsz == 0 ? 3.f : bsz == 1 ? 6.f : 12.f }); }
    static const char* CATS(int i) { static const char* C[8] = { "🎬 Película", "🐾 Animal", "👷 Profesión", "🏃 Acción", "🧸 Objeto", "🦸 Personaje", "⚽ Deporte", "🎵 Canción o dicho" }; return C[i]; }
    void start() override {
        int n = (int)pl.size(); teams.assign(2, {}); for (int i = 0; i < n; i++) teams[i % 2].push_back(i); rounds = opts.get("rounds", 1) + 1; int tm = opts.get("time", 1); secs = tm == 0 ? 45 : tm == 2 ? 90 : 60; drawM = !tabu && opts.get("mode", 0) == 1; int df = opts.get("diff", 1); lv = df == 0 ? std::vector<int>{ 1 } : df == 2 ? std::vector<int>{ 2, 3 } : std::vector<int>{ 1, 2, 3 };
        turns = 2 * (int)std::max(teams[0].size(), teams[1].size()) * rounds; buildDeck(); pickDesc(); phase = 0;
    }
    void buildDeck() {
        deck.clear(); if (tabu) { for (int i = 0; i < pd::TABU_N; i++) { std::string s = pd::TABU[i]; auto p = s.find(':'); Item it; it.word = s.substr(0, p); it.forb = split(s.substr(p + 1), ','); deck.push_back(it); } }
        else for (int i = 0; i < pd::CHARADAS_N; i++) { std::string s = pd::CHARADAS[i]; auto a = split(s, '|'); int l = atoi(a[1].c_str()); if (std::find(lv.begin(), lv.end(), l) == lv.end()) continue; Item it; it.cat = CATS(atoi(a[0].c_str())); it.word = a[2]; deck.push_back(it); }
        for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]);
    }
    void pickDesc() { auto& m = teams[t % 2]; desc = m.empty() ? 0 : m[(t / 2) % m.size()]; }
    int turnPlayer() const override { return over ? -1 : desc; }
    void drawCard() { if (deck.empty()) buildDeck(); card = deck.back(); deck.pop_back(); }
    std::string secret(int p) const override { if (p != desc || phase != 1) return ""; std::string s = card.word; if (tabu) { s += "\n🚫 " ; for (size_t i = 0; i < card.forb.size(); i++) s += (i ? ", " : "") + card.forb[i]; } else s = card.cat + "\n" + s; return s; }
    void begin() { segs.clear(); strokes.clear(); drawing = false; lastX = -1; cx = 620; cy = 330; phase = 1; timer = (float)secs; turnScore = 0; drawCard(); app->audio.play("click"); }
    void ok() { segs.clear(); strokes.clear(); score[desc % 2]++; turnScore++; flash = "✅ ¡Acertaron!"; flashT = 0.8f; drawCard(); app->audio.play("win"); }
    void skip() { segs.clear(); strokes.clear(); flash = "⏭ Pasada"; flashT = 0.6f; drawCard(); app->audio.play("click"); }
    void buzz() { score[desc % 2]--; turnScore--; flash = "🚫 ¡TABÚ! -1"; flashT = 1.f; drawCard(); app->audio.play("lose"); }
    void endTurn() { phase = 2; app->audio.play("place"); }
    void nextTurn() { t++; if (t >= turns) { over = true; phase = 3; return; } pickDesc(); phase = 0; }
    void resign(int) override { over = true; phase = 3; }
    void update(float dt, Input& in, UI&) override {
        flashT = std::max(0.f, flashT - dt);
        if (over) { endT += dt; if (endT > 2.f && !finished) { int w = score[0] == score[1] ? -1 : score[0] > score[1] ? 0 : 1; std::vector<PResult> r; for (size_t i = 0; i < pl.size(); i++) r.push_back({ score[i % 2], w < 0 ? 'd' : (int)(i % 2) == w ? 'w' : 'l' }); finish(r, w < 0 ? "Empate" : "Ganó el equipo " + std::string(w == 0 ? "A" : "B") + " (" + std::to_string(score[0]) + " a " + std::to_string(score[1]) + ")"); } return; }
        if (phase == 1 && drawM) { timer -= dt; Pad* d = padOf(in, desc);
            if (d) { float sp = 260.f * dt, mx = d->lx * sp, my = d->ly * sp; if (d->held[B_LEFT]) mx -= sp; if (d->held[B_RIGHT]) mx += sp; if (d->held[B_UP]) my -= sp; if (d->held[B_DOWN]) my += sp;
                float nx = std::max(CV.x, std::min(CV.x + CV.w, cx + mx)), ny = std::max(CV.y, std::min(CV.y + CV.h, cy + my)); bool pen = d->held[B_A];
                if (pen) { if (!drawing) { strokes.push_back(segs.size()); drawing = true; } if (nx != cx || ny != cy) addSeg(cx, cy, nx, ny); else addSeg(cx, cy, cx + 0.1f, cy); } else drawing = false;
                cx = nx; cy = ny; if (d->down[B_Y]) color = (color + 1) % 8; if (d->down[B_L]) bsz = std::max(0, bsz - 1); if (d->down[B_R]) bsz = std::min(2, bsz + 1); if (d->down[B_B] && !strokes.empty()) { segs.resize(strokes.back()); strokes.pop_back(); } if (d->down[B_X]) skip(); if (d->rollDown()) ok(); }
            for (size_t i = 0; i < pl.size(); i++) if ((int)(i % 2) == desc % 2 && (int)i != desc) { Pad* p = padOf(in, (int)i); if (p && p->down[B_A]) ok(); }
            if (in.touchHeld && CV.in(in.tx, in.ty)) { if (lastX < 0) { strokes.push_back(segs.size()); addSeg(in.tx, in.ty, in.tx + 0.1f, in.ty); } else addSeg(lastX, lastY, in.tx, in.ty); lastX = in.tx; lastY = in.ty; } else if (!drawing) lastX = -1;
            if (timer <= 0) endTurn(); }
        else if (phase == 1) { timer -= dt; Pad* d = padOf(in, desc); if (d && d->down[B_A]) ok(); if (d && d->down[B_X]) skip();
            if (tabu) for (size_t i = 0; i < pl.size(); i++) if ((int)(i % 2) != desc % 2) { Pad* p = padOf(in, (int)i); if (p && p->down[B_Y]) buzz(); }
            if (timer <= 0) endTurn(); }
        else if (phase == 0 || phase == 2) { Pad* d = padOf(in, desc); if (d && d->down[B_A]) { if (phase == 0) begin(); else nextTurn(); } }
    }
    void draw(Gfx& g, UI& ui) override {
        std::string nm = pl[desc].name; int team = desc % 2;
        banner(g, over ? "Fin de la partida" : phase == 0 ? "Turno de " + nm + " (equipo " + std::string(team ? "B" : "A") + ")" : phase == 1 ? nm + (tabu ? " describe" : " actúa") + " · quedan " + std::to_string((int)std::ceil(std::max(0.f, timer))) + " s" : "Fin del turno");
        for (int k = 0; k < 2; k++) { Rect r{ 290.f + k * 480, 66, 470, 52 }; bool on = !over && team == k; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); std::string s = std::string("Equipo ") + (k ? "B: " : "A: "); for (size_t i = 0; i < teams[k].size(); i++) s += pl[teams[k][i]].name + (i + 1 < teams[k].size() ? ", " : ""); g.text(s, r.x + 12, r.y + 17, 14, pal::white, F_BOLD, AL_L, 360); g.text(std::to_string(score[k]), r.x + r.w - 14, r.cy(), 28, pal::gold, F_TITLE, AL_R); }
        g.text("Turno " + std::to_string(std::min(t + 1, turns)) + "/" + std::to_string(turns), 1230, 134, 14, pal::cream, F_BODY, AL_R);
        if (phase == 0 || phase == 2) { g.rrect({ 330, 150, 870, 250 }, 20, pal::panelEdge); g.rrectGrad({ 333, 153, 864, 244 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c)); g.emoji(tabu ? "🚫" : "🎭", 765, 215, 80); if (phase == 2) g.text("Este turno: " + std::string(turnScore >= 0 ? "+" : "") + std::to_string(turnScore) + " puntos", 765, 330, 24, pal::gold, F_BOLD, AL_C);
            if (phase == 0) g.text(tabu ? nm + " tiene que lograr que su equipo adivine sin decir las palabras prohibidas. Los rivales vigilan (Y = ¡TABÚ!)." : nm + " actúa sin hablar y su equipo adivina en voz alta.", 765, 320, 17, pal::cream, F_BODY, AL_C, 800);
            if (ui.button(phase == 0 ? "¡Empezar turno!" : "Siguiente turno", { 520, 420, 330, 64 }, 3)) { if (phase == 0) begin(); else nextTurn(); return; } ui.setFocusIfNone(hid(phase == 0 ? "¡Empezar turno!" : "Siguiente turno", 520 * 7 + 420)); }
        if (phase == 1 && drawM) {
            g.rrect({ CV.x - 6, CV.y - 6, CV.w + 12, CV.h + 12 }, 10, rgb(0x5a3a1c)); g.rect(CV, rgb(0xfffdf6)); g.clip(&CV); for (auto& sg : segs) { g.line(sg.x1, sg.y1, sg.x2, sg.y2, sg.th, rgb(sg.col)); g.circle(sg.x2, sg.y2, sg.th / 2, rgb(sg.col)); } g.clip(nullptr); g.ring(cx, cy, 8, 2, rgb(0xe53935));
            bool see = canSee(ui, desc); g.rrect({ 960, 134, 290, 130 }, 14, rgb(0x120c08, 215)); g.text(see ? card.cat : phoneOf(desc) ? "Mira tu celular" : "Mantén L para ver", 1105, 160, 16, pal::cream, F_BOLD, AL_C, 270); if (see) g.textBox(card.word, { 970, 180, 270, 70 }, 28, pal::gold, F_TITLE, AL_C);
            g.rrect({ 960, 280, 290, 40 }, 10, rgb(0x120c08, 215)); g.text("Color:", 990, 300, 14, pal::cream, F_BOLD, AL_L); g.circle(1090, 300, 12, rgb(DCOL(color))); g.text(bsz == 0 ? "Fino" : bsz == 1 ? "Medio" : "Grueso", 1180, 300, 14, pal::cream, F_BOLD, AL_C);
            g.rrect({ 960, 334, 290, 200 }, 14, rgb(0x120c08, 215)); g.textBox("Dibuja con el stick (mantén A). Y color, L/R grosor, B deshacer, X pasar. Cuando acierten: ZL/ZR (o A en el mando de un compañero).", { 972, 344, 266, 180 }, 15, pal::cream, F_BODY, AL_C);
            if (flashT > 0) g.textShadow(flash, 620, 110, 26, pal::gold, F_TITLE, AL_C); return; }
        if (phase == 1) {
            bool see = canSee(ui, desc); if (tabu) for (size_t i = 0; i < pl.size(); i++) if ((int)(i % 2) != desc % 2 && !pl[i].ai && !phoneOf((int)i) && heldL(ui, (int)i) && humans() > 1) see = true;
            g.rrect({ 330, 150, 870, 300 }, 20, pal::panelEdge); g.rrectGrad({ 333, 153, 864, 294 }, 18, rgb(0x4a2e18), rgb(0x2e1a0c));
            if (see) { if (!tabu) g.text(card.cat, 765, 186, 22, rgb(0xd8b46a), F_BOLD, AL_C); g.textShadow(card.word, 765, tabu ? 220 : 280, tabu ? 40 : 52, pal::gold, F_TITLE, AL_C); if (tabu) { for (size_t i = 0; i < card.forb.size(); i++) g.text("🚫 " + card.forb[i], 765, 275 + i * 34.f, 24, rgb(0xff9a9a), F_BOLD, AL_C); } }
            else g.text(phoneOf(desc) ? "Mira tu celular: ahí está la " + std::string(tabu ? "carta" : "palabra") : "Mantén L para ver la " + std::string(tabu ? "carta" : "palabra") + (tabu ? " (rivales: también con L)" : ""), 765, 300, 24, pal::cream, F_BOLD, AL_C, 800);
            if (flashT > 0) g.textShadow(flash, 765, 480, 30, pal::gold, F_TITLE, AL_C);
            if (ui.button("✅ Acertaron", { 380, 500, 250, 60 }, 3)) ok(); if (ui.button("⏭ Pasar", { 650, 500, 200, 60 }, 2)) skip(); if (tabu && ui.button("🚫 ¡TABÚ!", { 870, 500, 250, 60 }, 1)) buzz(); if (ui.button("Terminar turno", { 650, 575, 220, 50 }, 2)) endTurn(); ui.setFocusIfNone(hid("✅ Acertaron", 380 * 7 + 500)); }
    }
    std::vector<Hint> hints() const override { return { { "A", "Acertaron / empezar" }, { "X", "Pasar" }, { "L", "Ver la palabra" }, { "DPAD/STICK", "Moverse" } }; }
    std::string help() const override { return tabu ? "Tabú, dos equipos (posiciones pares contra impares). Quien describe ve una palabra y 5 prohibidas; debe lograr que su equipo la diga sin nombrar ninguna prohibida. Los rivales vigilan y tocan ¡TABÚ! (botón Y de su mando) si se equivoca: -1. Acertar da +1. Se ve la carta manteniendo L (o en el celular)." : "Charadas, dos equipos. Quien actúa ve la palabra (manteniendo L o en su celular) y hace mímica sin hablar; su equipo adivina en voz alta. A = acertaron, X = pasar. Gana el equipo con más aciertos."; }
};
}
Game* makeVerdadOReto() { return new VerdadReto(); }
Game* makeTabu() { return new ActGame(true); }
Game* makeCharadas() { return new ActGame(false); }
