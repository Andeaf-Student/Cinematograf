#include <iostream>
#include <iomanip>
#include <sstream>
#include "Film.h"

using namespace std;

Film::Film(string titlu, string gen, int durata, int varstaMinima, Sala sala)
    : sala(sala) {
    this->titlu = titlu;
    this->gen = gen;
    this->durata = durata;
    this->varstaMinima = varstaMinima;
}

vector<string> Film::getLiniiAfisare(int index) const {
    vector<string> linii;
    stringstream ss;
    
    string header = "  [" + to_string(index) + "]";
    while(header.length() < 44) {
        header += " ";
    }
    linii.push_back(header);
    linii.push_back("  ┌────────────────────────────────────────┐");
    
    ss.str(""); ss.clear();
    ss << "  │ Titlu: " << left << setw(32) << titlu << "│";
    linii.push_back(ss.str());
    
    ss.str(""); ss.clear();
    ss << "  │ Gen: " << left << setw(34) << gen << "│";
    linii.push_back(ss.str());
    
    ss.str(""); ss.clear();
    ss << "  │ Durata: " << left << setw(23) << durata << " minute │";
    linii.push_back(ss.str());
    
    ss.str(""); ss.clear();
    ss << "  │ Varsta: " << left << setw(31) << to_string(varstaMinima) + "+" << "│";
    linii.push_back(ss.str());
    
    ss.str(""); ss.clear();
    ss << "  │ Sala: " << left << setw(33) << sala.getIndex() << "│";
    linii.push_back(ss.str());
    
    linii.push_back("  └────────────────────────────────────────┘");
    
    return linii;
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

string Film::getGen() const {
    return gen;
}

int Film::getDurata() const {
    return durata;
}


