// Juegos de fiesta 2: Dibuja y adivina, Película con emojis y Adivina el personaje.
#include "party.h"

namespace {
// ===================================================================== DIBUJA Y ADIVINA
struct Seg { float x1, y1, x2, y2; uint32_t col; float th; };
struct Dibuja : Party {
    int turn = 0, t = 0, turns = 0, rounds = 2, phase = 0, color = 0, size = 1, guessSel = -1; bool over = false, picking = false; std::vector<int> deck; std::string word; std::vector<int> score; std::vector<Seg> segs; std::vector<size_t> strokes; float timer = 0, total = 80, endT = 0, cx = 640, cy = 380, lastX = -1, lastY = -1; bool drawing = false; std::string res;
    static const uint32_t COLS(int i) { static const uint32_t C[8] = { 0x111111, 0xe53935, 0x1e88e5, 0x43a047, 0xfdd835, 0xfb8c00, 0x8e24aa, 0x8d6e63 }; return C[i]; }
    const Rect CV{ 300, 126, 640, 440 };
    void start() override { int n = (int)pl.size(); rounds = opts.get("rounds", 1) + 1; turns = n * rounds; score.assign(n, 0); int tm = opts.get("time", 1); total = tm == 0 ? 60.f : tm == 2 ? 100.f : 80.f; for (int i = 0; i < pd::DIBUJA_N; i++) deck.push_back(i); shuffle(); newWord(); }
    void shuffle() { for (int i = (int)deck.size() - 1; i > 0; i--) std::swap(deck[i], deck[rndInt(i + 1)]); }
    void newWord() { if (deck.empty()) { for (int i = 0; i < pd::DIBUJA_N; i++) deck.push_back(i); shuffle(); } word = pd::DIBUJA[deck.back()]; deck.pop_back(); }
    int turnPlayer() const override { return over ? -1 : turn; }
    std::string secret(int p) const override { return p == turn && phase == 1 ? "Dibuja:\n" + word : ""; }
    void resetCanvas() { segs.clear(); strokes.clear(); cx = CV.cx(); cy = CV.cy(); lastX = lastY = -1; drawing = false; }
    void begin() { phase = 1; timer = total; resetCanvas(); app->audio.play("click"); }
    void finishRound(int guesser) {
        int pts = 0; if (guesser >= 0) { pts = std::max(10, (int)(100 * timer / total)); score[guesser] += pts; score[turn] += 30; res = pl[guesser].name + " adivinó \"" + word + "\" (+" + std::to_string(pts) + ", dibujante +30)"; app->audio.play("win"); } else { res = "Se acabó el tiempo. Era: " + word; app->audio.play("lose"); }
        phase = 2; picking = false;
    }
    void nextTurn() { t++; if (t >= turns) { over = true; phase = 3; return; } turn = t % (int)pl.size(); newWord(); phase = 0; segs.clear(); strokes.clear(); }
    void resign(int) override { over = true; phase = 3; }
    void addSeg(float x1, float y1, float x2, float y2) { if (segs.size() < 8000) segs.push_back({ x1, y1, x2, y2, COLS(color), size == 0 ? 3.f : size == 1 ? 6.f : 12.f }); }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.f && !finished) { int best = *std::max_element(score.begin(), score.end()); std::vector<PResult> r; int nb = 0; for (int s : score) if (s == best) nb++; for (size_t i = 0; i < pl.size(); i++) r.push_back({ score[i], score[i] == best ? (nb > 1 ? 'd' : 'w') : 'l' }); finish(r, "Ganó " + [&] { std::string w; for (size_t i = 0; i < pl.size(); i++) if (score[i] == best) w += (w.empty() ? "" : " y ") + pl[i].name; return w; }() + " con " + std::to_string(best) + " puntos"); } return; }
        Pad* pd_ = padOf(in, turn);
        if (phase == 0) { if (pd_ && pd_->down[B_A]) begin(); }
        else if (phase == 1 && !picking) {
            timer -= dt; if (timer <= 0) { finishRound(-1); return; }
            if (pd_) { float sp = 260.f * dt, mx = 0, my = 0; mx += pd_->lx * sp; my += pd_->ly * sp; if (pd_->held[B_LEFT]) mx -= sp; if (pd_->held[B_RIGHT]) mx += sp; if (pd_->held[B_UP]) my -= sp; if (pd_->held[B_DOWN]) my += sp;
                float nx = std::max(CV.x, std::min(CV.x + CV.w, cx + mx)), ny = std::max(CV.y, std::min(CV.y + CV.h, cy + my)); bool pen = pd_->held[B_A];
                if (pen) { if (!drawing) { strokes.push_back(segs.size()); drawing = true; lastX = cx; lastY = cy; } if (nx != cx || ny != cy) addSeg(cx, cy, nx, ny); else addSeg(cx, cy, cx + 0.1f, cy); } else drawing = false;
                cx = nx; cy = ny; if (pd_->down[B_Y]) { color = (color + 1) % 8; } if (pd_->down[B_X]) { segs.clear(); strokes.clear(); } if (pd_->down[B_L]) size = std::max(0, size - 1); if (pd_->down[B_R]) size = std::min(2, size + 1); if (pd_->down[B_B] && !strokes.empty()) { segs.resize(strokes.back()); strokes.pop_back(); } }
            if (in.touchHeld && CV.in(in.tx, in.ty)) { if (lastX < 0) strokes.push_back(segs.size()); else addSeg(lastX, lastY, in.tx, in.ty); if (lastX < 0) addSeg(in.tx, in.ty, in.tx + 0.1f, in.ty); lastX = in.tx; lastY = in.ty; } else if (!drawing) lastX = -1;
        }
    }
    void draw(Gfx& g, UI& ui) override {
        banner(g, over ? "Fin de la partida" : phase == 0 ? "Turno de " + pl[turn].name + ": va a dibujar" : phase == 1 ? pl[turn].name + " dibuja · " + std::to_string((int)std::ceil(std::max(0.f, timer))) + " s" : phase == 2 ? "Fin de la ronda" : "");
        float pw = std::min(180.f, 940.f / pl.size()); for (size_t i = 0; i < pl.size(); i++) { Rect r{ 290 + i * (pw + 2), 64, pw - 4, 50 }; bool on = (int)i == turn && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(pl[i].name, r.x + 8, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 16); g.text(std::to_string(score[i]), r.x + 8, r.y + 36, 16, rgb(0xd8b46a), F_BOLD, AL_L); }
        g.rrect({ CV.x - 6, CV.y - 6, CV.w + 12, CV.h + 12 }, 10, rgb(0x5a3a1c)); g.rect(CV, rgb(0xfffdf6)); g.clip(&CV); for (auto& s : segs) { g.line(s.x1, s.y1, s.x2, s.y2, s.th, rgb(s.col)); g.circle(s.x2, s.y2, s.th / 2, rgb(s.col)); } g.clip(nullptr);
        if (phase == 1 && !picking) { g.ring(cx, cy, 8, 2, rgb(0xe53935)); g.circle(cx, cy, 2, rgb(0xe53935)); }
        if (phase == 0 && !over) { g.rrect({ 330, 250, 580, 160 }, 18, rgb(0x120c08, 225)); g.text(pl[turn].name + " dibuja. Los demás NO miran la palabra.", 620, 290, 18, pal::cream, F_BOLD, AL_C, 540); if (ui.button("Ver palabra y empezar", { 440, 330, 360, 56 }, 3)) { begin(); return; } ui.setFocusIfNone(hid("Ver palabra y empezar", 440 * 7 + 330)); }
        if (phase == 1) {
            bool see = canSee(ui, turn); g.rrect({ 960, 126, 290, 120 }, 14, rgb(0x120c08, 215)); g.text(see ? "Dibuja:" : phoneOf(turn) ? "Mira tu celular" : "Mantén L para ver", 1105, 150, 16, pal::cream, F_BOLD, AL_C, 270); if (see) g.textShadow(word, 1105, 195, 30, pal::gold, F_TITLE, AL_C);
            for (int i = 0; i < 8; i++) { Rect r{ 960.f + (i % 4) * 72, 262.f + (i / 4) * 52, 64, 44 }; uint32_t id = hid("dc", i); bool hit = ui.hit(id, r, !picking); g.rrect(r, 8, color == i ? pal::gold : rgb(0xffffff, 160)); g.rrect(r.inset(4), 6, rgb(COLS(i))); if (ui.focused(id)) ui.drawFocus(r, 8); if (hit) color = i; }
            for (int i = 0; i < 3; i++) if (ui.button(i == 0 ? "Fino" : i == 1 ? "Medio" : "Grueso", { 960.f + i * 98, 372, 92, 36 }, i == size ? 3 : 2, !picking)) size = i;
            if (ui.button("Deshacer", { 960, 416, 140, 38 }, 2, !strokes.empty() && !picking)) { segs.resize(strokes.back()); strokes.pop_back(); } if (ui.button("Borrar todo", { 1110, 416, 140, 38 }, 2, !picking)) { segs.clear(); strokes.clear(); }
            if (!picking) { if (ui.button("¡Adivinaron!", { 960, 466, 290, 52 }, 3)) picking = true; if (ui.button("Me rindo", { 960, 526, 290, 44 }, 2)) finishRound(-1); }
            else { g.rrect({ 330, 200, 600, 330 }, 18, rgb(0x120c08, 235)); g.text("¿Quién adivinó?", 630, 230, 22, pal::gold, F_BOLD, AL_C); int k = 0; for (size_t i = 0; i < pl.size(); i++) { if ((int)i == turn) continue; if (ui.button(pl[i].name, { 360.f + (k % 2) * 280, 260.f + (k / 2) * 56, 260, 48 }, 3)) { finishRound((int)i); return; } k++; } if (ui.button("Cancelar", { 480, 470, 300, 44 }, 2)) picking = false; ui.setFocusIfNone(hid(pl[turn == 0 ? 1 : 0].name, 360 * 7 + 260)); }
        }
        if (phase == 2) { g.rrect({ 330, 250, 580, 200 }, 18, rgb(0x120c08, 235)); g.textBox(res, { 350, 270, 540, 100 }, 20, pal::cream, F_BOLD, AL_C); if (ui.button("Siguiente", { 440, 390, 360, 52 }, 3)) { nextTurn(); return; } ui.setFocusIfNone(hid("Siguiente", 440 * 7 + 390)); }
    }
    std::vector<Hint> hints() const override { return { { "STICK", "Mover el lápiz" }, { "A", "Dibujar (mantén)" }, { "Y", "Color" }, { "X", "Borrar" }, { "B", "Deshacer" }, { "L", "Ver la palabra" } }; }
    std::string help() const override { return "Cada turno uno dibuja una palabra secreta (se ve manteniendo L o en su celular) y los demás la dicen en voz alta. Mueve el lápiz con el stick o la cruceta y mantén A para dibujar (o dibuja con el dedo en la pantalla táctil). Cuando alguien acierta, el dibujante toca ¡Adivinaron! y elige quién fue: cuanto más rápido, más puntos."; }
};

