#ifndef FILM_H
#define FILM_H

#include <string>
#include <vector>

using namespace std;

class Film {
private:
    string titlu;
    string gen;
    int durata;
    int varstaMinima;

public:
    Film(string titlu, string gen, int durata, int varstaMinima);
    vector<string> getLiniiAfisare(int index) const;

    int getVarstaMinima() const;
    string getTitlu() const;
    string getGen() const;
    int getDurata() const;

};

#endif
