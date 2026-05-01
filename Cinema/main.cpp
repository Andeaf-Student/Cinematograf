#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "Film.h"
#include "Suvenir.h"
#include <string.h>

using namespace std;

int main()
{
    vector<Film> filme;
    vector<Suvenir> suveniruri;

    ifstream fileFilme("Filme.txt");
    if (fileFilme.is_open()) {
        string line;
        while (getline(fileFilme, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string titlu, gen, temp;
            int durata, idSala, randuri, locuri;
            
            getline(ss, titlu, ',');
            getline(ss, gen, ',');
            
            getline(ss, temp, ',');
            durata = stoi(temp);
            
            getline(ss, temp, ',');
            idSala = stoi(temp);
            
            getline(ss, temp, ',');
            randuri = stoi(temp);
            
            getline(ss, temp, ',');
            locuri = stoi(temp);
            
            Sala s(idSala, randuri, locuri);
            filme.push_back(Film(titlu, gen, durata, s));
        }
        fileFilme.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Filme.txt!\n";
    }

    ifstream fileSuveniruri("Suveniruri.txt");
    if (fileSuveniruri.is_open()) {
        string line;
        while (getline(fileSuveniruri, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string nume, temp;
            int pret;
            
            getline(ss, nume, ',');
            getline(ss, temp, ',');
            pret = stoi(temp);
            
            suveniruri.push_back(Suvenir(nume, pret));
        }
        fileSuveniruri.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Suveniruri.txt!\n";
    }

    int optiune = 0;
    
    cout << "\n======================================================\n";
    cout << "   _____ _____ _   _ ______ __  __          \n";
    cout << "  / ____|_   _| \\ | |  ____|  \\/  |   /\\    \n";
    cout << " | |      | | |  \\| | |__  | \\  / |  /  \\   \n";
    cout << " | |      | | | . ` |  __| | |\\/| | / /\\ \\  \n";
    cout << " | |____ _| |_| |\\  | |____| |  | |/ ____ \\ \n";
    cout << "  \\_____|_____|_| \\_|______|_|  |_/_/    \\_\\\n";
    cout << "======================================================\n";
    cout << "                BUN VENIT!                            \n";
    cout << "======================================================\n\n";

    do
    {
        cout << "------------------------------------------------------\n";
        cout << "                  MENIU PRINCIPAL                     \n";
        cout << "------------------------------------------------------\n";
        cout << "  [1] Afiseaza lista de filme\n";
        cout << "  [2] Rezerva un loc la film\n";
        cout << "  [3] Cumpara suvenir\n";
        cout << "  [4] Iesire\n";
        cout << "------------------------------------------------------\n";
        cout << ">> Alege optiunea: ";
        cin >> optiune;

        switch(optiune)
        {
        case 1:
            cout << "\nLista filme:\n";
            for (int i = 0; i < filme.size(); i++)
            {
                cout << i+1 << ". ";
                filme[i].afiseaza();
                cout << endl;
            }
            break;

        case 2:
        {
            int indexFilm;
            cout << "Introdu indexul filmului: ";
            cin >> indexFilm;
            indexFilm--;

            if(indexFilm < 0 || indexFilm >= filme.size())
            {
                cout << "Index film invalid!\n";
                break;
            }

            filme[indexFilm].getSala().afiseazaLocuri();
            cout<<"Cate locuri vrei sa rezervi: ";
            int nr_locuri=1;
            cin>>nr_locuri;

            for (int i=1; i<=nr_locuri; i++)
            {
                cout<<"Rezervare "<<i<<": "<<endl;
                int rand, col;
                cout << "Introdu randul: ";
                cin >> rand;
                cout << "Introdu coloana: ";
                cin >> col;
                rand--;
                col--;

                try
                {
                    int nrRanduri = filme[indexFilm].getSala().getNumarRanduri(); // creează getter

                    if(rand >= 0 && rand <= 1)
                        cout << "Pret: 30 lei" << endl;
                    else if(rand > 1 && rand <= nrRanduri-2)
                        cout << "Pret: 35 lei" << endl;
                    else
                        cout << "Pret: 40 lei" << endl;


                    string conf;
                    cout << "Confirmare (DA/NU): ";
                    cin >> conf;


                    for(char &c : conf) c = toupper(c);

                    if(conf == "DA")
                    {
                        cout << "Rezervare efectuata!\n";
                        filme[indexFilm].getSala().rezervaLoc(rand, col);
                    }

                    else
                    {
                        cout << "Rezervare anulata!" << endl;
                        break;

                    }

                }
                catch (exception &e)
                {
                    cout << "Eroare: " << e.what() << endl;
                }
            }

            filme[indexFilm].getSala().afiseazaLocuri();


            break;
        }

        case 3:
        {
            cout << "\n--- Suveniruri disponibile ---\n";

            for (int i = 0; i < suveniruri.size(); i++)
            {
                cout << i << ". ";
                suveniruri[i].afiseaza();
            }

            int alegere;
            cout << "Alege suvenirul: ";
            cin >> alegere;

            if (alegere < 0 || alegere >= suveniruri.size())
            {
                cout << "Alegere invalida!\n";
                break;
            }

            string conf;
            cout << "Confirmare cumparare (DA/NU): ";
            cin >> conf;


            for (char &c : conf) c = toupper(c);

            if (conf == "DA")
            {
                cout << "Ai cumparat: "
                     << suveniruri[alegere].getNume()
                     << " pentru "
                     << suveniruri[alegere].getPret()
                     << " lei\n";
            }
            else
            {
                cout << "Comanda anulata!\n";
            }

            break;
        }

        case 4:
            cout << "La revedere!\n";
            break;

        default:
            cout << "Optiune invalida!\n";
        }

    }
    while(optiune != 4);

    return 0;
}
