// Yacht & Friends — versión nativa para Nintendo Switch (homebrew .nro) y PC (para probar).
// Núcleo: gráficos (coordenadas lógicas 1280x720 escaladas a 720p o 1080p), interfaz inmediata con foco para mandos,
// entrada (táctil + hasta 8 mandos; el mando 1 es el anfitrión), sonido y datos guardados.
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdint>
#include <cmath>
#include <random>
#include <array>

// ---------------------------------------------------------------- utilidades
struct Col { uint8_t r, g, b, a; };
inline Col rgb(uint32_t hex, uint8_t a = 255) { return Col{ uint8_t(hex >> 16), uint8_t(hex >> 8), uint8_t(hex), a }; }
inline Col withA(Col c, uint8_t a) { c.a = a; return c; }
inline Col mix(Col a, Col b, float t) { auto L = [t](uint8_t x, uint8_t y) { return uint8_t(x + (y - x) * t); }; return Col{ L(a.r, b.r), L(a.g, b.g), L(a.b, b.b), L(a.a, b.a) }; }
struct Rect { float x, y, w, h; bool in(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; } float cx() const { return x + w / 2; } float cy() const { return y + h / 2; } Rect inset(float d) const { return { x + d, y + d, w - 2 * d, h - 2 * d }; } };
void dbgLog(const std::string& m);   // línea de diagnóstico en la SD (Switch) o en YF_LOG (PC)
float rnd01();
int rndInt(int n);
std::string fmtTime(int ms);

// Paleta "mesa de juegos"
namespace pal {
    const Col wood = rgb(0x7a4a22), plate = rgb(0x1c1410), plateEdge = rgb(0xd8b46a), cream = rgb(0xf6e7bf), creamDark = rgb(0xe2c98d),
              brownText = rgb(0x3b2414), gold = rgb(0xf2c14e), purple = rgb(0x6a3fb5), purpleDark = rgb(0x3e2173), red = rgb(0xd9483b),
              redDark = rgb(0x8f2219), green = rgb(0x2f7d3a), white = rgb(0xffffff), black = rgb(0x000000), shadow = rgb(0x000000, 110),
              panel = rgb(0x3a2414, 235), panelEdge = rgb(0xb98a4a), focus = rgb(0xffe27a);
}

// ---------------------------------------------------------------- gráficos
enum FontId { F_BODY, F_BOLD, F_TITLE };
enum Align { AL_L = 0, AL_C = 1, AL_R = 2 };
struct Gfx {
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr;
    int outW = 1280, outH = 720; float S = 1.f;   // escala lógica -> píxeles
    bool init(int w, int h, bool hidden);
    void beginFrame();
    void endFrame();
    void resized();
    // primitivas (coordenadas lógicas 1280x720)
    void rect(Rect r, Col c);
    void gradRect(Rect r, Col top, Col bottom);
    void rrect(Rect r, float rad, Col c);
    void rrectGrad(Rect r, float rad, Col top, Col bottom);
    void rrectLine(Rect r, float rad, float th, Col c);
    void circle(float cx, float cy, float rad, Col c);
    void circleGrad(float cx, float cy, float rad, Col inner, Col outer);
    void ring(float cx, float cy, float rad, float th, Col c);
    void line(float x1, float y1, float x2, float y2, float th, Col c);
    void tri(float x1, float y1, float x2, float y2, float x3, float y3, Col c);
    // texto
    float text(const std::string& s, float x, float y, float size, Col c, int font = F_BODY, int align = AL_L, float maxW = 0);
    float textW(const std::string& s, float size, int font = F_BODY);
    void textShadow(const std::string& s, float x, float y, float size, Col c, int font = F_BOLD, int align = AL_L);
    void textBox(const std::string& s, Rect r, float size, Col c, int font = F_BODY, int align = AL_L);   // con salto de línea
    // imágenes
    SDL_Texture* tex(const std::string& path);          // romfs/<path>
    void image(SDL_Texture* t, Rect r, uint8_t alpha = 255);
    void imageTiled(SDL_Texture* t, Rect r, float tileW);
    void emoji(const std::string& e, float cx, float cy, float size, uint8_t alpha = 255);   // también acepta códigos de avatar "@av:..."
    void avatar(const std::string& code, float cx, float cy, float size, bool full = false);   // personaje dibujado: busto (cabeza y hombros) o cuerpo entero
    // recorte
    void clip(const Rect* r);
    // captura (para pruebas en PC)
    bool savePNG(const std::string& path);
    // fondo de madera de la mesa
    void woodTable();
    // capa ampliada (zoom de pantalla): se dibuja en una textura y se copia un recorte agrandado
    SDL_Texture* rt = nullptr; int rtW = 0, rtH = 0; bool inLayer = false;
    bool layerBegin();
    void layerEnd(Rect src, Rect dst);

