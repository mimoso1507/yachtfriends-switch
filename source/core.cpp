#include "core.h"
#include "net.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>
#include <sys/stat.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef __SWITCH__
// Si el programa se cierra por un error, deja un informe en la tarjeta SD (sdmc:/switch/YachtFriends/crash.txt) con las direcciones, para poder ubicar la causa.
extern "C" {
extern char __start__;
void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/YachtFriends", 0777);
    FILE* f = fopen("sdmc:/switch/YachtFriends/crash.txt", "w");
    if (!f) return;
    u64 base = (u64)&__start__;
    fprintf(f, "error_desc=0x%x\nbase=0x%lx\npc=0x%lx\nlr=0x%lx\nfp=0x%lx\nsp=0x%lx\nfar=0x%lx\n", ctx->error_desc, (unsigned long)base, (unsigned long)(ctx->pc.x - base), (unsigned long)(ctx->lr.x - base), (unsigned long)ctx->fp.x, (unsigned long)ctx->sp.x, (unsigned long)ctx->far.x);
    u64 fp = ctx->fp.x, sp = ctx->sp.x;   // recorre la pila de llamadas (solo si los punteros parecen válidos)
    for (int i = 0; i < 24 && fp && (fp & 7) == 0 && fp >= sp && fp < sp + 0x800000; i++) { u64* q = (u64*)fp; u64 ret = q[1]; fprintf(f, "bt%d=0x%lx\n", i, (unsigned long)(ret - base)); if (q[0] <= fp) break; fp = q[0]; }
    fclose(f);
}
}
#endif
void dbgLog(const std::string& m) {
#ifdef __SWITCH__
    FILE* f = fopen("sdmc:/switch/YachtFriends/log.txt", "a"); if (f) { fprintf(f, "%s\n", m.c_str()); fclose(f); }
#else
    if (const char* e = getenv("YF_LOG")) { FILE* f = fopen(e, "a"); if (f) { fprintf(f, "%s\n", m.c_str()); fclose(f); } }
#endif
}

static std::mt19937 RNG((unsigned)time(nullptr));
float rnd01() { return std::uniform_real_distribution<float>(0.f, 1.f)(RNG); }
int rndInt(int n) { return n <= 1 ? 0 : std::uniform_int_distribution<int>(0, n - 1)(RNG); }
std::string fmtTime(int ms) { int s = ms / 1000; char b[16]; snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); return b; }
std::string romfs(const std::string& p) {
#ifdef __SWITCH__
    return "romfs:/" + p;
#else
    static std::string base = [] { const char* e = getenv("YF_ROMFS"); return std::string(e ? e : "romfs/"); }();
    return base + p;
#endif
}
uint32_t hid(const std::string& s) { uint32_t h = 2166136261u; for (unsigned char c : s) { h ^= c; h *= 16777619u; } return h ? h : 1; }
uint32_t hid(const std::string& s, int i) { return hid(s + "#" + std::to_string(i)); }

