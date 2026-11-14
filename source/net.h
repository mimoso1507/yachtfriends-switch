// Servidor local mínimo (HTTP + WebSocket) para que los celulares sean mandos: se entra por la IP de la Switch.
#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Net {
    static const int MAXC = 8;
    struct Client {
        int fd = -1; bool ws = false, joined = false; std::string in, out, name;
        uint32_t mask = 0;          // botones pulsados (bits en el orden de Btn)
        uint32_t lastRx = 0, lastTx = 0; std::string lastState;
        int slot = -1;              // casilla de mando asignada (1..7)
    };
    bool running = false; int port = 8765; std::string ip, error, page;
    Client cl[MAXC];
    bool start(const std::string& webRoot);   // abre el puerto y lee la página del mando
    void stop();
    void poll(uint32_t nowMs);                 // acepta, lee y escribe (llamar cada cuadro)
    std::string url() const { return "http://" + ip + ":" + std::to_string(port) + "/"; }
    void send(int i, const std::string& json); // manda estado al celular i
    int joinedCount() const { int n = 0; for (auto& c : cl) if (c.fd >= 0 && c.joined) n++; return n; }
private:
    int lfd = -1;
    void closeClient(int i);
    void handleHttp(int i);
    void handleWs(int i);
    void flush(int i);
};
