// Ayudas comunes de los juegos de fiesta (sin IA): quién puede ver lo secreto y marcador por equipos.
#pragma once
#include "../app.h"
#include "party_data.h"
#include <algorithm>
#include <cmath>
#include <sstream>

struct Party : Game {
    int humans() const { int n = 0; for (auto& p : pl) if (!p.ai) n++; return n; }
    bool phoneOf(int p) const { return p >= 0 && p < (int)pl.size() && pl[p].pad >= 0 && app->in.isPhone(pl[p].pad); }
    bool heldL(UI& ui, int p) const { return p >= 0 && p < (int)pl.size() && pl[p].pad >= 0 && pl[p].pad < Input::MAXP && ui.in->pads[pl[p].pad].held[B_L]; }
    bool canSee(UI& ui, int p) const { if (phoneOf(p)) return false; return humans() <= 1 || heldL(ui, p); }   // en pantalla: solo con L apretado (o si juega una sola persona)
    Pad* padOf(Input& in, int p) { return (p >= 0 && p < (int)pl.size() && pl[p].pad >= 0 && pl[p].pad < Input::MAXP) ? &in.pads[pl[p].pad] : nullptr; }
    void banner(Gfx& g, const std::string& s) { g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(s, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930); }
    static std::vector<std::string> split(const std::string& s, char d) { std::vector<std::string> o; std::string t; std::stringstream ss(s); while (std::getline(ss, t, d)) o.push_back(t); return o; }
};
