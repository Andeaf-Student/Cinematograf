#include <iostream>
#include "Film.h"

using namespace std;

Film::Film(string titlu, string gen, int durata, int varstaMinima, Sala sala)
    : sala(sala) {
    this->titlu = titlu;
    this->gen = gen;
    this->durata = durata;
    this->varstaMinima = varstaMinima;
}

void Film::afiseaza() const {
    cout << "Titlu: " << titlu << endl;
    cout << "Gen: " << gen << endl;
    cout << "Durata: " << durata << " minute" << endl;
    cout << "Varsta minima: " << varstaMinima << "+" << endl;
    cout << "Sala: " << sala.getIndex() << endl;
}

Sala& Film::getSala() {
    return sala;
}

int Film::getVarstaMinima() const {
    return varstaMinima;
}

string Film::getTitlu() const {
    return titlu;
}


