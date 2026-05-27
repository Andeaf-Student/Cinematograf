#ifndef PROIECTIE_H
#define PROIECTIE_H

#include <string>
#include "Sala.h"

struct Proiectie {
    int idProiectie;    // unic, auto-incrementat
    int filmIndex;      // index în vectorul filme[]
    int idSala;
    std::string oraRulare;   // "HH:MM"
    Sala sala;          // matricea de locuri

    Proiectie(int id, int fIndex, int salaId, int randuri, int coloane, std::string ora)
        : idProiectie(id), filmIndex(fIndex), idSala(salaId), oraRulare(ora), sala(salaId, randuri, coloane) {}
};

#endif
