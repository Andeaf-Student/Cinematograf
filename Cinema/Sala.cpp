#include <iostream>
#include <stdexcept>
#include <conio.h>
#include <cstdlib>
#include "Sala.h"
using namespace std;

// Coduri ANSI pentru culori
#define ANSI_RESET  "\033[0m"
#define ANSI_RED    "\033[31m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"


Sala::Sala(int index, int randuri, int coloane) {
    this->index = index;
    locuri = vector<vector<bool>>(randuri, vector<bool>(coloane, false));
}


void Sala::afiseazaLocuri() const {
    cout << "\nSala " << index << endl;
    cout << "═══════════════════════\n";
    for (int i = 0; i < (int)locuri.size(); i++) {
        cout << "   " << (char)('A' + i) << "  ";
        for (int j = 0; j < (int)locuri[i].size(); j++) {
            if (locuri[i][j])
                cout << "[" << ANSI_RED   << "\xe2\x96\xa0" << ANSI_RESET << "]";
            else
                cout << "[" << ANSI_GREEN << "\xe2\x96\xa0" << ANSI_RESET << "]";
        }
        cout << endl;
    }
    cout << "\n   "
         << "[" << ANSI_RED   << "\xe2\x96\xa0" << ANSI_RESET << "]" << " Ocupat  "
         << "[" << ANSI_GREEN << "\xe2\x96\xa0" << ANSI_RESET << "]" << " Liber\n" << endl;
}


void Sala::rezervaLoc(int rand, int col) {
    if (rand < 0 || rand >= (int)locuri.size() ||
        col < 0 || col >= (int)locuri[0].size()) {
        throw out_of_range("Index invalid!");
    }

    if (locuri[rand][col]) {
        throw runtime_error("Loc deja ocupat!");
    }

    locuri[rand][col] = true;
}

void Sala::selecteazaLocInteractiv(int& outRand, int& outCol) const {
    int cursorR = 0, cursorC = 0;
    int randuri = (int)locuri.size();
    int coloane = randuri > 0 ? (int)locuri[0].size() : 0;

    while (true) {
        system("cls");
        cout << "\nFoloseste sagetile pentru a naviga. Apasa ENTER pentru a selecta.\n";
        cout << "═══════════════════════\n";

        for (int i = 0; i < randuri; i++) {
            cout << "   " << (char)('A' + i) << "  ";
            for (int j = 0; j < coloane; j++) {
                if (i == cursorR && j == cursorC) {
                    cout << "[" << ANSI_YELLOW << "\xe2\x96\xa3" << ANSI_RESET << "]";
                } else if (locuri[i][j]) {
                    cout << "[" << ANSI_RED    << "\xe2\x96\xa0" << ANSI_RESET << "]";
                } else {
                    cout << "[" << ANSI_GREEN  << " "            << ANSI_RESET << "]";
                }
            }
            if (i == cursorR) {
                cout << "  Selectat: " << (char)('A' + cursorR) << (cursorC + 1);
            }
            cout << endl;
        }
        cout << "\n   "
             << "[" << ANSI_RED    << "\xe2\x96\xa0" << ANSI_RESET << "]" << " Ocupat  "
             << "[" << ANSI_GREEN  << "\xe2\x96\xa0" << ANSI_RESET << "]" << " Liber  "
             << "[" << ANSI_YELLOW << "\xe2\x96\xa3" << ANSI_RESET << "]" << " Ales\n" << endl;

        int tasta = _getch();
        if (tasta == 224 || tasta == 0) {
            tasta = _getch();
            switch (tasta) {
                case 72: if (cursorR > 0) cursorR--; break;
                case 80: if (cursorR < randuri - 1) cursorR++; break;
                case 75: if (cursorC > 0) cursorC--; break;
                case 77: if (cursorC < coloane - 1) cursorC++; break;
            }
        } else if (tasta == 13) {
            outRand = cursorR;
            outCol = cursorC;
            break;
        }
    }
}


int Sala::getIndex() const {
    return index;
}

int Sala::getNumarRanduri() const {
    return (int)locuri.size();
}

int Sala::getNumarColoane() const {
    return locuri.empty() ? 0 : (int)locuri[0].size();
}
