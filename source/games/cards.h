// Cartas de póker dibujadas a mano (sin depender de glifos de fuente).
#pragma once
#include "../app.h"
namespace cards {
static const char* RANKS[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };
// palo: 0 picas, 1 corazones, 2 diamantes, 3 tréboles
inline void suit(Gfx& g, int s, float cx, float cy, float r, Col c) {
    if (s == 1) { g.circle(cx - r * 0.5f, cy - r * 0.3f, r * 0.55f, c); g.circle(cx + r * 0.5f, cy - r * 0.3f, r * 0.55f, c); g.tri(cx - r * 1.03f, cy - r * 0.05f, cx + r * 1.03f, cy - r * 0.05f, cx, cy + r * 1.0f, c); }
    else if (s == 2) { g.tri(cx - r * 0.8f, cy, cx + r * 0.8f, cy, cx, cy - r * 1.1f, c); g.tri(cx - r * 0.8f, cy, cx + r * 0.8f, cy, cx, cy + r * 1.1f, c); }
    else if (s == 0) { g.circle(cx - r * 0.5f, cy + r * 0.2f, r * 0.55f, c); g.circle(cx + r * 0.5f, cy + r * 0.2f, r * 0.55f, c); g.tri(cx - r * 1.03f, cy - r * 0.05f, cx + r * 1.03f, cy - r * 0.05f, cx, cy - r * 1.05f, c); g.tri(cx, cy + r * 0.1f, cx - r * 0.35f, cy + r * 1.05f, cx + r * 0.35f, cy + r * 1.05f, c); }
    else { g.circle(cx, cy - r * 0.55f, r * 0.45f, c); g.circle(cx - r * 0.55f, cy + r * 0.25f, r * 0.45f, c); g.circle(cx + r * 0.55f, cy + r * 0.25f, r * 0.45f, c); g.tri(cx, cy, cx - r * 0.3f, cy + r * 1.05f, cx + r * 0.3f, cy + r * 1.05f, c); }
}
inline bool red(int s) { return s == 1 || s == 2; }
inline void drawBack(Gfx& g, Rect r) {
    g.rrect({ r.x + 2, r.y + 5, r.w, r.h }, 10, rgb(0, 100)); g.rrect(r, 10, rgb(0xfff4d6)); g.rrectGrad(r.inset(5), 7, rgb(0x2e5fb0), rgb(0x1b3a78));
    for (int k = 0; k < 5; k++) g.ring(r.cx(), r.cy(), 8 + k * r.w * 0.09f, 1.5f, rgb(0xffffff, 70));
}
// v: 0..12 (2..A), s: palo. faceUp=false dibuja el dorso.
inline void draw(Gfx& g, Rect r, int v, int s, bool faceUp, bool glow = false) {
    g.rrect({ r.x + 2, r.y + 5, r.w, r.h }, 10, rgb(0, 100));
    if (glow) g.rrect({ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 13, pal::gold);
    if (!faceUp) {
        g.rrect(r, 10, rgb(0xfff4d6)); g.rrectGrad(r.inset(5), 7, rgb(0x2e5fb0), rgb(0x1b3a78));
        for (int k = 0; k < 5; k++) g.ring(r.cx(), r.cy(), 8 + k * r.w * 0.09f, 1.5f, rgb(0xffffff, 70));
        return;
    }
    g.rrect(r, 10, rgb(0xb9a888)); g.rrectGrad(r.inset(2), 8, rgb(0xffffff), rgb(0xf1ead8));
    Col c = red(s) ? rgb(0xd32f2f) : rgb(0x1c1c22);
    g.text(RANKS[v], r.x + r.w * 0.2f, r.y + r.h * 0.14f, r.w * 0.26f, c, F_TITLE, AL_C);
    suit(g, s, r.x + r.w * 0.2f, r.y + r.h * 0.3f, r.w * 0.09f, c);
    suit(g, s, r.cx(), r.cy() + r.h * 0.04f, r.w * 0.26f, c);
}
}
