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
    cout<<endl;
    cout << "Sala " << index << endl;
    cout<<endl;
    cout<<"\n--- ECRAN ---\n"<<endl;
    for (int i = 0; i < locuri.size(); i++) {
        for (int j = 0; j < locuri[i].size(); j++) {
            if (locuri[i][j])
                cout << "X ";
            else
                cout << "O ";
        }
        cout << endl;
    }
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
        cout << "\n--- ECRAN ---\n" << endl;
        cout << "Foloseste sagetile pentru a naviga. Apasa ENTER pentru a selecta.\n\n";
        
        for (int i = 0; i < randuri; i++) {
            for (int j = 0; j < coloane; j++) {
                if (i == cursorR && j == cursorC) {
                    if (locuri[i][j]) cout << "[X] ";
                    else cout << "[O] ";
                } else {
                    if (locuri[i][j]) cout << " X  ";
                    else cout << " O  ";
                }
            }
            cout << endl;
        }

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
