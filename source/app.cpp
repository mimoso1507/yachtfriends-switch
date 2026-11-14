#include "app.h"
#include "qr.h"
#include <algorithm>
#ifdef __SWITCH__
#include <switch.h>
#endif

App* gApp = nullptr;
static const char* LEVELS[] = { "", "Fácil", "Normal", "Difícil" };

// ---------------------------------------------------------------- catálogo
std::vector<GameInfo>& catalog() { static std::vector<GameInfo> c; return c; }
const GameInfo* findGame(const std::string& id) { for (auto& g : catalog()) if (g.id == id) return &g; return nullptr; }

bool App::init(bool hidden, int w, int h) {
    gApp = this;
    if (!g.init(w, h, hidden)) return false;
    in.init(); audio.init(); save.load();
    audio.musicOn = save.music; audio.sfxOn = save.sfx; aiLevel = save.level;
    registerGames(); defaultOpts();
    return true;
}
void App::defaultOpts() {
    opts.v.clear(); const GameInfo* gi = findGame(gameId); if (!gi) return;
    for (auto& o : gi->options) opts.v[o.key] = o.def;
}
bool App::hostPlays() const {   // el mando 1 siempre maneja los menús; solo juega si la pantalla no es "mirón" (o si no hay nadie más)
    if (solo || !spectator) return true;
    for (int i = 1; i < Input::MAXP; i++) if (in.pads[i].connected) return false;
    return true;
}
int App::humanCount() const {
    if (solo) return 1;
    int n = 0; for (int i = 0; i < Input::MAXP; i++) if ((i == 0 && hostPlays()) || (i > 0 && in.pads[i].connected)) n++;
    return n;
}
void App::buildSeats() {
    seats.clear(); const GameInfo* gi = findGame(gameId); if (!gi) return;
    int humans = std::min(humanCount(), gi->maxP);
    int k = 1;
    for (int i = 0; i < Input::MAXP && (int)seats.size() < humans; i++) {
        if (i != 0 && !in.pads[i].connected) continue;
        if (i == 0 && !hostPlays()) continue;
        Player p; p.pad = i; p.ai = false;
        (void)k; p.name = save.prof[i].name.empty() ? "Jugador " + std::to_string(i + 1) : save.prof[i].name; p.emoji = avCode(save.prof[i].av);
        seats.push_back(p);
    }
    while ((int)seats.size() < testHumans && (int)seats.size() < gi->maxP) { Player p; p.pad = -1; p.ai = false; p.name = "J" + std::to_string(seats.size() + 1); p.emoji = "🙂"; seats.push_back(p); }   // solo pruebas
    int ais = gi->ai ? aiCount : 0;
    ais = std::max(ais, gi->minP - (int)seats.size());
    ais = std::min(ais, gi->maxP - (int)seats.size());
    if (!gi->ai) ais = 0;
    for (int i = 0; i < ais; i++) { Player p; p.ai = true; p.level = aiLevel; p.name = std::string("IA ") + BOT_NAMES[i % 8]; p.emoji = "🤖"; seats.push_back(p); }
}
void App::startGame() {
    const GameInfo* gi = findGame(gameId); if (!gi || !gi->ready) return;
    buildSeats();
    if ((int)seats.size() < gi->minP) { toast("Faltan jugadores: conectá más mandos o sumá una IA"); return; }
    game.reset(gi->make()); game->app = this; game->pl = seats; game->opts = opts; game->start();
    scr = SC_GAME; menu.clear(); paused = false; resultT = 0; confirmQuit = false; showHelp = false;
    audio.play("click");
}
void App::endGameToLobby() { dbgLog("volver a la sala"); game.reset(); scr = SC_LOBBY; menu.clear(); paused = false; confirmQuit = false; showHelp = false; }

bool App::editName(std::string& s, const char* header) {
#ifdef __SWITCH__
    SwkbdConfig kbd; if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    swkbdConfigMakePresetDefault(&kbd); swkbdConfigSetHeaderText(&kbd, header); swkbdConfigSetInitialText(&kbd, s.c_str()); swkbdConfigSetStringLenMax(&kbd, 14);
    char out[64] = {}; Result rc = swkbdShow(&kbd, out, sizeof out); swkbdClose(&kbd);
    if (R_SUCCEEDED(rc) && out[0]) { s = out; return true; }
    return false;
#else
    (void)header; static const char* N[] = { "Jugador", "Ana", "Tomás", "Papi", "Mamá", "Abu" }; static int i = 0; s = N[++i % 6]; return true;
#endif
}

