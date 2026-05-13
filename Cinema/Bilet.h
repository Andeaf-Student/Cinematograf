#ifndef BILET_H
#define BILET_H

#include <string>
#include <iostream>
#include <iomanip>

class Bilet {
private:
    std::string titluFilm;
    std::string dataOra;
    std::string loc;
    std::string pret;
    std::string codBilet;

public:
    Bilet(std::string titlu, std::string data, std::string l, std::string p, std::string cod);
    void afiseaza() const;
};

#endif
