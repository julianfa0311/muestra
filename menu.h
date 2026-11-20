
#ifndef MENU_H
#define MENU_H

#include <windows.h>
#include <cstdlib>
#include <conio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "ascii_art.h"

using namespace std;

void posicionarCursorMenu(int x, int y)
{
    COORD pos = {(SHORT)x, (SHORT)y};
    SetConsoleCursorPosition(
        GetStdHandle(STD_OUTPUT_HANDLE), pos
    );
}

vector<string> obtenerLineasAscii(const string& arte)
{
    vector<string> lineas;
    stringstream ss(arte);
    string linea;

    while(getline(ss, linea))
    {
        if(!linea.empty() && linea.back() == '\r')
            linea.pop_back();

        size_t fin = linea.find_last_not_of(" \t");

        if(fin == string::npos)
            linea.clear();
        else
            linea.erase(fin + 1);

        lineas.push_back(linea);
    }

    while(!lineas.empty() && lineas.front().empty())
        lineas.erase(lineas.begin());

    while(!lineas.empty() && lineas.back().empty())
        lineas.pop_back();

    return lineas;
}

int mostrarMenu()
{
    HANDLE consolaMenu = GetStdHandle(STD_OUTPUT_HANDLE);

    // Ocultar cursor parpadeante
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(consolaMenu, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(consolaMenu, &cursorInfo);

    string opciones[3] = {
        "Principiante",
        "Intermedio",
        "Experto"
    };

    vector<string> lineas = obtenerLineasAscii(tituloMenu);

    int anchoTitulo = 0;

    for(const string& linea : lineas)
    {
        anchoTitulo = max(anchoTitulo, (int)linea.size());
    }

    int seleccion = 0;
    int separacion = 3;
    int altoTotal = (int)lineas.size() + separacion + 3;

    while(true)
    {
        system("cls");

        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(consolaMenu, &info);

        int ancho = info.srWindow.Right - info.srWindow.Left + 1;
        int alto = info.srWindow.Bottom - info.srWindow.Top + 1;

        // Centrar el bloque ASCII completo
        int inicioX = info.srWindow.Left +
                      (ancho - anchoTitulo) / 2;

        int inicioY = info.srWindow.Top +
                      (alto - altoTotal) / 2;

        if(inicioX < info.srWindow.Left)
            inicioX = info.srWindow.Left;

        if(inicioY < info.srWindow.Top)
            inicioY = info.srWindow.Top;

        SetConsoleTextAttribute(consolaMenu, 15);

        // Dibujar titulo
        for(int i = 0; i < (int)lineas.size(); i++)
        {
            posicionarCursorMenu(inicioX, inicioY + i);
            cout << lineas[i];
        }

        // Dibujar opciones
        int inicioOpciones = inicioY +
                            (int)lineas.size() +
                            separacion;

        for(int i = 0; i < 3; i++)
        {
            string texto = (seleccion == i ? "> " : "  ");
            texto += opciones[i];
            texto += (seleccion == i ? " <" : "  ");

            int x = info.srWindow.Left +
                    (ancho - (int)texto.size()) / 2;

            if(x < info.srWindow.Left)
                x = info.srWindow.Left;

            posicionarCursorMenu(x, inicioOpciones + i);

            if(seleccion == i)
                SetConsoleTextAttribute(consolaMenu, 14);
            else
                SetConsoleTextAttribute(consolaMenu, 7);

            cout << texto;
        }

        SetConsoleTextAttribute(consolaMenu, 7);

        // Control del menu
        int tecla = _getch();

        if(tecla == 224 || tecla == 0)
        {
            tecla = _getch();

            if(tecla == 72)
                seleccion = (seleccion + 2) % 3;

            if(tecla == 80)
                seleccion = (seleccion + 1) % 3;
        }
        else if(tecla == 13)
        {
            system("cls");
            return seleccion;
        }
    }
}

#endif
