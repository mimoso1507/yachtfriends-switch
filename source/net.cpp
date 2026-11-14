#include "net.h"
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <signal.h>
#include <cstdlib>
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// ---------------------------------------------------------------- SHA-1 y base64 (apretón de manos WebSocket)
static void sha1(const std::string& s, uint8_t out[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    std::string m = s; uint64_t bits = (uint64_t)s.size() * 8; m += (char)0x80; while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xff);
    auto rol = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
    for (size_t o = 0; o < m.size(); o += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = ((uint8_t)m[o + i * 4] << 24) | ((uint8_t)m[o + i * 4 + 1] << 16) | ((uint8_t)m[o + i * 4 + 2] << 8) | (uint8_t)m[o + i * 4 + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; } else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i]; e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    uint32_t hh[5] = { h0, h1, h2, h3, h4 }; for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[i * 4 + j] = (hh[i] >> (24 - j * 8)) & 0xff;
}
static std::string b64(const uint8_t* d, int n) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string r;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        r += T[(v >> 18) & 63]; r += T[(v >> 12) & 63]; r += i + 1 < n ? T[(v >> 6) & 63] : '='; r += i + 2 < n ? T[v & 63] : '=';
    }
    return r;
}

static void setNonBlock(int fd) { int f = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, f | O_NONBLOCK); }

bool Net::start(const std::string& webRoot) {
    if (running) return true;
    error.clear();
    { std::ifstream f(webRoot + "pad.html", std::ios::binary); if (!f) { error = "Falta la página del mando"; return false; } std::stringstream ss; ss << f.rdbuf(); page = ss.str(); }
#ifdef __SWITCH__
    static bool inited = false;
    if (!inited) { socketInitializeDefault(); nifmInitialize(NifmServiceType_User); inited = true; }
    u32 a = 0; if (R_FAILED(nifmGetCurrentIpAddress(&a)) || a == 0) { error = "La Switch no está conectada a una red WiFi"; return false; }
    char b[32]; snprintf(b, sizeof b, "%u.%u.%u.%u", a & 255, (a >> 8) & 255, (a >> 16) & 255, (a >> 24) & 255); ip = b;
#else
    signal(SIGPIPE, SIG_IGN);
    const char* e = getenv("YF_IP"); ip = e ? e : "127.0.0.1";
    if (!e) { int s = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in d{}; d.sin_family = AF_INET; d.sin_port = htons(53); inet_pton(AF_INET, "8.8.8.8", &d.sin_addr);
        if (s >= 0 && connect(s, (sockaddr*)&d, sizeof d) == 0) { sockaddr_in me{}; socklen_t l = sizeof me; if (getsockname(s, (sockaddr*)&me, &l) == 0) { char b[32]; inet_ntop(AF_INET, &me.sin_addr, b, sizeof b); ip = b; } }
        if (s >= 0) close(s); }
    if (const char* p = getenv("YF_PORT")) port = atoi(p);
#endif
    lfd = socket(AF_INET, SOCK_STREAM, 0); if (lfd < 0) { error = "No se pudo abrir la red"; return false; }
    int one = 1; setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_ANY); sa.sin_port = htons(port);
    if (bind(lfd, (sockaddr*)&sa, sizeof sa) < 0 || listen(lfd, 8) < 0) { error = "No se pudo abrir el puerto " + std::to_string(port); close(lfd); lfd = -1; return false; }
    setNonBlock(lfd); running = true; return true;
}
void Net::stop() { for (int i = 0; i < MAXC; i++) closeClient(i); if (lfd >= 0) close(lfd); lfd = -1; running = false; }
void Net::closeClient(int i) { if (cl[i].fd >= 0) close(cl[i].fd); cl[i] = Client(); }

