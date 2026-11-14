#pragma once
#include "../app.h"
namespace dice {
inline void draw(Gfx& g, Rect r, int v, bool hl = false, bool dim = false) {
    g.rrect({ r.x + 3, r.y + 6, r.w, r.h }, r.w * 0.18f, rgb(0, 90));
    g.rrectGrad(r, r.w * 0.18f, dim ? rgb(0xcfc8bb) : rgb(0xffffff), dim ? rgb(0xb5ad9e) : rgb(0xe4ddd0));
    if (hl) g.rrectLine(r.inset(-4), r.w * 0.2f, 4, pal::gold);
    static const int PAT[7][9] = { {}, { 0,0,0,0,1,0,0,0,0 }, { 1,0,0,0,0,0,0,0,1 }, { 1,0,0,0,1,0,0,0,1 }, { 1,0,1,0,0,0,1,0,1 }, { 1,0,1,0,1,0,1,0,1 }, { 1,0,1,1,0,1,1,0,1 } };
    if (v < 1 || v > 6) return;
    for (int k = 0; k < 9; k++) if (PAT[v][k]) { float px = r.x + r.w * (0.25f + 0.25f * (k % 3)), py = r.y + r.h * (0.25f + 0.25f * (k / 3)); g.circleGrad(px, py, r.w * 0.085f, rgb(v == 1 ? 0xc0392b : 0x2b1d14), rgb(v == 1 ? 0x8e1f12 : 0x000000)); }
}
}
