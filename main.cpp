
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <conio.h>
#include <windows.h>
#include "ascii_art.h"
#include "menu.h"
#include <sstream>
#include <string>
using namespace std;
HANDLE consola = GetStdHandle(STD_OUTPUT_HANDLE);

const int MAX_TABLERO = 16;

int dificul = 1;

int movi[8] = {0,0,1,-1,1,-1,1,-1};
int movj[8] = {1,-1,0,0,1,-1,-1,1};

int filas = 9;
int columnas = 9;

int tableroI[MAX_TABLERO][MAX_TABLERO];
bool tableroB[MAX_TABLERO][MAX_TABLERO];
bool visible[MAX_TABLERO][MAX_TABLERO];
char os[MAX_TABLERO][MAX_TABLERO];

#include <iostream>
using namespace std;


// ---------------- FLOOD FILL ----------------

void floodFill(int f, int c)
{
    if(f < 0 || f >= filas || c < 0 || c >= columnas)
        return;

    if(os[f][c] == 'b')
        return;

    if(visible[f][c] || tableroB[f][c])
        return;

    visible[f][c] = true;

    if(tableroI[f][c] != 0)
        return;

    for(int i = 0; i < 8; i++)
    {
        floodFill(f + movi[i], c + movj[i]);
    }
}

// ---------------- VECINOS ----------------

void vecinos(int ubi, int ubj)
{
    if(tableroB[ubi][ubj])
        return;

    int cont = 0;

    for(int i = 0; i < 8; i++)
    {
        int f = ubi + movi[i];
        int c = ubj + movj[i];

        if(f >= 0 && f < filas && c >= 0 && c < columnas)
        {
            if(tableroB[f][c])
                cont++;
        }
    }

    tableroI[ubi][ubj] = cont;
}

// ---------------- GENERAR MINAS ----------------

