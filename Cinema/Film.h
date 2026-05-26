#ifndef FILM_H
#define FILM_H

#include <string>
#include <vector>
#include "Sala.h"

using namespace std;

class Film {
private:
    string titlu;
    string gen;
    int durata;
    int varstaMinima;
    Sala sala;
    string oraRulare;

public:
    Film(string titlu, string gen, int durata, int varstaMinima, Sala sala, string oraRulare);
    vector<string> getLiniiAfisare(int index) const;

    Sala& getSala();
    int getVarstaMinima() const;
    string getTitlu() const;
    string getGen() const;
    int getDurata() const;
    string getOraRulare() const;

};

#endif