// ================================================================ GRÁFICOS
bool Gfx::init(int w, int h, bool hidden) {
    win = SDL_CreateWindow("Yacht & Friends", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!win) return false;
    ren = SDL_CreateRenderer(win, -1, hidden ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) return false;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    resized();
    return true;
}
void Gfx::resized() {
    int w, h; SDL_GetRendererOutputSize(ren, &w, &h);
    if (w != outW || h != outH || txtCache.empty()) {
        outW = w; outH = h; S = std::min(w / 1280.f, h / 720.f);
        for (auto& kv : txtCache) SDL_DestroyTexture(kv.second.t);
        txtCache.clear();
        for (auto& kv : fonts) TTF_CloseFont(kv.second);
        fonts.clear();
    }
}
void Gfx::beginFrame() {
    frameNo++;
#ifdef __SWITCH__
    // modo portátil 1280x720 · en la TV con el dock 1920x1080
    int want = appletGetOperationMode() == AppletOperationMode_Console ? 1080 : 720;
    if (outH != want) { SDL_SetWindowSize(win, want == 1080 ? 1920 : 1280, want); }
#endif
    resized();
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
}
bool Gfx::layerBegin() {
    if (!rt || rtW != outW || rtH != outH) {
        if (rt) SDL_DestroyTexture(rt);
        rt = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, outW, outH); rtW = outW; rtH = outH;
        if (!rt) return false;
    }
    SDL_RenderSetClipRect(ren, nullptr);
    if (SDL_SetRenderTarget(ren, rt) != 0) return false;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren); inLayer = true; return true;
}
void Gfx::layerEnd(Rect src, Rect dst) {
    if (!inLayer) return; inLayer = false;
    SDL_RenderSetClipRect(ren, nullptr); SDL_SetRenderTarget(ren, nullptr);
    SDL_Rect s{ (int)lroundf(src.x * S), (int)lroundf(src.y * S), (int)lroundf(src.w * S), (int)lroundf(src.h * S) };
    SDL_FRect d{ dst.x * S, dst.y * S, dst.w * S, dst.h * S };
    SDL_SetTextureBlendMode(rt, SDL_BLENDMODE_NONE); SDL_RenderCopyF(ren, rt, &s, &d);
}
void Gfx::endFrame() { SDL_RenderPresent(ren); if (frameNo % 60 == 0) flushText(); }
void Gfx::flushText() {
    for (auto it = txtCache.begin(); it != txtCache.end();) {
        if (frameNo - it->second.used > 180) { SDL_DestroyTexture(it->second.t); it = txtCache.erase(it); } else ++it;
    }
}
static SDL_Vertex V(float x, float y, Col c, float S) { SDL_Vertex v; v.position = { x * S, y * S }; v.color = { c.r, c.g, c.b, c.a }; v.tex_coord = { 0, 0 }; return v; }
void Gfx::rect(Rect r, Col c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); SDL_FRect f{ r.x * S, r.y * S, r.w * S, r.h * S }; SDL_RenderFillRectF(ren, &f); }
void Gfx::gradRect(Rect r, Col a, Col b) {
    SDL_Vertex v[4] = { V(r.x, r.y, a, S), V(r.x + r.w, r.y, a, S), V(r.x + r.w, r.y + r.h, b, S), V(r.x, r.y + r.h, b, S) };
    int idx[6] = { 0, 1, 2, 0, 2, 3 }; SDL_RenderGeometry(ren, nullptr, v, 4, idx, 6);
}
static void rrPath(std::vector<std::pair<float, float>>& pts, Rect r, float rad) {
    rad = std::min(rad, std::min(r.w, r.h) / 2);
    const int seg = 7;
    float cx[4] = { r.x + r.w - rad, r.x + r.w - rad, r.x + rad, r.x + rad }, cy[4] = { r.y + rad, r.y + r.h - rad, r.y + r.h - rad, r.y + rad };
    float a0[4] = { -90, 0, 90, 180 };
    for (int k = 0; k < 4; k++) for (int i = 0; i <= seg; i++) {
        float a = (a0[k] + 90.f * i / seg) * (float)M_PI / 180.f; pts.push_back({ cx[k] + cosf(a) * rad, cy[k] + sinf(a) * rad });
    }
}
void Gfx::rrectGrad(Rect r, float rad, Col top, Col bottom) {
    if (r.w <= 0 || r.h <= 0) return;
    std::vector<std::pair<float, float>> p; rrPath(p, r, rad);
    std::vector<SDL_Vertex> v; std::vector<int> idx;
    auto colAt = [&](float y) { return mix(top, bottom, std::max(0.f, std::min(1.f, (y - r.y) / r.h))); };
    v.push_back(V(r.cx(), r.cy(), colAt(r.cy()), S));
    for (auto& q : p) v.push_back(V(q.first, q.second, colAt(q.second), S));
    int n = (int)p.size(); for (int i = 0; i < n; i++) { idx.push_back(0); idx.push_back(1 + i); idx.push_back(1 + (i + 1) % n); }
    SDL_RenderGeometry(ren, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}
void Gfx::rrect(Rect r, float rad, Col c) { rrectGrad(r, rad, c, c); }
void Gfx::rrectLine(Rect r, float rad, float th, Col c) {
    std::vector<std::pair<float, float>> o, i; rrPath(o, r, rad); rrPath(i, r.inset(th), std::max(0.f, rad - th));
    std::vector<SDL_Vertex> v; std::vector<int> idx; int n = (int)o.size();
    for (int k = 0; k < n; k++) { v.push_back(V(o[k].first, o[k].second, c, S)); v.push_back(V(i[k].first, i[k].second, c, S)); }
    for (int k = 0; k < n; k++) { int a = 2 * k, b = 2 * ((k + 1) % n); idx.insert(idx.end(), { a, a + 1, b, b, a + 1, b + 1 }); }
    SDL_RenderGeometry(ren, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}
void Gfx::circleGrad(float cx, float cy, float rad, Col inner, Col outer) {
    int seg = std::max(16, std::min(64, (int)(rad * S / 2)));
    std::vector<SDL_Vertex> v; std::vector<int> idx; v.push_back(V(cx, cy, inner, S));
    for (int i = 0; i < seg; i++) { float a = 2 * (float)M_PI * i / seg; v.push_back(V(cx + cosf(a) * rad, cy + sinf(a) * rad, outer, S)); }
    for (int i = 0; i < seg; i++) { idx.push_back(0); idx.push_back(1 + i); idx.push_back(1 + (i + 1) % seg); }
    SDL_RenderGeometry(ren, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}
void Gfx::circle(float cx, float cy, float rad, Col c) { circleGrad(cx, cy, rad, c, c); }
void Gfx::ring(float cx, float cy, float rad, float th, Col c) {
    int seg = std::max(20, std::min(72, (int)(rad * S / 2)));
    std::vector<SDL_Vertex> v; std::vector<int> idx;
    for (int i = 0; i < seg; i++) { float a = 2 * (float)M_PI * i / seg; v.push_back(V(cx + cosf(a) * rad, cy + sinf(a) * rad, c, S)); v.push_back(V(cx + cosf(a) * (rad - th), cy + sinf(a) * (rad - th), c, S)); }
    for (int k = 0; k < seg; k++) { int a = 2 * k, b = 2 * ((k + 1) % seg); idx.insert(idx.end(), { a, a + 1, b, b, a + 1, b + 1 }); }
    SDL_RenderGeometry(ren, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}
void Gfx::line(float x1, float y1, float x2, float y2, float th, Col c) {
    float dx = x2 - x1, dy = y2 - y1, L = sqrtf(dx * dx + dy * dy); if (L < 0.001f) return;
    float nx = -dy / L * th / 2, ny = dx / L * th / 2;
    SDL_Vertex v[4] = { V(x1 + nx, y1 + ny, c, S), V(x2 + nx, y2 + ny, c, S), V(x2 - nx, y2 - ny, c, S), V(x1 - nx, y1 - ny, c, S) };
    int idx[6] = { 0, 1, 2, 0, 2, 3 }; SDL_RenderGeometry(ren, nullptr, v, 4, idx, 6);
}
void Gfx::tri(float x1, float y1, float x2, float y2, float x3, float y3, Col c) { SDL_Vertex v[3] = { V(x1, y1, c, S), V(x2, y2, c, S), V(x3, y3, c, S) }; SDL_RenderGeometry(ren, nullptr, v, 3, nullptr, 0); }

TTF_Font* Gfx::font(int id, int px) {
    auto k = std::make_pair(id, px); auto it = fonts.find(k); if (it != fonts.end()) return it->second;
    const char* f = id == F_TITLE ? "fonts/future-narrow.ttf" : id == F_BOLD ? "fonts/DejaVuSans-Bold.ttf" : "fonts/DejaVuSans.ttf";
    TTF_Font* t = TTF_OpenFont(romfs(f).c_str(), px); fonts[k] = t; return t;
}
Gfx::TxtVal* Gfx::textTex(const std::string& s, int fid, int px, Col c) {
    if (s.empty() || px < 4) return nullptr;
    TxtKey k{ s, fid, px, (uint32_t)(c.r << 24 | c.g << 16 | c.b << 8 | c.a) };
    auto it = txtCache.find(k); if (it != txtCache.end()) { it->second.used = frameNo; return &it->second; }
    TTF_Font* f = font(fid, px); if (!f) return nullptr;
    SDL_Surface* su = TTF_RenderUTF8_Blended(f, s.c_str(), SDL_Color{ c.r, c.g, c.b, 255 }); if (!su) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, su); TxtVal v{ t, su->w, su->h, frameNo }; SDL_FreeSurface(su);
    if (c.a != 255) SDL_SetTextureAlphaMod(t, c.a);
    return &(txtCache[k] = v);
}
// Texto con emojis: los emojis se dibujan con las imágenes (Twemoji) y el resto con la fuente.
struct Seg { std::string s; bool emo; };
static bool emojiCp(uint32_t c) { return (c >= 0x1F000 && c <= 0x1FAFF) || (c >= 0x2600 && c <= 0x27BF && c != 0x2713 && c != 0x271A && c != 0x270E) || c == 0x2B50 || c == 0x2B55 || c == 0x23F1 || c == 0x23F8 || c == 0x231B || c == 0x21A9; }
static void segments(const std::string& str, std::vector<Seg>& out) {
    out.clear(); const unsigned char* s = (const unsigned char*)str.c_str(); size_t i = 0, n = str.size();
    auto dec = [&](size_t at, uint32_t& c) { int k; if (s[at] < 0x80) { c = s[at]; k = 1; } else if ((s[at] >> 5) == 6) { c = s[at] & 31; k = 2; } else if ((s[at] >> 4) == 14) { c = s[at] & 15; k = 3; } else { c = s[at] & 7; k = 4; } for (int j = 1; j < k && at + j < n; j++) c = (c << 6) | (s[at + j] & 63); return k; };
    while (i < n) {
        if (str.compare(i, 4, "@av:") == 0 && i + 4 + AV_N <= n) { out.push_back({ str.substr(i, 4 + AV_N), true }); i += 4 + AV_N; continue; }   // avatar dibujado
        uint32_t c; int k = dec(i, c);
        if (emojiCp(c)) {
            size_t j = i + k; // incluir FE0F, ZWJ y lo que sigue a un ZWJ
            while (j < n) { uint32_t c2; int k2 = dec(j, c2); if (c2 == 0xFE0F || (c2 >= 0x1F3FB && c2 <= 0x1F3FF)) { j += k2; continue; } if (c2 == 0x200D) { j += k2; if (j < n) { j += dec(j, c2); } continue; } break; }
            out.push_back({ str.substr(i, j - i), true }); i = j;
        } else { if (out.empty() || out.back().emo) out.push_back({ "", false }); out.back().s += str.substr(i, k); i += k; }
    }
}
float Gfx::textW(const std::string& s, float size, int fid) {
    if (s.empty()) return 0; std::vector<Seg> sg; segments(s, sg); float w = 0; int px = (int)lroundf(size * S); TTF_Font* f = font(fid, px); if (!f) return 0;
    for (auto& g : sg) { if (g.emo) w += size * 1.2f; else { int tw = 0, th = 0; TTF_SizeUTF8(f, g.s.c_str(), &tw, &th); w += tw / S; } }
    return w;
}
float Gfx::text(const std::string& s, float x, float y, float size, Col c, int fid, int align, float maxW) {
    if (s.empty()) return 0;
    if (maxW > 0) { float w = textW(s, size, fid); if (w > maxW) size = std::max(6.f / S, size * maxW / w); }
    std::vector<Seg> sg; segments(s, sg); float w = textW(s, size, fid), xx = align == AL_C ? x - w / 2 : align == AL_R ? x - w : x;
    int px = (int)lroundf(size * S);
    for (auto& g : sg) {
        if (g.emo) { emoji(g.s, xx + size * 0.6f, y, size * 1.12f, c.a); xx += size * 1.2f; continue; }
        TxtVal* t = textTex(g.s, fid, px, c); if (!t) continue;
        SDL_FRect d{ xx * S, y * S - t->h / 2.f, (float)t->w, (float)t->h }; SDL_RenderCopyF(ren, t->t, nullptr, &d);   // y = centro vertical
        xx += t->w / S;
    }
    return w;
}
void Gfx::textShadow(const std::string& s, float x, float y, float size, Col c, int fid, int align) {
    text(s, x + 2, y + 2.5f, size, rgb(0, 150), fid, align); text(s, x, y, size, c, fid, align);
}
void Gfx::textBox(const std::string& s, Rect r, float size, Col c, int fid, int align) {
    std::vector<std::string> lines; std::stringstream para(s); std::string pl;
    while (std::getline(para, pl)) {
        std::stringstream ws(pl); std::string w, cur;
        while (ws >> w) { std::string t = cur.empty() ? w : cur + " " + w; if (textW(t, size, fid) > r.w && !cur.empty()) { lines.push_back(cur); cur = w; } else cur = t; }
        lines.push_back(cur);
    }
    float lh = size * 1.32f, y = r.y + lh / 2;
    for (auto& l : lines) { if (y > r.y + r.h) break; text(l, align == AL_C ? r.cx() : align == AL_R ? r.x + r.w : r.x, y, size, c, fid, align); y += lh; }
}
SDL_Texture* Gfx::tex(const std::string& p) {
    auto it = texCache.find(p); if (it != texCache.end()) return it->second;
    SDL_Texture* t = IMG_LoadTexture(ren, romfs(p).c_str()); if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    texCache[p] = t; return t;
}
void Gfx::image(SDL_Texture* t, Rect r, uint8_t a) { if (!t) return; SDL_SetTextureAlphaMod(t, a); SDL_FRect d{ r.x * S, r.y * S, r.w * S, r.h * S }; SDL_RenderCopyF(ren, t, nullptr, &d); }
void Gfx::imageTiled(SDL_Texture* t, Rect r, float tileW) {
    if (!t) return; int tw, th; SDL_QueryTexture(t, nullptr, nullptr, &tw, &th); float tileH = tileW * th / tw;
    for (float y = r.y; y < r.y + r.h; y += tileH) for (float x = r.x; x < r.x + r.w; x += tileW) image(t, { x, y, tileW, tileH });
}
std::string emojiFile(const std::string& e) {
    std::string out; const unsigned char* s = (const unsigned char*)e.c_str(); std::vector<uint32_t> cps;
    while (*s) {
        uint32_t c; int n;
        if (*s < 0x80) { c = *s; n = 1; } else if ((*s >> 5) == 6) { c = *s & 31; n = 2; } else if ((*s >> 4) == 14) { c = *s & 15; n = 3; } else { c = *s & 7; n = 4; }
        for (int i = 1; i < n && s[i]; i++) c = (c << 6) | (s[i] & 63);
        s += n; cps.push_back(c);
    }
    bool zwj = std::find(cps.begin(), cps.end(), 0x200d) != cps.end();
    for (auto c : cps) { if (c == 0xfe0f && !zwj) continue; char b[12]; snprintf(b, sizeof b, "%x", c); if (!out.empty()) out += "-"; out += b; }
    return "emoji/" + out + ".png";
}
void Gfx::emoji(const std::string& e, float cx, float cy, float size, uint8_t alpha) {
    if (e.empty()) return;
    if (e.rfind("@av:", 0) == 0) { avatar(e, cx, cy, size, false); return; } SDL_Texture* t = tex(emojiFile(e));
    if (t) image(t, { cx - size / 2, cy - size / 2, size, size }, alpha);
    else { TxtVal* tv = textTex(e, F_BOLD, (int)(size * 0.8f * S), withA(pal::white, alpha)); if (tv) { SDL_FRect d{ cx * S - tv->w / 2.f, cy * S - tv->h / 2.f, (float)tv->w, (float)tv->h }; SDL_RenderCopyF(ren, tv->t, nullptr, &d); } }
}
// ---------------------------------------------------------------- avatares
const int AV_OPTS[AV_N] = { 6, 7, 8, 4, 7, 5, 8, 3, 6, 5, 4 };
const char* const AV_LABEL[AV_N] = { "Piel", "Peinado", "Color de pelo", "Cara", "Accesorio", "Camisa", "Color de camisa", "Pantalón", "Color del pantalón", "Zapatos", "Gesto" };
static const uint32_t AV_SKIN[] = { 0xffdfc4, 0xf5c9a0, 0xe0a370, 0xc68642, 0x8d5524, 0x5c3a21 };
static const uint32_t AV_HAIR[] = { 0x2b1b12, 0x5a3825, 0xb5651d, 0xe2b24a, 0xf3dc8c, 0xc13c1c, 0x9a9aa4, 0x6a3fd6 };
static const uint32_t AV_SHIRT[] = { 0xe53935, 0xf06fa0, 0xfb8c00, 0xfdd835, 0x43a047, 0x29b6f6, 0x3949ab, 0x8e24aa };
static const uint32_t AV_PANTS[] = { 0x3b5998, 0x2b2b2e, 0x2e7d32, 0x6d4c41, 0x78909c, 0xad1457 };
static const uint32_t AV_SHOE[] = { 0xf5f5f5, 0x2b2b2e, 0xe53935, 0xfdd835, 0x1e88e5 };
static const char* const AV_N_SKIN[] = { "Clara", "Durazno", "Trigueña", "Morena", "Oscura", "Muy oscura" };
static const char* const AV_N_HAIR[] = { "Calvo", "Corto", "Largo", "Rulos", "Flequillo", "Cola", "Mohicano" };
static const char* const AV_N_COL[] = { "Negro", "Castaño", "Cobrizo", "Rubio", "Rubio claro", "Pelirrojo", "Gris", "Violeta" };
static const char* const AV_N_FACE[] = { "Limpia", "Bigote", "Barba", "Lentes" };
static const char* const AV_N_HAT[] = { "Ninguno", "Gorra", "Casco", "Sombrero", "Moño", "Corona", "Antenas" };
static const char* const AV_N_SHIRT[] = { "Lisa", "Rayas", "Estrella", "Bolsillo", "Cuello" };
static const char* const AV_N_PANTS[] = { "Pantalón", "Falda", "Short" };
static const char* const AV_N_SHOE[] = { "Blancos", "Negros", "Rojos", "Amarillos", "Azules" };
static const char* const AV_N_MOUTH[] = { "Sonrisa", "Seria", "Sorpresa", "Risa" };
static const char* const AV_N_PCOL[] = { "Jean", "Negro", "Verde", "Café", "Gris", "Magenta" };
std::string avName(int f, int v) {
    static const char* const* T[AV_N] = { AV_N_SKIN, AV_N_HAIR, AV_N_COL, AV_N_FACE, AV_N_HAT, AV_N_SHIRT, AV_N_COL, AV_N_PANTS, AV_N_PCOL, AV_N_SHOE, AV_N_MOUTH };
    static const char* const* SC[] = { nullptr };
    (void)SC;
    if (f == 6) { static const char* const N[] = { "Rojo", "Rosa", "Naranja", "Amarillo", "Verde", "Celeste", "Azul", "Violeta" }; return N[v % 8]; }
    return T[f][v % AV_OPTS[f]];
}
std::string avCode(const int* a) { std::string s = "@av:"; for (int i = 0; i < AV_N; i++) s += (char)('a' + a[i]); return s; }
bool avParse(const std::string& c, int* a) {
    if (c.size() != 4 + AV_N || c.rfind("@av:", 0) != 0) return false;
    for (int i = 0; i < AV_N; i++) { int v = c[4 + i] - 'a'; a[i] = (v < 0 || v >= AV_OPTS[i]) ? 0 : v; }
    return true;
}
void avRandom(int* a) { for (int i = 0; i < AV_N; i++) a[i] = rndInt(AV_OPTS[i]); }
void avDefault(int slot, int* a) {
    static const int D[8][AV_N] = {
        { 1, 1, 1, 1, 2, 1, 4, 0, 4, 0, 0 },   // gorra verde (casco) y bigote
        { 3, 3, 5, 0, 4, 2, 1, 1, 0, 2, 0 },   // moño rosa, estrella
        { 0, 2, 1, 3, 0, 0, 5, 0, 1, 1, 3 },
        { 4, 4, 0, 0, 1, 4, 0, 2, 3, 3, 0 },
        { 2, 5, 3, 0, 5, 1, 2, 1, 5, 4, 3 },
        { 5, 0, 7, 2, 0, 3, 6, 0, 0, 1, 1 },
        { 1, 6, 6, 0, 3, 0, 3, 0, 4, 0, 0 },
        { 3, 1, 2, 0, 6, 2, 7, 1, 2, 2, 2 } };
    for (int i = 0; i < AV_N; i++) a[i] = D[slot & 7][i];
}
void Gfx::avatar(const std::string& code, float cx, float cy, float size, bool full) {
    int a[AV_N]; if (!avParse(code, a)) { avDefault(0, a); }
    Col skin = rgb(AV_SKIN[a[0]]), skinD = mix(skin, rgb(0), 0.18f), hair = rgb(AV_HAIR[a[2]]), shirt = rgb(AV_SHIRT[a[6]]), shirtD = mix(shirt, rgb(0), 0.18f), pants = rgb(AV_PANTS[a[8]]), shoe = rgb(AV_SHOE[a[9]]);
    Col ink = rgb(0x24160e), white = rgb(0xffffff);
    float k, ox, oy;   // origen = parte alta del centro; 100 unidades de alto
    Rect box{ cx - size / 2, cy - size / 2, size, size };
    if (full) { k = size / 104.f; ox = cx; oy = cy - size / 2 + 8 * k; }
    else { k = size / 64.f; ox = cx; oy = cy - size / 2 + 9 * k; clip(&box); }
    auto R = [&](float x, float y, float w, float h, float rad, Col c) { rrect({ ox + x * k, oy + y * k, w * k, h * k }, rad * k, c); };
    auto C = [&](float x, float y, float r, Col c) { circle(ox + x * k, oy + y * k, r * k, c); };
    auto L = [&](float x1, float y1, float x2, float y2, float t, Col c) { line(ox + x1 * k, oy + y1 * k, ox + x2 * k, oy + y2 * k, t * k, c); };
    auto T = [&](float x1, float y1, float x2, float y2, float x3, float y3, Col c) { tri(ox + x1 * k, oy + y1 * k, ox + x2 * k, oy + y2 * k, ox + x3 * k, oy + y3 * k, c); };
    int hs = a[1], hat = a[4], sh = a[5], ps = a[7];
    if (full) rrect({ cx - 22 * k, oy + 95 * k, 44 * k, 7 * k }, 3.5f * k, rgb(0, 50));   // sombra en el piso
    // pelo de atrás
    if (hs == 2) R(-24, 10, 48, 46, 14, hair);
    if (hs == 5) { C(-23, 20, 7, hair); C(23, 20, 7, hair); }
    if (hs == 3) { for (int i = 0; i < 7; i++) { float an = 3.14159f * (0.9f + i * 0.2f); C(cosf(an) * 22, 22 + sinf(an) * 21, 8, hair); } }
    if (full) {
        // piernas, zapatos
        if (ps == 1) { R(-9, 74, 7, 18, 3, skin); R(2, 74, 7, 18, 3, skin); }
        else if (ps == 2) { R(-12, 74, 10, 8, 2, pants); R(2, 74, 10, 8, 2, pants); R(-9, 82, 7, 10, 3, skin); R(2, 82, 7, 10, 3, skin); }
        else { R(-13, 70, 12, 22, 4, pants); R(1, 70, 12, 22, 4, pants); }
        R(-14, 90, 13, 8, 4, shoe); R(1, 90, 13, 8, 4, shoe);
        if (ps == 1) R(-19, 70, 38, 15, 6, pants);
    }
    // brazos
    R(-28, 47, 11, 26, 5, shirtD); R(17, 47, 11, 26, 5, shirtD);
    if (full) { C(-22.5f, 75, 5, skin); C(22.5f, 75, 5, skin); }
    // torso
    R(-18, 44, 36, 31, 8, shirt);
    if (sh == 1) { Col st = mix(shirt, white, 0.75f); for (int i = 0; i < 4; i++) R(-16, 50 + i * 7, 32, 3, 0, st); }
    if (sh == 2) { Col gd = rgb(0xffd23f); T(0, 51, 7, 66, -7, 66, gd); T(0, 70, 7, 55, -7, 55, gd); }
    if (sh == 3) { R(2, 56, 11, 11, 3, shirtD); L(2, 59, 13, 59, 1.4f, mix(shirt, white, 0.5f)); }
    if (sh == 4) { T(-9, 44, 9, 44, 0, 58, white); T(-9, 44, 0, 58, -3, 44, shirt); }
    // cuello y cabeza
    R(-6, 38, 12, 10, 3, skinD);
    C(-21, 27, 4.5f, skin); C(21, 27, 4.5f, skin);   // orejas
    bool bald = hs == 0;
    if (!bald) { C(0, 21, 23.5f, hair); if (hs == 6) { R(-5, -6, 10, 14, 4, hair); } }
    C(0, bald ? 25 : 26, bald ? 22 : 20, skin);
    if (hs == 4) { R(-20, 6, 40, 13, 7, hair); C(14, 16, 8, hair); }
    if (hs == 1 || hs == 5 || hs == 2 || hs == 3) { T(-20, 14, 20, 14, 0, 4, hair); R(-20, 6, 40, 9, 4, hair); }
    // cara
    int face = a[3], mouth = a[10];
    if (face == 2) { clip(nullptr); Rect cl{ ox - 24 * k, oy + 32 * k, 48 * k, 20 * k }; if (!full) { Rect inter{ std::max(cl.x, box.x), std::max(cl.y, box.y), 0, 0 }; inter.w = std::min(cl.x + cl.w, box.x + box.w) - inter.x; inter.h = std::min(cl.y + cl.h, box.y + box.h) - inter.y; clip(&inter); } else clip(&cl);
        C(0, bald ? 25 : 26, bald ? 22 : 20, hair); if (full) clip(nullptr); else clip(&box); }
    float ey = 27;
    C(-8, ey, 2.8f, ink); C(8, ey, 2.8f, ink); C(-7.3f, ey - 1, 1, white); C(8.7f, ey - 1, 1, white);
    C(-14, 33, 3.6f, rgb(0xff7a7a, 90)); C(14, 33, 3.6f, rgb(0xff7a7a, 90));
    if (face == 1) { C(-3.6f, 34.5f, 3.4f, hair); C(3.6f, 34.5f, 3.4f, hair); }
    Col mc = rgb(0x7a2a1c);
    if (mouth == 0) { L(-5, 39, -2, 41.5f, 1.8f, mc); L(-2, 41.5f, 2, 41.5f, 1.8f, mc); L(2, 41.5f, 5, 39, 1.8f, mc); }
    else if (mouth == 1) L(-4, 41, 4, 41, 1.8f, mc);
    else if (mouth == 2) { C(0, 41, 3.4f, mc); C(0, 41, 1.8f, rgb(0xd9605a)); }
    else { C(0, 39.5f, 6, mc); R(-7, 33, 14, 6.5f, 0, face == 2 ? hair : skin); C(0, 41.5f, 3, rgb(0xd9605a)); if (face == 1) { C(-4, 35, 4.2f, hair); C(4, 35, 4.2f, hair); } }
    if (face == 3) { Col f = rgb(0x2b2b2e); ring(ox - 8 * k, oy + ey * k, 6.4f * k, 1.7f * k, f); ring(ox + 8 * k, oy + ey * k, 6.4f * k, 1.7f * k, f); L(-1.6f, ey, 1.6f, ey, 1.6f, f); }
    // accesorios de cabeza
    Col hc = shirt;
    if (hat == 1 || hat == 2) {
        Rect cl{ ox - 26 * k, oy - 6 * k, 52 * k, 20 * k }; Rect use = cl; if (!full) { use.x = std::max(cl.x, box.x); use.y = std::max(cl.y, box.y); use.w = std::min(cl.x + cl.w, box.x + box.w) - use.x; use.h = std::min(cl.y + cl.h, box.y + box.h) - use.y; }
        clip(&use); C(0, 21, 24.5f, hc); if (hat == 2) { R(-3, -4, 6, 20, 0, white); }
        if (full) clip(nullptr); else clip(&box);
        if (hat == 1) R(-4, 11, 30, 5, 2.5f, mix(hc, rgb(0), 0.15f)); else R(-22, 12, 44, 4, 2, mix(hc, rgb(0), 0.2f));
    }
    if (hat == 3) { R(-31, 10, 62, 6, 3, mix(hc, rgb(0), 0.1f)); R(-15, -9, 30, 21, 6, hc); R(-15, 5, 30, 4, 0, rgb(0x2b2b2e)); }
    if (hat == 4) { Col bw = rgb(0xff5fa2); T(8, 6, 26, -2, 26, 14, bw); T(8, 6, -10, -2, -10, 14, bw); C(8, 6, 4, rgb(0xd81b60)); }
    if (hat == 5) { Col g1 = rgb(0xffd23f); R(-14, 2, 28, 9, 2, g1); T(-14, 3, -8, 3, -14, -8, g1); T(-6, 3, 6, 3, 0, -11, g1); T(8, 3, 14, 3, 14, -8, g1); C(0, 6.5f, 2, rgb(0xe53935)); }
    if (hat == 6) { L(-9, 0, -15, -11, 1.6f, ink); L(9, 0, 15, -11, 1.6f, ink); C(-15, -11, 3.4f, rgb(0xe53935)); C(15, -11, 3.4f, rgb(0xe53935)); }
    if (!full) clip(nullptr);
}

void Gfx::clip(const Rect* r) {
    if (!r) { SDL_RenderSetClipRect(ren, nullptr); return; }
    SDL_Rect c{ (int)(r->x * S), (int)(r->y * S), (int)ceilf(r->w * S), (int)ceilf(r->h * S) }; SDL_RenderSetClipRect(ren, &c);
}
bool Gfx::savePNG(const std::string& path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ABGR8888); if (!s) return false;
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ABGR8888, s->pixels, s->pitch);
    int r = IMG_SavePNG(s, path.c_str()); SDL_FreeSurface(s); return r == 0;
}
void Gfx::woodTable() {
    SDL_Texture* w = tex("img/wood.jpg");
    if (w) { int tw, th; SDL_QueryTexture(w, nullptr, nullptr, &tw, &th); float sc = std::max(1280.f / tw, 720.f / th); image(w, { 640 - tw * sc / 2, 360 - th * sc / 2, tw * sc, th * sc }); }
    else rect({ 0, 0, 1280, 720 }, pal::wood);
    // viñeta suave como en la versión de PC
    gradRect({ 0, 0, 1280, 140 }, rgb(0x140800, 120), rgb(0x140800, 0));
    gradRect({ 0, 580, 1280, 140 }, rgb(0x140800, 0), rgb(0x140800, 140));
}

// ================================================================ ENTRADA
#ifdef __SWITCH__
static PadState npads[Input::MAXP];
// sensor de movimiento (giroscopio) de cada mando, para el cursor opcional de cada jugador
static HidSixAxisSensorHandle sixH[Input::MAXP][2]; static int sixN[Input::MAXP]; static u32 sixStyle[Input::MAXP]; static uint32_t sixLast = 0;
#endif
void Input::init() {
#ifdef __SWITCH__
    padConfigureInput(MAXP, HidNpadStyleSet_NpadStandard);
    hidSetNpadJoyHoldType(HidNpadJoyHoldType_Vertical);   // un solo Joy-Con en vertical por jugador (un Pro Controller o un par de Joy-Con también sirven)
    for (int i = 0; i < MAXP; i++) hidSetNpadJoyAssignmentModeSingleByDefault((HidNpadIdType)(HidNpadIdType_No1 + i));
    padInitialize(&npads[0], HidNpadIdType_No1, HidNpadIdType_Handheld);
    for (int i = 1; i < MAXP; i++) padInitialize(&npads[i], (HidNpadIdType)(HidNpadIdType_No1 + i));
    hidInitializeTouchScreen();
#else
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}
bool Input::dualPaired() const {
#ifdef __SWITCH__
    for (int i = 0; i < MAXP; i++) if (padIsConnected(&npads[i]) && (padGetStyleSet(&npads[i]) & HidNpadStyleTag_NpadJoyDual)) return true;
#endif
    return false;
}
void Input::openControllerApplet(int players) {
#ifdef __SWITCH__
    HidLaControllerSupportArg arg; hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = 1; arg.hdr.player_count_max = (s8)std::max(1, std::min(MAXP, players));
    arg.hdr.enable_permit_joy_dual = 0; arg.hdr.enable_single_mode = 1; arg.hdr.enable_take_over_connection = 1; arg.hdr.enable_left_justify = 1;
    HidLaControllerSupportResultInfo info; hidLaShowControllerSupport(&info, &arg);
#else
    (void)players;
#endif
}
void Input::poll(Gfx& g) {
    touchDown = touchUp = false; anyActivity = false;
    for (auto& p : pads) for (int b = 0; b < B_COUNT; b++) { p.down[b] = p.rep[b] = false; }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quit = true; break;
        case SDL_FINGERDOWN: touchDown = touchHeld = true; tx = e.tfinger.x * g.outW / g.S; ty = e.tfinger.y * g.outH / g.S; anyActivity = true; break;
        case SDL_FINGERMOTION: tx = e.tfinger.x * g.outW / g.S; ty = e.tfinger.y * g.outH / g.S; break;
        case SDL_FINGERUP: touchUp = true; touchHeld = false; tx = e.tfinger.x * g.outW / g.S; ty = e.tfinger.y * g.outH / g.S; break;
#ifndef __SWITCH__
        case SDL_MOUSEBUTTONDOWN: if (e.button.which != SDL_TOUCH_MOUSEID) { touchDown = touchHeld = true; tx = e.button.x / g.S; ty = e.button.y / g.S; anyActivity = true; } break;
        case SDL_MOUSEMOTION: if (e.motion.which != SDL_TOUCH_MOUSEID) { tx = e.motion.x / g.S; ty = e.motion.y / g.S; } break;
        case SDL_MOUSEBUTTONUP: if (e.button.which != SDL_TOUCH_MOUSEID) { touchUp = true; touchHeld = false; tx = e.button.x / g.S; ty = e.button.y / g.S; } break;
        case SDL_CONTROLLERDEVICEADDED: { SDL_GameController* c = SDL_GameControllerOpen(e.cdevice.which); if (c) gcs.push_back(c); break; }
        case SDL_KEYDOWN: case SDL_KEYUP: {
            bool d = e.type == SDL_KEYDOWN; int b = -1;
            switch (e.key.keysym.sym) {
            case SDLK_RETURN: case SDLK_SPACE: case SDLK_z: b = B_A; break; case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_x: b = B_B; break;
            case SDLK_c: b = B_X; break; case SDLK_v: b = B_Y; break; case SDLK_q: b = B_L; break; case SDLK_w: b = B_R; break;
            case SDLK_p: b = B_PLUS; break; case SDLK_m: b = B_MINUS; break;
            case SDLK_UP: b = B_UP; break; case SDLK_DOWN: b = B_DOWN; break; case SDLK_LEFT: b = B_LEFT; break; case SDLK_RIGHT: b = B_RIGHT; break;
            }
            if (b >= 0) kHeld[b] = d; break;
        }
#endif
        }
    }
    uint32_t now = SDL_GetTicks();
    auto setPad = [&](Pad& p, bool conn, const bool* h, float lx, float ly) {
        p.connected = conn; p.lx = lx; p.ly = ly;
        for (int b = 0; b < B_COUNT; b++) {
            bool was = p.held[b]; p.held[b] = h[b]; p.down[b] = h[b] && !was;
            if (p.down[b]) { p.holdT[b] = now; p.rep[b] = true; anyActivity = true; }
            else if (h[b] && b >= B_UP && now - p.holdT[b] > 460) { p.rep[b] = true; p.holdT[b] = now - 460 + 230; }
        }
    };
