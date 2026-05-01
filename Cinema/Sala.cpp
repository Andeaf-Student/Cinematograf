#include <iostream>
#include <stdexcept>
#include <conio.h>
#include <cstdlib>
#include "Sala.h"
using namespace std;


Sala::Sala(int index, int randuri, int coloane) {
    this->index = index;
    locuri = vector<vector<bool>>(randuri, vector<bool>(coloane, false));
}


void Sala::afiseazaLocuri() const {
    cout << "\nSala " << index << endl;
    cout << "═══════════════════════\n";
    for (int i = 0; i < locuri.size(); i++) {
        cout << "   " << (char)('A' + i) << "  ";
        for (int j = 0; j < locuri[i].size(); j++) {
            if (locuri[i][j])
                cout << "[■]";
            else
                cout << "[ ]";
        }
        cout << endl;
    }
    cout << "\n   [■] Ocupat  [ ] Liber\n" << endl;
}


void Sala::rezervaLoc(int rand, int col) {
    if (rand < 0 || rand >= locuri.size() ||
        col < 0 || col >= locuri[0].size()) {
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
                    cout << "[▣]";
                } else {
                    if (locuri[i][j]) cout << "[■]";
                    else cout << "[ ]";
                }
            }
            if (i == cursorR) {
                cout << " Selectat: " << (char)('A' + cursorR) << (cursorC + 1);
            }
            cout << endl;
        }
        cout << "\n   [■] Ocupat  [ ] Liber  [▣] Ales\n" << endl;

        int tasta = _getch();
        if (tasta == 224 || tasta == 0) { // Taste speciale (sageti)
            tasta = _getch();
            switch (tasta) {
                case 72: // Sus
                    if (cursorR > 0) cursorR--;
                    break;
                case 80: // Jos
                    if (cursorR < randuri - 1) cursorR++;
                    break;
                case 75: // Stanga
                    if (cursorC > 0) cursorC--;
                    break;
                case 77: // Dreapta
                    if (cursorC < coloane - 1) cursorC++;
                    break;
            }
        } else if (tasta == 13) { // Enter
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
    return locuri.size();
}
