#pragma once
#include "core.h"
#include "net.h"

enum Screen { SC_HOME, SC_PROFILE, SC_LOBBY, SC_GAME, SC_EDIT, SC_QR };   // SC_PROFILE = jugadores (tarjetas), SC_EDIT = editor de personaje

struct App {
    int testHumans = 0;   // solo pruebas en PC
    Gfx g; Input in; UI ui; Audio audio; Save save;
    Screen scr = SC_HOME;
    bool solo = true;                 // 1 Jugador (contra la IA) o Multijugador (varios mandos en la misma consola)
    std::string gameId = "yacht";
    Opts opts; int aiCount = 1, aiLevel = 2;
    std::vector<Player> seats;        // jugadores de la próxima partida
    std::unique_ptr<Game> game;
    std::string menu;                 // menú abierto de la barra morada ("opts", "acts", "help", "")
    bool paused = false, showHelp = false, confirmQuit = false;
    float resultT = 0; int lobbyScroll = 0;
    bool dualAsked = false; int editSlot = 0, plPage = 0;     // jugador que se edita / página de tarjetas (4 por página)
    Net net; bool spectator = false;   // celulares como mandos; spectator = la pantalla de la Switch no juega (mirón)
    bool hostPlays() const;
    void drawQR(); void updatePhones();
    void drawEdit(); void drawJoyCon(float cx, float cy, float w, int kind, bool hiTop);
    std::string toastMsg; uint32_t toastUntil = 0;
    float speed = 1.f;                // pruebas: acelera las IA
    // área de juego (coordenadas lógicas): a la derecha de las placas de jugadores
    Rect area{ 262, 14, 1004, 692 };

    bool init(bool hidden, int w, int h);
    void frame(float dt); void renderFrame(float dt);
    void run();
    void toast(const std::string& s) { toastMsg = s; toastUntil = SDL_GetTicks() + 2600; }
    void startGame();
    void endGameToLobby();
    void buildSeats();
    int humanCount() const;
    bool editName(std::string& s, const char* header);
    // pantallas
    void drawHome(); void drawProfile(); void drawLobby(); void drawGame(float dt);
    void drawBar(); void drawMenu(); void drawPlates(); void drawResults(); void drawHelp(); void drawToast();
    int hk = 0;   // tipo de mando que se está guiando (0 completo, 1 Joy-Con izq., 2 Joy-Con der.)
    std::string mapKey(const std::string& k) const;
    void drawHints(); float glyph(const std::string& k, float x, float cy, float s, bool draw = true);
    void defaultOpts();
    // tamaño de pantalla (zoom) y estadísticas en modo "solo tablero"
    bool zoomPanel = false, statsOn = false, zActive = false; float camX = 640, camY = 360; Rect zSrc{ 0, 0, 1280, 720 }, zDst{ 0, 0, 1280, 720 };
    float rawTx = 0, rawTy = 0, rawSx = 0, rawSy = 0;
    bool boardMode() const { return save.zmode == 1 && scr == SC_GAME && game; }
    void zoomBegin(Rect bounds, Rect dst, float z); void zoomEnd(); void drawZoomPanel(); void plusMinus();
    // cursor con el sensor de movimiento (por jugador, se elige en su perfil)
    float curX[Input::MAXP] = {}, curY[Input::MAXP] = {}; uint32_t curT[Input::MAXP] = {}; bool curA[Input::MAXP] = {};
    uint32_t pmT[Input::MAXP][2] = {}; bool pmWas[Input::MAXP][2] = {}, pmFired[Input::MAXP][2] = {};
    void gyroCursors(int activePad); void drawCursors();
};
extern App* gApp;