#ifdef __SWITCH__
    for (int i = 0; i < MAXP; i++) {
        padUpdate(&npads[i]); hw[i] = padIsConnected(&npads[i]); if (!hw[i] && owner[i] >= 0) continue;   // casilla ocupada por un celular
        u64 k = padGetButtons(&npads[i]); bool h[B_COUNT] = {};
        u32 style = padGetStyleSet(&npads[i]); const u32 FULLS = HidNpadStyleTag_NpadFullKey | HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual | HidNpadStyleTag_NpadSystem | HidNpadStyleTag_NpadSystemExt;
        if (padIsConnected(&npads[i])) pads[i].kind = (style & FULLS) ? Pad::PK_FULL : (style & HidNpadStyleTag_NpadJoyLeft) ? Pad::PK_LEFT : (style & HidNpadStyleTag_NpadJoyRight) ? Pad::PK_RIGHT : Pad::PK_FULL;
        pads[i].vert = pads[i].single();   // orientación vertical fija
        const float T = 16000.f; float lx = 0, ly = 0;
        if (pads[i].kind == Pad::PK_LEFT && pads[i].vert) {
            // Joy-Con izquierdo en vertical (como viene): palanca arriba y los 4 botones de flechas abajo
            HidAnalogStickState st = padGetStickPos(&npads[i], 0);
            h[B_A] = k & HidNpadButton_Right; h[B_B] = k & HidNpadButton_Down; h[B_X] = k & HidNpadButton_Up; h[B_Y] = k & HidNpadButton_Left;
            h[B_L] = k & HidNpadButton_L; h[B_R] = k & HidNpadButton_ZL; h[B_PLUS] = k & HidNpadButton_Minus; h[B_MINUS] = k & HidNpadButton_StickL;
            h[B_UP] = st.y > T; h[B_DOWN] = st.y < -T; h[B_LEFT] = st.x < -T; h[B_RIGHT] = st.x > T; lx = st.x / 32767.f; ly = -st.y / 32767.f;
        } else if (pads[i].kind == Pad::PK_RIGHT && pads[i].vert) {
            // Joy-Con derecho en vertical
            HidAnalogStickState st = padGetStickPos(&npads[i], 1);
            h[B_A] = k & HidNpadButton_A; h[B_B] = k & HidNpadButton_B; h[B_X] = k & HidNpadButton_X; h[B_Y] = k & HidNpadButton_Y;
            h[B_L] = k & HidNpadButton_R; h[B_R] = k & HidNpadButton_ZR; h[B_PLUS] = k & HidNpadButton_Plus; h[B_MINUS] = k & HidNpadButton_StickR;
            h[B_UP] = st.y > T; h[B_DOWN] = st.y < -T; h[B_LEFT] = st.x < -T; h[B_RIGHT] = st.x > T; lx = st.x / 32767.f; ly = -st.y / 32767.f;
        } else if (pads[i].kind == Pad::PK_LEFT) {
            // Joy-Con izquierdo de costado (girado a la izquierda, rieles arriba): palanca a la izquierda y los 4 botones a la derecha
            HidAnalogStickState st = padGetStickPos(&npads[i], 0);
            h[B_A] = k & HidNpadButton_Down; h[B_B] = k & HidNpadButton_Left; h[B_X] = k & HidNpadButton_Right; h[B_Y] = k & HidNpadButton_Up;
            h[B_L] = k & HidNpadButton_LeftSL; h[B_R] = k & HidNpadButton_LeftSR; h[B_PLUS] = k & HidNpadButton_Minus; h[B_MINUS] = k & (HidNpadButton_ZL | HidNpadButton_L);
            h[B_UP] = st.x > T; h[B_DOWN] = st.x < -T; h[B_LEFT] = st.y > T; h[B_RIGHT] = st.y < -T; lx = -st.y / 32767.f; ly = -st.x / 32767.f;
        } else if (pads[i].kind == Pad::PK_RIGHT) {
            // Joy-Con derecho de costado (girado a la derecha, rieles arriba)
            HidAnalogStickState st = padGetStickPos(&npads[i], 1);
            h[B_A] = k & HidNpadButton_A; h[B_B] = k & HidNpadButton_B; h[B_X] = k & HidNpadButton_X; h[B_Y] = k & HidNpadButton_Y;
            h[B_L] = k & HidNpadButton_RightSL; h[B_R] = k & HidNpadButton_RightSR; h[B_PLUS] = k & HidNpadButton_Plus; h[B_MINUS] = k & (HidNpadButton_ZR | HidNpadButton_R);
            h[B_UP] = st.x < -T; h[B_DOWN] = st.x > T; h[B_LEFT] = st.y < -T; h[B_RIGHT] = st.y > T; lx = st.y / 32767.f; ly = st.x / 32767.f;
        } else {
            h[B_A] = k & HidNpadButton_A; h[B_B] = k & HidNpadButton_B; h[B_X] = k & HidNpadButton_X; h[B_Y] = k & HidNpadButton_Y;
            h[B_L] = k & HidNpadButton_L; h[B_R] = k & HidNpadButton_R; h[B_ZL] = k & HidNpadButton_ZL; h[B_ZR] = k & HidNpadButton_ZR;
            h[B_PLUS] = k & HidNpadButton_Plus; h[B_MINUS] = k & HidNpadButton_Minus;
            h[B_UP] = k & (HidNpadButton_Up | HidNpadButton_StickLUp); h[B_DOWN] = k & (HidNpadButton_Down | HidNpadButton_StickLDown);
            h[B_LEFT] = k & (HidNpadButton_Left | HidNpadButton_StickLLeft); h[B_RIGHT] = k & (HidNpadButton_Right | HidNpadButton_StickLRight);
            HidAnalogStickState st = padGetStickPos(&npads[i], 0); lx = st.x / 32767.f; ly = -st.y / 32767.f;
        }
        setPad(pads[i], padIsConnected(&npads[i]), h, lx, ly);
        pads[i].gx = pads[i].gy = 0; pads[i].gyroOk = false;
        if (padIsConnected(&npads[i])) {
            if (style != sixStyle[i]) {
                for (int k = 0; k < sixN[i]; k++) hidStopSixAxisSensor(sixH[i][k]);
                sixN[i] = 0; sixStyle[i] = style;
                HidNpadStyleTag tag = (style & HidNpadStyleTag_NpadJoyLeft) ? HidNpadStyleTag_NpadJoyLeft : (style & HidNpadStyleTag_NpadJoyRight) ? HidNpadStyleTag_NpadJoyRight
                    : (style & HidNpadStyleTag_NpadJoyDual) ? HidNpadStyleTag_NpadJoyDual : (style & HidNpadStyleTag_NpadHandheld) ? HidNpadStyleTag_NpadHandheld : HidNpadStyleTag_NpadFullKey;
                HidNpadIdType id = tag == HidNpadStyleTag_NpadHandheld ? HidNpadIdType_Handheld : (HidNpadIdType)(HidNpadIdType_No1 + i);
                int n = (tag == HidNpadStyleTag_NpadJoyDual || tag == HidNpadStyleTag_NpadHandheld) ? 2 : 1;
                if (R_SUCCEEDED(hidGetSixAxisSensorHandles(sixH[i], n, id, tag))) { for (int k = 0; k < n; k++) hidStartSixAxisSensor(sixH[i][k]); sixN[i] = n; }
            }
            if (sixN[i] > 0) {
                HidSixAxisSensorState st; memset(&st, 0, sizeof st);
                if (hidGetSixAxisSensorStates(sixH[i][sixN[i] - 1], &st, 1) > 0) {
                    float dt = sixLast ? std::min(0.05f, (now - sixLast) / 1000.f) : 1 / 60.f;
                    auto dz = [](float v) { return fabsf(v) < 0.012f ? 0.f : v; };   // zona muerta: el cursor no se arrastra solo
                    // velocidad angular en vueltas por segundo -> grados en este cuadro (giro a los lados = eje z, arriba/abajo = eje x)
                    pads[i].gx = -dz(st.angular_velocity.z) * 360.f * dt; pads[i].gy = -dz(st.angular_velocity.x) * 360.f * dt; pads[i].gyroOk = true;
                }
            }
        } else if (sixN[i]) { for (int k = 0; k < sixN[i]; k++) hidStopSixAxisSensor(sixH[i][k]); sixN[i] = 0; sixStyle[i] = 0; }
    }
    sixLast = now;
