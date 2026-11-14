// Ahorcado: cada jugador pone una palabra secreta por turno y los demás la adivinan letra por letra (6 errores). Una ronda por jugador.
#include "../app.h"
#include "words_es.h"
#include <algorithm>
#include <sstream>

namespace {
typedef std::vector<std::string> Letters;   // letras en UTF-8 (Ñ ocupa 2 bytes)
Letters split8(const std::string& s) { Letters r; for (size_t i = 0; i < s.size();) { size_t n = 1; unsigned char c = s[i]; if (c >= 0xF0) n = 4; else if (c >= 0xE0) n = 3; else if (c >= 0xC0) n = 2; r.push_back(s.substr(i, n)); i += n; } return r; }
std::string normLetter(const std::string& l) {
    static const char* M[][2] = { { "á", "A" }, { "é", "E" }, { "í", "I" }, { "ó", "O" }, { "ú", "U" }, { "ü", "U" }, { "Á", "A" }, { "É", "E" }, { "Í", "I" }, { "Ó", "O" }, { "Ú", "U" }, { "Ü", "U" }, { "ñ", "Ñ" } };
    for (auto& m : M) if (l == m[0]) return m[1]; if (l == "Ñ") return l;
    if (l.size() == 1) { char c = l[0]; if (c >= 'a' && c <= 'z') return std::string(1, (char)(c - 32)); if ((c >= 'A' && c <= 'Z') || c == ' ') return l; }
    return "";
}
Letters normWord(const std::string& s) { Letters r; for (auto& l : split8(s)) { std::string n = normLetter(l); if (!n.empty()) r.push_back(n); } while (!r.empty() && r.back() == " ") r.pop_back(); while (!r.empty() && r.front() == " ") r.erase(r.begin()); return r; }
const char* const ABC[27] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "Ñ", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };
const char* const FREQ[27] = { "E", "A", "O", "S", "R", "N", "I", "D", "L", "C", "T", "U", "M", "P", "B", "H", "Q", "Y", "V", "G", "F", "J", "Z", "X", "K", "W", "Ñ" };
std::vector<Letters>& dict() { static std::vector<Letters> d; if (d.empty()) { std::stringstream ss(WORDS_ES); std::string w; while (ss >> w) d.push_back(split8(w)); } return d; }