// ===================================================================== PELÍCULA CON EMOJIS
const char* PAL[] = { "😀", "😂", "😍", "😱", "😴", "🤔", "😈", "👻", "💀", "👽", "🤖", "🐶", "🐱", "🐭", "🦁", "🐯", "🐻", "🐼", "🐸", "🐵", "🐧", "🦄", "🐉", "🦈", "🐙", "🐢", "🌹", "🌲", "🌍", "⭐", "🌙", "☀️", "⛄", "🔥", "💧", "⚡", "🍎", "🍕", "🍔", "🍦", "🍰", "🍌", "🎃", "🎄", "🎁", "🎈", "🎬", "🎵", "⚽", "🚗", "✈️", "🚀", "🚢", "🚂", "🏠", "🏰", "🔑", "💎", "👑", "💰", "📚", "🔪", "💣", "❤️", "💔", "💍", "👶", "👦", "👧", "👨", "👩", "👴", "🧙", "🦸", "🧟", "👮", "👷", "🏝️", "🌊", "🌴" };
const int PN = sizeof(PAL) / sizeof(PAL[0]);
const char* FILMS[] = { "Titanic", "Tiburón", "El Rey León", "Toy Story", "Buscando a Nemo", "Harry Potter", "Frozen", "Shrek", "Spider-Man", "Batman", "Superman", "Jurassic Park", "King Kong", "Piratas del Caribe", "El Señor de los Anillos", "Star Wars", "Avatar", "Volver al Futuro", "Los Increíbles", "Cars", "Aladdín", "La Sirenita", "Mary Poppins", "Rocky", "Matrix", "Ghostbusters", "E.T.", "Gladiador", "Rápido y Furioso", "Los Vengadores", "Mi Pobre Angelito", "Kung Fu Panda", "Madagascar", "Moana", "Coco", "Intensamente", "Dumbo", "Bambi", "Pinocho", "Mulán" };
const int FN = sizeof(FILMS) / sizeof(FILMS[0]);
struct EmojiPeli : Party {
    int creator = 0, round = 1, total = 10, phase = 0, maxE = 10; std::string title; std::vector<int> seq; std::vector<int> score; bool over = false; float timer = 0, gT = 60, endT = 0; std::string res; int lastWinner = -1;
    void start() override { int n = (int)pl.size(); score.assign(n, 0); int o = opts.get("rounds", 1); total = o == 0 ? 6 : o == 2 ? 15 : 10; int g2 = opts.get("time", 1); gT = g2 == 0 ? 45.f : g2 == 2 ? 90.f : 60.f; creator = 0; phase = 0; }
    int turnPlayer() const override { return over ? -1 : phase == 3 ? creator : creator; }
    std::string secret(int p) const override { return p == creator && (phase == 1 || phase == 2) && !title.empty() ? "Tu película:\n" + title : ""; }
    void resign(int) override { over = true; }
    void nextRound(int winner) { round++; if (round > total) { over = true; return; } creator = winner >= 0 ? winner : (creator + 1) % (int)pl.size(); phase = 0; title.clear(); seq.clear(); }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 2.f && !finished) { int best = *std::max_element(score.begin(), score.end()); int nb = 0; for (int s : score) if (s == best) nb++; std::vector<PResult> r; for (size_t i = 0; i < pl.size(); i++) r.push_back({ score[i], score[i] == best ? (nb > 1 ? 'd' : 'w') : 'l' }); std::string w; for (size_t i = 0; i < pl.size(); i++) if (score[i] == best) w += (w.empty() ? "" : " y ") + pl[i].name; finish(r, "Ganó " + w + " con " + std::to_string(best) + " puntos"); } return; }
        if (phase == 2) { timer -= dt; if (timer <= 0) { res = "Nadie acertó. Era: " + title; phase = 3; lastWinner = -1; app->audio.play("lose"); } }
        (void)in;
    }
    void draw(Gfx& g, UI& ui) override {
        banner(g, over ? "Fin de la partida" : phase == 0 ? pl[creator].name + ": elige la película" : phase == 1 ? pl[creator].name + " arma la película con emojis" : phase == 2 ? "¿Qué película es? Díganlo en voz alta · " + std::to_string((int)std::ceil(std::max(0.f, timer))) + " s" : "Resultado");
        float pw = std::min(180.f, 940.f / pl.size()); for (size_t i = 0; i < pl.size(); i++) { Rect r{ 290 + i * (pw + 2), 64, pw - 4, 50 }; bool on = (int)i == creator && !over; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08)); g.text(pl[i].name, r.x + 8, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 16); g.text(std::to_string(score[i]), r.x + 8, r.y + 36, 16, rgb(0xd8b46a), F_BOLD, AL_L); }
        g.text("Ronda " + std::to_string(std::min(round, total)) + "/" + std::to_string(total), 1230, 130, 14, pal::cream, F_BODY, AL_R);
        g.rrect({ 300, 140, 930, 100 }, 16, rgb(0x120c08, 215)); for (size_t i = 0; i < seq.size(); i++) g.emoji(PAL[seq[i]], 330 + i * 86.f, 190, 64);
        if (seq.empty() && phase <= 1) g.text("Aquí aparecerán los emojis", 765, 190, 18, pal::cream, F_BODY, AL_C);
        if (phase == 0) { g.rrect({ 330, 260, 870, 280 }, 18, rgb(0x120c08, 225)); g.text("Elige una película en secreto (nadie más debe mirar)", 765, 290, 20, pal::cream, F_BOLD, AL_C, 800); if (ui.button("✎ Escribir el título", { 400, 330, 340, 60 }, 3)) { std::string s; if (app->editName(s, "Película")) { if (!s.empty()) { title = s; phase = 1; } } return; } if (ui.button("🎲 Que elija el juego", { 760, 330, 340, 60 }, 3)) { title = FILMS[rndInt(FN)]; phase = 1; return; } ui.setFocusIfNone(hid("✎ Escribir el título", 400 * 7 + 330)); }
        if (phase == 1) { bool see = canSee(ui, creator); g.text(see ? "Tu película: " + title : phoneOf(creator) ? "Tu película está en tu celular" : "Mantén L para ver el título", 765, 262, 18, pal::gold, F_BOLD, AL_C, 900);
            for (int i = 0; i < PN; i++) { Rect r{ 300.f + (i % 20) * 46, 280.f + (i / 20) * 46, 44, 44 }; uint32_t id = hid("ep", i); bool hit = ui.hit(id, r, (int)seq.size() < maxE); g.rrect(r, 8, rgb(0xfffaf0, 235)); g.emoji(PAL[i], r.cx(), r.cy(), 32); if (ui.focused(id)) ui.drawFocus(r, 8); if (hit) { seq.push_back(i); app->audio.play("click"); } }
            ui.setFocusIfNone(hid("ep", 0)); if (ui.button("Borrar último", { 330, 480, 220, 48 }, 2, !seq.empty())) seq.pop_back(); if (ui.button("¡Listo! Que adivinen", { 570, 480, 330, 48 }, 3, !seq.empty())) { phase = 2; timer = gT; app->audio.play("win"); return; } }
        if (phase == 2) { g.text("Creador: " + pl[creator].name + " · toca a quien acertó primero", 765, 270, 18, pal::cream, F_BOLD, AL_C); int k = 0; for (size_t i = 0; i < pl.size(); i++) { if ((int)i == creator) continue; if (ui.button(pl[i].name, { 330.f + (k % 3) * 300, 300.f + (k / 3) * 56, 280, 48 }, 3)) { score[i] += 50; res = pl[i].name + " acertó: " + title + " (+50)"; lastWinner = (int)i; phase = 3; app->audio.play("win"); return; } k++; } if (ui.button("Nadie acertó", { 480, 300.f + ((k + 2) / 3) * 56, 300, 48 }, 1)) { res = "Nadie acertó. Era: " + title; lastWinner = -1; phase = 3; app->audio.play("lose"); return; } }
        if (phase == 3) { g.rrect({ 330, 270, 870, 180 }, 18, rgb(0x120c08, 235)); g.textBox(res, { 350, 290, 830, 80 }, 24, pal::gold, F_BOLD, AL_C); if (ui.button("Siguiente ronda", { 540, 380, 360, 52 }, 3)) { nextRound(lastWinner); return; } ui.setFocusIfNone(hid("Siguiente ronda", 540 * 7 + 380)); }
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" }, { "L", "Ver el título" } }; }
    std::string help() const override { return "Un creador elige en secreto una película (la escribe o el juego sugiere una) y la arma con hasta 10 emojis. Los demás la dicen en voz alta; el creador toca a quien acertó primero (+50) y esa persona crea la siguiente. Si nadie acierta, sigue el próximo jugador."; }
};