#else
    // PC: teclado = mando 1; mandos USB = 2, 3, 4...
    setPad(pads[0], true, kHeld, 0, 0);
    for (int i = 1; i < MAXP; i++) {
        bool h[B_COUNT] = {}; bool conn = i - 1 < (int)gcs.size(); float lx = 0, ly = 0; hw[i] = conn;
        if (!conn && owner[i] >= 0) continue;
        if (conn) {
            SDL_GameController* c = gcs[i - 1];
            auto B = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(c, b) != 0; };
            lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f; ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
            h[B_A] = B(SDL_CONTROLLER_BUTTON_B); h[B_B] = B(SDL_CONTROLLER_BUTTON_A); h[B_X] = B(SDL_CONTROLLER_BUTTON_Y); h[B_Y] = B(SDL_CONTROLLER_BUTTON_X);
            h[B_L] = B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); h[B_R] = B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); h[B_PLUS] = B(SDL_CONTROLLER_BUTTON_START); h[B_MINUS] = B(SDL_CONTROLLER_BUTTON_BACK);
            h[B_UP] = B(SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -0.6f; h[B_DOWN] = B(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > 0.6f;
            h[B_LEFT] = B(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -0.6f; h[B_RIGHT] = B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > 0.6f;
        }
        setPad(pads[i], conn, h, lx, ly);
    }
    hw[0] = true;
#endif
    pollPhones();
}
void Input::pollPhones() {
    if (!net || !net->running) return;
    uint32_t now = SDL_GetTicks(); net->poll(now);
    for (int s = 0; s < MAXP; s++) {   // soltar casillas: el celular se fue o un mando físico ocupó la casilla
        int ci = owner[s]; if (ci < 0) continue;
        if (net->cl[ci].fd < 0 || !net->cl[ci].joined || hw[s] || net->cl[ci].slot != s) {
            bool z[B_COUNT] = {}; pads[s].connected = false; for (int b = 0; b < B_COUNT; b++) { pads[s].held[b] = false; pads[s].down[b] = false; pads[s].rep[b] = false; } (void)z;
            if (net->cl[ci].slot == s) net->cl[ci].slot = -1; owner[s] = -1;
        }
    }
    for (int ci = 0; ci < Net::MAXC; ci++) {
        Net::Client& c = net->cl[ci]; if (c.fd < 0 || !c.joined || c.slot >= 0) continue;
        for (int s = 1; s < MAXP; s++) if (!hw[s] && owner[s] < 0) { owner[s] = ci; c.slot = s; pads[s].kind = 0; pads[s].vert = false; anyActivity = true; break; }
    }
    for (int s = 1; s < MAXP; s++) {
        int ci = owner[s]; if (ci < 0) continue; Net::Client& c = net->cl[ci];
        bool h[B_COUNT] = {}; for (int b = 0; b < B_COUNT; b++) h[b] = (c.mask >> b) & 1;
        float lx = (h[B_RIGHT] ? 1.f : 0.f) - (h[B_LEFT] ? 1.f : 0.f), ly = (h[B_DOWN] ? 1.f : 0.f) - (h[B_UP] ? 1.f : 0.f);
        pads[s].kind = 0; pads[s].vert = false;
        // setPad es local de poll(): se repite aquí con la misma lógica de bordes y autorrepetición
        Pad& p = pads[s]; p.connected = true; p.lx = lx; p.ly = ly;
        for (int b = 0; b < B_COUNT; b++) { bool was = p.held[b]; p.held[b] = h[b]; p.down[b] = h[b] && !was;
            if (p.down[b]) { p.holdT[b] = now; p.rep[b] = true; anyActivity = true; }
            else if (h[b] && b >= B_UP && now - p.holdT[b] > 460) { p.rep[b] = true; p.holdT[b] = now - 460 + 230; } }
    }
}

