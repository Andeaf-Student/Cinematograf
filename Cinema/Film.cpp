#include <iostream>
#include "Film.h"

using namespace std;

Film::Film(string titlu, string gen, int durata, Sala sala)
    : sala(sala) {
    this->titlu = titlu;
    this->gen = gen;
    this->durata = durata;
}

void Film::afiseaza() const {
    cout << "Titlu: " << titlu << endl;
    cout << "Gen: " << gen << endl;
    cout << "Durata: " << durata << " minute" << endl;
    cout << "Sala: " << sala.getIndex() << endl;
}

Sala& Film::getSala() {
    return sala;
}