void dibut(int protei, int protej)
{
    int minas = 0;

    // Limpiar tableros
    for(int i = 0; i < filas; i++)
    {
        for(int j = 0; j < columnas; j++)
        {
            tableroB[i][j] = false;
            tableroI[i][j] = 0;
        }
    }

    // Colocar minas
    while(minas < dificul)
    {
        int f = rand() % filas;
        int c = rand() % columnas;

        // Proteger primera casilla y alrededores
        if(abs(f - protei) <= 1 &&
                abs(c - protej) <= 1)
            continue;

        // Evitar repetir minas
        if(tableroB[f][c])
            continue;

        tableroB[f][c] = true;
        minas++;
    }

    // Calcular números
    for(int i = 0; i < filas; i++)
    {
        for(int j = 0; j < columnas; j++)
        {
            vecinos(i, j);
        }
    }
}
// Mostrar tablero
void mostrar(int flei[2], int flej[2] )
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(consola, &info);

    COORD inicio = info.dwCursorPosition;

    for(int i = 0; i < filas; i++)
    {
        for(int j = 0; j < columnas; j++)
        {
            SetConsoleTextAttribute(consola, 15 | (7 << 4));

            if(os[i][j] == 'b')
            {
                DWORD escritos;
                WriteConsoleW(consola, L'\u2691' == 0 ? L"" : L"\u2691", 1, &escritos, NULL);
            }
            else if(os[i][j] == 'X')
            {
                SetConsoleTextAttribute(consola, 4 | (14 << 4));
                cout << os[i][j];
            }
            else if(visible[i][j])
            {
                switch(tableroI[i][j])
                {
                case 1:
                    SetConsoleTextAttribute(consola, 1 | (6 << 4));
                    break;

                case 2:
                    SetConsoleTextAttribute(consola, 2 | (6 << 4));
                    break;

                case 3:
                    SetConsoleTextAttribute(consola, 4 | (6 << 4));
                    break;

                case 4:
                    SetConsoleTextAttribute(consola, 5 | (6 << 4));
                    break;

                default:

                    SetConsoleTextAttribute(consola, 7 | (6 << 4));
                    break;
                }

                if(i == flei[0] && j == flej[0])
                {
                    SetConsoleTextAttribute(consola, 0 | (6 << 4));
                }

                if(tableroI[i][j])
                    cout << tableroI[i][j];
                else
                    cout << ".";

                SetConsoleTextAttribute(consola, 7 | (6 << 4));
            }
            else
            {
                SetConsoleTextAttribute(consola, 7 | (14 << 4));

                if(i == flei[0] && j == flej[0])
                {
                    SetConsoleTextAttribute(consola, 0 | (14 << 4));
                }

                cout << os[i][j];

                SetConsoleTextAttribute(consola, 7 | (14 << 4));
            }

            cout << " ";
        }

        cout << endl;
    }
}
void cambiarZoom(int tam)
{
    CONSOLE_FONT_INFOEX fuente = {0};
    fuente.cbSize = sizeof(fuente);

    if(GetCurrentConsoleFontEx(consola, FALSE, &fuente))
    {
        fuente.dwFontSize.X = 0;
        fuente.dwFontSize.Y = tam;

        SetCurrentConsoleFontEx(consola, FALSE, &fuente);
    }
}
void maximizarConsola()
{
    HWND ventana = GetConsoleWindow();
    ShowWindow(ventana, SW_MAXIMIZE);
}
// ---------------- MAIN ----------------
int main()
{
    srand(time(nullptr));

    while(true)
    {
        // MENU PRINCIPAL
        ShowWindow(GetConsoleWindow(), SW_RESTORE);

        cambiarZoom(16);
        system("mode con: cols=145 lines=40");

        maximizarConsola();
        system("cls");

        int dificultad = mostrarMenu();
        int zoomJuego;
        switch(dificultad)
        {
            case 0: // Principiante
                dificul = 10;
                filas = 9;
                columnas = 9;
                zoomJuego = 32;
                break;

            case 1: // Intermedio
                dificul = 20;
                filas = 12;
                columnas = 12;
                zoomJuego = 24;
                break;

            case 2: // Experto
                dificul = 35;
                filas = 16;
                columnas = 16;
                zoomJuego = 18;
                break;
            }


            // CONFIGURAR JUEGO
            cambiarZoom(zoomJuego);
            system("mode con: cols=45 lines=26");
            maximizarConsola();
            system("cls");

            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);

            CONSOLE_CURSOR_INFO cursor;
            cursor.dwSize = 100;
            cursor.bVisible = false;
            SetConsoleCursorInfo(consola, &cursor);

            int cont = 0;
            int corec = 0;

            bool jugando = true;
            bool k = false;
            bool terminoPartida = false;

            int flei[2] = {0, 0};
            int flej[2] = {0, 0};

            // REINICIAR TABLERO
            for(int i = 0; i < filas; i++)
            {
                for(int j = 0; j < columnas; j++)
                {
                    os[i][j] = '.';
                    visible[i][j] = false;
                    tableroB[i][j] = false;
                    tableroI[i][j] = 0;
                }
            }

            // JUEGO
            while(jugando)
            {
                system("cls");
                mostrar(flei, flej);

                SetConsoleTextAttribute(consola, 15);

                cout << "\nFlechas: mover";
                cout << "\nENTER: pala";
                cout << "\nBACKSPACE: bandera";
                cout << "\nESC: salir";
                cout << "\nBANDERAS=" << dificul - cont;

                int a = _getch();

                if(a == 224 || a == 0)
                    a = _getch();

                flei[1] = flei[0];
                flej[1] = flej[0];

                switch(a)
                {
                case 72: // Arriba
                    flei[1]--;
                    break;

                case 80: // Abajo
                    flei[1]++;
                    break;

                case 75: // Izquierda
                    flej[1]--;
                    break;

                case 77: // Derecha
                    flej[1]++;
                    break;

                case 13: // Pala

                    if(tableroB[flei[0]][flej[0]] && k)
                    {
                        // Marcar banderas incorrectas
                        for(int i = 0; i < filas; i++)
                        {
                            for(int j = 0; j <columnas; j++)
                            {
                                if(os[i][j] == 'b' && !tableroB[i][j])
                                    os[i][j] = 'X';
                            }
                        }

                        // Preparar pantalla de derrota
                        ShowWindow(GetConsoleWindow(), SW_RESTORE);
                        cambiarZoom(16);
                        system("mode con: cols=100 lines=38");
                        maximizarConsola();
                        system("cls");

                        // Tablero a la izquierda
                        COORD pos;
                        pos.X = 0;
                        pos.Y = 12;
                        SetConsoleCursorPosition(consola, pos);

                        mostrar(flei, flej);

                        // ASCII Art a la derecha
                        SetConsoleTextAttribute(consola, 12);

                        stringstream texto(tituloPerdiste);
                        string linea;

                        int y = 3;

                        while(getline(texto, linea))
                        {
                            pos.X = columnas *2 +8;
                            pos.Y = y;

                            SetConsoleCursorPosition(consola, pos);
                            cout << linea;

                            y++;
                        }

                        SetConsoleTextAttribute(consola, 15);

                        terminoPartida = true;
                        jugando = false;
                        break;
                    }

                    if(!k)
                        dibut(flei[0], flej[0]);

                    k = true;

                    floodFill(flei[0], flej[0]);
                    break;

                case 8: // Bandera

                    if(!visible[flei[0]][flej[0]])
                    {
                        if(os[flei[0]][flej[0]] == 'b')
                        {
                            os[flei[0]][flej[0]] = '.';
                            cont--;

                            if(tableroB[flei[0]][flej[0]])
                                corec--;
                        }
                        else if(cont < dificul)
                        {
                            os[flei[0]][flej[0]] = 'b';
                            cont++;

                            if(tableroB[flei[0]][flej[0]])
                                corec++;
                        }
                    }
                    break;

                case 27: // ESC
                    jugando = false;
                    break;
                }

                // COMPROBAR LIMITES
                if(flei[1] >= 0 && flei[1] < filas &&
                        flej[1] >= 0 && flej[1] < columnas)
                {
                    flei[0] = flei[1];
                    flej[0] = flej[1];
                }

                // COMPROBAR VICTORIA
                if(!terminoPartida && corec == dificul)
                {
                    cambiarZoom(18);
                    system("mode con: cols=145 lines=35");
                    maximizarConsola();

                    system("cls");
                    SetConsoleTextAttribute(consola, 10);
                    ganaste();

                    terminoPartida = true;
                    jugando = false;
                }
            }

            // ESPERAR 10 SEGUNDOS
            if(terminoPartida)
            {
                Sleep(10000);
                system("cls");
            }

        } // Vuelve al menu

        return 0;
    }

