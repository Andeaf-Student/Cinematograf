#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Film.h"
#include "Suvenir.h"
#include <string.h>
#include <conio.h>

using namespace std;

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    // Activare suport coduri ANSI in consola Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    vector<Film> filme;
    vector<Suvenir> suveniruri;

    ifstream fileFilme("Filme.txt");
    if (fileFilme.is_open()) {
        string line;
        while (getline(fileFilme, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string titlu, gen, temp;
            int durata, varstaMinima, idSala, randuri, locuri;
            
            getline(ss, titlu, ',');
            getline(ss, gen, ',');
            
            getline(ss, temp, ',');
            durata = stoi(temp);

            getline(ss, temp, ',');
            varstaMinima = stoi(temp);
            
            getline(ss, temp, ',');
            idSala = stoi(temp);
            
            getline(ss, temp, ',');
            randuri = stoi(temp);
            
            getline(ss, temp, ',');
            locuri = stoi(temp);
            
            Sala s(idSala, randuri, locuri);
            filme.push_back(Film(titlu, gen, durata, varstaMinima, s));
        }
        fileFilme.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Filme.txt!\n";
    }

    random_device rd_init;
    mt19937 gen_init(rd_init());

    for (auto& film : filme) {
        int r = film.getSala().getNumarRanduri();
        int c = film.getSala().getNumarColoane();
        
        if (r > 0 && c > 0) {
            uniform_int_distribution<> distR(0, r - 1);
            uniform_int_distribution<> distC(0, c - 1);
            
            int ocupate = 0;
            while (ocupate < 3 && ocupate < r * c) {
                int randR = distR(gen_init);
                int randC = distC(gen_init);
                try {
                    film.getSala().rezervaLoc(randR, randC);
                    ocupate++;
                } catch (...) {
                    // ignoram daca e deja ocupat
                }
            }
        }
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

    // --- Splash Screen cu efect typewriter ---
    // Returneaza true daca utilizatorul a apasat o tasta (skip)
    auto typewrite = [](const string& text, int delayMs = 30) -> bool {
        for (char c : text) {
            if (_kbhit()) {
                _getch(); // consuma tasta
                return true; // skip!
            }
            cout << c;
            cout.flush();
            Sleep(delayMs);
        }
        return false;
    };

    auto sleepOrSkip = [](int ms) -> bool {
        const int step = 50;
        for (int elapsed = 0; elapsed < ms; elapsed += step) {
            if (_kbhit()) {
                _getch();
                return true;
            }
            Sleep(min(step, ms - elapsed));
        }
        return false;
    };

    bool skipped = false;

    system("cls");

    if (!skipped) { cout << "\n\n"; cout << "  ======================================================\n"; }
    if (!skipped) skipped = typewrite("   _____ _____ _   _ ______ __  __\n", 15);
    if (!skipped) skipped = typewrite("  / ____|_   _| \\ | |  ____|  \\/  |   /\\\\\n", 15);
    if (!skipped) skipped = typewrite(" | |      | | |  \\| | |__  | \\  / |  /  \\\\\n", 15);
    if (!skipped) skipped = typewrite(" | |      | | | . ` |  __| | |\\/| | / /\\ \\\\\n", 15);
    if (!skipped) skipped = typewrite(" | |____ _| |_| |\\  | |____| |  | |/ ____ \\\\\n", 15);
    if (!skipped) skipped = typewrite("  \\_____|_____|_| \\_|______|_|  |_/_/    \\_\\\\\n", 15);
    if (!skipped) { cout << "  ======================================================\n"; }
    if (!skipped) skipped = sleepOrSkip(300);
    if (!skipped) skipped = typewrite("\n         * * *  CINEMA AURORA  * * *\n", 50);
    if (!skipped) skipped = sleepOrSkip(200);
    if (!skipped) skipped = typewrite("        Bun venit! Alege filmul perfect.\n", 40);
    if (!skipped) skipped = sleepOrSkip(200);
    if (!skipped) { cout << "\n  ======================================================\n\n"; }
    if (!skipped) sleepOrSkip(800);

    system("cls");


    // --- Functie helper pentru afisarea logo-ului permanent ---
    auto afiseazaLogo = []() {
        cout << "\n";
        cout << "  ======================================================\n";
        cout << "   _____ _____ _   _ ______ __  __\n";
        cout << "  / ____|_   _| \\ | |  ____|  \\/  |   /\\\n";
        cout << " | |      | | |  \\| | |__  | \\  / |  /  \\\n";
        cout << " | |      | | | . ` |  __| | |\\/| | / /\\ \\\n";
        cout << " | |____ _| |_| |\\  | |____| |  | |/ ____ \\\n";
        cout << "  \\_____|_____|_| \\_|______|_|  |_/_/    \\_\\\n";
        cout << "  ======================================================\n";
        cout << "         * * *  CINEMA AURORA  * * *\n";
        cout << "  ======================================================\n\n";
    };

    afiseazaLogo();

    do
    {
        cout << "╔════════════════════════════════════════════════════╗\n";
        cout << "║                  MENIU PRINCIPAL                   ║\n";
        cout << "╠════════════════════════════════════════════════════╣\n";
        cout << "║  [1] Afiseaza lista de filme                       ║\n";
        cout << "║  [2] Rezerva un loc la film                        ║\n";
        cout << "║  [3] Cumpara suvenir                               ║\n";
        cout << "║  [4] Iesire                                        ║\n";
        cout << "╚════════════════════════════════════════════════════╝\n";
        cout << ">> Alege optiunea: ";
        cin >> optiune;

        switch(optiune)
        {
        case 1:
            cout << "\nLista filme:\n\n";
            for (size_t i = 0; i < filme.size(); i += 2)
            {
                vector<string> linii1 = filme[i].getLiniiAfisare(i + 1);
                vector<string> linii2;
                
                if (i + 1 < filme.size()) {
                    linii2 = filme[i + 1].getLiniiAfisare(i + 2);
                }
                
                for (size_t j = 0; j < linii1.size(); j++) {
                    cout << linii1[j];
                    if (!linii2.empty()) {
                        cout << "    " << linii2[j];
                    }
                    cout << "\n";
                }
                cout << "\n";
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
                int rand, col;
                filme[indexFilm].getSala().selecteazaLocInteractiv(rand, col);
                cout<<"\nRezervare "<<i<<": Rand " << rand + 1 << ", Loc " << col + 1 << endl;

                try
                {
                    int nrRanduri = filme[indexFilm].getSala().getNumarRanduri(); // creează getter

                    int pretCalculat = 0;
                    if(rand >= 0 && rand <= 1) {
                        pretCalculat = 30;
                        cout << "Pret: 30 lei" << endl;
                    } else if(rand > 1 && rand <= nrRanduri-2) {
                        pretCalculat = 35;
                        cout << "Pret: 35 lei" << endl;
                    } else {
                        pretCalculat = 40;
                        cout << "Pret: 40 lei" << endl;
                    }


                    string conf;
                    cout << "Confirmare (DA/NU): ";
                    cin >> conf;


                    for(char &c : conf) c = toupper(c);

                    if(conf == "DA")
                    {
                        cout << "Rezervare efectuata!\n";
                        filme[indexFilm].getSala().rezervaLoc(rand, col);
                        
                        time_t t = time(nullptr);
                        tm* now = localtime(&t);
                        
                        const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                        string codBilet = "#";
                        
                        random_device rd;
                        mt19937 gen(rd());
                        uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);
                        
                        for (int k = 0; k < 5; ++k) {
                            codBilet += alphanum[distrib(gen)];
                        }

                        stringstream locSs;
                        locSs << "Randul " << (char)('A' + rand) << ", Scaunul " << col + 1;
                        
                        stringstream pretSs;
                        pretSs << pretCalculat << " RON";
                        
                        cout << "\n┌─────────────────────────────────────────┐\n";
                        cout << "│   🎬 CINEMA AURORA                      │\n";
                        cout << "│   Film: " << left << setw(32) << filme[indexFilm].getTitlu() << "│\n";
                        cout << "│   Data: " << put_time(now, "%d.%m.%Y  %H:%M") << setw(16) << setfill(' ') << " " << "│\n";
                        cout << "│   Loc: " << left << setw(33) << locSs.str() << "│\n";
                        cout << "│   Pret: " << left << setw(32) << pretSs.str() << "│\n";
                        cout << "│   Cod: " << left << setw(33) << codBilet << "│\n";
                        cout << "└─────────────────────────────────────────┘\n\n";
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
