#ifndef SUVENIR_H
#define SUVENIR_H

#include <string>
using namespace std;

class Suvenir {
private:
    string nume;
    double pret;

public:

    Suvenir(string nume, double pret);


    void afiseaza() const;

    string getNume() const;
    double getPret() const;
};

#endif