// ================================================================ INTERFAZ
void UI::begin(Gfx& gfx, Input& inp, int pad) {
    g = &gfx; in = &inp; activePad = pad; prev.swap(cur); cur.clear(); tapUsed = false;
    if (in->touchDown) { sx = in->tx; sy = in->ty; padNav = false; }
    if (pad < 0 || pad >= Input::MAXP) return;
    const Pad& p = in->pads[pad];
    int dir = p.rep[B_UP] ? 0 : p.rep[B_RIGHT] ? 1 : p.rep[B_DOWN] ? 2 : p.rep[B_LEFT] ? 3 : -1;
    if (dir < 0 && !p.down[B_A]) return;
    const Item* f = nullptr; for (auto& it : prev) if (it.id == focus) f = &it;
    if (!padNav) { padNav = true; if (f && dir >= 0) return; }   // la primera pulsación solo muestra dónde está el foco
    if (!f) { for (auto& it : prev) if (it.enabled) { focus = it.id; break; } return; }
    if (dir < 0) return;
    float best = 1e9f; uint32_t bid = 0;
    for (auto& it : prev) {
        if (it.id == f->id || !it.enabled) continue;
        float dx = it.r.cx() - f->r.cx(), dy = it.r.cy() - f->r.cy();
        float along = dir == 0 ? -dy : dir == 1 ? dx : dir == 2 ? dy : -dx, across = (dir == 0 || dir == 2) ? fabsf(dx) : fabsf(dy);
        // solapamiento en el eje cruzado: preferir los que están "en la misma fila/columna"
        float ovl = (dir == 0 || dir == 2) ? std::min(it.r.x + it.r.w, f->r.x + f->r.w) - std::max(it.r.x, f->r.x) : std::min(it.r.y + it.r.h, f->r.y + f->r.h) - std::max(it.r.y, f->r.y);
        if (along <= 1) continue;
        float sc = along + across * (ovl > 0 ? 0.3f : 2.2f);
        if (sc < best) { best = sc; bid = it.id; }
    }
    if (bid) focus = bid;
}
void UI::end() {}
bool UI::padDown(int b) const { return activePad >= 0 && activePad < Input::MAXP && in->pads[activePad].down[b]; }
bool UI::back() const { return padDown(B_B); }
void UI::setFocusIfNone(uint32_t id) { for (auto& it : prev) if (it.id == focus) return; focus = id; }
bool UI::hit(uint32_t id, Rect r, bool enabled) {
    cur.push_back({ id, r, enabled, g && g->inLayer }); if (!enabled) return false;
    if (in->touchUp && !tapUsed && r.in(in->tx, in->ty) && r.in(sx, sy) && fabsf(in->tx - sx) + fabsf(in->ty - sy) < 18) { tapUsed = true; focus = id; return true; }
    if (padNav && focus == id && padDown(B_A)) { bool seen = false; for (auto& it : prev) if (it.id == id) seen = true; if (seen) return true; }
    return false;
}
void UI::drawFocus(Rect r, float rad) {
    float t = SDL_GetTicks() / 1000.f, a = 0.8f + 0.2f * sinf(t * 6);
    g->rrect(r, rad, rgb(0xffe27a, (uint8_t)(55 * a)));                                              // relleno suave
    g->rrectLine({ r.x - 7, r.y - 7, r.w + 14, r.h + 14 }, rad + 7, 5, rgb(0x000000, 190));          // halo oscuro (se ve sobre fondos claros)
    g->rrectLine({ r.x - 5, r.y - 5, r.w + 10, r.h + 10 }, rad + 5, 5, withA(pal::focus, (uint8_t)(255 * a)));
    g->rrectLine({ r.x - 1, r.y - 1, r.w + 2, r.h + 2 }, rad + 1, 2, rgb(0xffffff, (uint8_t)(230 * a)));
}
bool UI::button(const std::string& label, Rect r, int style, bool enabled, const std::string& icon) {
    uint32_t id = hid(label + (icon.empty() ? "" : icon), (int)(r.x * 7 + r.y));
    bool pressed = enabled && in->touchHeld && r.in(in->tx, in->ty) && r.in(sx, sy);
    bool clicked = hit(id, r, enabled);
    Col top, bot, txt, edge;
    switch (style) {
    case 1: top = rgb(0xf26a55); bot = rgb(0xc8331f); txt = pal::white; edge = rgb(0x7a1d12); break;
    case 2: top = rgb(0x5a3a22); bot = rgb(0x3a2414); txt = pal::cream; edge = rgb(0x8a6234); break;
    case 3: top = rgb(0xffe08a); bot = rgb(0xe8b84a); txt = pal::brownText; edge = rgb(0x8a6234); break;
    default: top = rgb(0xfff4d6); bot = rgb(0xead39c); txt = pal::brownText; edge = rgb(0x5a3a1c);
    }
    if (!enabled) { top = mix(top, rgb(0x808080), 0.55f); bot = mix(bot, rgb(0x707070), 0.55f); txt = withA(txt, 150); }
    float off = pressed ? 2 : 0, rad = std::min(14.f, r.h / 3);
    g->rrect({ r.x, r.y + 4, r.w, r.h }, rad, rgb(0, 90));
    g->rrect({ r.x, r.y + off, r.w, r.h }, rad, edge);
    g->rrectGrad({ r.x + 2, r.y + 2 + off, r.w - 4, r.h - 4 }, rad - 2, top, bot);
    float fs = std::min(r.h * 0.42f, 22.f), cx = r.cx();
    if (!icon.empty()) {
        float tw = label.empty() ? 0 : g->textW(label, fs, F_TITLE) + 8; float es = fs * 1.35f, x0 = cx - (tw + es) / 2;
        g->emoji(icon, x0 + es / 2, r.cy() + off, es); if (!label.empty()) g->text(label, x0 + es + 8, r.cy() + off, fs, txt, F_TITLE, AL_L, r.w - es - 24);
    } else g->text(label, cx, r.cy() + off, fs, txt, F_TITLE, AL_C, r.w - 16);
    if (focused(id)) drawFocus(r, rad);
    return clicked;
}
bool UI::card(const std::string& title, const std::string& sub, const std::string& emo, Rect r, bool enabled, bool selected) {
    uint32_t id = hid("card:" + title); bool clicked = hit(id, r, enabled);
    g->rrect({ r.x, r.y + 5, r.w, r.h }, 14, rgb(0, 100));
    g->rrect(r, 14, selected ? pal::gold : rgb(0x5a3a1c));
    g->rrectGrad(r.inset(3), 11, enabled ? rgb(0xfff4d6) : rgb(0xbfb3a0), enabled ? rgb(0xecd6a0) : rgb(0xa89a85));
    float es = std::min(r.h * 0.42f, 64.f);
    g->emoji(emo, r.cx(), r.y + r.h * 0.36f, es, enabled ? 255 : 140);
    g->text(title, r.cx(), r.y + r.h * 0.72f, std::min(18.f, r.h * 0.15f), enabled ? pal::brownText : rgb(0x5a5040), F_BOLD, AL_C, r.w - 10);
    if (!sub.empty()) g->text(sub, r.cx(), r.y + r.h * 0.87f, std::min(13.f, r.h * 0.11f), enabled ? rgb(0x7a5a3a) : rgb(0x6a6050), F_BODY, AL_C, r.w - 10);
    if (focused(id)) drawFocus(r, 14);
    return clicked;
}
bool UI::stepper(const std::string& label, const std::string& value, Rect r, int& delta) {
    delta = 0; uint32_t id = hid("step:" + label);
    Rect lb{ r.x + r.w * 0.52f, r.y + 4, 44, r.h - 8 }, rb{ r.x + r.w - 48, r.y + 4, 44, r.h - 8 };
    bool c = hit(id, r, true);
    if (in->touchUp && !tapUsed) { if (lb.in(in->tx, in->ty)) { delta = -1; tapUsed = true; focus = id; } else if (rb.in(in->tx, in->ty)) { delta = 1; tapUsed = true; focus = id; } }
    if (focused(id)) { if (padDown(B_LEFT)) delta = -1; if (padDown(B_RIGHT)) delta = 1; if (c) delta = 1; }
    else if (c) delta = 1;
    g->text(label, r.x + 14, r.cy(), 17, pal::cream, F_BODY, AL_L, r.w * 0.5f - 14);
    g->rrect({ lb.x, lb.y, rb.x + rb.w - lb.x, lb.h }, 10, rgb(0x2a180c));
    g->text("◀", lb.cx(), lb.cy(), 18, pal::gold, F_BOLD, AL_C); g->text("▶", rb.cx(), rb.cy(), 18, pal::gold, F_BOLD, AL_C);
    g->text(value, (lb.x + lb.w + rb.x) / 2, r.cy(), 16, pal::white, F_BOLD, AL_C, rb.x - lb.x - lb.w - 6);
    g->line(r.x + 8, r.y + r.h, r.x + r.w - 8, r.y + r.h, 1, rgb(0xb98a4a, 70));
    if (focused(id)) drawFocus(r, 8);
    return delta != 0;
}
bool UI::toggle(const std::string& label, bool& v, Rect r) {
    uint32_t id = hid("tog:" + label); bool c = hit(id, r, true); if (c) v = !v;
    g->text(label, r.x + 14, r.cy(), 17, pal::cream, F_BODY, AL_L, r.w - 90);
    Rect b{ r.x + r.w - 46, r.cy() - 15, 30, 30 }; g->rrect(b, 6, pal::cream);
    if (v) { g->line(b.x + 7, b.cy(), b.x + 13, b.y + 23, 4, pal::green); g->line(b.x + 13, b.y + 23, b.x + 24, b.y + 7, 4, pal::green); }
    g->line(r.x + 8, r.y + r.h, r.x + r.w - 8, r.y + r.h, 1, rgb(0xb98a4a, 70));
    if (focused(id)) drawFocus(r, 8);
    return c;
}