// ===================================================================== ADIVINA EL PERSONAJE
struct Ch { int hair, glasses, extra; const char* name; };
const char* HN[4] = { "rubio", "moreno", "pelirrojo", "canoso" }; const uint32_t HC[4] = { 0xf2d16b, 0x4a3020, 0xd2571f, 0xcfcfd6 };
const char* CNAMES[24] = { "Ana", "Beto", "Carla", "Dani", "Eva", "Fito", "Gala", "Hugo", "Inés", "Jano", "Kira", "Leo", "Mica", "Nico", "Olga", "Pipo", "Quique", "Rita", "Sami", "Tita", "Ulises", "Vera", "Wilo", "Yaco" };
struct Q { const char* t; int kind, val; };   // kind: 0 pelo, 1 gafas, 2 extra
const Q QS[] = { { "¿Tiene el pelo rubio?", 0, 0 }, { "¿Tiene el pelo oscuro?", 0, 1 }, { "¿Es pelirrojo?", 0, 2 }, { "¿Tiene canas?", 0, 3 }, { "¿Usa gafas?", 1, 1 }, { "¿Usa sombrero?", 2, 1 }, { "¿Tiene bigote?", 2, 2 }, { "¿No lleva sombrero ni bigote?", 2, 0 } };
const int QN = sizeof(QS) / sizeof(QS[0]);
struct Personajes : Party {
    Ch chars[24]; int phase = 0, pickP = 0, turn = 0, secretCh[2] = { -1, -1 }, mode = 0; bool down[2][24] = {}; bool over = false; int winner = -1; std::string msg, lastQ; float endT = 0;
    void start() override { int k = 0; for (int h = 0; h < 4; h++) for (int gl = 0; gl < 2; gl++) for (int ex = 0; ex < 3; ex++) { chars[k] = { h, gl, ex, CNAMES[k] }; k++; } for (int p = 0; p < 2; p++) if (phoneOf(p)) secretCh[p] = rndInt(24); phase = 0; pickP = 0; skipPicked(); }
    void skipPicked() { while (pickP < 2 && secretCh[pickP] >= 0) pickP++; if (pickP >= 2) { phase = 1; turn = rndInt(2); msg = pl[turn].name + " empieza"; } }
    int turnPlayer() const override { return over ? -1 : phase == 0 ? pickP : turn; }
    std::string descOf(const Ch& c) const { return std::string(HN[c.hair]) + (c.glasses ? ", con gafas" : "") + (c.extra == 1 ? ", sombrero" : c.extra == 2 ? ", bigote" : ""); }
    std::string secret(int p) const override { if (p < 0 || p > 1 || secretCh[p] < 0) return ""; const Ch& c = chars[secretCh[p]]; std::string s = std::string("Tu personaje: ") + c.name + "\n" + descOf(c) + "\n\nTablero:"; for (int i = 0; i < 24; i++) s += std::string("\n") + (down[p][i] ? "x " : "· ") + chars[i].name + ": " + descOf(chars[i]); return s; }
    void resign(int p) override { over = true; winner = 1 - p; }
    bool match(const Ch& c, const Q& q) const { return q.kind == 0 ? c.hair == q.val : q.kind == 1 ? c.glasses == q.val : c.extra == q.val; }
    void face(Gfx& g, Rect r, int i, bool dn, bool glow) {
        const Ch& c = chars[i]; g.rrect(r, 10, glow ? pal::gold : rgb(0x8a7a50)); g.rrectGrad(r.inset(3), 8, rgb(0xfffaf0), rgb(0xe8dcc0)); float cx = r.cx(), cy = r.y + r.h * 0.46f, R = r.w * 0.3f;
        g.circle(cx, cy - R * 0.2f, R * 1.08f, rgb(HC[c.hair])); g.circle(cx, cy, R, rgb(0xf3c9a0)); g.rect({ cx - R * 1.08f, cy - R * 0.15f, R * 0.22f, R * 0.9f }, rgb(HC[c.hair])); g.rect({ cx + R * 0.86f, cy - R * 0.15f, R * 0.22f, R * 0.9f }, rgb(HC[c.hair]));
        g.circle(cx, cy - R * 0.15f, R * 0.94f, rgb(0xf3c9a0)); g.circle(cx - R * 0.35f, cy - R * 0.1f, R * 0.09f, rgb(0x222222)); g.circle(cx + R * 0.35f, cy - R * 0.1f, R * 0.09f, rgb(0x222222)); g.line(cx - R * 0.25f, cy + R * 0.45f, cx + R * 0.25f, cy + R * 0.45f, 2, rgb(0x8a2a2a));
        if (c.glasses) { g.ring(cx - R * 0.35f, cy - R * 0.1f, R * 0.24f, 2, rgb(0x222222)); g.ring(cx + R * 0.35f, cy - R * 0.1f, R * 0.24f, 2, rgb(0x222222)); g.line(cx - R * 0.11f, cy - R * 0.1f, cx + R * 0.11f, cy - R * 0.1f, 2, rgb(0x222222)); }
        if (c.extra == 1) { g.rect({ cx - R * 0.8f, cy - R * 1.15f, R * 1.6f, R * 0.18f }, rgb(0x6a3a2a)); g.rrect({ cx - R * 0.55f, cy - R * 1.6f, R * 1.1f, R * 0.5f }, 6, rgb(0x6a3a2a)); } else if (c.extra == 2) g.rrect({ cx - R * 0.4f, cy + R * 0.22f, R * 0.8f, R * 0.14f }, 4, rgb(0x2a1a10));
        g.text(c.name, cx, r.y + r.h - 10, r.w * 0.2f, rgb(0x2a1a10), F_BOLD, AL_C, r.w); if (dn) { g.rrect(r, 10, rgb(0, 150)); g.line(r.x + 8, r.y + 8, r.x + r.w - 8, r.y + r.h - 8, 4, rgb(0xe53935)); g.line(r.x + r.w - 8, r.y + 8, r.x + 8, r.y + r.h - 8, 4, rgb(0xe53935)); }
    }
    void update(float dt, Input&, UI&) override { if (over) { endT += dt; if (endT > 2.f && !finished) { std::vector<PResult> r = { { winner == 0 ? 1 : 0, winner == 0 ? 'w' : 'l' }, { winner == 1 ? 1 : 0, winner == 1 ? 'w' : 'l' } }; finish(r, "Ganó " + pl[winner].name); } } }
    void draw(Gfx& g, UI& ui) override {
        banner(g, over ? "¡Ganó " + pl[winner].name + "!" : phase == 0 ? pl[pickP].name + ": elige tu personaje secreto" : phase == 1 ? pl[turn].name + ": pregunta o adivina" : phase == 2 ? pl[turn].name + ": toca el personaje del rival" : "Respuesta");
        int who = phase == 0 ? pickP : turn; bool see = phase != 0 || canSee(ui, pickP) || phoneOf(pickP); if (phase == 0 && phoneOf(pickP)) see = false;
        if (phase == 0 && !see) { g.rrect({ 400, 260, 560, 120 }, 16, rgb(0x120c08, 225)); g.text("Mantén L para ver los personajes y elige el tuyo", 680, 320, 20, pal::cream, F_BOLD, AL_C, 520); }
        else for (int i = 0; i < 24; i++) { Rect r{ 300.f + (i % 8) * 92, 130.f + (i / 8) * 110, 86, 104 }; uint32_t id = hid("pj", i); bool enabled = phase == 0 || phase == 1 || phase == 2; bool hit = ui.hit(id, r, enabled && !over && (phase != 3)); face(g, r, i, phase != 0 && down[who][i], phase == 0 ? false : secretCh[who] == i && phase == 1 && false); if (ui.focused(id)) ui.drawFocus(r, 10);
            if (hit) { if (phase == 0) { secretCh[pickP] = i; pickP++; skipPicked(); app->audio.play("click"); return; } else if (phase == 2) { if (i == secretCh[1 - turn]) { over = true; winner = turn; msg = "¡Acertó!"; app->audio.play("win"); } else { over = true; winner = 1 - turn; msg = "Falló: era " + std::string(chars[secretCh[1 - turn]].name); app->audio.play("lose"); } return; } else down[who][i] = !down[who][i]; } }
        if (phase == 0) ui.setFocusIfNone(hid("pj", 0)); if (phase == 1 || phase == 2) ui.setFocusIfNone(hid("pj", 0));
        if (phase == 1) { g.rrect({ 300, 590, 740, 34 }, 17, rgb(0x120c08, 200)); g.text("Toca un personaje para bajarlo/subirlo · Preguntas:", 670, 607, 15, pal::cream, F_BOLD, AL_C); }
        if (phase == 1 && !over) { for (int q = 0; q < QN; q++) if (ui.button(QS[q].t, { 1050.f, 130.f + q * 52, 200, 46 }, 2)) { bool yes = match(chars[secretCh[1 - turn]], QS[q]); lastQ = std::string(QS[q].t) + "  →  " + (yes ? "SÍ" : "NO"); phase = 3; app->audio.play(yes ? "win" : "click"); return; } if (ui.button("¡Adivinar!", { 1050, 130.f + QN * 52 + 10, 200, 50 }, 1)) { phase = 2; } }
        if (phase == 3) { g.rrect({ 1040, 130, 220, 260 }, 16, rgb(0x120c08, 230)); g.textBox(lastQ, { 1050, 150, 200, 130 }, 20, pal::gold, F_BOLD, AL_C); if (ui.button("Pasar turno", { 1050, 300, 200, 56 }, 3)) { turn = 1 - turn; phase = 1; return; } ui.setFocusIfNone(hid("Pasar turno", 1050 * 7 + 300)); }
        if (phase == 2 && ui.button("Cancelar", { 1050, 130, 200, 48 }, 2)) phase = 1;
        if (!msg.empty() && over) g.text(msg, 670, 660, 20, pal::gold, F_BOLD, AL_C);
    }
    std::vector<Hint> hints() const override { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir / bajar personaje" }, { "L", "Ver (al elegir)" } }; }
    std::string help() const override { return "Cada jugador elige en secreto un personaje de los 24 (mantén L para ver). Por turnos haces UNA pregunta de sí/no sobre el personaje del rival (el juego responde solo) o arriesgas una adivinanza: si aciertas ganas, si fallas gana el otro. Toca los personajes para bajarlos (descartarlos)."; }
};
}
Game* makeDibuja() { return new Dibuja(); }
Game* makeEmojiPeli() { return new EmojiPeli(); }
Game* makePersonajes() { return new Personajes(); }
