#pragma once
#include <string>
#include <vector>
// Generador de códigos QR (modo bytes, corrección L, versiones 1-10). Devuelve la matriz de módulos (true = oscuro); vacía si no cabe.
std::vector<std::vector<bool>> qrEncode(const std::string& text);
