#include <iostream>
#include <stdexcept>
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


int Sala::getIndex() const {
    return index;
}

int Sala::getNumarRanduri() const {
    return locuri.size();
}