struct Hang : Game {
    enum Ph { SET, GUESS, END } ph = SET;
    int N = 0, setter = 0, guesser = 0, wrong = 0, round = 1; std::vector<int> scores; Letters word; std::vector<bool> shown; std::vector<std::string> used; bool solved = false, over = false;
    float aiT = 0, aiWait = 1, endT = 0, roundT = 0; int resigned = -1; std::string msg;
    void start() override { N = (int)pl.size(); scores.assign(N, 0); beginSet(); }
    void beginSet() { ph = SET; word.clear(); shown.clear(); used.clear(); wrong = 0; solved = false; roundT = 0; guesser = -1;
        resetAi(); }
    void setWord(const Letters& w) { word = w; shown.assign(w.size(), false); for (size_t i = 0; i < w.size(); i++) if (w[i] == " ") shown[i] = true; ph = GUESS; guesser = nextG(setter); msg = pl[setter].name + " puso la palabra"; app->audio.play("place"); resetAi(); }
    int nextG(int from) const { int p = from; for (int i = 0; i < N; i++) { p = (p + 1) % N; if (p != setter && p != resigned) return p; } return from; }
    void resetAi() { aiT = 0; aiWait = 0.8f + rnd01() * 0.9f; }
    int turnPlayer() const override { return over ? -1 : ph == SET ? setter : ph == GUESS ? guesser : -1; }
    bool wasUsed(const std::string& l) const { return std::find(used.begin(), used.end(), l) != used.end(); }
    void guess(const std::string& l) {
        if (ph != GUESS || wasUsed(l)) return; used.push_back(l); bool hit = false; for (size_t i = 0; i < word.size(); i++) if (word[i] == l) { shown[i] = true; hit = true; }
        app->audio.play(hit ? "win" : "lose");
        if (hit) { bool all = true; for (bool b : shown) if (!b) all = false; if (all) { scores[guesser]++; solved = true; endRound(pl[guesser].name + " adivinó la palabra"); return; } }
        else { wrong++; if (wrong >= 6) { scores[setter]++; for (size_t i = 0; i < shown.size(); i++) shown[i] = true; endRound("Nadie adivinó: punto para " + pl[setter].name); return; } guesser = nextG(guesser); }
        resetAi();
    }
    void endRound(const std::string& m) { ph = END; roundT = 0; msg = m; }
    void resign(int p) override { resigned = p; if (p == setter && ph != END) { ph = END; msg = "Se fue " + pl[p].name; } else if (ph == GUESS && p == guesser) guesser = nextG(p);
        int a = 0; for (int i = 0; i < N; i++) if (i != resigned) a++; if (a <= 1) over = true; }
    std::string pattern() const { std::string s; for (size_t i = 0; i < word.size(); i++) s += shown[i] ? word[i] : "_"; return s; }
    void aiGuess() {
        int lv = std::max(1, std::min(3, pl[guesser].level)); std::vector<std::string> cand;
        std::vector<int> ok; auto& d = dict(); std::vector<std::string> wrongs; for (auto& u : used) { bool in = false; for (size_t i = 0; i < word.size(); i++) if (shown[i] && word[i] == u) in = true; if (!in) wrongs.push_back(u); }
        for (size_t k = 0; k < d.size(); k++) { auto& w = d[k]; if (w.size() != word.size()) continue; bool f = true;
            for (size_t i = 0; i < w.size() && f; i++) { if (word[i] == " ") { f = w[i] == " "; continue; } if (shown[i]) f = w[i] == word[i]; else f = !wasUsed(w[i]) ; }
            if (f) ok.push_back((int)k); }
        std::map<std::string, float> sc; for (int i = 0; i < 27; i++) sc[FREQ[i]] = (27 - i) * 0.25f;
        if (!ok.empty() && lv >= 2) { float conf = ok.size() >= 40 ? 0.3f : ok.size() >= 10 ? 0.6f : 1.f; std::map<std::string, int> has; for (int k : ok) { std::vector<std::string> seen; for (size_t i = 0; i < d[k].size(); i++) if (!shown[i] && std::find(seen.begin(), seen.end(), d[k][i]) == seen.end()) { seen.push_back(d[k][i]); has[d[k][i]]++; } }
            for (auto& h : has) sc[h.first] += 14 * conf * h.second / ok.size(); }
        std::string best; float bs = -1e9f; for (int i = 0; i < 27; i++) { std::string l = ABC[i]; if (wasUsed(l)) continue; float s = sc[l] + rnd01() * (lv == 1 ? 8.f : lv == 2 ? 1.f : 0.2f); if (s > bs) { bs = s; best = l; } }
        if (!best.empty()) guess(best);
    }
    void update(float dt, Input& in, UI&) override {
        if (over) { endT += dt; if (endT > 0.8f && !finished) conclude(); return; }
        if (ph == END) { roundT += dt; bool adv = roundT > 3.5f; if (roundT > 0.8f) for (auto& p : pl) { (void)p; } if (!adv) for (size_t i = 0; i < pl.size(); i++) if (!pl[i].ai && pl[i].pad >= 0 && in.pads[pl[i].pad].down[B_A] && roundT > 0.8f) adv = true;
            if (adv) { if (round >= N) { over = true; return; } round++; setter = (setter + 1) % N; beginSet(); } return; }
        int t = turnPlayer(); if (t < 0 || !pl[t].ai) return; aiT += dt; if (aiT < aiWait) return;
        if (ph == SET) { auto& d = dict(); Letters w; do { w = d[rndInt((int)d.size())]; } while (w.size() < 4 || w.size() > 8); setWord(w); }
        else if (ph == GUESS) aiGuess();
    }
    void conclude() {
        int best = *std::max_element(scores.begin(), scores.end()); int cnt = (int)std::count(scores.begin(), scores.end(), best); std::vector<PResult> r;
        for (int i = 0; i < N; i++) r.push_back({ scores[i], scores[i] == best ? (cnt > 1 ? 'd' : 'w') : 'l' }); if (resigned >= 0) r[resigned].r = 'x';
        std::string sm = "Puntos:"; for (int s : scores) sm += " " + std::to_string(s); finish(r, sm);
    }
    void drawGallows(Gfx& g, float x, float y, float s) {
        Col c = rgb(0x3a2414), b = rgb(0x1c1410); g.rect({ x, y + s * 1.9f, s * 1.3f, 8 }, c); g.rect({ x + s * 0.2f, y, 8, s * 1.9f }, c); g.rect({ x + s * 0.2f, y, s * 0.9f, 8 }, c); g.line(x + s * 0.2f, y + s * 0.4f, x + s * 0.6f, y, 6, c); g.rect({ x + s * 1.1f, y, 4, s * 0.3f }, c);
        float hx = x + s * 1.1f + 2, hy = y + s * 0.3f + s * 0.18f; if (wrong >= 1) g.ring(hx, hy, s * 0.18f, 5, b);
        if (wrong >= 2) g.line(hx, hy + s * 0.18f, hx, hy + s * 0.75f, 5, b); if (wrong >= 3) g.line(hx, hy + s * 0.3f, hx - s * 0.28f, hy + s * 0.55f, 5, b); if (wrong >= 4) g.line(hx, hy + s * 0.3f, hx + s * 0.28f, hy + s * 0.55f, 5, b);
        if (wrong >= 5) g.line(hx, hy + s * 0.75f, hx - s * 0.22f, hy + s * 1.12f, 5, b); if (wrong >= 6) g.line(hx, hy + s * 0.75f, hx + s * 0.22f, hy + s * 1.12f, 5, b);
    }
    void draw(Gfx& g, UI& ui) override {
        int t = turnPlayer(); bool mine = t >= 0 && !pl[t].ai && !over;
        std::string banner = over ? "Fin de las rondas" : ph == END ? msg : ph == SET ? (mine ? pl[setter].name + ": escribe tu palabra secreta" : pl[setter].name + " está pensando una palabra…") : (mine ? pl[guesser].name + ": elige una letra" : "Adivina " + pl[guesser].name + "…");
        g.rrect({ 290, 14, 950, 44 }, 22, rgb(0x120c08, 210)); g.text(banner, 765, 36, 20, pal::gold, F_BOLD, AL_C, 930);
        float pw = std::min(180.f, 940.f / N);
        for (int i = 0; i < N; i++) { Rect r{ 290 + i * (pw + 2), 66, pw - 4, 52 }; bool on = i == t; g.rrect(r, 10, on ? pal::gold : rgb(0x5a3a1c)); g.rrectGrad(r.inset(2), 8, rgb(0x2a1e18), rgb(0x120c08));
            g.emoji(pl[i].emoji, r.x + 20, r.cy(), 22); g.text(pl[i].name, r.x + 38, r.y + 16, 13, on ? pal::gold : pal::white, F_BOLD, AL_L, pw - 50); g.text(std::string(i == setter && ph != END ? "pone · " : "") + std::to_string(scores[i]) + " pts", r.x + 38, r.y + 36, 13, rgb(0xd8b46a), F_BODY, AL_L, pw - 50); }
        g.text("Ronda " + std::to_string(round) + " de " + std::to_string(N), 1180, 140, 15, pal::cream, F_BODY, AL_R);
        drawGallows(g, 330, 140, 150);
        if (ph == SET) {
            if (mine) { g.text("La palabra es privada: míra solo tú la pantalla", 800, 220, 20, pal::cream, F_BOLD, AL_C, 560);
                if (ui.button("Escribir palabra secreta  ✎", { 600, 260, 420, 60 }, 3)) { std::string s; if (app->editName(s, "Palabra secreta")) { Letters w = normWord(s); if (w.size() >= 2) setWord(w); else app->toast("Muy corta"); } }
                if (ui.button("Elegir una al azar", { 600, 336, 420, 52 }, 2)) { auto& d = dict(); Letters w; do { w = d[rndInt((int)d.size())]; } while (w.size() < 4 || w.size() > 8); setWord(w); } }
            else g.text("Esperando la palabra…", 800, 250, 22, pal::cream, F_BOLD, AL_C);
            return; }
        // palabra
        std::string pat = pattern(); float cw = std::min(60.f, 600.f / std::max<size_t>(1, word.size())); float x0 = 800 - cw * word.size() / 2;
        for (size_t i = 0; i < word.size(); i++) { if (word[i] == " ") continue; Rect r{ x0 + i * cw + 3, 170, cw - 6, cw }; g.rrect(r, 8, rgb(0xfff4d6)); g.rect({ r.x + 6, r.y + r.h - 6, r.w - 12, 4 }, rgb(0x8a6234)); if (shown[i]) g.text(word[i], r.cx(), r.cy(), cw * 0.62f, ph == END && !solved && wrong >= 6 ? rgb(0xc8331f) : rgb(0x2b1a0c), F_TITLE, AL_C); }
        g.text("Errores: " + std::to_string(wrong) + " / 6", 800, 270, 20, wrong >= 4 ? rgb(0xff8a80) : pal::cream, F_BOLD, AL_C);
        if (ph == END) { g.text("A para seguir", 800, 300, 16, rgb(0xd8b46a), F_BODY, AL_C); }
        // teclado
        for (int i = 0; i < 27; i++) { int rr = i / 9, cc = i % 9; Rect r{ 470.f + cc * 70, 330.f + rr * 66, 62, 56 }; std::string l = ABC[i]; bool u = wasUsed(l); bool isHit = false; for (size_t k = 0; k < word.size(); k++) if (word[k] == l && shown[k]) isHit = true;
            uint32_t id = hid("ab", i); bool en = mine && ph == GUESS && !u; bool hit = ui.hit(id, r, en);
            g.rrect({ r.x, r.y + 3, r.w, r.h }, 10, rgb(0, 90)); g.rrectGrad(r, 10, u ? (isHit ? rgb(0x4caf50) : rgb(0x8a3a30)) : rgb(0xfff4d6), u ? (isHit ? rgb(0x2e7d32) : rgb(0x5a1f18)) : rgb(0xead39c));
            g.text(l, r.cx(), r.cy(), 28, u ? rgb(0xffffff, 200) : rgb(0x3b2414), F_TITLE, AL_C); if (ui.focused(id)) ui.drawFocus(r, 10); if (hit) guess(l); }
        if (mine && ph == GUESS) ui.setFocusIfNone(hid("ab", 4));
    }
    std::vector<Hint> hints() const override { if (ph == END) return { { "A", "Seguir" } }; return { { "DPAD/STICK", "Moverse" }, { "A", ph == SET ? "Elegir" : "Probar la letra" } }; }
    std::string help() const override { return "Cada jugador, por turno, escribe una palabra secreta y los demás la adivinan probando letras. Con 6 errores se completa el muñeco y gana un punto quien puso la palabra; si la adivinan, el punto es de quien completó la palabra. Hay una ronda por jugador; gana quien suma más puntos."; }
};
}
Game* makeAhorcado() { return new Hang(); }
