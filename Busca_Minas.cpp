#include <iostream>
#include <cstdlib>
#include <ctime>
#include <conio.h>
#include <windows.h>
using namespace std;
HANDLE consola = GetStdHandle(STD_OUTPUT_HANDLE);

int dificul = 10;

int movi[8] = {0,0,1,-1,1,-1,1,-1};
int movj[8] = {1,-1,0,0,1,-1,-1,1};

int tableroI[9][9];
bool tableroB[9][9];
bool visible[9][9];
char os[9][9];

#include <iostream>
using namespace std;

void titulo()
{
    cout << R"(.---------------------------------------------------------------------------------------------------------------------------------------.
|_____/\\\\\\\\\\\\_____/\\\\\\\\\_____/\\\\\_____/\\\_____/\\\\\\\\\________/\\\\\\\\\\\____/\\\\\\\\\\\\\\\__/\\\\\\\\\\\\\\\_        |
| ___/\\\//////////____/\\\\\\\\\\\\\__\/\\\\\\___\/\\\___/\\\\\\\\\\\\\____/\\\/////////\\\_\///////\\\/////__\/\\\///////////__       |
|  __/\\\______________/\\\/////////\\\_\/\\\/\\\__\/\\\__/\\\/////////\\\__\//\\\______\///________\/\\\_______\/\\\_____________      |
|   _\/\\\____/\\\\\\\_\/\\\_______\/\\\_\/\\\//\\\_\/\\\_\/\\\_______\/\\\___\////\\\_______________\/\\\_______\/\\\\\\\\\\\_____     |
|    _\/\\\___\/////\\\_\/\\\\\\\\\\\\\\\_\/\\\\//\\\\/\\\_\/\\\\\\\\\\\\\\\______\////\\\____________\/\\\_______\/\\\///////______    |
|     _\/\\\_______\/\\\_\/\\\/////////\\\_\/\\\_\//\\\/\\\_\/\\\/////////\\\_________\////\\\_________\/\\\_______\/\\\_____________   |
|      _\/\\\_______\/\\\_\/\\\_______\/\\\_\/\\\__\//\\\\\\_\/\\\_______\/\\\__/\\\______\//\\\________\/\\\_______\/\\\_____________  |
|       _\//\\\\\\\\\\\\/__\/\\\_______\/\\\_\/\\\___\//\\\\\_\/\\\_______\/\\\_\///\\\\\\\\\\\/_________\/\\\_______\/\\\\\\\\\\\\\\\_ |
|        __\////////////____\///________\///__\///_____\/////__\///________\///____\///////////___________\///________\///////////////__|
'---------------------------------------------------------------------------------------------------------------------------------------'
)";
}
// ---------------- FLOOD FILL ----------------

void floodFill(int f, int c)
{
    if(f < 0 || f >= 9 || c < 0 || c >= 9)
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

        if(f >= 0 && f < 9 && c >= 0 && c < 9)
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
    for(int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            tableroB[i][j] = false;
            tableroI[i][j] = 0;
        }
    }

    // Colocar minas
    while(minas < dificul)
    {
        int f = rand() % 9;
        int c = rand() % 9;

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
    for(int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            vecinos(i, j);
        }
    }
}
// Mostrar tablero
void mostrar(int flei[2], int flej[2] )
{
    for(int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            SetConsoleTextAttribute(consola, 15 | (7 << 4));

            if(os[i][j] == 'b')
            {
                cout << u8"\u2691";
            }else if(os[i][j] == 'X')
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
// ---------------- MAIN ----------------

int main()
{
    int cont = 0, corec = 0;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);


    CONSOLE_CURSOR_INFO cursor;
    srand(time(nullptr));

    cursor.dwSize = 100;
    cursor.bVisible = false;

    SetConsoleCursorInfo(consola, &cursor);

    // Inicializar tablero
    for(int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            os[i][j] = '.';
            visible[i][j] = false;
            tableroB[i][j] = false;
            tableroI[i][j] = 0;
        }
    }
    // Posición inicial
    int flei[2] = {0,0};
    int flej[2] = {0,0};

    bool jugando = true;
    bool k = false;

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

        // Leer flechas
        if(a == 224 || a == 0)
        {
            a = _getch();
        }

        // Guardar posición actual
        flei[1] = flei[0];
        flej[1] = flej[0];

        switch(a)
        {
        case 72: // Arriba
            flei[1] += movi[3];
            flej[1] += movj[3];
            break;

        case 80: // Abajo
            flei[1] += movi[2];
            flej[1] += movj[2];
            break;

        case 75: // Izquierda
            flei[1] += movi[1];
            flej[1] += movj[1];
            break;

        case 77: // Derecha
            flei[1] += movi[0];
            flej[1] += movj[0];
            break;

        case 13: // Pala

            if(tableroB[flei[0]][flej[0]] && k)
            {
                system("cls");
                cout << "BOOOM!!!" << endl;
                for(int i = 0; i < 9; i++)
                {
                    for(int j = 0; j < 9; j++)
                    {
                        if(os[i][j]=='b' && !tableroB[i][j]){
                           os[i][j]='X';
                        }
                    }
                }

                mostrar(flei, flej);
                 SetConsoleTextAttribute(consola, 15);
                return 0;
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
                else
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

        // Comprobar límites
        if(flei[1] >= 0 && flei[1] < 9 &&
                flej[1] >= 0 && flej[1] < 9)
        {
            flei[0] = flei[1];
            flej[0] = flej[1];
        }

        // Comprobar victoria
        if(corec == dificul)
        {
            system("cls");

            titulo();
            return 0;
        }
    }

    return 0;
}