// ================================================================ SONIDO
void Audio::init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    Mix_Init(MIX_INIT_OGG); ok = true; Mix_AllocateChannels(16);
    for (const char* n : { "click", "dice", "flip", "lose", "place", "win" }) { Mix_Chunk* c = Mix_LoadWAV(romfs(std::string("snd/") + n + ".ogg").c_str()); if (c) { Mix_VolumeChunk(c, (int)(MIX_MAX_VOLUME * (std::string(n) == "win" || std::string(n) == "lose" ? 0.6 : 0.8))); sfx[n] = c; } }
    tracks = { "mus/juego-de-mesa.ogg", "mus/tarde-tranquila.ogg", "mus/sala-de-juegos.ogg" };
    Mix_VolumeMusic((int)(MIX_MAX_VOLUME * 0.42));
}
void Audio::play(const std::string& n) { if (!ok || !sfxOn) return; auto it = sfx.find(n); if (it != sfx.end()) Mix_PlayChannel(-1, it->second, 0); }
void Audio::setMusic(bool on) { musicOn = on; if (!ok) return; if (!on) { Mix_FadeOutMusic(800); } else pauseUntil = 0; }
void Audio::update() {
    // lista al azar de canciones tranquilas, con una pausa de silencio entre una y otra, sin repetir la misma dos veces seguidas
    if (!ok || !musicOn || Mix_PlayingMusic()) return;
    uint32_t now = SDL_GetTicks();
    if (pauseUntil == 0) { pauseUntil = now + (cur ? 15000 + rndInt(20000) : 600); return; }
    if (now < pauseUntil) return;
    pauseUntil = 0;
    if (pos >= (int)order.size()) { int last = order.empty() ? -1 : order.back(); order.clear(); for (int i = 0; i < (int)tracks.size(); i++) order.push_back(i); std::shuffle(order.begin(), order.end(), RNG); if (order[0] == last && order.size() > 1) std::swap(order[0], order[1]); pos = 0; }
    if (cur) { Mix_FreeMusic(cur); cur = nullptr; }
    cur = Mix_LoadMUS(romfs(tracks[order[pos++]]).c_str()); if (cur) Mix_FadeInMusic(cur, 1, 3000);
}