void Net::flush(int i) {
    Client& c = cl[i]; if (c.fd < 0 || c.out.empty()) return;
    ssize_t n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
    if (n > 0) c.out.erase(0, n); else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) closeClient(i);
    if (i >= 0 && cl[i].fd >= 0 && cl[i].out.size() > 1 << 20) closeClient(i);
}
void Net::send(int i, const std::string& json) {
    Client& c = cl[i]; if (c.fd < 0 || !c.ws) return;
    std::string f; f += (char)0x81; size_t n = json.size();
    if (n < 126) f += (char)n; else { f += (char)126; f += (char)(n >> 8); f += (char)(n & 255); }
    c.out += f + json; flush(i);
}
void Net::handleHttp(int i) {
    Client& c = cl[i]; auto e = c.in.find("\r\n\r\n"); if (e == std::string::npos) { if (c.in.size() > 8192) closeClient(i); return; }
    std::string head = c.in.substr(0, e); c.in.erase(0, e + 4);
    auto line = head.substr(0, head.find("\r\n")); bool upgrade = false; std::string key;
    std::string low = head; for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
    if (low.find("upgrade: websocket") != std::string::npos) upgrade = true;
    auto kp = low.find("sec-websocket-key:"); if (kp != std::string::npos) { auto ke = head.find("\r\n", kp); key = head.substr(kp + 18, ke == std::string::npos ? std::string::npos : ke - kp - 18); while (!key.empty() && key[0] == ' ') key.erase(0, 1); while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back(); }
    if (upgrade && !key.empty()) {
        uint8_t h[20]; sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", h);
        c.out += "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + b64(h, 20) + "\r\n\r\n"; c.ws = true; flush(i); return;
    }
    std::string body, type = "text/html; charset=utf-8", status = "200 OK";
    if (line.rfind("GET / ", 0) == 0 || line.rfind("GET /?", 0) == 0 || line.rfind("GET /index", 0) == 0) body = page;
    else { status = "404 Not Found"; body = "No encontrado"; type = "text/plain; charset=utf-8"; }
    c.out += "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n" + body;
    flush(i); if (cl[i].fd >= 0) { cl[i].in.clear(); shutdown(cl[i].fd, SHUT_WR); }
}
void Net::handleWs(int i) {
    Client& c = cl[i];
    for (;;) {
        std::string& b = c.in; if (b.size() < 2) return;
        uint8_t b0 = b[0], b1 = b[1]; bool masked = b1 & 0x80; uint64_t len = b1 & 0x7f; size_t off = 2;
        if (len == 126) { if (b.size() < 4) return; len = ((uint8_t)b[2] << 8) | (uint8_t)b[3]; off = 4; }
        else if (len == 127) { closeClient(i); return; }
        if (!masked) { closeClient(i); return; }
        if (b.size() < off + 4 + len) return;
        uint8_t mk[4] = { (uint8_t)b[off], (uint8_t)b[off + 1], (uint8_t)b[off + 2], (uint8_t)b[off + 3] };
        std::string p = b.substr(off + 4, len); for (size_t k = 0; k < p.size(); k++) p[k] ^= mk[k & 3];
        b.erase(0, off + 4 + len); int op = b0 & 0x0f;
        if (op == 8) { closeClient(i); return; }
        if (op == 9) { std::string f; f += (char)0x8A; f += (char)p.size(); c.out += f + p; flush(i); continue; }
        if (op != 1) continue;
        if (p.rfind("k", 0) == 0) c.mask = (uint32_t)strtoul(p.c_str() + 1, nullptr, 10) & 0x3fff;
        else if (p.rfind("h", 0) == 0) { std::string n = p.substr(1); for (auto& ch : n) if ((unsigned char)ch < 32) ch = ' '; if (n.size() > 14) n.resize(14); while (!n.empty() && (unsigned char)n.back() >= 0x80 && ((unsigned char)n.back() & 0xC0) == 0x80) n.pop_back(); c.name = n; c.joined = true; }
    }
}
void Net::poll(uint32_t now) {
    if (!running) return;
    for (int k = 0; k < 4; k++) {   // aceptar nuevas conexiones
        int fd = accept(lfd, nullptr, nullptr); if (fd < 0) break;
        int slot = -1; for (int i = 0; i < MAXC; i++) if (cl[i].fd < 0) { slot = i; break; }
        if (slot < 0) { close(fd); continue; }
        setNonBlock(fd); cl[slot] = Client(); cl[slot].fd = fd; cl[slot].lastRx = now;
    }
    for (int i = 0; i < MAXC; i++) {
        Client& c = cl[i]; if (c.fd < 0) continue;
        char buf[2048]; ssize_t n;
        while ((n = recv(c.fd, buf, sizeof buf, 0)) > 0) { c.in.append(buf, n); c.lastRx = now; if (c.in.size() > 65536) break; }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { closeClient(i); continue; }
        if (!c.ws) handleHttp(i); else handleWs(i);
        if (cl[i].fd < 0) continue;
        flush(i);
        if (cl[i].fd >= 0 && !cl[i].ws && now - cl[i].lastRx > 8000) closeClient(i);
        else if (cl[i].fd >= 0 && cl[i].ws && now - cl[i].lastRx > 30000) closeClient(i);   // el celular manda "k" y un latido cada 5 s
    }
}
