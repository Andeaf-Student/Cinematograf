#ifndef FILM_H
#define FILM_H

#include <string>
#include "Sala.h"

using namespace std;

class Film {
private:
    string titlu;
    string gen;
    int durata;
    Sala sala;

public:
    Film(string titlu, string gen, int durata, Sala sala);

    void afiseaza() const;

    Sala& getSala();

};

#endif
