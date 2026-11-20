#define _WIN32_WINNT 0x0600
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <windows.h>

using namespace std;

// =========================================================
// 1. CONFIGURACIÓN GENERAL
// =========================================================
const int ANCHO = 200;
const int ALTO = 60;
const int MAPA_TAM = 32;

const int VIDAS_MAX = 4;
const float ENERGIA_MAX = 100.0f;     // Energía compartida para Dash [Q] y Bomba [E]
const float COSTO_HABILIDAD = 50.0f;  // Ambos gastan 50% de la barra
const float REGEN_ENERGIA = 14.0f;    // Regeneración por segundo
const float DURACION_BOMBA = 9.0f;

// =========================================================
// 2. ESTRUCTURAS DE DATOS E IA
// =========================================================
enum EstadoIA {
    IA_PATRULLA,
    IA_PERSECUCION,
    IA_FLANQUEO,
    IA_EVASION
};

struct Enemigo {
    float x, y;
    bool vivo;
    float tiempoReaparicion;
    float distancia;
    EstadoIA estado;
    float dirFlanco;      // -1.0 izquierda, +1.0 derecha
    float timerDecision;
    float dirPatrullaX, dirPatrullaY;
};

struct AgujeroNegro {
    float x, y;
    float tiempoVida;
    bool activo;
};

struct EscenaCinematica {
    string titulo;
    vector<string> lineas;
    float duracion;
};
struct Arbol {
    float x, y;
};
struct Nivel {
    EscenaCinematica cinematicaInicio;
    string mapa;
    float inicioX, inicioY, inicioAngulo;
    float portalX, portalY;
    vector<Enemigo> enemigos;
    vector<Arbol> arboles;
};

// =========================================================
// 3. UTILIDADES DE DIBUJO Y FÍSICA
// =========================================================
CHAR_INFO celda(char caracter, WORD color) {
    CHAR_INFO ci;
    ci.Char.AsciiChar = caracter;
    ci.Attributes = color;
    return ci;
}

void escribir(vector<CHAR_INFO>& pantalla, int x, int y, const string& texto, WORD color) {
    if (y < 0 || y >= ALTO) return;
    for (int i = 0; i < (int)texto.size(); i++) {
        if (x + i >= 0 && x + i < ANCHO) {
            pantalla[y * ANCHO + x + i] = celda(texto[i], color);
        }
    }
}

void dibujarBarra(vector<CHAR_INFO>& pantalla, int x, int y, int largo, float pct, WORD color) {
    pct = max(0.0f, min(1.0f, pct));
    int llenos = (int)(largo * pct + 0.5f);

    escribir(pantalla, x, y, "[", color);
    for (int i = 0; i < largo; i++) {
        pantalla[y * ANCHO + x + 1 + i] = (i < llenos)
            ? celda((char)219, color)
            : celda((char)176, FOREGROUND_INTENSITY);
    }
    escribir(pantalla, x + 1 + largo, y, "]", color);
}

bool esTransitable(const string& mapa, float x, float y) {
    int ix = (int)x, iy = (int)y;
    if (ix < 0 || ix >= MAPA_TAM || iy < 0 || iy >= MAPA_TAM) return false;
    return mapa[iy * MAPA_TAM + ix] == '.';
}

bool hayLineaDeVision(const string& mapa, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    int pasos = (int)(sqrtf(dx * dx + dy * dy) / 0.2f);
    for (int i = 1; i < pasos; i++) {
        float t = (float)i / pasos;
        if (!esTransitable(mapa, x1 + dx * t, y1 + dy * t)) return false;
    }
    return true;
}

// =========================================================
// 4. INTELIGENCIA ARTIFICIAL TÁCTICA DE ENEMIGOS
// =========================================================
void actualizarIAEnemigos(
    vector<Enemigo>& enemigos, const string& mapa,
    float& jugadorX, float& jugadorY, float dirJugadorX, float dirJugadorY,
    bool jugadorEnDash, const AgujeroNegro& agujero,
    int& vidas, float& inmunidad, float& flashGolpe, bool& derrota,
    float& distMasCercano, float dt
) {
    distMasCercano = 999.0f;

    int enemigosAlertados = 0;
    for (const auto& e : enemigos) {
        if (e.vivo && hypot(e.x - jugadorX, e.y - jugadorY) < 15.0f &&
            hayLineaDeVision(mapa, e.x, e.y, jugadorX, jugadorY)) {
            enemigosAlertados++;
        }
    }

    for (auto& e : enemigos) {
        if (!e.vivo) {
            e.tiempoReaparicion -= dt;
            if (e.tiempoReaparicion <= 0.0f) {
                e.vivo = true;
                e.estado = IA_PATRULLA;
            }
            continue;
        }

        float vx = jugadorX - e.x;
        float vy = jugadorY - e.y;
        float dist = hypot(vx, vy);
        e.distancia = dist;
        distMasCercano = min(distMasCercano, dist);

        // Ataque cuerpo a cuerpo (invulnerable durante el impulso del Dash)
        if (dist <= 1.05f) {
            if (inmunidad <= 0.0f && !jugadorEnDash) {
                vidas--;
                inmunidad = 0.5f;
                flashGolpe = 0.65f;
                float empujeX = jugadorX + (vx / dist) * 0.65f;
                float empujeY = jugadorY + (vy / dist) * 0.65f;
                if (esTransitable(mapa, empujeX, jugadorY)) jugadorX = empujeX;
                if (esTransitable(mapa, jugadorX, empujeY)) jugadorY = empujeY;
                if (vidas <= 0) derrota = true;
            }
            continue;
        }

        e.timerDecision -= dt;
        bool teVe = (dist < 16.0f) && hayLineaDeVision(mapa, e.x, e.y, jugadorX, jugadorY);

        float anguloMira = atan2f(-vx, -vy) - atan2f(dirJugadorX, dirJugadorY);
        if (anguloMira < -3.14159f) anguloMira += 6.28318f;
        if (anguloMira > 3.14159f) anguloMira -= 6.28318f;
        bool siendoApuntado = teVe && (fabs(anguloMira) < 0.16f) && (dist < 13.0f);

        bool cercaDeBomba = false;
        float bx = 0.0f, by = 0.0f;
        if (agujero.activo) {
            bx = e.x - agujero.x;
            by = e.y - agujero.y;
            if (hypot(bx, by) < 5.5f) cercaDeBomba = true;
        }

        if (e.timerDecision <= 0.0f) {
            e.timerDecision = 0.45f;
            if (cercaDeBomba || (siendoApuntado && dist > 3.2f)) {
                e.estado = IA_EVASION;
                e.dirFlanco = (rand() % 2 == 0) ? 1.0f : -1.0f;
            } else if (teVe) {
                if (enemigosAlertados >= 2 && dist > 4.0f) {
                    e.estado = IA_FLANQUEO;
                } else {
                    e.estado = IA_PERSECUCION;
                }
            } else {
                e.estado = IA_PATRULLA;
                float angAleatorio = ((rand() % 360) * 3.14159f) / 180.0f;
                e.dirPatrullaX = sinf(angAleatorio);
                e.dirPatrullaY = cosf(angAleatorio);
            }
        }

        float moveX = 0.0f, moveY = 0.0f;
        float velEnemigo = 2.1f;

        if (e.estado == IA_PATRULLA) {
            velEnemigo = 1.1f;
            moveX = e.dirPatrullaX;
            moveY = e.dirPatrullaY;
            if (!esTransitable(mapa, e.x + moveX * 0.6f, e.y + moveY * 0.6f)) {
                e.dirPatrullaX = -e.dirPatrullaX;
                e.dirPatrullaY = -e.dirPatrullaY;
            }
        } else if (e.estado == IA_PERSECUCION) {
            velEnemigo = 2.45f;
            moveX = vx / dist;
            moveY = vy / dist;
        } else if (e.estado == IA_FLANQUEO) {
            velEnemigo = 2.3f;
            float nx = vx / dist, ny = vy / dist;
            float perpX = -ny * e.dirFlanco;
            float perpY = nx * e.dirFlanco;
            moveX = nx * 0.65f + perpX * 0.75f;
            moveY = ny * 0.65f + perpY * 0.75f;
        } else if (e.estado == IA_EVASION) {
            velEnemigo = 3.1f;
            if (cercaDeBomba) {
                float bDist = max(0.1f, hypot(bx, by));
                moveX = bx / bDist;
                moveY = by / bDist;
            } else {
                float nx = vx / dist, ny = vy / dist;
                moveX = -ny * e.dirFlanco;
                moveY = nx * e.dirFlanco;
            }
        }

        float mag = hypot(moveX, moveY);
        if (mag > 0.0f) {
            float pasoX = (moveX / mag) * velEnemigo * dt;
            float pasoY = (moveY / mag) * velEnemigo * dt;
            if (esTransitable(mapa, e.x + pasoX, e.y)) e.x += pasoX;
            if (esTransitable(mapa, e.x, e.y + pasoY)) e.y += pasoY;
        }
    }
}

