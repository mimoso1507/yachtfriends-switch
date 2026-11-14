// Baraja española de 40 cartas dibujada a mano. palo: 0 oros, 1 copas, 2 espadas, 3 bastos. rango: 1-7, 10 sota, 11 caballo, 12 rey.
#pragma once
#include "cards.h"
namespace sp {
static const int RANKS[10] = { 1, 2, 3, 4, 5, 6, 7, 10, 11, 12 };
inline Col suitCol(int s) { return s == 0 ? rgb(0xd9a400) : s == 1 ? rgb(0xc62828) : s == 2 ? rgb(0x1e5fb0) : rgb(0x2e7d32); }
inline const char* suitName(int s) { return s == 0 ? "oros" : s == 1 ? "copas" : s == 2 ? "espadas" : "bastos"; }
inline void symbol(Gfx& g, int s, float cx, float cy, float r) {
    Col c = suitCol(s);
    if (s == 0) { g.circle(cx, cy, r, rgb(0x8a6500)); g.circle(cx, cy, r * 0.88f, c); g.ring(cx, cy, r * 0.55f, r * 0.12f, rgb(0xfff2a8)); g.circle(cx, cy, r * 0.22f, rgb(0xfff2a8)); }
    else if (s == 1) { g.tri(cx - r * 0.85f, cy - r * 0.75f, cx + r * 0.85f, cy - r * 0.75f, cx, cy + r * 0.35f, c); g.rect({ cx - r * 0.1f, cy + r * 0.3f, r * 0.2f, r * 0.45f }, c); g.rect({ cx - r * 0.5f, cy + r * 0.72f, r, r * 0.16f }, c); }
    else if (s == 2) { g.tri(cx - r * 0.22f, cy + r * 0.3f, cx + r * 0.22f, cy + r * 0.3f, cx, cy - r, c); g.rect({ cx - r * 0.55f, cy + r * 0.28f, r * 1.1f, r * 0.14f }, rgb(0x6b4a12)); g.rect({ cx - r * 0.07f, cy + r * 0.4f, r * 0.14f, r * 0.5f }, rgb(0x6b4a12)); }
    else { g.line(cx - r * 0.35f, cy + r * 0.9f, cx + r * 0.35f, cy - r * 0.9f, r * 0.28f, rgb(0x6b4a12)); g.circle(cx + r * 0.3f, cy - r * 0.55f, r * 0.26f, c); g.circle(cx - r * 0.08f, cy - r * 0.05f, r * 0.24f, c); g.circle(cx - r * 0.3f, cy + r * 0.5f, r * 0.22f, c); }
}
inline void draw(Gfx& g, Rect r, int s, int rank, bool faceUp, bool glow = false, bool dim = false) {
    if (!faceUp) { cards::drawBack(g, r); return; }
    g.rrect({ r.x + 2, r.y + 5, r.w, r.h }, 10, rgb(0, 100)); if (glow) g.rrect({ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 13, pal::gold);
    g.rrect(r, 10, rgb(0xb9a888)); g.rrectGrad(r.inset(2), 8, rgb(0xfffaf0), rgb(0xf0e4c8)); Col c = suitCol(s);
    std::string n = std::to_string(rank); g.text(n, r.x + r.w * 0.2f, r.y + r.h * 0.13f, r.w * 0.24f, c, F_TITLE, AL_C);
    symbol(g, s, r.x + r.w * 0.2f, r.y + r.h * 0.28f, r.w * 0.08f);
    if (rank >= 10) { symbol(g, s, r.cx(), r.cy() + r.h * 0.06f, r.w * 0.2f); g.text(rank == 10 ? "SOTA" : rank == 11 ? "CABALLO" : "REY", r.cx(), r.y + r.h * 0.82f, r.w * 0.13f, c, F_BOLD, AL_C, r.w - 6); }
    else { int k = rank; float cxs[3] = { 0.3f, 0.5f, 0.7f }; float rr = r.w * (k <= 3 ? 0.14f : k <= 6 ? 0.11f : 0.09f);
        static const float P[8][8][2] = { {}, { { 0.5f, 0.5f } }, { { 0.5f, 0.33f }, { 0.5f, 0.7f } }, { { 0.5f, 0.28f }, { 0.5f, 0.5f }, { 0.5f, 0.72f } },
            { { 0.34f, 0.35f }, { 0.66f, 0.35f }, { 0.34f, 0.7f }, { 0.66f, 0.7f } }, { { 0.34f, 0.32f }, { 0.66f, 0.32f }, { 0.5f, 0.52f }, { 0.34f, 0.74f }, { 0.66f, 0.74f } },
            { { 0.34f, 0.3f }, { 0.66f, 0.3f }, { 0.34f, 0.52f }, { 0.66f, 0.52f }, { 0.34f, 0.74f }, { 0.66f, 0.74f } }, { { 0.34f, 0.3f }, { 0.66f, 0.3f }, { 0.5f, 0.42f }, { 0.34f, 0.56f }, { 0.66f, 0.56f }, { 0.34f, 0.76f }, { 0.66f, 0.76f } } };
        (void)cxs; for (int i = 0; i < k; i++) symbol(g, s, r.x + r.w * P[k][i][0], r.y + r.h * P[k][i][1], rr); }
    if (dim) g.rrect(r, 10, rgb(0, 120));
}
}
