#include <iostream>
#include "Suvenir.h"

using namespace std;


Suvenir::Suvenir(string nume, double pret) {
    this->nume = nume;
    this->pret = pret;
}

void Suvenir::afiseaza() const {
    cout << nume << " - " << pret << " lei" << endl;
}

string Suvenir::getNume() const {
    return nume;
}

double Suvenir::getPret() const {
    return pret;
}
