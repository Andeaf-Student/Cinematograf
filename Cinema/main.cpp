#include <iostream>
#include <vector>
#include "Film.h"
#include "Suvenir.h"
#include <string.h>

using namespace std;

int main()
{
    vector<Film> filme;


    Sala s1(1, 5, 5);
    Sala s2(2, 4, 4);
    Sala s3(3, 6, 6);
    Sala s4(4, 3, 5);
    Sala s5(5, 5, 4);
    Sala s6(6, 4, 6);

    vector<Suvenir> suveniruri;

    suveniruri.push_back(Suvenir("Ochelari Spiderman", 25));
    suveniruri.push_back(Suvenir("Cana Hulk", 40));
    suveniruri.push_back(Suvenir("Tricou Batman", 60));
    suveniruri.push_back(Suvenir("Figurina Iron Man", 80));
    suveniruri.push_back(Suvenir("Poster Avengers", 20));

    filme.push_back(Film("Inception", "SF", 148, s1));
    filme.push_back(Film("Titanic", "Drama", 180, s2));
    filme.push_back(Film("Avatar", "Actiune", 162, s3));
    filme.push_back(Film("The Godfather", "Crima", 175, s4));
    filme.push_back(Film("Joker", "Drama", 122, s5));
    filme.push_back(Film("Interstellar", "SF", 169, s6));

    int optiune = 0;
    cout<<"Bun venit la cinematograf!"<<endl;
    do
    {

        cout << "1. Afiseaza lista de filme\n";
        cout << "2. Rezerva un loc la film\n";
        cout << "3. Cumpara suvenir\n";
        cout << "4. Iesire\n";
        cout << "Alege optiunea: ";
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
