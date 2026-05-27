#ifndef SALA_H
#define SALA_H

#include <vector>
using namespace std;

class Sala {
private:
    int index;
    vector<vector<bool>> locuri;

public:

    Sala(int index, int randuri, int coloane);


    void afiseazaLocuri() const;
    void rezervaLoc(int rand, int col);
    void elibereazaLoc(int rand, int col);
    void selecteazaLocInteractiv(int& outRand, int& outCol) const;

    int getIndex() const;
    int getNumarRanduri() const;
    int getNumarColoane() const;
    const vector<vector<bool>>& getLocuri() const;
};

#endif