// ================================================================ DATOS GUARDADOS
static std::string savePath() {
#ifdef __SWITCH__
    mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/YachtFriends", 0777); return "sdmc:/switch/YachtFriends/datos.txt";
#else
    const char* e = getenv("YF_SAVE"); return e ? e : "yf-datos.txt";
#endif
}
void Save::load() {
    std::ifstream f(savePath()); std::string line;
    while (std::getline(f, line)) {
        auto p = line.find('='); if (p == std::string::npos) continue; std::string k = line.substr(0, p), v = line.substr(p + 1);
        if (k == "name") prof[0].name = v;
        else if (k.size() > 2 && k[0] == 'p' && isdigit((unsigned char)k[1]) && k[2] == '.') {
            int i = k[1] - '0'; std::string f = k.substr(3); if (i < 0 || i > 7) continue;
            if (f == "used") prof[i].used = v == "1" || i == 0; else if (f == "name") prof[i].name = v;
            else if (f == "o") prof[i].orient = atoi(v.c_str()) ? 1 : 0;
            else if (f == "gyro") prof[i].gyro = v == "1";
            else if (f == "av") { std::string c = "@av:"; c += v; avParse(c, prof[i].av); }
        }
        else if (k == "zoom") zoom = std::max(100, std::min(150, atoi(v.c_str()))); else if (k == "zmode") zmode = atoi(v.c_str()) ? 1 : 0;
        else if (k == "music") music = v == "1"; else if (k == "sfx") sfx = v == "1";
        else if (k == "level") level = std::max(1, std::min(3, atoi(v.c_str())));
        else if (k == "bank") bank = std::max(0, atoi(v.c_str())); else if (k == "jackpot") jackpot = std::max(5000, atoi(v.c_str()));
        else if (k.rfind("st.", 0) == 0) { int a = 0, b = 0; sscanf(v.c_str(), "%d,%d", &a, &b); stats[k.substr(3)] = { a, b }; }
    }
}
void Save::store() {
    std::ofstream f(savePath());
    for (int i = 0; i < 8; i++) { f << "p" << i << ".used=" << (prof[i].used ? 1 : 0) << "\np" << i << ".name=" << prof[i].name << "\np" << i << ".o=" << prof[i].orient << "\np" << i << ".gyro=" << (prof[i].gyro ? 1 : 0) << "\np" << i << ".av=" << avCode(prof[i].av).substr(4) << "\n"; }
    f << "zoom=" << zoom << "\nzmode=" << zmode << "\n";
    f << "music=" << (music ? 1 : 0) << "\nsfx=" << (sfx ? 1 : 0) << "\nlevel=" << level << "\nbank=" << bank << "\njackpot=" << jackpot << "\n";
    for (auto& kv : stats) f << "st." << kv.first << "=" << kv.second[0] << "," << kv.second[1] << "\n";
}
