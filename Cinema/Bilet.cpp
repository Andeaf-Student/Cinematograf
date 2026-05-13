#include "Bilet.h"

using namespace std;

Bilet::Bilet(string titlu, string data, string l, string p, string cod)
    : titluFilm(titlu), dataOra(data), loc(l), pret(p), codBilet(cod) {}

void Bilet::afiseaza() const {
    cout << "\n┌─────────────────────────────────────────┐\n";
    cout << "│   🎬 CINEMA AURORA                      │\n";
    cout << "│   Film: " << left << setw(32) << titluFilm << "│\n";
    cout << "│   Data: " << left << setw(32) << dataOra << "│\n";
    cout << "│   Loc: " << left << setw(33) << loc << "│\n";
    cout << "│   Pret: " << left << setw(32) << pret << "│\n";
    cout << "│   Cod: " << left << setw(33) << codBilet << "│\n";
    cout << "└─────────────────────────────────────────┘\n";
}