    std::map<std::pair<int, int>, TTF_Font*> fonts;
    struct TxtKey { std::string s; int font, px; uint32_t col; bool operator==(const TxtKey& o) const { return px == o.px && font == o.font && col == o.col && s == o.s; } };
    struct TxtHash { size_t operator()(const TxtKey& k) const { return std::hash<std::string>()(k.s) ^ (k.px * 131) ^ (k.font * 7) ^ k.col; } };
    struct TxtVal { SDL_Texture* t; int w, h; uint32_t used; };
    std::unordered_map<TxtKey, TxtVal, TxtHash> txtCache;
    std::map<std::string, SDL_Texture*> texCache;
    uint32_t frameNo = 0;
    TTF_Font* font(int id, int px);
    TxtVal* textTex(const std::string& s, int font, int px, Col c);
    void flushText();
};
std::string romfs(const std::string& p);
std::string emojiFile(const std::string& e);

// ---------------------------------------------------------------- entrada
enum Btn { B_A, B_B, B_X, B_Y, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_MINUS, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_COUNT };
struct Pad {
    enum { PK_FULL = 0, PK_LEFT = 1, PK_RIGHT = 2 };   // Pro / Joy-Con en pareja / portátil, o un solo Joy-Con izquierdo o derecho (de costado)
    int kind = 0; bool vert = false; bool single() const { return kind != PK_FULL; }
    bool connected = false, held[B_COUNT] = {}, down[B_COUNT] = {}, rep[B_COUNT] = {};   // rep = pulsación + autorrepetición (flechas)
    float lx = 0, ly = 0; uint32_t holdT[B_COUNT] = {};
    float gx = 0, gy = 0; bool gyroOk = false;   // giro del sensor de movimiento en este cuadro (grados), si hay sensor
    // tirar dados: ZL o ZR (en un Joy-Con solo, el ZL/ZR que tiene queda como R); cambiar de lado: L (o R en un mando completo)
    bool rollDown() const { return down[B_ZL] || down[B_ZR] || (single() && down[B_R]); }
    bool sideDown() const { return down[B_L] || (!single() && down[B_R]); }
    bool any() const { for (int i = 0; i < B_COUNT; i++) if (down[i]) return true; return false; }
};
struct Net;
struct Input {
    static const int MAXP = 8;
    Net* net = nullptr; int owner[MAXP] = { -1, -1, -1, -1, -1, -1, -1, -1 }; bool hw[MAXP] = {};   // celulares: casilla -> cliente; hw = mando físico conectado
    bool isPhone(int i) const { return i >= 0 && i < MAXP && owner[i] >= 0; }
    void pollPhones();
    Pad pads[MAXP]; int orient[MAXP] = {};   // por casilla: 0 Joy-Con solo de costado, 1 en vertical
    bool touchDown = false, touchHeld = false, touchUp = false; float tx = 0, ty = 0;   // lógico
    bool quit = false;
    bool anyActivity = false;
    void init();
    void poll(Gfx& g);
    int connectedCount() const { int n = 0; for (auto& p : pads) if (p.connected) n++; return n; }
    bool dualPaired() const;   // Switch: hay dos Joy-Con unidos como un solo mando
    void openControllerApplet(int players);   // Switch: pantalla del sistema para conectar Joy-Con en vertical
#ifndef __SWITCH__
    std::vector<SDL_GameController*> gcs;
    bool kHeld[B_COUNT] = {};
#endif
};

// ---------------------------------------------------------------- interfaz inmediata (botones con foco para el mando)
struct UI {
    struct Item { uint32_t id; Rect r; bool enabled; bool lay = false; };
    std::vector<Item> cur, prev;
    uint32_t focus = 0; int activePad = 0;   // qué mando mueve el foco ahora
    bool padNav = false;                      // el foco se ve solo cuando se usa el mando
    bool tapUsed = false;
    float sx = -1, sy = -1;                   // dónde empezó el toque (para distinguir toque de arrastre)
    Gfx* g = nullptr; Input* in = nullptr;
    void begin(Gfx& gfx, Input& inp, int pad);
    void end();
    bool hit(uint32_t id, Rect r, bool enabled = true);           // registra un elemento enfocable y dice si se activó
    bool focused(uint32_t id) const { return padNav && focus == id; }
    void drawFocus(Rect r, float rad);
    // widgets con el estilo de la mesa
    bool button(const std::string& label, Rect r, int style = 0, bool enabled = true, const std::string& icon = "");   // 0 crema, 1 rojo, 2 madera oscura, 3 dorado
    bool card(const std::string& title, const std::string& sub, const std::string& emo, Rect r, bool enabled = true, bool selected = false);
    bool stepper(const std::string& label, const std::string& value, Rect r, int& delta);   // ◀ valor ▶
    bool toggle(const std::string& label, bool& v, Rect r);
    void setFocusIfNone(uint32_t id);
    void forceFocus(uint32_t id) { focus = id; }
    bool back() const;   // botón B del mando activo
    bool padDown(int b) const;
};
uint32_t hid(const std::string& s);
uint32_t hid(const std::string& s, int i);