// =========================================================
// 5. MOTOR 3D (PAREDES, OBJETOS Y DEMONIOS)
// =========================================================
void dibujarEscena3D(
    vector<CHAR_INFO>& pantalla, const string& mapa,
    float camX, float camY, float camAngulo, float fov, int desfaseVertical,
    vector<Enemigo>& enemigos, const vector<Arbol>& arboles,
    const AgujeroNegro& agujero, float portalX, float portalY, float reloj
) {
    const float PROF_MAX = 28.0f;
    float zBuffer[ANCHO];
    float dirX = sinf(camAngulo), dirY = cosf(camAngulo);
    int centroVertical = (ALTO / 2) + desfaseVertical;

    // --- A. PAREDES, TECHO Y SUELO ---
    for (int x = 0; x < ANCHO; x++) {
        float anguloRayo = (camAngulo - fov / 2.0f) + ((float)x / ANCHO) * fov;
        float rayoX = sinf(anguloRayo), rayoY = cosf(anguloRayo);
        float dist = 0.0f;
        bool choco = false, esquina = false;
        char tipo = '#';

        while (!choco && dist < PROF_MAX) {
            dist += 0.04f;
            int tx = (int)(camX + rayoX * dist);
            int ty = (int)(camY + rayoY * dist);

            if (tx < 0 || tx >= MAPA_TAM || ty < 0 || ty >= MAPA_TAM) {
                choco = true;
                dist = PROF_MAX;
            } else if (mapa[ty * MAPA_TAM + tx] != '.') {
                choco = true;
                tipo = mapa[ty * MAPA_TAM + tx];
                float ix = (camX + rayoX * dist) - tx;
                float iy = (camY + rayoY * dist) - ty;
                if ((ix < 0.05f || ix > 0.95f) && (iy < 0.05f || iy > 0.95f)) esquina = true;
            }
        }

        zBuffer[x] = dist;
        int alturaMuro = (int)(ALTO / dist);
        int techo = centroVertical - alturaMuro;
        int suelo = centroVertical + alturaMuro;

        WORD colorBase = FOREGROUND_RED;
        if (tipo == 'B') colorBase = FOREGROUND_BLUE;
        if (tipo == 'G') colorBase = FOREGROUND_GREEN;
        if (tipo == 'Y') colorBase = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN;

        char cPared = ' ';
        WORD colPared = colorBase;
        if (dist <= PROF_MAX / 4.5f)     { cPared = (char)219; colPared |= FOREGROUND_INTENSITY; }
        else if (dist < PROF_MAX / 3.0f) { cPared = (char)178; colPared |= FOREGROUND_INTENSITY; }
        else if (dist < PROF_MAX / 1.8f) { cPared = (char)177; }
        else if (dist < PROF_MAX)        { cPared = (char)176; }

        if (esquina) { cPared = '|'; colPared = FOREGROUND_INTENSITY; }

        for (int y = 0; y < ALTO; y++) {
            int i = y * ANCHO + x;
            if (y <= techo) pantalla[i] = celda(' ', 0);
            else if (y <= suelo) pantalla[i] = celda(cPared, colPared);
            else {
                float s = 1.0f - (((float)y - centroVertical) / (ALTO / 2.0f));
                if (s < 0.35f)      pantalla[i] = celda((char)177, FOREGROUND_RED | FOREGROUND_GREEN);
                else if (s < 0.70f) pantalla[i] = celda((char)176, FOREGROUND_INTENSITY);
                else                pantalla[i] = celda('.', FOREGROUND_INTENSITY);
            }
        }
    }
    //arboles
    for (const auto& arb : arboles) {
        float vx = arb.x - camX;
        float vy = arb.y - camY;
        float dist = hypot(vx, vy);
        float ang = atan2f(vx, vy) - atan2f(dirX, dirY);
        if (ang < -3.14159f) ang += 6.28318f;
        if (ang > 3.14159f) ang -= 6.28318f;

        if (fabs(ang) < fov / 2.0f + 0.35f && dist >= 0.45f && dist < PROF_MAX) {
            float techoObj = centroVertical - (ALTO / dist);
            float altoObj = (ALTO / dist) * 2.0f;
            float anchoObj = altoObj * 1.15f;
            float cx = (0.5f * (ang / (fov / 2.0f)) + 0.5f) * ANCHO;

            for (float lx = 0; lx < anchoObj; lx++) {
                int col = (int)(cx + lx - anchoObj / 2.0f);
                if (col >= 0 && col < ANCHO && zBuffer[col] >= dist) {
                    float px = lx / anchoObj;
                    for (float ly = 0; ly < altoObj; ly++) {
                        int fila = (int)(techoObj + ly);
                        if (fila < 0 || fila >= ALTO) continue;
                        float py = ly / altoObj;
                        int idx = fila * ANCHO + col;

                        // Tronco marrón/amarillo
                        if (py >= 0.58f && py <= 0.99f && px >= 0.42f && px <= 0.58f) {
                            char texTronco = (px < 0.47f) ? (char)178 : (char)219;
                            pantalla[idx] = celda(texTronco, FOREGROUND_RED | FOREGROUND_GREEN);
                        }
                        // Copa verde frondosa
                        else if (py >= 0.04f && py <= 0.62f) {
                            float dxCopa = (px - 0.5f) * 2.0f;
                            float dyCopa = (py - 0.33f) / 0.29f;
                            float radio = hypot(dxCopa, dyCopa);
                            if (radio <= 0.95f) {
                                if (py > 0.46f || radio > 0.78f)
                                    pantalla[idx] = celda((char)178, FOREGROUND_RED);
                                else
                                    pantalla[idx] = celda((char)219, FOREGROUND_RED | FOREGROUND_RED);
                            }
                        }
                    }
                }
            }
        }
    }

    // --- B. AGUJERO NEGRO EN EL SUELO ---
    if (agujero.activo) {
        float vx = agujero.x - camX, vy = agujero.y - camY;
        float dist = hypot(vx, vy);
        float ang = atan2f(vx, vy) - atan2f(dirX, dirY);
        if (ang < -3.14159f) ang += 6.28318f;
        if (ang > 3.14159f) ang -= 6.28318f;

        if (fabs(ang) < fov / 2.0f + 0.3f && dist >= 0.4f && dist < PROF_MAX) {
            float sueloObj = centroVertical + (ALTO / dist);
            float hTotal = (ALTO / dist) * 2.0f;
            float altoD = hTotal * 0.25f, anchoD = hTotal * 0.95f;
            float cx = (0.5f * (ang / (fov / 2.0f)) + 0.5f) * ANCHO;

            for (float lx = 0; lx < anchoD; lx++) {
                int col = (int)(cx + lx - anchoD / 2.0f);
                if (col >= 0 && col < ANCHO && zBuffer[col] >= dist) {
                    float nx = (lx / anchoD) * 2.0f - 1.0f;
                    for (float ly = 0; ly < altoD; ly++) {
                        int fila = (int)(sueloObj - altoD + ly);
                        if (fila >= 0 && fila < ALTO) {
                            float ny = (ly / altoD) * 2.0f - 1.0f;
                            float r = hypot(nx, ny);
                            if (r <= 1.0f) {
                                pantalla[fila * ANCHO + col] = (r < 0.45f)
                                    ? celda((char)219, 0)
                                    : celda((char)178, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                            }
                        }
                    }
                }
            }
        }
    }

    // --- C. PORTAL DE SALIDA ---
    {
        float vx = portalX - camX, vy = portalY - camY;
        float dist = hypot(vx, vy);
        float ang = atan2f(vx, vy) - atan2f(dirX, dirY);
        if (ang < -3.14159f) ang += 6.28318f;
        if (ang > 3.14159f) ang -= 6.28318f;

        if (fabs(ang) < fov / 2.0f + 0.2f && dist >= 0.4f && dist < PROF_MAX) {
            float techoObj = centroVertical - (ALTO / dist);
            float altoObj = (ALTO / dist) * 2.0f;
            float anchoObj = altoObj * 0.75f;
            float cx = (0.5f * (ang / (fov / 2.0f)) + 0.5f) * ANCHO;

            for (float lx = 0; lx < anchoObj; lx++) {
                int col = (int)(cx + lx - anchoObj / 2.0f);
                if (col >= 0 && col < ANCHO && zBuffer[col] >= dist) {
                    float nx = (lx / anchoObj) * 2.0f - 1.0f;
                    for (float ly = 0; ly < altoObj; ly++) {
                        int fila = (int)(techoObj + ly);
                        if (fila >= 0 && fila < ALTO) {
                            float ny = (ly / altoObj) * 2.0f - 1.0f;
                            float r = hypot(nx, ny);
                            if (r <= 0.95f) {
                                float onda = sinf(r * 10.0f - reloj * 6.0f);
                                WORD colP = (r > 0.72f)
                                    ? (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
                                    : ((onda > 0.0f) ? (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
                                                     : (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY));
                                pantalla[fila * ANCHO + col] = celda((char)219, colP);
                            }
                        }
                    }
                }
            }
        }
    }

    // --- D. DEMONIOS (AQUÍ PUEDES EDITAR SU DISEÑO VISUAL) ---
    for (auto& e : enemigos) e.distancia = hypot(e.x - camX, e.y - camY);
    sort(enemigos.begin(), enemigos.end(), [](const Enemigo& a, const Enemigo& b) {
        return a.distancia > b.distancia;
    });

    for (const auto& e : enemigos) {
        if (!e.vivo) continue;
        float vx = e.x - camX, vy = e.y - camY;
        float ang = atan2f(vx, vy) - atan2f(dirX, dirY);
        if (ang < -3.14159f) ang += 6.28318f;
        if (ang > 3.14159f) ang -= 6.28318f;

        if (fabs(ang) < fov / 2.0f + 0.25f && e.distancia >= 0.45f && e.distancia < PROF_MAX) {
            float techoObj = centroVertical - (ALTO / e.distancia);
            float altoObj = (ALTO / e.distancia) * 2.0f;
            float anchoObj = altoObj * 0.85f;
            float cx = (0.5f * (ang / (fov / 2.0f)) + 0.5f) * ANCHO;
            float paso = sinf(reloj * 8.0f + e.x) * 0.03f;

            // Los ojos brillan cian si está esquivando (IA_EVASION)
            WORD colorOjos = (e.estado == IA_EVASION)
                ? (0)
                : (0);

            for (float lx = 0; lx < anchoObj; lx++) {
                int col = (int)(cx + lx - anchoObj / 2.0f);
                if (col >= 0 && col < ANCHO && zBuffer[col] >= e.distancia) {
                    float px = lx / anchoObj; // 0.0 (izquierda) a 1.0 (derecha)
                    for (float ly = 0; ly < altoObj; ly++) {
                        int fila = (int)(techoObj + ly);
                        if (fila < 0 || fila >= ALTO) continue;
                        float py = ly / altoObj; // 0.0 (arriba) a 1.0 (pies)
                        int idx = fila * ANCHO + col;

                        // 1. Cuernos
                        if (py >= 0.04f && py < 0.18f &&
                           ((px >= 0.22f && px <= 0.31f && py > (0.30f - px) * 0.8f) ||
                            (px >= 0.69f && px <= 0.78f && py > (px - 0.70f) * 0.8f)||
                            (px >= 0.45f && px <= 0.56f && py > (px - 0.50f) * 0.8f))) {

                            pantalla[idx] = celda((char)219, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                        }
                        // 2. Cabeza, ojos y boca
                        else if (py >= 0.16f && py < 0.35f && px >= 0.32f && px <= 0.68f) {
                            pantalla[idx] = celda((char)219, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );

                            if (py >= 0.21f && py <= 0.25f && ((px >= 0.37f && px <= 0.44f) || (px >= 0.56f && px <= 0.63f)))
                                pantalla[idx] = celda('@', colorOjos);
                            else if (py >= 0.28f && py <= 0.33f && px >= 0.39f && px <= 0.61f)
                                pantalla[idx] = (px < 0.43f || px > 0.57f || (px > 0.48f && px < 0.52f))
                                    ? celda('V', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
                                    : celda((char)219, 0);
                        }
                        // 3. Torso y pecho
                        else if (py >= 0.35f && py < 0.68f && px >= 0.24f && px <= 0.76f) {
                            pantalla[idx] = (px >= 0.33f && px <= 0.67f && py >= 0.39f && py <= 0.58f)
                                ? celda((char)219, FOREGROUND_RED | FOREGROUND_GREEN  | FOREGROUND_BLUE)
                                : celda((char)178, FOREGROUND_RED | FOREGROUND_GREEN  | FOREGROUND_BLUE);
                        }
                        // 4. Brazos y garras
                        else if (py >= 0.37f && py < 0.73f && ((px >= 0.11f && px <= 0.23f) || (px >= 0.77f && px <= 0.89f))) {
                            pantalla[idx] = (py >= 0.65f)
                                ? celda('W', FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
                                : celda((char)177, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                        }
                        // 5. Piernas y pezuñas
                        else if (py >= 0.68f && py < 0.98f &&
                                ((px >= 0.26f && px <= 0.44f && py < 0.96f + paso) ||
                                 (px >= 0.56f && px <= 0.74f && py < 0.96f - paso))) {
                            pantalla[idx] = (py >= 0.90f)
                                ? celda((char)219, FOREGROUND_INTENSITY)
                                : celda((char)219, FOREGROUND_BLUE | FOREGROUND_GREEN  | FOREGROUND_BLUE);
                        }
                    }
                }
            }
        }
    }
}

// =========================================================
// 6. REPRODUCTOR DE CINEMÁTICAS
// =========================================================
void reproducirCinematica(
    HANDLE hConsola, SMALL_RECT& ventanaFinal, vector<CHAR_INFO>& pantalla,
    Nivel& nivel, const EscenaCinematica& escena
) {
    DWORD inicio = GetTickCount();
    AgujeroNegro sinAgujero = { 0, 0, 0, false };

    while (true) {
        float t = (GetTickCount() - inicio) / 1000.0f;
        if (t >= escena.duracion) break;
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) { Sleep(180); break; }

        float prog = t / escena.duracion;
        float camX = nivel.inicioX + sinf(prog * 3.14159f) * 1.1f;
        float camY = nivel.inicioY + cosf(prog * 3.14159f) * 1.1f;
        if (!esTransitable(nivel.mapa, camX, camY)) { camX = nivel.inicioX; camY = nivel.inicioY; }

        dibujarEscena3D(pantalla, nivel.mapa, camX, camY, nivel.inicioAngulo - 0.5f + prog,
                        3.14159f / 3.5f, 0, nivel.enemigos,nivel.arboles, sinAgujero, nivel.portalX, nivel.portalY, t);

        for (int y = 0; y < 9; y++) {
            for (int x = 0; x < ANCHO; x++) {
                pantalla[y * ANCHO + x] = celda(' ', 0);
                pantalla[(ALTO - 1 - y) * ANCHO + x] = celda(' ', 0);
            }
        }

        WORD oro = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        WORD blanco = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

        escribir(pantalla, (ANCHO - (int)escena.titulo.size()) / 2, 4, escena.titulo, oro);

        for (int i = 0; i < (int)escena.lineas.size(); i++) {
            float retraso = i * 1.1f;
            if (t > retraso) {
                int letras = min((int)escena.lineas[i].size(), (int)((t - retraso) * 38.0f));
                string textoVisible = escena.lineas[i].substr(0, letras);
                int posX = (ANCHO - (int)escena.lineas[i].size()) / 2;
                int posY = ALTO - 7 + (i * 2);
                WORD colLinea = (i == (int)escena.lineas.size() - 1) ? oro : blanco;
                escribir(pantalla, posX, posY, textoVisible, colLinea);
            }
        }

        escribir(pantalla, ANCHO - 26, 1, "[ENTER para continuar]", FOREGROUND_INTENSITY);
        WriteConsoleOutputA(hConsola, pantalla.data(), { (SHORT)ANCHO, (SHORT)ALTO }, { 0, 0 }, &ventanaFinal);
        Sleep(16);
    }
}

// =========================================================
// 7. EDITOR DE NIVELES E HISTORIA
// =========================================================
vector<Nivel> crearNiveles() {
    vector<Nivel> niveles;
    auto crearEnemigo = [](float x, float y) -> Enemigo {
        return { x, y, true, 0.0f, 0.0f, IA_PATRULLA, 1.0f, 0.0f, 1.0f, 0.0f };
    };

    // NIVEL 1
    Nivel n1;
    n1.cinematicaInicio = {
        "INICIO: EL SUEÑO SOBRE LORD PYTHON",
        {
            "ANO 2026.MAÑANA ES LA MUESTRA PERO COMO NO HICISTE NADA TE QUEDASTE HASTA LAS...",
            "4 DE LA MAÑANA PROGRAMANDO EL JUEGO EN C++ LLAMADO LORD PYTHON                  ",
            "TE DORMISTE SOBRE EL TECLADO Y TE DESPERTASTE EN LOS BATHROOMS ."
        },
        6.5f
    };
    n1.mapa =
        "################################"
        "#.........YYYYYYYYYYYY.........#"
        "#........................Y.....#"
        "#.....G......B...........YYY...#"
        "#.....G......B.............Y...#"
        "#.....G.....B...BBB..BBB...Y...#"
        "#...........B..B.......B...Y...#"
        "#...........B.B........B...Y...#"
        "#.....G.....BB.........B...Y...#"
        "#.....G................B...YYYY#"
        "#.....G......BBBBBBB...B...Y...#"
        "#.....G............Y...B...Y...#"
        "#.....GGGGGGGG.....Y...B...Y...#"
        "#............YYYYYYY...........#"
        "#..............................#"
        "#.....GGGGGGGG.................#"
        "#.............YYYYYYYY...YYYYYY#"
        "#....########...Y..............#"
        "#....#......#...Y..............#"
        "#....#......#...Y..............#"
        "#....###........Y.......Y......#"
        "#...............Y..............#"
        "#...YYYYYYYYYYYYY..............#"
        "#...Y......Y...................#"
        "#...Y......Y.......BBBBBB......#"
        "#..........Y.......B...........#"
        "#.......YYYY.......Y....BBBB...#"
        "#...Y..Y...........Y....B......#"
        "#YYY...G...........Y....B......#"
        "#......GGGGGGGGGGGGG....B......#"
        "#..............................#"
        "################################";
    n1.inicioX = 3.5f;  n1.inicioY = 3.5f;  n1.inicioAngulo = 0.78f;
    n1.portalX = 28.5f; n1.portalY = 28.5f;
    n1.enemigos = {
        crearEnemigo(16, 8), crearEnemigo(26, 5), crearEnemigo(6, 26),
        crearEnemigo(16, 16), crearEnemigo(25, 22), crearEnemigo(12, 25)
    };
    n1.arboles = {
        { 8.5f,  4.5f },
        { 14.5f, 14.5f },
        { 22.5f, 14.5f },
        { 20.5f, 20.5f },
        { 26.5f, 22.5f },
        { 50.5f, 30.5f },
        { 70.5f, 50.5f },
        { 4.5f, 24.5f },
        { 10.5f, 30.5f },
        { 7.5f, 2.5f },
        { 12.5f, 2.5f },
        { 10.5f, 30.5f },
        { 16.5f, 25.5f },
        { 28.5f, 2.5f },
        { 8.5f, 2.5f },
        { 12.5f, 3.5f },
        { 26.5f, 18.5f },
        { 28.5f, 20.5f },
        { 25.5f, 22.5f } ,
        { 29.5f, 24.5f }
            };
    niveles.push_back(n1);

    // NIVEL 2
    Nivel n2;
    n2.cinematicaInicio = {
        "NUDO: TE ESTAS CANSANDO DE VER LORD PYTHONS",
        {
            "ESTA LLENO DE LORDS PYTHONS.",
            "BUSCA LA SALIDA ES MUY FACIL.",
            "ESTE ES EL ULTIMO NIVEL."
        },
        6.0f
    };
    n2.mapa =
        	"################################"
        "#..............................#"
        "#...................#..........#"
        "#...................#..........#" // <-- SALIDA / PORTAL (x=28, y=3)
        "#..YYYYYYYYYYYYYY...#..........#"
        "#..Y............Y...#..........#"
        "#..Y.....##.....Y...#..........#"
        "#..Y.....##.....Y...#..GGGGGGGG#"
        "#..Y.....##.....Y...#..G.......#"
        "#..Y.....##.....Y...#..G.......#"
        "#..Y.....##.....Y...#..G.......#"
        "#..Y.....##.....Y......G.......#"
        "#...............YYYYYYYG.......#"
        "#......................G...YGGG#"
        "#YYY......##########...G...Y...#"
        "#......................G...Y...#"
        "#.......#....#.....#...G...Y...#"
        "#..Y....#....#.....#...G...Y...#"
        "#..Y....#....#.....#...G...Y...#"
        "#..Y....#....#.....#...G...Y...#"
        "#..Y....#....#.....#...G...Y...#"
        "#..Y....#....#.....#...G...Y...#"
        "#..Y...................G...Y...#"
        "#..Y...#################...Y...#"
        "#..........................Y...#"
        "#..........................Y...#"
        "#......#####################...#"
        "#......#.......................#"
        "#......#.......................#" // <-- INICIO JUGADOR (x=3, y=28)
        "#......#.......................#"
        "#..............................#"
        "################################";
    n2.inicioX = 3.5f;  n2.inicioY = 28.5f; n2.inicioAngulo = 3.14159f;
    n2.portalX = 28.5f; n2.portalY = 3.5f;
    n2.enemigos = {
        crearEnemigo(15, 28), // Pasillo inferior (después de salir de tu zona inicial)
        crearEnemigo(14, 24), // Corredor ancho debajo de la muralla ###
        crearEnemigo(29, 20), // Pasillo largo derecho (lado este)
        crearEnemigo(26, 10), // Sala verde (G) antes de llegar al portal
        crearEnemigo(12, 13), // Patio interior grande (zona central)
        crearEnemigo(6,  18), // Zona de pilares verticales (lado izquierdo)
        crearEnemigo(23, 4),
        crearEnemigo(29, 27),
        crearEnemigo(29, 30),
        crearEnemigo(29, 25),
        crearEnemigo(7.5, 28),
        crearEnemigo(16.5, 28.5),
    };
    n2.arboles = {
        { 12.5f,  6.5f }, // Dentro de la sala superior amarilla (Y)
        {  5.5f, 10.5f }, // Lado izquierdo de la sala amarilla
        { 16.5f, 15.5f }, // Centro del patio interior
        { 15.5f, 21.5f }, // Debajo de los pilares verticales
        {  8.5f, 24.5f }, // Explanada abierta abajo a la izquierda
        { 25.5f,  4.5f },  // Junto a la zona del portal (arriba a la derecha)
        { 3.5f,  15.5f },
        { 4.5f,  17.5f },
        { 28.5f, 7.5f },
    };
    niveles.push_back(n2);

    // NIVEL 3
    Nivel n3;
    n3.cinematicaInicio = {
        "DESENLACE",
        {
            "VAMOS YA VAN A SER LAS 6 DE LA MAÑANA ",
            "Y TU MAMA TE VA DESPERTAR PERO TODAVIA FALTA.",
            "CRUZA EL ULTIMO PORTAL Y DESPIERTATE ANTES QUE TU MAMI"
        },
        6.0f
    };
    n3.mapa =
       	"################################"
        "#......................#..Y....#"
        "#...####################..Y....#"
        "#...#.....................Y....#" // <-- SALIDA / PORTAL (x=16, y=3)
        "#.........................Y....#"
        "#...#######################....#"
        "#...G..........................#"
        "#...G..........#############...#"
        "#......................G.....Y.#"
        "#..#####################.....Y.#"
        "#............YYY.............Y.#"
        "######.............YY........Y.#"
        "#........................G...Y.#"
        "#......GGGGGGGGGGGGGG....G...Y.#"
        "#...................G....G...Y.#"
        "#....Y..............G....GGG...#"
        "#....YYYYYYYY...YY..G......G...#"
        "#...............YY..G......G...#"
        "#....YYYYYYYY.......G......G...#"
        "#...............YY.....GG..G...#"
        "#YYYY..........YY..............#"
        "#..........YYYYY....#......YYYY#"
        "#...................#..GG..G...#"
        "#....YYYYYYYYY......#......G...#"
        "#...................#......G...#"
        "#..YYYYYYYYY........#..GG..G...#"
        "#............########......G...#"
        "#...##########.............G...#"
        "#.....##............########...#" // <-- INICIO DEL JUGADOR (x=16, y=29)
        "#..........###.................#"
        "################################";
    n3.inicioX = 16.0f; n3.inicioY = 29.0f; n3.inicioAngulo = 3.14159f;
    n3.portalX = 16.0f; n3.portalY = 3.0f;
    n3.enemigos = {
        crearEnemigo(15.5, 28.5), // Pasillo inferior (después de salir de tu zona inicial)
        crearEnemigo(19.5, 24.5), // Corredor ancho debajo de la muralla ###
        crearEnemigo(29.5, 20.5), // Pasillo largo derecho (lado este)
        crearEnemigo(26.5, 10.5), // Sala verde (G) antes de llegar al portal
        crearEnemigo(12.5, 13.5), // Patio interior grande (zona central)
        crearEnemigo(6.5,  18.5), // Zona de pilares verticales (lado izquierdo)
        crearEnemigo(23.5, 4.5),
        crearEnemigo(29.5, 27.5),
        crearEnemigo(29.5, 30.5),
        crearEnemigo(29.5, 25.5),
        crearEnemigo(4.5, 28.5),
        crearEnemigo(26.5, 28.5),
        crearEnemigo(26.6, 28.6),
        crearEnemigo(21.5, 21.5),
    }; n3.arboles = {
         { 12.5f,  6.5f }, // Dentro de la sala superior amarilla (Y)
        {  5.5f, 10.5f }, // Lado izquierdo de la sala amarilla
        { 16.5f, 15.5f }, // Centro del patio interior
        { 15.5f, 21.5f }, // Debajo de los pilares verticales
        {  8.5f, 24.5f }, // Explanada abierta abajo a la izquierda
        { 25.5f,  4.5f },  // Junto a la zona del portal (arriba a la derecha)
        { 3.5f,  15.5f },
        { 4.5f,  17.5f },  { 5.5f,  13.5f },
        { 28.5f, 7.5f },
        { 20.5f, 7.5f },};
    niveles.push_back(n3);

    return niveles;
}

// =========================================================
// 8. MINIMAPA CUADRADO ROTATORIO (SINCRONIZADO CON 3D) + UI
// =========================================================
void dibujarRadarRotatorio(
    vector<CHAR_INFO>& pantalla, const Nivel& nivel,
    float jugadorX, float jugadorY, float angulo,
    const AgujeroNegro& agujero
) {
    // Cuadrícula visualmente cuadrada (17x17 celdas, usando 2 columnas por celda para compensar fuente 6x10)
    const int RADIO = 8;
    const int TAM = RADIO * 2 + 1;
    const int MARGEN_X = 3;
    const int MARGEN_Y = 2;

    // Vectores exactos de la cámara 3D:
    // "Adelante" es (sin(angulo), cos(angulo))
    // "Derecha"  es (cos(angulo), -sin(angulo)) -> coincide con girar a la derecha en 3D
    float adelanteX = sinf(angulo);
    float adelanteY = cosf(angulo);
    float derechaX  = cosf(angulo);
    float derechaY  = -sinf(angulo);

    // Marco exterior cuadrado
    WORD colMarco = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    int anchoVisual = TAM * 2;

    for (int i = -1; i <= anchoVisual; i++) {
        pantalla[(MARGEN_Y - 1) * ANCHO + (MARGEN_X + i)] = celda((char)205, colMarco);   // ═
        pantalla[(MARGEN_Y + TAM) * ANCHO + (MARGEN_X + i)] = celda((char)205, colMarco); // ═
    }
    for (int i = 0; i < TAM; i++) {
        pantalla[(MARGEN_Y + i) * ANCHO + (MARGEN_X - 1)] = celda((char)186, colMarco);           // ║
        pantalla[(MARGEN_Y + i) * ANCHO + (MARGEN_X + anchoVisual)] = celda((char)186, colMarco); // ║
    }
    pantalla[(MARGEN_Y - 1) * ANCHO + (MARGEN_X - 1)] = celda((char)201, colMarco);           // ╔
    pantalla[(MARGEN_Y - 1) * ANCHO + (MARGEN_X + anchoVisual)] = celda((char)187, colMarco); // ╗
    pantalla[(MARGEN_Y + TAM) * ANCHO + (MARGEN_X - 1)] = celda((char)200, colMarco);         // ╚
    pantalla[(MARGEN_Y + TAM) * ANCHO + (MARGEN_X + anchoVisual)] = celda((char)188, colMarco);// ╝

    // 1. Dibujar terreno rotado según tu ángulo de visión
    for (int ry = -RADIO; ry <= RADIO; ry++) {
        for (int rx = -RADIO; rx <= RADIO; rx++) {
            float avance = (float)(-ry); // ry negativo es ARRIBA (adelante)
            float lateral = (float)(rx); // rx positivo es DERECHA

            float wx = jugadorX + adelanteX * avance + derechaX * lateral;
            float wy = jugadorY + adelanteY * avance + derechaY * lateral;

            int mx = (int)floorf(wx);
            int my = (int)floorf(wy);

            int sx = MARGEN_X + (rx + RADIO) * 2;
            int sy = MARGEN_Y + (ry + RADIO);
            int idx = sy * ANCHO + sx;

            if (mx < 0 || mx >= MAPA_TAM || my < 0 || my >= MAPA_TAM) {
                pantalla[idx] = celda(' ', 0);
                pantalla[idx + 1] = celda(' ', 0);
            } else {
                char c = nivel.mapa[my * MAPA_TAM + mx];
                if (c == '.') {
                    bool puntoGrilla = ((mx + my) % 2 == 0);
                    pantalla[idx] = puntoGrilla ? celda((char)250, FOREGROUND_INTENSITY) : celda(' ', 0);
                    pantalla[idx + 1] = celda(' ', 0);
                } else {
                    WORD col = FOREGROUND_RED | FOREGROUND_INTENSITY;
                    if (c == 'B') col = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                    else if (c == 'G') col = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                    else if (c == 'Y') col = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                    pantalla[idx] = celda((char)219, col);
                    pantalla[idx + 1] = celda((char)219, col);
                }
            }
        }
    }

    // 2. Proyectar entidades usando producto punto con los ejes "derecha" y "adelante"
    auto dibujarEntidadEnRadar = [&](float ex, float ey, char simbolo, WORD color) {
        float dx = ex - jugadorX;
        float dy = ey - jugadorY;

        float lateral = dx * derechaX + dy * derechaY;
        float avance  = dx * adelanteX + dy * adelanteY;

        int rx = (int)roundf(lateral);
        int ry = (int)roundf(-avance);

        rx = max(-RADIO, min(RADIO, rx));
        ry = max(-RADIO, min(RADIO, ry));

        int sx = MARGEN_X + (rx + RADIO) * 2;
        int sy = MARGEN_Y + (ry + RADIO);
        pantalla[sy * ANCHO + sx] = celda(simbolo, color);
        pantalla[sy * ANCHO + sx + 1] = celda(simbolo, color);
    };
    for (const auto& arb : nivel.arboles) {
    dibujarEntidadEnRadar(arb.x, arb.y, 'T' , FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    dibujarEntidadEnRadar(nivel.portalX, nivel.portalY, 'X', FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    if (agujero.activo) {
        dibujarEntidadEnRadar(agujero.x, agujero.y, '@', FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }
    for (const auto& e : nivel.enemigos) {
        if (e.vivo) {
            dibujarEntidadEnRadar(e.x, e.y, 'E', FOREGROUND_RED | FOREGROUND_INTENSITY);
        }
    }

    // 3. Jugador siempre en el centro mirando hacia arriba (/\)
    int centroX = MARGEN_X + RADIO * 2;
    int centroY = MARGEN_Y + RADIO;
    WORD colJugador = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    pantalla[centroY * ANCHO + centroX] = celda('/', colJugador);
    pantalla[centroY * ANCHO + centroX + 1] = celda('\\', colJugador);

    escribir(pantalla, MARGEN_X, MARGEN_Y + TAM + 1, "/\\:Tu  EE:Enemigo  XX:Portal", FOREGROUND_INTENSITY);
}

void dibujarUI(
    vector<CHAR_INFO>& pantalla, const Nivel& nivel, int nivelActual, int totalNiveles,
    float jugadorX, float jugadorY, float angulo,
    int vidas, float energia, const AgujeroNegro& agujero,
    int bajas, float recargaDisparo, float flashGolpe
) {
    // 1. Mira central
    int cx = ANCHO / 2, cy = ALTO / 2;
    WORD colMira = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    pantalla[cy * ANCHO + cx] = celda('+', colMira);
    pantalla[cy * ANCHO + cx - 1] = celda('-', colMira);
    pantalla[cy * ANCHO + cx + 1] = celda('-', colMira);

    // 2. Escopeta
    int retroceso = (recargaDisparo > 0.2f) ? -2 : 0;
    vector<string> arma = {
        "      [||||]      ",
        "     /[||||]\\     ",
        "    / |====| \\    ",
        "   |  |====|  |   ",
        "  /___|____|___\\  ",
        " |______________| "
    };
    for (int gy = 0; gy < (int)arma.size(); gy++) {
        for (int gx = 0; gx < (int)arma[gy].size(); gx++) {
            if (arma[gy][gx] != ' ')
                pantalla[(ALTO - 6 + gy + retroceso) * ANCHO + (cx - 9 + gx)] =
                    celda(arma[gy][gx], FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }
    }

    // 3. Alerta roja en los bordes al recibir un golpe
    if (flashGolpe > 0.0f) {
        WORD colGolpe = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        for (int x = 0; x < ANCHO; x++) { pantalla[x] = celda('!', colGolpe); pantalla[(ALTO - 1) * ANCHO + x] = celda('!', colGolpe); }
        for (int y = 0; y < ALTO; y++)  { pantalla[y * ANCHO] = celda('!', colGolpe); pantalla[y * ANCHO + ANCHO - 1] = celda('!', colGolpe); }
        string aviso = " !!! IMPACTO - VIDAS RESTANTES: " + to_string(vidas) + "/" + to_string(VIDAS_MAX) + " !!! ";
        escribir(pantalla, (ANCHO - (int)aviso.size()) / 2, 4, aviso, colGolpe);
    }

    // 4. Radar cuadrado rotatorio en la esquina superior izquierda
    dibujarRadarRotatorio(pantalla, nivel, jugadorX, jugadorY, angulo, agujero);

    // 5. Información flotante (esquina superior derecha)
    string infoNivel = "ACTO " + to_string(nivelActual + 1) + "/" + to_string(totalNiveles) + "  |  BAJAS: " + to_string(bajas);
    escribir(pantalla, ANCHO - (int)infoNivel.size() - 3, 1, infoNivel, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    escribir(pantalla, ANCHO - 34, 2, "[WASD] Mover [Q] Dash [E] Bomba", FOREGROUND_INTENSITY);

    // 6. Barra de VIDA (esquina inferior izquierda)
    WORD colVida = (vidas == 1) ? (FOREGROUND_RED | FOREGROUND_INTENSITY)
                 : (vidas <= 3 ? (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
                               : (FOREGROUND_GREEN | FOREGROUND_INTENSITY));
    escribir(pantalla, 3, ALTO - 3, "VIDA " + to_string(vidas) + "/" + to_string(VIDAS_MAX), colVida);
    dibujarBarra(pantalla, 3, ALTO - 2, 18, (float)vidas / VIDAS_MAX, colVida);

    // 7. Barra de ENERGÍA COMPARTIDA para Dash [Q] y Bomba [E] (esquina inferior derecha)
    WORD colEnergia = (energia >= COSTO_HABILIDAD)
        ? (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
        : FOREGROUND_INTENSITY;
    string txtEnergia = "ENERGIA [Q] DASH / [E] BOMBA: " + to_string((int)energia) + "%";
    escribir(pantalla, ANCHO - (int)txtEnergia.size() - 3, ALTO - 3, txtEnergia, colEnergia);
    dibujarBarra(pantalla, ANCHO - 23, ALTO - 2, 18, energia / ENERGIA_MAX, colEnergia);
}

// =========================================================
// 9. BUCLE PRINCIPAL DEL JUEGO
// =========================================================
int main() {
    srand(GetTickCount());

    vector<CHAR_INFO> pantalla(ANCHO * ALTO);
    HANDLE hConsola = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(hConsola);

    SMALL_RECT vMin = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(hConsola, TRUE, &vMin);
    SetConsoleScreenBufferSize(hConsola, { (SHORT)ANCHO, (SHORT)ALTO });

    CONSOLE_FONT_INFOEX fuente = { sizeof(fuente), 0, {6, 10}, FF_DONTCARE, FW_NORMAL };
    const wchar_t* nomFuente = L"Consolas";
    for (int i = 0; nomFuente[i] != L'\0'; i++) { fuente.FaceName[i] = nomFuente[i]; fuente.FaceName[i + 1] = L'\0'; }
    SetCurrentConsoleFontEx(hConsola, FALSE, &fuente);

    SMALL_RECT ventanaFinal = { 0, 0, (SHORT)(ANCHO - 1), (SHORT)(ALTO - 1) };
    SetConsoleWindowInfo(hConsola, TRUE, &ventanaFinal);

    CONSOLE_CURSOR_INFO cursor = { 1, FALSE };
    SetConsoleCursorInfo(hConsola, &cursor);

    vector<Nivel> niveles = crearNiveles();
    EscenaCinematica cinematicaVictoria = {
        "EPILOGO:TE HAS DESPERTADO DE TU FEO SUEÑO",
        {
            "TE LEVANTAS PERO TE DAS CUENTA QUE TU MAMA...",
            "ES EL LORD PYTHON.",
            "EL LORD PYTHON VA A VOLVER EN FORMA DE FICHAS "
        },
        6.0f
    };

    int nivelActual = 0, vidas = VIDAS_MAX, bajas = 0;
    float jugadorX = niveles[0].inicioX, jugadorY = niveles[0].inicioY, angulo = niveles[0].inicioAngulo;
    float energia = ENERGIA_MAX, tiempoDash = 0.0f, dashVelX = 0.0f, dashVelY = 0.0f;
    float recargaDisparo = 0.0f, inmunidad = 0.0f, flashGolpe = 0.0f;
    bool usarRaton = true, teclaMPulsada = false, teclaQPulsada = false, teclaEPulsada = false;
    bool derrota = false, victoria = false;
    AgujeroNegro agujero = { 0, 0, 0, false };

    reproducirCinematica(hConsola, ventanaFinal, pantalla, niveles[0], niveles[0].cinematicaInicio);

    DWORD tiempoAnterior = GetTickCount();
    float relojGlobal = 0.0f;
    HWND hwnd = GetConsoleWindow();

    while (!derrota && !victoria) {
        DWORD ahora = GetTickCount();
        float dt = min(0.1f, (ahora - tiempoAnterior) / 1000.0f);
        tiempoAnterior = ahora;
        relojGlobal += dt;

        Nivel& nivel = niveles[nivelActual];
        recargaDisparo = max(0.0f, recargaDisparo - dt);
        inmunidad = max(0.0f, inmunidad - dt);
        flashGolpe = max(0.0f, flashGolpe - dt);
        tiempoDash = max(0.0f, tiempoDash - dt);
        energia = min(ENERGIA_MAX, energia + REGEN_ENERGIA * dt);

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;

        if (GetAsyncKeyState('M') & 0x8000) {
            if (!teclaMPulsada) { usarRaton = !usarRaton; teclaMPulsada = true; }
        } else teclaMPulsada = false;

        if (usarRaton && GetForegroundWindow() == hwnd) {
            RECT r; POINT p;
            if (GetWindowRect(hwnd, &r) && GetCursorPos(&p)) {
                int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
                if (p.x != cx) { angulo += (p.x - cx) * 0.0028f; SetCursorPos(cx, cy); }
            }
        }
        if ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('J') & 0x8000)) angulo -= 3.0f * dt;
        if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('L') & 0x8000)) angulo += 3.0f * dt;

        float dirX = sinf(angulo), dirY = cosf(angulo);
        float inputX = 0.0f, inputY = 0.0f;
        if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000))   { inputX += dirX; inputY += dirY; }
        if ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000)) { inputX -= dirX; inputY -= dirY; }
        if (GetAsyncKeyState('A') & 0x8000) { inputX -= dirY; inputY += dirX; }
        if (GetAsyncKeyState('D') & 0x8000) { inputX += dirY; inputY -= dirX; }

        float magInput = hypot(inputX, inputY);
        if (magInput > 0.0f) { inputX /= magInput; inputY /= magInput; }

        // DASH [Q] (gasta 50% de la energía compartida)
        if (GetAsyncKeyState('Q') & 0x8000) {
            if (!teclaQPulsada && energia >= COSTO_HABILIDAD && tiempoDash <= 0.0f) {
                energia -= COSTO_HABILIDAD;
                tiempoDash = 0.16f;
                float dX = (magInput > 0.0f) ? inputX : dirX;
                float dY = (magInput > 0.0f) ? inputY : dirY;
                dashVelX = dX * 22.0f;
                dashVelY = dY * 22.0f;
                teclaQPulsada = true;
            }
        } else teclaQPulsada = false;

        // BOMBA DE AGUJERO NEGRO [E] (gasta 50% de la misma energía compartida)
        if (GetAsyncKeyState('E') & 0x8000) {
            if (!teclaEPulsada && !agujero.activo && energia >= COSTO_HABILIDAD) {
                energia -= COSTO_HABILIDAD;
                float px = jugadorX, py = jugadorY;
                for (float d = 0.2f; d <= 3.8f; d += 0.1f) {
                    float tx = jugadorX + dirX * d, ty = jugadorY + dirY * d;
                    if (esTransitable(nivel.mapa, tx, ty)) { px = tx; py = ty; }
                    else break;
                }
                agujero = { px, py, DURACION_BOMBA, true };
                teclaEPulsada = true;
            }
        } else teclaEPulsada = false;

        float movX = (tiempoDash > 0.0f) ? (dashVelX * dt) : (inputX * 6.0f * dt);
        float movY = (tiempoDash > 0.0f) ? (dashVelY * dt) : (inputY * 6.0f * dt);

        if (esTransitable(nivel.mapa, jugadorX + movX, jugadorY)) jugadorX += movX;
        if (esTransitable(nivel.mapa, jugadorX, jugadorY + movY)) jugadorY += movY;

        // Cruzar portal
        if (hypot(nivel.portalX - jugadorX, nivel.portalY - jugadorY) < 1.0f) {
            nivelActual++;
            agujero.activo = false;

            if (nivelActual >= (int)niveles.size()) {
                reproducirCinematica(hConsola, ventanaFinal, pantalla, niveles.back(), cinematicaVictoria);
                victoria = true;
                break;
            }
            jugadorX = niveles[nivelActual].inicioX;
            jugadorY = niveles[nivelActual].inicioY;
            angulo = niveles[nivelActual].inicioAngulo;
            reproducirCinematica(hConsola, ventanaFinal, pantalla, niveles[nivelActual], niveles[nivelActual].cinematicaInicio);
            tiempoAnterior = GetTickCount();
            continue;
        }

        // Disparar escopeta
        bool disparar = (GetAsyncKeyState(VK_SPACE) & 0x8000) || (usarRaton && (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
        if (disparar && recargaDisparo <= 0.0f) {
            recargaDisparo = 0.35f;
            sort(nivel.enemigos.begin(), nivel.enemigos.end(), [](const Enemigo& a, const Enemigo& b) { return a.distancia < b.distancia; });

            for (auto& e : nivel.enemigos) {
                if (!e.vivo) continue;
                float vx = e.x - jugadorX, vy = e.y - jugadorY;
                float ang = atan2f(vx, vy) - atan2f(dirX, dirY);
                if (ang < -3.14159f) ang += 6.28318f;
                if (ang > 3.14159f) ang -= 6.28318f;

                if (fabs(ang) < 0.13f && hypot(vx, vy) < 18.0f && hayLineaDeVision(nivel.mapa, jugadorX, jugadorY, e.x, e.y)) {
                    e.vivo = false; e.tiempoReaparicion = 7.0f; bajas++;
                    break;
                }
            }
        }

        // Actualizar agujero negro
        if (agujero.activo) {
            agujero.tiempoVida -= dt;
            if (agujero.tiempoVida <= 0.0f) agujero.activo = false;

            for (auto& e : nivel.enemigos) {
                if (!e.vivo || !agujero.activo) continue;
                float dx = agujero.x - e.x, dy = agujero.y - e.y;
                float dist = hypot(dx, dy);
                if (dist < 4.8f) {
                    if (dist < 0.65f) {
                        e.vivo = false; e.tiempoReaparicion = 7.0f; bajas++;
                        agujero.activo = false;
                        break;
                    }
                    float px = (dx / dist) * 4.8f * dt, py = (dy / dist) * 4.8f * dt;
                    if (esTransitable(nivel.mapa, e.x + px, e.y)) e.x += px;
                    if (esTransitable(nivel.mapa, e.x, e.y + py)) e.y += py;
                }
            }
        }

        // Actualizar IA de enemigos
        float distMasCercano = 999.0f;
        actualizarIAEnemigos(nivel.enemigos, nivel.mapa, jugadorX, jugadorY, dirX, dirY,
                             (tiempoDash > 0.0f), agujero, vidas, inmunidad, flashGolpe, derrota, distMasCercano, dt);

        // Minitemblor sutil por proximidad (< 4.5 unidades)
        float temblorAngulo = 0.0f;
        int temblorVertical = 0;
        const float RADIO_TEMBLOR = 4.5f;
        if (distMasCercano < RADIO_TEMBLOR) {
            float cercania = (RADIO_TEMBLOR - distMasCercano) / RADIO_TEMBLOR;
            float intensidad = cercania * cercania;
            temblorAngulo = sinf(relojGlobal * 55.0f) * (intensidad * 0.011f);
            temblorVertical = (int)(cosf(relojGlobal * 70.0f) * (intensidad * 0.75f));
        }
        if (flashGolpe > 0.0f) temblorAngulo += sinf(relojGlobal * 55.0f) * 0.035f;

        float fovActual = (3.14159f / 3.5f) + ((tiempoDash > 0.0f) ? 0.12f : 0.0f);

        dibujarEscena3D(pantalla, nivel.mapa, jugadorX, jugadorY, angulo + temblorAngulo,
                fovActual, temblorVertical, nivel.enemigos, nivel.arboles,
                agujero, nivel.portalX, nivel.portalY, relojGlobal);
        dibujarUI(pantalla, nivel, nivelActual, (int)niveles.size(),
                  jugadorX, jugadorY, angulo, vidas, energia, agujero, bajas, recargaDisparo, flashGolpe);

        WriteConsoleOutputA(hConsola, pantalla.data(), { (SHORT)ANCHO, (SHORT)ALTO }, { 0, 0 }, &ventanaFinal);
    }

    if (derrota || victoria) {
        string msg = victoria
            ? (" VICTORIA: PROYECTO EREBUS SELLADO - BAJAS: " + to_string(bajas) + " - [ESC] ")
            : (" HAS CAIDO EN COMBATE - BAJAS: " + to_string(bajas) + " - [ESC] ");
        WORD colFin = (victoria ? BACKGROUND_GREEN : BACKGROUND_RED) | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        escribir(pantalla, (ANCHO - (int)msg.size()) / 2, ALTO / 2, msg, colFin);
        WriteConsoleOutputA(hConsola, pantalla.data(), { (SHORT)ANCHO, (SHORT)ALTO }, { 0, 0 }, &ventanaFinal);
        while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) Sleep(50);
    }

    return 0;
}