// ---------------------------------------------------------------- bucle
void App::run() {
    uint32_t last = SDL_GetTicks();
#ifdef __SWITCH__
    while (appletMainLoop() && !in.quit) {
#else
    while (!in.quit) {
#endif
        uint32_t now = SDL_GetTicks(); float dt = std::min(0.05f, (now - last) / 1000.f); last = now;
        frame(dt);
    }
}
void App::frame(float dt) {
    in.net = &net; in.poll(g); updatePhones();
    renderFrame(dt); g.endFrame(); audio.update();
}
void App::renderFrame(float dt) {
    // en partida, el foco del mando lo maneja quien tiene el turno; en menús, solo el mando 1 (anfitrión)
    int pad = 0;
    if (scr == SC_GAME && game && menu.empty() && !paused && !showHelp && !confirmQuit && !game->finished) {
        int t = game->turnPlayer(); int hp = game->humanPad(t); pad = hp >= 0 ? hp : 0;
    }
    if (scr == SC_EDIT && in.pads[editSlot].connected) pad = editSlot;   // cada jugador edita con su propio mando
    if (zoomPanel) pad = 0;   // el tamaño de pantalla solo lo cambia el mando 1
    plusMinus(); gyroCursors(pad);
    // con el panel de tamaño abierto, los toques no llegan a la pantalla de abajo
    bool tD = in.touchDown, tU = in.touchUp, tH = in.touchHeld; if (zoomPanel) { in.touchDown = in.touchUp = false; in.touchHeld = false; }
    g.beginFrame(); ui.begin(g, in, pad);
    // "afectar todo": toda la pantalla se dibuja en una capa y se muestra un recorte agrandado
    bool allZoom = save.zmode == 0 && save.zoom > 100;
    if (allZoom) zoomBegin({ 0, 0, 1280, 720 }, { 0, 0, 1280, 720 }, save.zoom / 100.f);
    if (showHelp && scr != SC_GAME) { g.woodTable(); drawHelp(); }
    else switch (scr) {
    case SC_HOME: drawHome(); break;
    case SC_PROFILE: drawProfile(); break;
    case SC_EDIT: drawEdit(); break;
    case SC_QR: drawQR(); break;
    case SC_LOBBY: drawLobby(); break;
    case SC_GAME: drawGame(dt); break;
    }
    // − (menos) del mando 1: ayuda y controles, en cualquier pantalla
    if (menu.empty() && !confirmQuit && !paused && !zoomPanel && in.pads[0].down[B_MINUS]) { showHelp = !showHelp; audio.play("click"); }
    if (!boardMode() || statsOn) drawHints();
    drawToast();
    if (allZoom) zoomEnd();
    if (zoomPanel) { in.touchDown = tD; in.touchUp = tU; in.touchHeld = tH; if (tD) { ui.sx = in.tx; ui.sy = in.ty; } drawZoomPanel(); }
    drawCursors();
    ui.end();
}

// ---------------------------------------------------------------- pantalla inicial
void App::drawHome() {
    g.woodTable();
    float t = SDL_GetTicks() / 1000.f;
    g.emoji("🎲", 350, 120 + sinf(t * 2) * 4, 64);
    g.textShadow("YACHT & FRIENDS", 660, 122, 54, pal::gold, F_TITLE, AL_C);
    g.text("49 juegos de mesa y casino", 640, 178, 20, rgb(0xf5e6c8), F_BODY, AL_C);
    Rect a{ 300, 240, 320, 230 }, b{ 660, 240, 320, 230 };
    ui.setFocusIfNone(hid("card:1 JUGADOR"));
    if (ui.card("1 JUGADOR", "Contra la IA", "🕹️", a)) { solo = true; audio.play("click"); scr = SC_LOBBY; aiCount = 1; }
    if (ui.card("MULTIJUGADOR", "Un mando por jugador", "👥", b)) { solo = false; audio.play("click"); scr = SC_LOBBY; aiCount = 0; }
    g.text("Cada jugador usa un Joy-Con (en vertical), un par o un Pro Controller. El mando 1 es el anfitrión.", 640, 540, 16, rgb(0xf5e6c8, 210), F_BODY, AL_C);
    g.text("Abajo siempre ves qué botón hace cada cosa. También podés tocar la pantalla.", 640, 568, 15, rgb(0xf5e6c8, 170), F_BODY, AL_C);
    drawBar(); drawMenu();
    if (menu.empty() && ui.padDown(B_PLUS)) menu = "opts";
}

// ---------------------------------------------------------------- jugadores (tarjetas) y editor de personaje
void App::drawJoyCon(float cx, float cy, float w, int kind, bool hiTop) {
    // Joy-Con en vertical: el izquierdo tiene la palanca ARRIBA y las flechas abajo; el derecho tiene los botones A B X Y ARRIBA y la palanca abajo
    bool right = kind == 2; float bw = w * 0.5f, bh = w * 0.74f; Col body = right ? rgb(0xf0525a) : rgb(0x4fc3e0), edge = mix(body, rgb(0), 0.6f), dk = rgb(0x1b1b1f);
    float rx = right ? cx + bw / 2 : cx - bw / 2 - bw * 0.07f;   // riel del lado de afuera
    g.rrect({ rx, cy - bh * 0.46f, bw * 0.07f, bh * 0.92f }, 3, dk);
    Rect r{ cx - bw / 2, cy - bh / 2, bw, bh }; float rad = bw * 0.42f;
    g.rrect({ r.x, r.y + 3, r.w, r.h }, rad, rgb(0, 60)); g.rrect(r, rad, edge); g.rrectGrad(r.inset(3), rad - 3, mix(body, rgb(0xffffff), 0.3f), body);
    float sy = right ? cy + bh * 0.24f : cy - bh * 0.24f, by = right ? cy - bh * 0.2f : cy + bh * 0.2f, d = bw * 0.2f, br = bw * 0.105f;
    g.circle(cx, sy, bw * 0.24f, dk); g.circle(cx, sy, bw * 0.15f, rgb(0x3a3a40));
    for (int i = 0; i < 4; i++) { float px[4] = { 0, d, 0, -d }, py[4] = { -d, 0, d, 0 }; bool top = i == 0 && hiTop;
        g.circle(cx + px[i], by + py[i], br, top ? pal::gold : dk); if (top) g.ring(cx + px[i], by + py[i], br * 1.6f, 2, rgb(0xffffff, 220)); }
}
void App::drawProfile() {
    g.woodTable();
    // si hay dos Joy-Con unidos como un solo mando, abre la pantalla del sistema para separarlos (un Joy-Con por jugador)
    if (!dualAsked && SDL_GetTicks() > 1500 && in.dualPaired()) { dualAsked = true; toast("Separá los Joy-Con: uno por jugador"); in.openControllerApplet(8); return; }
    g.textShadow("¿QUIÉN JUEGA?", 640, 36, 34, pal::gold, F_TITLE, AL_C);
    float cw = 292, ch = 486, gap = 8, x0 = (1280 - (4 * cw + 3 * gap)) / 2, y0 = 66;
    // mandos: el dueño de cada casilla edita con L/R (SL/SR en un Joy-Con solo)
    for (int j = 0; j < 8; j++) if (in.pads[j].connected && save.prof[j].used && (in.pads[j].down[B_L] || in.pads[j].down[B_R])) { editSlot = j; scr = SC_EDIT; audio.play("click"); return; }
    for (int c = 0; c < 4; c++) {
        int i = plPage * 4 + c; Rect r{ x0 + c * (cw + gap), y0, cw, ch }; auto& pr = save.prof[i];
        g.rrect({ r.x, r.y + 6, r.w, r.h }, 18, rgb(0, 110));
        if (pr.used) {
            g.rrect(r, 18, rgb(0xffffff, 230)); g.rrectGrad(r.inset(3), 16, mix(PCOL[i], rgb(0xffffff), 0.25f), mix(PCOL[i], rgb(0), 0.35f));
            g.text("Jugador " + std::to_string(i + 1), r.cx(), r.y + 20, 17, pal::white, F_BOLD, AL_C);
            g.avatar(avCode(pr.av), r.cx(), r.y + 178, 250, true);
            g.textShadow(pr.name, r.cx(), r.y + 322, 32, pal::white, F_TITLE, AL_C);
            g.text(in.pads[i].connected ? (in.pads[i].kind == 1 ? "Joy-Con izquierdo" : in.pads[i].kind == 2 ? "Joy-Con derecho" : "Mando conectado") : "Sin mando conectado", r.cx(), r.y + 352, 14, rgb(0xffffff, 200), F_BODY, AL_C);
            Rect be{ r.x + 14, r.y + ch - 66, 126, 46 }, bq{ r.x + r.w - 140, r.y + ch - 66, 126, 46 };
            bool ce = ui.hit(hid("pedit", i), be); bool cq = ui.hit(hid("pdel", i), bq, i != 0);
            for (int q = 0; q < 2; q++) { Rect b = q ? bq : be; bool en = q == 0 || i != 0; Col base = mix(PCOL[i], rgb(0), 0.45f);
                g.rrect({ b.x, b.y + 3, b.w, b.h }, 18, rgb(0, 90)); g.rrect(b, 18, en ? base : mix(base, rgb(0x808080), 0.6f)); g.rrectLine(b, 18, 2, rgb(0xffffff, en ? 150 : 60));
                g.text(q ? "Quitar" : "Editar", b.cx(), b.cy(), 19, en ? pal::white : rgb(0xffffff, 110), F_BOLD, AL_C);
                if (ui.focused(hid(q ? "pdel" : "pedit", i))) ui.drawFocus(b, 18); }
            if (ce) { editSlot = i; scr = SC_EDIT; audio.play("click"); return; }
            if (cq) { pr = Save::Profile(); pr.name = "Jugador " + std::to_string(i + 1); avDefault(i, pr.av); save.store(); audio.play("click"); }
            ui.setFocusIfNone(hid("pedit", 0));
        } else {
            g.rrect(r, 18, rgb(0xffffff, 240)); g.rrectGrad(r.inset(3), 16, rgb(0xf4f7f2), rgb(0xd5e3da));
            g.text("Jugador " + std::to_string(i + 1), r.cx(), r.y + 22, 19, rgb(0x3b2414), F_BOLD, AL_C);
            bool con = in.pads[i].connected; int kd = in.pads[i].kind;
            drawJoyCon(r.cx(), r.y + 132, 212, kd == 2 ? 2 : 1, con);
            if (con) { g.text("Presiona el botón", r.cx(), r.y + 232, 20, rgb(0x2b2b2e), F_BODY, AL_C, cw - 20); g.text("de ARRIBA", r.cx(), r.y + 268, 36, rgb(0x111111), F_BOLD, AL_C, cw - 20); g.text("de tu control", r.cx(), r.y + 304, 22, rgb(0x2b2b2e), F_BODY, AL_C);
                int top = B_X;   // el de arriba: flecha ↑ del izquierdo / X del derecho (y de un mando completo)
                if (in.pads[i].down[top]) { pr.used = true; save.store(); editSlot = i; scr = SC_EDIT; audio.play("click"); return; } }
            else { g.text("Conectá otro mando", r.cx(), r.y + 250, 21, rgb(0x2b2b2e), F_BOLD, AL_C, cw - 20); g.text("o tocá «Agregar»", r.cx(), r.y + 282, 17, rgb(0x4a4a50), F_BODY, AL_C); }
            Rect ba{ r.x + 40, r.y + ch - 72, r.w - 80, 50 };
            if (ui.button("Agregar", ba, 3, true, "➕")) { pr.used = true; save.store(); editSlot = i; scr = SC_EDIT; audio.play("click"); return; }
        }
    }
    int played = 0, won = 0; for (auto& kv : save.stats) { played += kv.second[0]; won += kv.second[1]; }
    g.text("Partidas jugadas: " + std::to_string(played) + "   ·   Ganadas: " + std::to_string(won), 640, 636, 15, rgb(0xf5e6c8, 210), F_BODY, AL_C);
    if (plPage == 0) { if (ui.button("Jugadores 5-8", { 400, 572, 190, 40 }, 2, true, "▶")) { plPage = 1; audio.play("click"); ui.forceFocus(hid("pedit", 4)); } }
    else if (ui.button("Jugadores 1-4", { 400, 572, 190, 40 }, 2, true, "◀")) { plPage = 0; audio.play("click"); }
    if (ui.button("Separar / conectar Joy-Con", { 44, 572, 340, 40 }, 2, true, "🎮")) in.openControllerApplet(8);
    if (ui.button("Unir celulares (QR)", { 610, 572, 300, 40 }, 2, true, "📱")) { scr = SC_QR; audio.play("click"); }
    if (ui.button("¡Listo!", { 1010, 584, 230, 52 }, 1) || ui.back()) { scr = SC_HOME; audio.play("click"); }
    drawBar(); drawMenu();
}
void App::drawEdit() {
    g.woodTable(); auto& pr = save.prof[editSlot];
    Rect pv{ 30, 22, 400, 636 };
    g.rrect({ pv.x, pv.y + 6, pv.w, pv.h }, 20, rgb(0, 110)); g.rrect(pv, 20, rgb(0xffffff, 230)); g.rrectGrad(pv.inset(3), 18, mix(PCOL[editSlot], rgb(0xffffff), 0.25f), mix(PCOL[editSlot], rgb(0), 0.35f));
    g.text("Jugador " + std::to_string(editSlot + 1), pv.cx(), pv.y + 24, 20, pal::white, F_BOLD, AL_C);
    g.avatar(avCode(pr.av), pv.cx(), pv.y + 290, 420, true);
    g.textShadow(pr.name, pv.cx(), pv.y + pv.h - 46, 38, pal::white, F_TITLE, AL_C);
    Rect P{ 450, 22, 800, 636 };
    g.rrect({ P.x, P.y + 6, P.w, P.h }, 16, rgb(0, 100)); g.rrect(P, 16, pal::panelEdge); g.rrectGrad(P.inset(3), 14, rgb(0x4a2e18), rgb(0x2e1a0c));
    g.textShadow("TU PERSONAJE", P.x + 24, P.y + 34, 26, pal::gold, F_TITLE, AL_L);
    if (ui.button(pr.name + "  ✎", { P.x + 24, P.y + 62, P.w - 48, 50 }, 0)) { std::string s = pr.name; if (editName(s, "Tu nombre")) { pr.name = s; save.store(); } }
    int d; ui.setFocusIfNone(hid("step:" + std::string(AV_LABEL[0])));
    for (int f = 0; f < AV_N; f++) {
        int col = f < 6 ? 0 : 1, row = f < 6 ? f : f - 6; Rect r{ P.x + 16 + col * 390.f, P.y + 132 + row * 52.f, 376, 48 };
        std::string val = avName(f, pr.av[f]);
        if (ui.stepper(AV_LABEL[f], val, r, d)) { pr.av[f] = (pr.av[f] + d + AV_OPTS[f]) % AV_OPTS[f]; save.store(); audio.play("click"); }
    }
    if (ui.button("Al azar", { P.x + 16 + 390, P.y + 132 + 5 * 52, 376, 46 }, 2, true, "🎲")) { avRandom(pr.av); save.store(); audio.play("click"); }
    // cursor con el sensor de movimiento del Joy-Con: preferencia de cada jugador, solo se cambia aquí
    { bool v = pr.gyro; Rect tr{ P.x + 16, P.y + 132 + 6 * 52 + 6, 766, 48 };
      if (ui.toggle("🎯 Cursor con movimiento del Joy-Con", v, tr)) { pr.gyro = v; save.store(); audio.play("click"); if (v) { curX[editSlot] = 640; curY[editSlot] = 360; } }
      g.text(pr.gyro ? "Apuntá y moveré un cursor. A = tocar. Centrar: L + ZL (o R + ZR)." : "Apagado: te movés con la palanca o las flechas.", tr.x + 14, tr.y + tr.h + 18, 14, rgb(0xf6e7bf, 200), F_BODY, AL_L, 740); }
    if (ui.button("Listo", { P.x + P.w - 260, P.y + P.h - 76, 236, 54 }, 1) || ui.back()) { save.store(); scr = SC_PROFILE; audio.play("click"); }
    drawToast();
}

// ---------------------------------------------------------------- celulares como mandos
static std::string jesc(const std::string& s) { std::string r; for (unsigned char c : s) { if (c == '"' || c == '\\') { r += '\\'; r += (char)c; } else if (c == '\n') r += "\\n"; else if (c < 32) r += ' '; else r += (char)c; } return r; }
void App::updatePhones() {
    if (!net.running) return;
    uint32_t now = SDL_GetTicks();
    for (int ci = 0; ci < Net::MAXC; ci++) {
        Net::Client& c = net.cl[ci]; if (c.fd < 0 || !c.joined || c.slot < 0) continue;
        auto& pr = save.prof[c.slot];
        if (c.lastState.empty()) { pr.used = true; if (!c.name.empty()) pr.name = c.name; save.store(); }   // el celular elige su nombre al entrar
        std::string msg, hints = "[]", secret; bool turn = false;
        if (scr == SC_GAME && game) {
            if (game->finished) msg = "Terminó la partida. Mira la pantalla";
            else {
                int t = game->turnPlayer();
                if (t >= 0 && game->humanPad(t) == c.slot) turn = true; else if (t >= 0 && t < (int)game->pl.size()) msg = "Juega " + game->pl[t].name;
                else if (t < 0) turn = true;
                int mine = -1; for (size_t q = 0; q < game->pl.size(); q++) if (game->humanPad((int)q) == c.slot) mine = (int)q;
                if (mine >= 0) secret = game->secret(mine);
                if (turn) { if (secret.empty()) secret = game->secret(t); msg = "¡Tu turno!"; hints = "["; bool f = true; for (auto& h : game->hints()) { hints += std::string(f ? "" : ",") + "[\"" + jesc(h.k) + "\",\"" + jesc(h.label) + "\"]"; f = false; } hints += "]"; }
            }
        } else if (scr == SC_PROFILE || scr == SC_EDIT) msg = "Mira la pantalla de la Switch";
        else msg = "El anfitrión elige el juego en la Switch";
        std::string st = "{\"slot\":" + std::to_string(c.slot + 1) + ",\"name\":\"" + jesc(pr.name) + "\",\"turn\":" + (turn ? "1" : "0") + ",\"msg\":\"" + jesc(msg) + "\",\"hints\":" + hints + (secret.empty() ? "" : ",\"secret\":\"" + jesc(secret) + "\"") + "}";
        if (st != c.lastState || now - c.lastTx > 3000) { net.send(ci, st); c.lastState = st; c.lastTx = now; }
    }
}
void App::drawQR() {
    g.woodTable();
    if (!net.running && net.error.empty()) { net.start(romfs("web/")); in.net = &net; }
    g.textShadow("UNIR CELULARES", 640, 36, 34, pal::gold, F_TITLE, AL_C);
    Rect L{ 40, 70, 440, 500 }, R{ 500, 70, 740, 500 };
    g.rrect({ L.x, L.y + 6, L.w, L.h }, 18, rgb(0, 110)); g.rrect(L, 18, rgb(0xffffff)); 
    g.rrect({ R.x, R.y + 6, R.w, R.h }, 18, rgb(0, 100)); g.rrect(R, 18, pal::panelEdge); g.rrectGrad(R.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
    if (net.running) {
        auto m = qrEncode(net.url()); int n = (int)m.size();
        if (n) { float cs = std::floor(400.f / (n + 4)); float qx = L.cx() - cs * n / 2, qy = L.y + 28 + (400 - cs * n) / 2 + cs * 2;
            for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (m[y][x]) g.rect({ qx + x * cs, qy + y * cs, cs + 0.5f, cs + 0.5f }, rgb(0x111111)); }
        g.text(net.url(), L.cx(), L.y + L.h - 40, 22, rgb(0x2b2b2e), F_BOLD, AL_C, L.w - 20);
        g.text("1. Conecta el celular al mismo WiFi que la Switch", R.x + 24, R.y + 34, 18, pal::cream, F_BODY, AL_L, R.w - 40);
        g.text("2. Escanea el código QR (o escribe la dirección)", R.x + 24, R.y + 62, 18, pal::cream, F_BODY, AL_L, R.w - 40);
        g.text("3. Pon tu nombre y toca ¡Entrar!", R.x + 24, R.y + 90, 18, pal::cream, F_BODY, AL_L, R.w - 40);
        g.text("El mando se abre en pantalla completa. Hasta 8 jugadores en total.", R.x + 24, R.y + 120, 15, rgb(0xd8b46a), F_BODY, AL_L, R.w - 40);
        float y = R.y + 156;
        for (int s = 1; s < 8; s++) {
            Rect r{ R.x + 20, y, R.w - 40, 30 }; int ci = -1; for (int k = 0; k < Net::MAXC; k++) if (net.cl[k].fd >= 0 && net.cl[k].joined && net.cl[k].slot == s) ci = k;
            g.rrect(r, 8, rgb(0x1c1410)); g.circle(r.x + 16, r.cy(), 6, ci >= 0 ? rgb(0x43a047) : in.pads[s].connected ? rgb(0x29b6f6) : rgb(0x6a6a70));
            std::string t = "Jugador " + std::to_string(s + 1) + ": " + (ci >= 0 ? net.cl[ci].name + "  (celular)" : in.pads[s].connected ? "mando conectado" : "libre");
            g.text(t, r.x + 32, r.cy(), 16, ci >= 0 ? pal::white : rgb(0xf6e7bf, 170), F_BOLD, AL_L, r.w - 44); y += 34;
        }
    } else {
        g.text("No se pudo abrir la red", L.cx(), L.cy() - 20, 22, rgb(0x2b2b2e), F_BOLD, AL_C, L.w - 20);
        g.text(net.error, L.cx(), L.cy() + 16, 16, rgb(0x555555), F_BODY, AL_C, L.w - 30);
        if (ui.button("Reintentar", { R.x + 40, R.y + 40, 240, 46 }, 3)) { net.error.clear(); }
    }
    bool es = spectator;
    if (ui.button(es ? "Esta pantalla: MIRÓN" : "Esta pantalla: JUGADOR 1", { 40, 590, 440, 48 }, es ? 2 : 3, true, "📺")) { spectator = !spectator; audio.play("click"); }
    g.text(solo ? "Para jugar con celulares elige «Multijugador»" : spectator ? "La Switch solo mira; el mando 1 sigue manejando los menús" : "La Switch juega como Jugador 1", 500, 650, 14, rgb(0xf5e6c8, 190), F_BODY, AL_L, 520);
    if (ui.button("Listo", { 1010, 584, 230, 52 }, 1) || ui.back()) { scr = SC_PROFILE; audio.play("click"); }
}

// ---------------------------------------------------------------- sala
void App::drawLobby() {
    g.woodTable();
    const GameInfo* gi = findGame(gameId);
    // panel izquierdo: jugadores + opciones
    Rect L{ 16, 14, 440, 630 };
    g.rrect({ L.x, L.y + 6, L.w, L.h }, 16, rgb(0, 100)); g.rrect(L, 16, pal::panelEdge); g.rrectGrad(L.inset(3), 14, rgb(0x4a2e18), rgb(0x2e1a0c));
    g.textShadow(solo ? "1 JUGADOR" : "MULTIJUGADOR", L.x + 20, L.y + 30, 24, pal::gold, F_TITLE, AL_L);
    if (gi) { g.emoji(gi->emoji, L.x + L.w - 40, L.y + 30, 34); g.text(gi->name, L.x + L.w - 66, L.y + 30, 20, pal::cream, F_BOLD, AL_R, 200); }
    buildSeats();
    float y = L.y + 62;
    g.text("Jugadores", L.x + 20, y, 15, rgb(0xd8b46a), F_BOLD); y += 22;
    for (size_t i = 0; i < seats.size() && i < 8; i++) {
        auto& s = seats[i]; Rect r{ L.x + 16, y, L.w - 32, 30 };
        g.rrect(r, 8, rgb(0x1c1410)); g.emoji(s.emoji, r.x + 18, r.cy(), 22);
        g.text((i == 0 ? "👑 " : "") + s.name, r.x + 36, r.cy(), 16, pal::white, F_BOLD, AL_L, 220);
        g.text(s.ai ? std::string("IA · ") + LEVELS[s.level] : "Mando " + std::to_string(s.pad + 1), r.x + r.w - 10, r.cy(), 14, rgb(0xd8b46a), F_BODY, AL_R);
        y += 34;
    }
    y += 4;
    if (!solo) {
        if (ui.button("Conectar mandos", { L.x + 16, y, L.w - 32, 40 }, 2, true, "🎮")) { in.openControllerApplet(gi ? gi->maxP : 4); }
        y += 48;
    }
    int d;
    if (gi && gi->ai) {
        int mx = std::max(0, (gi ? gi->maxP : 2) - humanCount());
        if (ui.stepper("🤖 Jugadores IA", std::to_string(aiCount), { L.x + 6, y, L.w - 12, 40 }, d)) { aiCount = std::max(solo ? std::max(0, gi->minP - 1) : 0, std::min(mx, aiCount + d)); audio.play("click"); }
        y += 42;
        if (ui.stepper("🤖 Nivel de las IA", LEVELS[aiLevel], { L.x + 6, y, L.w - 12, 40 }, d)) { aiLevel = (aiLevel - 1 + d + 3) % 3 + 1; save.level = aiLevel; save.store(); audio.play("click"); }
        y += 42;
    }
    if (gi) for (auto& o : gi->options) {
        int& v = opts.v[o.key];
        if (o.check) { bool b = v != 0; if (ui.toggle(o.label, b, { L.x + 6, y, L.w - 12, 40 })) { v = b; audio.play("click"); } }
        else if (ui.stepper(o.label, o.choices[v % o.choices.size()].second, { L.x + 6, y, L.w - 12, 40 }, d)) { int n = (int)o.choices.size(); v = (v + d + n) % n; audio.play("click"); }
        y += 42; if (y > L.y + L.h - 110) break;
    }
    bool can = gi && gi->ready;
    if (ui.button(can ? "INICIAR PARTIDA" : "PRÓXIMAMENTE", { L.x + 16, L.y + L.h - 64, L.w - 32, 52 }, 1, can)) startGame();

    // panel derecho: juegos
    Rect R{ 472, 14, 792, 630 };
    g.rrect({ R.x, R.y + 6, R.w, R.h }, 16, rgb(0, 100)); g.rrect(R, 16, pal::panelEdge); g.rrectGrad(R.inset(3), 14, rgb(0x4a2e18), rgb(0x2e1a0c));
    g.textShadow("ELEGÍ UN JUEGO", R.x + 20, R.y + 30, 24, pal::gold, F_TITLE, AL_L);
    std::vector<const GameInfo*> list;
    for (auto& c : catalog()) if (!solo || c.ai || c.minP <= 1) list.push_back(&c);
    std::stable_sort(list.begin(), list.end(), [](const GameInfo* a, const GameInfo* b) { return a->ready > b->ready; });
    int cols = 5; float cw = 142, ch = 128, gx = 10, gy = 12, x0 = R.x + 22, y0 = R.y + 62, viewH = R.h - 74;
    int rows = ((int)list.size() + cols - 1) / cols; float maxScroll = std::max(0.f, rows * (ch + gy) - viewH);
    // que la tarjeta con foco quede a la vista
    for (size_t i = 0; i < list.size(); i++) if (ui.focus == hid("card:" + list[i]->name) && ui.padNav) {
        float top = (i / cols) * (ch + gy); if (top < lobbyScroll) lobbyScroll = (int)top; if (top + ch > lobbyScroll + viewH) lobbyScroll = (int)(top + ch - viewH);
    }
    static float dragY = -1; static int dragStart = 0;
    if (in.touchDown && R.in(in.tx, in.ty)) { dragY = in.ty; dragStart = lobbyScroll; }
    if (in.touchHeld && dragY >= 0) lobbyScroll = dragStart + (int)(dragY - in.ty);
    if (!in.touchHeld) dragY = -1;
    lobbyScroll = (int)std::max(0.f, std::min(maxScroll, (float)lobbyScroll));
    Rect clipR{ R.x + 6, y0 - 6, R.w - 12, viewH + 4 }; g.clip(&clipR);
    for (size_t i = 0; i < list.size(); i++) {
        auto* c = list[i]; Rect r{ x0 + (i % cols) * (cw + gx), y0 + (i / cols) * (ch + gy) - lobbyScroll, cw, ch };
        if (r.y + r.h < y0 - 10 || r.y > y0 + viewH + 10) { ui.cur.push_back({ hid("card:" + c->name), r, true }); continue; }
        std::string sub = c->ready ? (std::to_string(c->minP) + (c->maxP > c->minP ? "-" + std::to_string(c->maxP) : "") + " jugadores") : "Próximamente";
        if (ui.card(c->name, sub, c->emoji, r, true, c->id == gameId)) {
            audio.play("click");
            if (c->ready) { gameId = c->id; defaultOpts(); aiCount = solo ? std::max(1, std::min(aiCount, c->maxP - 1)) : std::min(aiCount, std::max(0, c->maxP - humanCount())); if (solo && c->minP <= 1 && c->id == "laberinto") aiCount = std::min(aiCount, 3); }
            else toast(c->name + " llega en la próxima tanda");
        }
    }
    g.clip(nullptr);
    if (maxScroll > 0) { float h = viewH * viewH / (viewH + maxScroll); g.rrect({ R.x + R.w - 12, y0 + (viewH - h) * lobbyScroll / maxScroll, 6, h }, 3, rgb(0xd8b46a, 160)); }
    ui.setFocusIfNone(hid("card:" + (gi ? gi->name : std::string(""))));
    if (ui.back() && menu.empty()) { scr = SC_HOME; audio.play("click"); }
    if (menu.empty() && ui.padDown(B_PLUS)) startGame();
    drawBar(); drawMenu();
}

// ---------------------------------------------------------------- partida
void App::drawPlates() {
    if (!game) return;
    const GameInfo* gi = findGame(gameId);
    if (gi) { g.emoji(gi->emoji, 34, 34, 30); g.textShadow(gi->name, 56, 34, 20, pal::gold, F_TITLE, AL_L); }
    int t = game->turnPlayer(); float y = 64;
    for (size_t i = 0; i < game->pl.size(); i++) {
        auto& p = game->pl[i]; Rect r{ 16, y, 230, 40 }; bool on = (int)i == t && !game->finished;
        g.rrect({ r.x, r.y + 3, r.w, r.h }, 10, rgb(0, 100)); g.rrect(r, 10, on ? pal::gold : pal::plateEdge); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
        g.circle(r.x + 10, r.cy(), 4, PCOL[i % 8]);
        g.emoji(p.emoji, r.x + 30, r.cy(), 24);
        g.text(p.name, r.x + 48, r.cy(), 16, on ? pal::gold : pal::white, F_BOLD, AL_L, 140);
        if (!p.ai && !solo) g.text("🎮" + std::to_string(p.pad + 1), r.x + r.w - 8, r.cy(), 12, rgb(0xd8b46a), F_BODY, AL_R);
        y += 46;
    }
}
void App::drawGame(float dt) {
    g.woodTable();
    if (!game) { scr = SC_LOBBY; return; }
    bool overlay = !menu.empty() || paused || showHelp || confirmQuit || zoomPanel;
    bool board = save.zmode == 1;
    if (board) {   // "solo tablero": el área del juego se agranda sola; placas y botones se ven con + / −
        Rect bounds{ 262, 0, 1018, 720 }; float k = std::min(1280.f / bounds.w, 720.f / bounds.h);
        zoomBegin(bounds, { 640 - bounds.w * k / 2, 360 - bounds.h * k / 2, bounds.w * k, bounds.h * k }, save.zoom / 100.f);
        g.woodTable();
    }
    if (!overlay) game->update(dt * speed, in, ui);
    game->draw(g, ui);
    if (board) zoomEnd();
    if (!board || statsOn) {
        if (board) { g.rrect({ 6, 6, 252, 66.f + 46 * game->pl.size() + 48 }, 14, rgb(0x120c08, 215)); g.rrectLine({ 6, 6, 252, 66.f + 46 * game->pl.size() + 48 }, 14, 2, rgb(0xd8b46a, 160)); }
        drawPlates();
        // botones fijos a la izquierda (debajo de las placas)
        float y = 70 + 46 * game->pl.size();
        if (!game->finished) {
            if (ui.button("Rendirse", { 16, y, 230, 38 }, 2, true, "🏳️")) confirmQuit = true;
        }
    }
    if (game->finished) { resultT += dt; if (resultT > 0.9f) drawResults(); }
    if (paused) {
        g.rect({ 0, 0, 1280, 720 }, rgb(0, 150)); g.textShadow("PAUSA", 640, 300, 60, pal::gold, F_TITLE, AL_C);
        if (ui.button("Seguir jugando", { 520, 380, 240, 54 }, 1) || ui.back() || ui.padDown(B_PLUS)) paused = false;
    }
    if (confirmQuit) {
        g.rect({ 0, 0, 1280, 720 }, rgb(0, 150));
        Rect p{ 390, 230, 500, 240 }; g.rrect(p, 18, pal::panelEdge); g.rrectGrad(p.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
        g.textShadow("¿Terminar la partida?", 640, 290, 28, pal::gold, F_TITLE, AL_C);
        g.text("Se cuenta como abandono.", 640, 330, 17, pal::cream, F_BODY, AL_C);
        if (ui.button("Seguir", { 420, 380, 200, 52 }, 0)) confirmQuit = false;
        if (ui.button("Volver a la sala", { 660, 380, 200, 52 }, 1)) { save.stats[gameId].resize(2); save.stats[gameId][0]++; save.store(); endGameToLobby(); return; }
        if (ui.back()) confirmQuit = false;
        ui.setFocusIfNone(hid("Seguir", (int)(420 * 7 + 380)));
    }
    if (showHelp) drawHelp();
    if (!board || statsOn || !menu.empty()) drawBar();
    drawMenu();
    // atajos del anfitrión
    if (!overlay && !game->finished && in.pads[0].down[B_PLUS]) { menu = "acts"; ui.activePad = 0; }
}
void App::drawResults() {
    g.rect({ 0, 0, 1280, 720 }, rgb(0, 140));
    Rect p{ 360, 110, 560, 480 }; g.rrect({ p.x, p.y + 8, p.w, p.h }, 20, rgb(0, 120)); g.rrect(p, 20, pal::gold); g.rrectGrad(p.inset(3), 18, rgb(0x4a2e18), rgb(0x2e1a0c));
    int me = -1; for (size_t i = 0; i < game->pl.size(); i++) if (!game->pl[i].ai && game->pl[i].pad == 0) me = (int)i;
    bool iWon = me >= 0 && me < (int)game->results.size() && game->results[me].r == 'w';
    static Game* counted = nullptr;
    if (counted != game.get()) { dbgLog("resultados: " + gameId + " jugadores=" + std::to_string(game->pl.size()) + " resultados=" + std::to_string(game->results.size())); }
    if (counted != game.get()) { counted = game.get(); auto& st = save.stats[gameId]; st.resize(2); st[0]++; if (iWon) st[1]++; save.store(); audio.play(iWon ? "win" : (me >= 0 && me < (int)game->results.size() && game->results[me].r == 'd') ? "place" : "lose"); }
    g.emoji(iWon ? "🏆" : "🎲", 640, 170, 70);
    g.textShadow(iWon ? "¡GANASTE!" : "FIN DE LA PARTIDA", 640, 236, 36, pal::gold, F_TITLE, AL_C);
    g.text(game->summary, 640, 274, 17, pal::cream, F_BODY, AL_C, 520);
    std::vector<int> ord; for (size_t i = 0; i < game->pl.size(); i++) ord.push_back((int)i);
    auto pts = [&](int i) { return i >= 0 && i < (int)game->results.size() ? game->results[i].pts : 0; };   // sin salirse del arreglo si un juego devuelve menos resultados que jugadores
    auto rank = [&](int i) { char r = i < (int)game->results.size() ? game->results[i].r : 'l'; return r == 'w' ? 0 : r == 'd' ? 1 : r == 'l' ? 2 : 3; };
    std::stable_sort(ord.begin(), ord.end(), [&](int a, int b) { if (rank(a) != rank(b)) return rank(a) < rank(b); return pts(a) > pts(b); });
    float y = 312; const char* med[] = { "🥇", "🥈", "🥉" };
    for (size_t k = 0; k < ord.size() && k < 6; k++) {
        int i = ord[k]; Rect r{ 400, y, 480, 36 }; g.rrect(r, 8, rgb(0x1c1410));
        if (k < 3 && rank(i) < 2) g.emoji(med[k], r.x + 20, r.cy(), 24);
        g.emoji(game->pl[i].emoji, r.x + 52, r.cy(), 24); g.text(game->pl[i].name, r.x + 72, r.cy(), 17, pal::white, F_BOLD, AL_L, 260);
        g.text(std::to_string(pts(i)) + " pts", r.x + r.w - 12, r.cy(), 17, pal::gold, F_BOLD, AL_R);
        y += 42;
    }
    { static Game* logged = nullptr; if (logged != game.get()) { logged = game.get(); dbgLog("resultados dibujados"); } }
    if (ui.button("Otra partida", { 400, 522, 230, 52 }, 1)) { startGame(); return; }
    if (ui.button("Volver a la sala", { 650, 522, 230, 52 }, 0) || ui.back()) { endGameToLobby(); return; }
    ui.setFocusIfNone(hid("Otra partida", (int)(400 * 7 + 522)));
}
void App::drawHelp() {
    g.rect({ 0, 0, 1280, 720 }, rgb(0, 150));
    Rect p{ 250, 70, 780, 560 }; g.rrect(p, 18, pal::panelEdge); g.rrectGrad(p.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
    const GameInfo* gi = findGame(gameId);
    g.textShadow(scr == SC_GAME && gi ? ("CÓMO SE JUEGA: " + gi->name) : std::string("AYUDA"), 640, 112, 28, pal::gold, F_TITLE, AL_C);
    std::string t = (scr == SC_GAME && game) ? game->help() + "\n\n" : "";
    t += "Mandos: un Joy-Con por jugador (en vertical, el juego detecta si es el izquierdo o el derecho y adapta las guías), un par de Joy-Con o un Pro Controller. El mando 1 es el anfitrión: maneja los menús, elige el juego y las opciones.\n"
         "Flechas o palanca izquierda: moverte · A: elegir · B: volver o cerrar · ✚: opciones en el inicio, iniciar partida en la sala y menú durante la partida · −: esta ayuda. "
         "Abajo de la pantalla (o a la izquierda durante la partida) siempre aparece qué botón hace cada cosa.\n"
         "También podés jugar tocando la pantalla en modo portátil.\n\nEmojis: Twemoji (CC-BY 4.0). Sonidos y fuentes: Kenney (CC0). Música: Pixabay.";
    g.textBox(t, { p.x + 34, p.y + 80, p.w - 68, p.h - 170 }, 18, pal::cream);
    if (ui.button("Entendido", { 540, p.y + p.h - 76, 200, 52 }, 1) || ui.back()) showHelp = false;
    ui.setFocusIfNone(hid("Entendido", (int)(540 * 7 + p.y + p.h - 76)));
}
void App::drawToast() {
    if (SDL_GetTicks() > toastUntil || toastMsg.empty()) return;
    float w = g.textW(toastMsg, 18, F_BOLD) + 40; Rect r{ 640 - w / 2, 600, w, 44 };
    g.rrect(r, 22, rgb(0x1c1410, 235)); g.rrectLine(r, 22, 2, pal::gold); g.text(toastMsg, 640, r.cy(), 18, pal::white, F_BOLD, AL_C);
}

// ---------------------------------------------------------------- guía de botones del Joy-Con
// Dibuja el botón con la forma del Joy-Con: A B X Y redondos oscuros, ✚ −, flechas (4 botones en rombo), palanca, L R ZL ZR.
// Con un solo Joy-Con (en vertical) los botones lógicos cambian de lugar físico.
std::string App::mapKey(const std::string& k) const {
    if (!hk && k.find("LS") == std::string::npos && k.find("ROLL") == std::string::npos) return k;
    std::string out, tok; std::vector<std::string> seen;
    auto flush = [&] {
        if (tok.empty()) return;
        std::string m = tok;
        if (tok == "ROLL") m = hk > 2 ? (hk == 3 ? "ZL" : "ZR") : "ZL/ZR";          // tirar: ZL (Joy-Con izq.) o ZR (der.)
        else if (tok == "LS") m = hk > 2 ? (hk == 3 ? "L" : "R") : "L/R";           // cambiar de lado: L (izq.) o R (der.)
        else if (!hk) m = tok;
        else if (tok == "DPAD" || tok == "STICK" || tok == "LR" || tok == "UD") m = tok == "LR" ? "STICKLR" : tok == "UD" ? "STICKUD" : "STICK";
        else if (tok == "L") m = hk > 2 ? (hk == 3 ? "L" : "R") : "SL"; else if (tok == "R") m = hk > 2 ? (hk == 3 ? "ZL" : "ZR") : "SR";
        else if (tok == "+") m = (hk == 1 || hk == 3) ? "-" : "+";
        else if (tok == "-") m = hk > 2 ? "STICKC" : hk == 1 ? "ZL" : "ZR";
        else if (tok == "A" || tok == "B" || tok == "X" || tok == "Y") m = "F" + tok;
        if (std::find(seen.begin(), seen.end(), m) == seen.end()) { seen.push_back(m); out += (out.empty() ? "" : "/") + m; }
        tok.clear();
    };
    for (char c : k) { if (c == '/') flush(); else tok += c; }
    flush(); return out;
}
float App::glyph(const std::string& k0, float x, float cy, float s, bool draw) {
    std::string k = k0;
    if (hk && k.rfind("F", 0) != 0 && k.rfind("STICK", 0) != 0 && k != "SL" && k != "SR") k = mapKey(k0);
    if (k.size() > 1 && k[0] == 'F' && (k[1] == 'A' || k[1] == 'B' || k[1] == 'X' || k[1] == 'Y') && k.size() == 2) {
        // rombo de 4 botones con el activo encendido; posición según cómo se sostiene el Joy-Con de costado
        char c = k[1]; int pos;   // 0 arriba,1 derecha,2 abajo,3 izquierda
        if (hk == 1 || hk == 3 || hk == 4) pos = c == 'A' ? 1 : c == 'B' ? 2 : c == 'X' ? 0 : 3;
        else pos = c == 'A' ? 2 : c == 'B' ? 3 : c == 'X' ? 1 : 0;
        float w = s * 1.15f, rr = w / 2, d = rr * 0.46f, br = rr * 0.34f; Col dark = rgb(0x2b2b2e), edge = rgb(0xd9d9d9);
        if (draw) {
            float px[4] = { 0, d, 0, -d }, py[4] = { -d, 0, d, 0 };
            for (int i = 0; i < 4; i++) {
                float bx = x + rr + px[i], by = cy + py[i];
                if (i == pos) { g.circle(bx, by, br * 1.6f, dark); g.ring(bx, by, br * 1.6f, 1.5f, edge); g.text(std::string(1, c), bx, by, s * 0.5f, pal::white, F_BOLD, AL_C); }
                else { g.circle(bx, by, br * 0.55f, rgb(0x2b2b2e, 70)); g.ring(bx, by, br * 0.55f, 1, rgb(0xd9d9d9, 70)); }
            }
        }
        return w;
    }
    if (k == "STICKLR" || k == "STICKUD") {
        float w = glyph("STICK", x, cy, s, draw);
        if (draw) { float a = s * 0.16f, e = s * 0.78f, cx = x + s / 2; Col c = pal::white;
            if (k == "STICKLR") { g.tri(cx - e, cy - a, cx - e, cy + a, cx - e - a * 1.4f, cy, c); g.tri(cx + e, cy - a, cx + e, cy + a, cx + e + a * 1.4f, cy, c); }
            else { g.tri(cx - a, cy - e, cx + a, cy - e, cx, cy - e - a * 1.4f, c); g.tri(cx - a, cy + e, cx + a, cy + e, cx, cy + e + a * 1.4f, c); } }
        return w + s * 0.5f;
    }
    if (k == "STICKC") {   // clic de la palanca
        float w = glyph("STICK", x, cy, s, draw);
        if (draw) { g.circle(x + s / 2, cy, s * 0.2f, rgb(0xffffff)); }
        return w;
    }
    if (k == "SL" || k == "SR") {
        float w = s * 1.2f;
        if (draw) { Rect b{ x, cy - s * 0.32f, w, s * 0.64f }; g.rrect(b, 5, rgb(0x2b2b2e)); g.rrectLine(b, 5, 1.5f, rgb(0xd9d9d9)); g.text(k, b.cx(), cy, s * 0.5f, pal::white, F_BOLD, AL_C); }
        return w;
    }
    auto sep = k.find('/');
    if (sep != std::string::npos) {
        int sv = hk; hk = 0; struct R { int& r; int v; ~R() { r = v; } } rs{ hk, sv };
        float w = glyph(k.substr(0, sep), x, cy, s, draw); float sw = g.textW("/", s * 0.6f, F_BOLD) + 6;
        if (draw) g.text("/", x + w + sw / 2, cy, s * 0.6f, rgb(0xf5e6c8, 180), F_BOLD, AL_C);
        return w + sw + glyph(k.substr(sep + 1), x + w + sw, cy, s, draw);
    }
    Col dark = rgb(0x2b2b2e), edge = rgb(0xd9d9d9), white = pal::white; float r = s / 2;
    if (k == "A" || k == "B" || k == "X" || k == "Y") {
        if (draw) { g.circle(x + r, cy + 1.5f, r, rgb(0, 120)); g.circle(x + r, cy, r, dark); g.ring(x + r, cy, r, 1.5f, edge); g.text(k, x + r, cy, s * 0.62f, white, F_BOLD, AL_C); }
        return s;
    }
    if (k == "+" || k == "-") {
        if (draw) {
            g.circle(x + r, cy, r * 0.82f, dark); g.ring(x + r, cy, r * 0.82f, 1.5f, edge);
            g.line(x + r - r * 0.42f, cy, x + r + r * 0.42f, cy, 2.5f, white);
            if (k == "+") g.line(x + r, cy - r * 0.42f, x + r, cy + r * 0.42f, 2.5f, white);
        }
        return s;
    }
    if (k == "DPAD" || k == "LR" || k == "UD") {
        if (draw) {
            float d = r * 0.62f, br = r * 0.36f;
            float px[4] = { 0, d, 0, -d }, py[4] = { -d, 0, d, 0 };   // arriba, derecha, abajo, izquierda
            for (int i = 0; i < 4; i++) {
                bool on = k == "DPAD" || (k == "LR" && (i == 1 || i == 3)) || (k == "UD" && (i == 0 || i == 2));
                g.circle(x + r + px[i], cy + py[i], br, on ? dark : rgb(0x2b2b2e, 90)); g.ring(x + r + px[i], cy + py[i], br, 1.2f, on ? edge : rgb(0xd9d9d9, 80));
                if (on) { float cx = x + r + px[i], cyy = cy + py[i], a = br * 0.5f;
                    if (i == 0) g.tri(cx - a, cyy + a * 0.6f, cx + a, cyy + a * 0.6f, cx, cyy - a * 0.8f, white);
                    if (i == 2) g.tri(cx - a, cyy - a * 0.6f, cx + a, cyy - a * 0.6f, cx, cyy + a * 0.8f, white);
                    if (i == 1) g.tri(cx - a * 0.6f, cyy - a, cx - a * 0.6f, cyy + a, cx + a * 0.8f, cyy, white);
                    if (i == 3) g.tri(cx + a * 0.6f, cyy - a, cx + a * 0.6f, cyy + a, cx - a * 0.8f, cyy, white); }
            }
        }
        return s;
    }
    if (k == "STICK") {
        if (draw) { g.circle(x + r, cy + 1.5f, r, rgb(0, 120)); g.circle(x + r, cy, r, rgb(0x1a1a1c)); g.ring(x + r, cy, r, 1.5f, edge); g.circle(x + r, cy, r * 0.55f, rgb(0x4a4a50)); g.ring(x + r, cy, r * 0.55f, 1, rgb(0x8a8a90)); }
        return s;
    }
    if (k == "TOUCH") { if (draw) g.emoji("👆", x + r, cy, s); return s; }
    // L R ZL ZR
    float w = std::max(s * 1.25f, g.textW(k, s * 0.55f, F_BOLD) + 12);
    if (draw) { Rect b{ x, cy - r * 0.8f, w, s * 0.8f }; g.rrect({ b.x, b.y + 1.5f, b.w, b.h }, b.h / 2, rgb(0, 120)); g.rrect(b, b.h / 2, dark); g.rrectLine(b, b.h / 2, 1.5f, edge); g.text(k, b.cx(), cy, s * 0.55f, white, F_BOLD, AL_C); }
    return w;
}
void App::drawHints() {
    { auto& ap = in.pads[std::min(std::max(ui.activePad, 0), Input::MAXP - 1)]; hk = ap.kind ? ap.kind + (ap.vert ? 2 : 0) : 0; }
    std::vector<Hint> h; bool side = false;   // side: columna izquierda durante la partida (abajo puede estar el tablero)
    const std::vector<Hint> nav{ { "DPAD/STICK", "Moverse" }, { "A", "Elegir" } };
    if (zoomPanel) { h = { { "DPAD/STICK", "Moverse" }, { "LR", "Tamaño" }, { "A", "Marcar" }, { "B", "Listo" } }; }
    else if (showHelp) { h = { { "A", "Entendido" }, { "B", "Cerrar" } }; side = scr == SC_GAME; }
    else if (!menu.empty()) { h = nav; h.push_back({ "B", "Cerrar menú" }); side = scr == SC_GAME; }
    else if (scr == SC_HOME) { h = nav; h.push_back({ "+", "Opciones" }); h.push_back({ "-", "Ayuda" }); h.push_back({ "TOUCH", "Tocar la pantalla" }); }
    else if (scr == SC_PROFILE) { h = { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" }, { "L/R", "Editar mi personaje" }, { "B", "Volver" }, { "-", "Ayuda" } }; }
    else if (scr == SC_QR) { h = { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" }, { "B", "Volver" } }; }
    else if (scr == SC_EDIT) { h = { { "DPAD/STICK", "Moverse" }, { "LR", "Cambiar" }, { "A", "Elegir" }, { "B", "Listo" } }; }
    else if (scr == SC_LOBBY) { h = nav; h.push_back({ "LR", "Cambiar opción" }); h.push_back({ "+", "Iniciar partida" }); h.push_back({ "B", "Volver" }); h.push_back({ "-", "Ayuda" }); }
    else if (scr == SC_GAME && game) {
        side = true;
        if (confirmQuit) { h = nav; h.push_back({ "B", "Seguir jugando" }); }
        else if (paused) h = { { "A/B/+", "Seguir jugando" } };
        else if (game->finished) { if (resultT > 0.9f) { h = nav; h.push_back({ "B", "Volver a la sala" }); } }
        else {
            int t = game->turnPlayer();
            if (t >= 0 && t < (int)game->pl.size() && game->pl[t].ai) h.push_back({ "", "Juega " + game->pl[t].name + "…" });
            else for (auto& x : game->hints()) h.push_back(x);
            if (save.zmode == 1) { h.push_back({ "+", "Estadísticas (mantener: menú)" }); }
            else { h.push_back({ "+", "Menú de la partida" }); h.push_back({ "-", "Cómo se juega" }); }
        }
    }
    if (h.empty()) return;
    float s = 26;
    if (side) {
        float rowH = 30, y = 650 - h.size() * rowH;
        float minY = 70 + 46 * game->pl.size() + 48; if (y < minY) y = minY;
        Rect bg{ 12, y - 8, 238, h.size() * rowH + 10 };
        g.rrect(bg, 12, rgb(0x120c08, 190)); g.rrectLine(bg, 12, 1.5f, rgb(0xd8b46a, 110));
        for (auto& x : h) {
            float cy = y + rowH / 2 - 3;
            if (x.k.empty()) { g.text(x.label, 24, cy, 15, rgb(0xd8b46a), F_BOLD, AL_L, 216); y += rowH; continue; }
            float w = glyph(x.k, 22, cy, s);
            g.text(x.label, 22 + w + 8, cy, 15, pal::cream, F_BOLD, AL_L, 238 - w - 20);
            y += rowH;
        }
        return;
    }
    // tira de abajo, a la derecha de la barra morada
    float fs = 16, gap = 22, total = 0;
    for (int pass = 0; pass < 2; pass++) {
        total = 0; for (auto& x : h) total += (x.k.empty() ? 0 : glyph(x.k, 0, 0, s, false) + 7) + g.textW(x.label, fs, F_BOLD) + gap;
        total -= gap; if (total <= 860) break; fs = 13; gap = 14;
    }
    Rect bg{ 384, 670, std::min(888.f, total + 28), 42 };
    g.rrect(bg, 21, rgb(0x120c08, 190)); g.rrectLine(bg, 21, 1.5f, rgb(0xd8b46a, 110));
    float x = bg.x + 14, cy = bg.cy();
    for (auto& it : h) {
        if (!it.k.empty()) x += glyph(it.k, x, cy, s) + 7;
        x += g.text(it.label, x, cy, fs, pal::cream, F_BOLD, AL_L) + gap;
    }
}

// ---------------------------------------------------------------- barra morada y menús
void App::drawBar() {
    Rect b{ 0, 662, 372, 58 };
    g.rrectGrad({ b.x - 20, b.y, b.w + 20, b.h + 20 }, 16, rgb(0x8a5ad6), rgb(0x4a2390));
    g.rrectLine({ b.x - 20, b.y, b.w + 20, b.h + 20 }, 16, 2, rgb(0xc9a8ff, 160));
    const char* K[] = { "prof", "games", "opts", "acts", "help" }; const char* I[] = { "👤", "🏆", "⚙️", "⚡", "❓" }; const char* N[] = { "PERFIL", "JUEGOS", "OPCIONES", "ACCIONES", "AYUDA" };
    bool hostOnly = ui.activePad != 0;   // durante el turno de otro mando, la barra solo responde al toque
    for (int i = 0; i < 5; i++) {
        Rect r{ 6.f + i * 72, b.y + 3, 68, 52 }; uint32_t id = hid(std::string("bar:") + K[i]); bool c;
        if (hostOnly) { c = in.touchUp && r.in(in.tx, in.ty); }
        else c = ui.hit(id, r);
        if (menu == K[i]) g.rrect(r, 8, rgb(0xffffff, 50));
        g.emoji(I[i], r.cx(), r.y + 19, 26); g.text(N[i], r.cx(), r.y + 43, 11, pal::white, F_TITLE, AL_C);
        if (!hostOnly && ui.focused(id)) ui.drawFocus(r, 8);
        if (!c) continue;
        audio.play("click"); std::string k = K[i];
        if (menu == k) { menu.clear(); continue; }
        if (k == "prof") { if (scr == SC_GAME) toast("El perfil se edita fuera de una partida"); else scr = SC_PROFILE; menu.clear(); }
        else if (k == "games") { menu.clear(); if (scr == SC_GAME && game && !game->finished) confirmQuit = true; else if (scr == SC_GAME) endGameToLobby(); else scr = SC_LOBBY; }
        else menu = k;
    }
}
void App::drawMenu() {
    if (menu.empty()) return;
    struct It { std::string icon, label; std::function<void()> f; bool en; };
    std::vector<It> items; float x = 6;
    bool inGame = scr == SC_GAME && game && !game->finished;
    if (menu == "opts") {
        x = 150;
        items.push_back({ audio.musicOn ? "🎵" : "🔇", std::string("Música de fondo: ") + (audio.musicOn ? "sí" : "no"), [this] { audio.setMusic(!audio.musicOn); save.music = audio.musicOn; save.store(); }, true });
        items.push_back({ "🔊", std::string("Sonidos: ") + (audio.sfxOn ? "sí" : "no"), [this] { audio.sfxOn = !audio.sfxOn; save.sfx = audio.sfxOn; save.store(); }, true });
        items.push_back({ "🔍", "Tamaño de pantalla: " + std::to_string(save.zoom) + "% · " + (save.zmode ? "solo tablero" : "todo"), [this] { zoomPanel = true; ui.forceFocus(hid("zoom:step")); }, true });
        items.push_back({ "🎮", "Conectar mandos (un Joy-Con c/u)", [this] { in.openControllerApplet(8); }, !inGame });
        items.push_back({ "📱", "Unir celulares (QR)", [this] { scr = SC_QR; menu.clear(); }, !inGame });
    } else if (menu == "acts") {
        x = 222;
        items.push_back({ "⏸️", "Pausa", [this] { paused = true; }, inGame });
        items.push_back({ "🏳️", "Rendirse", [this] { confirmQuit = true; }, inGame });
        items.push_back({ "↩️", "Volver a la sala", [this] { if (scr == SC_GAME && game && !game->finished) confirmQuit = true; else if (scr == SC_GAME) endGameToLobby(); else scr = SC_LOBBY; }, scr != SC_LOBBY });
        items.push_back({ "🏠", "Pantalla inicial", [this] { if (scr == SC_GAME) return; scr = SC_HOME; }, scr != SC_GAME && scr != SC_HOME });
    } else if (menu == "help") {
        x = 294;
        items.push_back({ "📖", scr == SC_GAME ? "Cómo se juega" : "Controles", [this] { showHelp = true; }, true });
        items.push_back({ "🏆", "Mis estadísticas", [this] { if (scr != SC_GAME) scr = SC_PROFILE; else toast("Fuera de la partida, en Perfil"); }, true });
    }
    float h = 50.f * items.size() + 12, w = 340; x = std::min(x, 1270 - w);
    Rect p{ x, 656 - h, w, h };
    g.rrect({ p.x, p.y + 5, p.w, p.h }, 14, rgb(0, 120)); g.rrect(p, 14, rgb(0xc9a8ff)); g.rrectGrad(p.inset(2), 12, rgb(0xfff8e6), rgb(0xf0e0b8));
    ui.activePad = 0;
    for (size_t i = 0; i < items.size(); i++) {
        Rect r{ p.x + 6, p.y + 6 + i * 50.f, p.w - 12, 46 }; uint32_t id = hid("menu:" + menu, (int)i);
        bool c = ui.hit(id, r, items[i].en);
        g.emoji(items[i].icon, r.x + 24, r.cy(), 26, items[i].en ? 255 : 110);
        g.text(items[i].label, r.x + 48, r.cy(), 17, items[i].en ? pal::brownText : rgb(0x9a8a70), F_BOLD, AL_L, r.w - 56);
        if (ui.focused(id)) ui.drawFocus(r, 8);
        if (c) { audio.play("click"); std::string m = menu; menu.clear(); items[i].f(); return; }
        if (i == 0) ui.setFocusIfNone(id);
    }
    if (ui.padDown(B_B) || (in.touchUp && !p.in(in.tx, in.ty) && !Rect{ 0, 662, 372, 58 }.in(in.tx, in.ty))) menu.clear();
}

// ---------------------------------------------------------------- arranque
#ifndef YF_NO_MAIN
int main(int argc, char** argv) {
#ifdef __SWITCH__
    romfsInit();
    (void)argc; (void)argv;
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    TTF_Init(); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    App app;
#ifdef __SWITCH__
    if (!app.init(false, 1280, 720)) return 1;
    app.run();
#else
    // PC: pruebas. --shot archivo.png --scene home|lobby|lobby-multi|game:<id>[:jugadores] --frames N --size 1920x1080 --allai
    std::string shot, scene = "home"; int frames = 30, W = 1280, H = 720; bool allai = false, autotest = false; int pkind = 0; bool pvert = false, realtime = false, logpads = false, phone0 = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shot = argv[++i]; else if (a == "--scene" && i + 1 < argc) scene = argv[++i];
        else if (a == "--kind" && i + 1 < argc) { std::string v = argv[++i]; pkind = v == "left" ? 1 : v == "right" ? 2 : 0; } else if (a == "--vert") pvert = true; else if (a == "--realtime") realtime = true; else if (a == "--logpads") logpads = true; else if (a == "--frames" && i + 1 < argc) frames = atoi(argv[++i]); else if (a == "--size" && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
        else if (a == "--phone0") phone0 = true;
        else if (a == "--allai") allai = true; else if (a == "--autotest") autotest = true;
    }
    bool hidden = !shot.empty() || autotest;
    if (!app.init(hidden, W, H)) { fprintf(stderr, "no se pudo iniciar SDL: %s\n", SDL_GetError()); return 1; }
    if (autotest) {
        // cada juego listo, solo IA, hasta el final
        int bad = 0;
        for (auto& gi : catalog()) {
            if (!gi.ready) continue;
            for (int np = std::max(1, gi.minP); np <= std::min(gi.maxP, 4); np++) {
                if (!gi.ai) {   // sin IA: prueba de humo (arranca y dibuja sin romperse)
                    app.testHumans = np; app.gameId = gi.id; app.defaultOpts(); app.solo = false; app.startGame(); app.testHumans = 0;
                    if (!app.game) { printf("%s: no arrancó\n", gi.id.c_str()); bad++; continue; }
                    for (int f = 0; f < 900; f++) { app.ui.begin(app.g, app.in, -1); app.game->update(1 / 30.f, app.in, app.ui); if (f % 30 == 0) { app.g.beginFrame(); app.game->draw(app.g, app.ui); } }
                    printf("%-12s %d jugadores: humo OK\n", gi.id.c_str(), np); app.endGameToLobby(); continue; }
                if (np < 2) continue;
                app.gameId = gi.id; app.defaultOpts(); app.solo = true; app.aiCount = np - 1; app.startGame();
                if (!app.game) { printf("%s: no arrancó\n", gi.id.c_str()); bad++; continue; }
                for (auto& p : app.game->pl) { p.ai = true; p.level = 1 + (int)(rnd01() * 3); }
                int f = 0; while (!app.game->finished && f < 60 * 60 * 30) { app.ui.begin(app.g, app.in, -1); app.game->update(1 / 30.f, app.in, app.ui); f++; if (f % 60 == 0) { app.g.beginFrame(); app.game->draw(app.g, app.ui); } }
                printf("%-12s %d jugadores: %s en %d pasos · %s\n", gi.id.c_str(), np, app.game->finished ? "terminó" : "NO TERMINÓ", f, app.game->summary.c_str());
                if (!app.game->finished) bad++;
                else {   // la pantalla de resultados debe tener un resultado por jugador y dibujarse sin romperse
                    if (app.game->results.size() != app.game->pl.size()) { printf("  RESULTADOS: %zu para %zu jugadores\n", app.game->results.size(), app.game->pl.size()); bad++; }
                    app.scr = SC_GAME; app.resultT = 5; app.ui.begin(app.g, app.in, -1); app.g.beginFrame(); app.drawResults();
                }
                app.endGameToLobby();
            }
        }
        printf(bad ? "FALLAS: %d\n" : "todo OK\n", bad);
        return bad ? 2 : 0;
    }
    if (!shot.empty()) {
        if (scene == "lobby") app.scr = SC_LOBBY;
        else if (scene == "lobby-multi") { app.scr = SC_LOBBY; app.solo = false; app.aiCount = 1; }
        else if (scene == "profile") app.scr = SC_PROFILE;
        else if (scene == "players") { app.scr = SC_PROFILE; app.save.prof[1].used = true; app.save.prof[2].used = true; app.in.pads[3].connected = true; app.in.pads[3].kind = 2; }
        else if (scene == "qr") app.scr = SC_QR;
        else if (scene == "zoompanel") { app.scr = SC_LOBBY; app.zoomPanel = true; }
        else if (scene.rfind("edit", 0) == 0) { app.scr = SC_EDIT; app.editSlot = scene.size() > 4 ? scene[4] - '0' : 0; }
        else if (scene.rfind("game:", 0) == 0) {
            std::string rest = scene.substr(5); int np = 2; auto c = rest.find(':'); if (c != std::string::npos) { np = atoi(rest.substr(c + 1).c_str()); rest = rest.substr(0, c); }
            app.gameId = rest; app.defaultOpts(); app.aiCount = np - 1; if (auto* fg = findGame(rest)) if (!fg->ai) app.testHumans = np; app.startGame();
            if (allai && app.game) for (auto& p : app.game->pl) p.ai = true;
            app.speed = allai ? 4.f : 1.f;
        } else if (scene.rfind("menu:", 0) == 0) { app.scr = SC_LOBBY; app.menu = scene.substr(5); }
        app.in.pads[0].kind = pkind; app.in.pads[0].vert = pvert;
        if (phone0) app.in.owner[0] = 0;   // pruebas: el jugador 1 usa un celular (mano privada)
        for (int i = 0; i < frames; i++) { app.frame(1 / 30.f); if (realtime) SDL_Delay(33);
            if (logpads && i % 15 == 0) { printf("f%d:", i); for (int q = 0; q < 8; q++) if (app.in.pads[q].connected) { printf(" p%d[", q); for (int b = 0; b < B_COUNT; b++) if (app.in.pads[q].held[b]) printf("%d,", b); printf("]"); } printf(" prof1=%s\n", app.save.prof[1].name.c_str()); fflush(stdout); } }
        if (scene == "players") { app.in.pads[3].connected = true; app.in.pads[3].kind = 2; app.in.pads[1].connected = true; app.in.pads[1].kind = 1; }
        app.renderFrame(0); app.g.savePNG(shot); return 0;
    }
    app.scr = SC_PROFILE;   // al abrir: elegir jugadores
    app.run();
#endif
    Mix_CloseAudio(); TTF_Quit(); SDL_Quit();
#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}
#endif

// ---------------------------------------------------------------- tamaño de pantalla (zoom)
void App::zoomBegin(Rect bounds, Rect dst, float z) {
    z = std::max(1.f, std::min(1.5f, z));
    float w = bounds.w / z, h = bounds.h / z;
    // la cámara sigue lo que se está usando: el foco del mando, el cursor de movimiento o el dedo
    bool have = false; float tx = camX, ty = camY;
    auto toLayer = [&](float sx, float sy, float& lx, float& ly) { lx = zSrc.x + (sx - zDst.x) * zSrc.w / std::max(1.f, zDst.w); ly = zSrc.y + (sy - zDst.y) * zSrc.h / std::max(1.f, zDst.h); };
    int ap = ui.activePad;
    if (ap >= 0 && ap < Input::MAXP && save.prof[ap].gyro && in.pads[ap].gyroOk && SDL_GetTicks() - curT[ap] < 1500) { toLayer(curX[ap], curY[ap], tx, ty); have = true; }
    else if (in.touchHeld) { toLayer(in.tx, in.ty, tx, ty); have = true; }
    else if (ui.padNav) for (auto& it : ui.prev) if (it.lay && it.id == ui.focus) { tx = it.r.cx(); ty = it.r.cy(); have = true; }
    if (!have) { tx = bounds.cx(); ty = bounds.y; have = true; }   // sin nada que seguir: arriba al centro (cartel del turno y la mesa)
    if (z <= 1.001f) { camX = bounds.cx(); camY = bounds.cy(); }
    else if (have) {
        float mx = w * 0.28f, my = h * 0.28f, dx = camX, dy = camY;
        if (tx < camX - mx) dx = tx + mx; else if (tx > camX + mx) dx = tx - mx;
        if (ty < camY - my) dy = ty + my; else if (ty > camY + my) dy = ty - my;
        camX += (dx - camX) * 0.22f; camY += (dy - camY) * 0.22f;
    }
    camX = std::max(bounds.x + w / 2, std::min(bounds.x + bounds.w - w / 2, camX));
    camY = std::max(bounds.y + h / 2, std::min(bounds.y + bounds.h - h / 2, camY));
    zSrc = { camX - w / 2, camY - h / 2, w, h }; zDst = dst;
    if (!g.layerBegin()) { zActive = false; return; }
    zActive = true;
    rawTx = in.tx; rawTy = in.ty; rawSx = ui.sx; rawSy = ui.sy;
    toLayer(rawTx, rawTy, in.tx, in.ty); toLayer(rawSx, rawSy, ui.sx, ui.sy);
}
void App::zoomEnd() {
    if (!zActive) return; zActive = false;
    g.layerEnd(zSrc, zDst);
    in.tx = rawTx; in.ty = rawTy; ui.sx = rawSx; ui.sy = rawSy;
}
void App::drawZoomPanel() {
    ui.activePad = 0;
    g.rect({ 0, 0, 1280, 720 }, rgb(0, 140));
    Rect p{ 330, 150, 620, 410 };
    g.rrect({ p.x, p.y + 6, p.w, p.h }, 18, rgb(0, 120)); g.rrect(p, 18, pal::panelEdge); g.rrectGrad(p.inset(3), 16, rgb(0x4a2e18), rgb(0x2e1a0c));
    g.textShadow("TAMAÑO DE PANTALLA", p.cx(), p.y + 38, 28, pal::gold, F_TITLE, AL_C);
    // deslizador 100 - 150 %
    uint32_t sid = hid("zoom:step"); Rect sr{ p.x + 24, p.y + 74, p.w - 48, 76 };
    bool c = ui.hit(sid, sr); (void)c; int z = save.zoom;
    Rect tr{ sr.x + 26, sr.y + 46, sr.w - 150, 12 };
    if (ui.focused(sid)) { if (ui.padDown(B_LEFT) || in.pads[0].rep[B_LEFT]) z -= 5; if (ui.padDown(B_RIGHT) || in.pads[0].rep[B_RIGHT]) z += 5; }
    if (in.touchHeld && Rect{ tr.x - 16, tr.y - 22, tr.w + 32, tr.h + 44 }.in(in.tx, in.ty) && Rect{ tr.x - 16, tr.y - 22, tr.w + 32, tr.h + 44 }.in(ui.sx, ui.sy)) { z = 100 + (int)lroundf((in.tx - tr.x) / tr.w * 10) * 5; ui.forceFocus(sid); }
    z = std::max(100, std::min(150, z)); if (z != save.zoom) { save.zoom = z; save.store(); audio.play("click"); }
    g.text("Agranda letras, botones, tableros y dibujos", sr.x + 26, sr.y + 18, 16, pal::cream, F_BODY, AL_L);
    g.rrect(tr, 6, rgb(0x1a0e06)); float f = (save.zoom - 100) / 50.f; g.rrect({ tr.x, tr.y, tr.w * f, tr.h }, 6, pal::gold);
    for (int k = 0; k <= 10; k++) g.rect({ tr.x + tr.w * k / 10.f - 1, tr.y + tr.h + 4, 2, k % 2 ? 5.f : 9.f }, rgb(0xd8b46a, 160));
    g.circle(tr.x + tr.w * f, tr.y + tr.h / 2, 15, pal::cream); g.ring(tr.x + tr.w * f, tr.y + tr.h / 2, 15, 3, pal::gold);
    g.textShadow(std::to_string(save.zoom) + "%", sr.x + sr.w - 60, tr.y + tr.h / 2, 30, pal::gold, F_TITLE, AL_C);
    if (ui.focused(sid)) ui.drawFocus(sr, 12);
    // casillas: afectar todo / solo tablero (una u otra)
    bool a = save.zmode == 0, b = save.zmode == 1;
    if (ui.toggle("Afectar todo  (toda la pantalla)", a, { p.x + 24, p.y + 170, p.w - 48, 50 })) { save.zmode = a ? 0 : 1; save.store(); audio.play("click"); }
    if (ui.toggle("Solo tablero  (+ / − muestra las estadísticas)", b, { p.x + 24, p.y + 228, p.w - 48, 50 })) { save.zmode = b ? 1 : 0; statsOn = false; save.store(); audio.play("click"); }
    g.text(save.zmode ? "En la partida: tocá + o − una vez para ver u ocultar jugadores y botones; mantenelo para el menú." : "La pantalla sigue al foco del mando, al cursor o al dedo.", p.cx(), p.y + 304, 14, rgb(0xf6e7bf, 200), F_BODY, AL_C, p.w - 40);
    if (ui.button("Listo", { p.cx() - 110, p.y + p.h - 74, 220, 52 }, 1) || ui.back()) { zoomPanel = false; audio.play("click"); }
    ui.setFocusIfNone(sid);
}
// + / − en "solo tablero": un toque muestra u oculta las estadísticas; mantenido (mando 1) hace lo de siempre (menú / ayuda)
void App::plusMinus() {
    bool on = save.zmode == 1 && scr == SC_GAME && game && !game->finished;
    uint32_t now = SDL_GetTicks(); bool synth[2] = { false, false };
    for (int i = 0; i < Input::MAXP; i++) {
        Pad& p = in.pads[i];
        for (int k = 0; k < 2; k++) {
            int b = k ? B_MINUS : B_PLUS;
            if (!on) { pmWas[i][k] = p.held[b]; pmFired[i][k] = true; continue; }
            if (p.down[b]) { pmT[i][k] = now; pmFired[i][k] = false; }
            if (p.held[b] && !pmFired[i][k] && now - pmT[i][k] >= 500) { pmFired[i][k] = true; if (i == 0) synth[k] = true; }
            if (!p.held[b] && pmWas[i][k] && !pmFired[i][k]) {
                pmFired[i][k] = true;
                if (menu.empty() && !paused && !confirmQuit && !showHelp && !zoomPanel) { statsOn = !statsOn; audio.play("click"); }
            }
            pmWas[i][k] = p.held[b]; p.down[b] = false;
        }
    }
    if (synth[0]) in.pads[0].down[B_PLUS] = true;
    if (synth[1]) in.pads[0].down[B_MINUS] = true;
}
// cursor con el sensor de movimiento: cada jugador lo activa en su perfil. A = tocar donde apunta; L + ZL (o R + ZR) = centrar
void App::gyroCursors(int activePad) {
    static int pend[Input::MAXP] = { -1, -1, -1, -1, -1, -1, -1, -1 }; static uint32_t pendT[Input::MAXP] = {};
    uint32_t now = SDL_GetTicks();
    for (int i = 0; i < Input::MAXP; i++) {
        Pad& p = in.pads[i];
        if (!p.connected || !save.prof[i].gyro || !p.gyroOk) { curA[i] = false; pend[i] = -1; continue; }
        if (!curT[i]) { curX[i] = 640; curY[i] = 360; curT[i] = now; }
        if (fabsf(p.gx) + fabsf(p.gy) > 0.02f) { curX[i] += p.gx * 16.f; curY[i] += p.gy * 16.f; curT[i] = now; }
        curX[i] = std::max(0.f, std::min(1279.f, curX[i])); curY[i] = std::max(0.f, std::min(719.f, curY[i]));
        // centrar: los dos gatillos del mismo lado juntos. Un gatillo solo espera un instante por si llega el otro.
        auto partner = [&](int b) { if (p.single()) return b == B_L ? B_R : b == B_R ? B_L : -1; return b == B_L ? B_ZL : b == B_ZL ? B_L : b == B_R ? B_ZR : b == B_ZR ? B_R : -1; };
        auto center = [&]() { curX[i] = 640; curY[i] = 360; curT[i] = now; pend[i] = -1; };
        if (pend[i] >= 0) { int q = partner(pend[i]); if (q >= 0 && p.held[q]) center(); else if (now - pendT[i] >= 130) { if (p.held[pend[i]] || now - pendT[i] < 260) { p.down[pend[i]] = true; p.rep[pend[i]] = true; } pend[i] = -1; } }
        for (int b : { B_L, B_R, B_ZL, B_ZR }) {
            if (!p.down[b]) continue; p.down[b] = false; p.rep[b] = false;
            int q = partner(b); if (q >= 0 && p.held[q]) center(); else if (pend[i] < 0) { pend[i] = b; pendT[i] = now; }
        }
        // A del jugador que maneja la pantalla = toque donde apunta el cursor
        if (i == activePad) {
            if (p.down[B_A]) { in.touchDown = in.touchHeld = true; in.tx = curX[i]; in.ty = curY[i]; curA[i] = true; p.down[B_A] = false; p.rep[B_A] = false; curT[i] = now; }
            else if (curA[i] && p.held[B_A]) { in.touchHeld = true; in.tx = curX[i]; in.ty = curY[i]; }
            else if (curA[i]) { in.touchUp = true; in.touchHeld = false; in.tx = curX[i]; in.ty = curY[i]; curA[i] = false; }
        } else curA[i] = false;
    }
}
void App::drawCursors() {
    uint32_t now = SDL_GetTicks();
    for (int i = 0; i < Input::MAXP; i++) {
        const Pad& p = in.pads[i];
        if (!p.connected || !save.prof[i].gyro || !p.gyroOk || (now - curT[i] > 5000 && !curA[i])) continue;
        float x = curX[i], y = curY[i]; bool act = i == ui.activePad; Col c = PCOL[i % 8];
        g.circle(x + 2, y + 3, act ? 17 : 13, rgb(0, 90));
        g.ring(x, y, act ? 16 : 12, act ? 5 : 3, c); g.ring(x, y, act ? 19 : 14, 2, rgb(0xffffff, 220));
        g.circle(x, y, curA[i] ? 7 : 4, curA[i] ? pal::gold : pal::white);
        g.text(std::to_string(i + 1), x + 20, y + 18, 13, pal::white, F_BOLD, AL_L);
    }
}

// ---------------------------------------------------------------- manos privadas en el celular
bool Game::onPhone(int p) const { return app && p >= 0 && p < (int)pl.size() && !pl[p].ai && app->in.isPhone(pl[p].pad); }
std::string Game::mark(const char* prefix, int i, bool me) const { return me && app && app->ui.focus == hid(prefix, i) ? "▶ " : "· "; }
void Game::coverHand(Gfx& g, UI& ui, const char* prefix, int n, int p) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f; Rect foc{ 0, 0, 0, 0 }; bool hasF = false;
    for (int i = 0; i < n; i++) { uint32_t id = hid(prefix, i);
        for (auto& it : ui.cur) if (it.id == id) { x0 = std::min(x0, it.r.x); y0 = std::min(y0, it.r.y); x1 = std::max(x1, it.r.x + it.r.w); y1 = std::max(y1, it.r.y + it.r.h); if (ui.focused(id)) { foc = it.r; hasF = true; } } }
    if (x1 < x0) return;
    Rect r{ x0 - 10, y0 - 26, x1 - x0 + 20, y1 - y0 + 36 };
    g.rrect(r, 14, rgb(0x2a180c)); g.rrectGrad(r.inset(3), 12, rgb(0x5a3a1c), rgb(0x2e1a0c)); g.rrectLine(r, 14, 2, pal::plateEdge);
    for (float x = r.x + 18; x < r.x + r.w - 10; x += 26) g.line(x, r.y + 6, x + 14, r.y + r.h - 6, 2, rgb(0xd8b46a, 40));
    g.textShadow("📱 " + (p >= 0 && p < (int)pl.size() ? pl[p].name : std::string("")) + ": la mano está en su celular", r.cx(), r.cy(), std::min(22.f, r.h * 0.3f), pal::gold, F_BOLD, AL_C);
    if (hasF) { g.rrect({ foc.x + 4, r.y + r.h - 12, std::max(8.f, foc.w - 8), 6 }, 3, pal::focus); }   // dónde está el foco (sin mostrar la carta)
}