// ---------------------------------------------------------------- sonido
struct Audio {
    bool ok = false, musicOn = true, sfxOn = true;
    std::map<std::string, Mix_Chunk*> sfx;
    std::vector<std::string> tracks; std::vector<int> order; int pos = 0; Mix_Music* cur = nullptr; uint32_t pauseUntil = 0;
    void init();
    void play(const std::string& name);
    void update();
    void setMusic(bool on);
};

// ---------------------------------------------------------------- jugadores, partida y juegos
static const char* const AVATARS[] = { "😀", "😎", "🤠", "🦊", "🐼", "🐯", "🦄", "🐸", "🐙", "🚀", "👑", "🎩" };
static const int NAVATARS = 12;
static const char* const BOT_NAMES[] = { "Robi", "Chispa", "Turbo", "Luna", "Nova", "Pixel", "Bolt", "Max" };
static const Col PCOL[] = { rgb(0xe53935), rgb(0x1e88e5), rgb(0xfdd835), rgb(0x8e24aa), rgb(0x43a047), rgb(0xfb8c00), rgb(0x00acc1), rgb(0xec407a) };

// ---------------------------------------------------------------- avatares (dibujados por código, sin imágenes)
static const int AV_N = 11;   // piel, pelo, color de pelo, cara, accesorio, camisa, color de camisa, pantalón, color de pantalón, zapatos, gesto
extern const int AV_OPTS[AV_N]; extern const char* const AV_LABEL[AV_N];
std::string avName(int field, int v);
std::string avCode(const int* a);
bool avParse(const std::string& code, int* a);
void avDefault(int slot, int* a);   // personaje de arranque, distinto por casilla
void avRandom(int* a);

struct Player { std::string name, emoji; bool ai = false; int pad = -1; int level = 2; };
struct PResult { int pts = 0; char r = 'l'; };   // w gana, l pierde, d empate, x se fue
struct Hint { std::string k, label; };   // k: A B X Y + - L R ZL ZR DPAD LR UD STICK TOUCH; varios con "/"
struct OptDef { std::string key, label; std::vector<std::pair<std::string, std::string>> choices; int def = 0; bool check = false; };
struct Opts { std::map<std::string, int> v; int get(const std::string& k, int d = 0) const { auto it = v.find(k); return it == v.end() ? d : it->second; } };

struct App;
struct Game {
    App* app = nullptr;
    std::vector<Player> pl;
    Opts opts;
    bool finished = false; std::vector<PResult> results; std::string summary;
    virtual ~Game() {}
    virtual void start() = 0;
    virtual void update(float dt, Input& in, UI& ui) = 0;   // lógica + IA
    virtual void draw(Gfx& g, UI& ui) = 0;                  // dibuja y procesa toques/botones
    virtual int turnPlayer() const { return -1; }            // quién juega ahora (para el foco del mando y la placa resaltada)
    virtual void resign(int p) { (void)p; }
    virtual std::string help() const { return ""; }
    virtual std::string secret(int p) const { (void)p; return ""; }   // texto privado que solo ve el jugador p en su celular (palabra a actuar, etc.)
    virtual std::vector<Hint> hints() const { return { { "DPAD/STICK", "Moverse" }, { "A", "Elegir" } }; }   // botones del turno actual
    void finish(std::vector<PResult> r, const std::string& s) { results = r; summary = s; finished = true; }
    int humanPad(int p) const { return (p >= 0 && p < (int)pl.size() && !pl[p].ai) ? pl[p].pad : -1; }
    // mano privada: el jugador p juega con su celular, así que en la tele su mano va tapada y la ve en el celular
    bool onPhone(int p) const;
    void coverHand(Gfx& g, UI& ui, const char* prefix, int n, int p);     // tapa los elementos hid(prefix, 0..n-1) dibujados en este cuadro
    std::string mark(const char* prefix, int i, bool me) const;           // "▶ " en la carta que tiene el foco del mando
};
struct GameInfo {
    std::string id, name, emoji, cat; int minP, maxP; bool ai, ready; int dif;
    std::vector<OptDef> options;
    std::function<Game*()> make;
};
std::vector<GameInfo>& catalog();
const GameInfo* findGame(const std::string& id);
void registerGames();   // games.cpp

// ---------------------------------------------------------------- datos guardados
struct Save {
    struct Profile { bool used = false; std::string name; int av[AV_N] = {}; int orient = 0; bool gyro = false; };   // gyro: mueve un cursor con el sensor de movimiento del Joy-Con
    Profile prof[8];   // un perfil por mando (casilla 1 = anfitrión); se guardan en la SD
    int zoom = 100, zmode = 0;   // tamaño de pantalla 100-150 %; zmode 0 = afectar todo, 1 = solo tablero
    bool music = true, sfx = true; int level = 2; int bank = 1000, jackpot = 5000;   // casino: banca acumulada y jackpot progresivo
    Save() { for (int i = 0; i < 8; i++) { prof[i].name = i ? "Jugador " + std::to_string(i + 1) : "Jugador"; avDefault(i, prof[i].av); } prof[0].used = true; }
    std::map<std::string, std::vector<int>> stats;   // id -> [jugadas, ganadas]
    std::vector<std::string> favs;
    void load(); void store();
};
