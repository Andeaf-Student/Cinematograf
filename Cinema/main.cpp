#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
  #elif _WIN32_WINNT < 0x0A00
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
  #endif
#endif

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>
#include <thread>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "httplib.h"
#include "Film.h"
#include "Suvenir.h"
#include "Bilet.h"
#include <string.h>
#include <conio.h>

using namespace std;

// ─── Shared state mutex ─────────────────────────────────────────────────────
static mutex filmeMutex;

// ─── JSON helpers ────────────────────────────────────────────────────────────
static string je(const string& s) {
    string o;
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else                o += c;
    }
    return o;
}

static string genCod() {
    static const char alpha[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    random_device rd; mt19937 g(rd());
    uniform_int_distribution<> d(0, 35);
    string cod = "#";
    for (int i = 0; i < 6; ++i) cod += alpha[d(g)];
    return cod;
}

static int parseJsonInt(const string& body, const string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == string::npos) return -1;
    pos = body.find(':', pos);
    if (pos == string::npos) return -1;
    auto vpos = body.find_first_of("-0123456789", pos);
    if (vpos == string::npos) return -1;
    return stoi(body.substr(vpos));
}

static string parseJsonStr(const string& body, const string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == string::npos) return "";
    auto colon = body.find(':', pos);
    if (colon == string::npos) return "";
    auto q1 = body.find('"', colon + 1);
    if (q1 == string::npos) return "";
    auto q2 = body.find('"', q1 + 1);
    if (q2 == string::npos) return "";
    return body.substr(q1 + 1, q2 - q1 - 1);
}

static string readFile(const string& path) {
    ifstream f(path);
    if (!f.is_open()) return "";
    return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}

// ─── HTTP Server ─────────────────────────────────────────────────────────────
void startHttpServer(vector<Film>& filme) {
    httplib::Server svr;

    auto setCors = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    // Serve index.html
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string html = readFile("index.html");
        if (html.empty()) {
            res.set_content("<h1>index.html not found</h1>", "text/html");
            return;
        }
        res.set_content(html, "text/html; charset=utf-8");
    });

    // GET /api/filme
    svr.Get("/api/filme", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        string json = "[";
        for (size_t i = 0; i < filme.size(); i++) {
            if (i > 0) json += ",";
            auto& f = filme[i];
            json += "{";
            json += "\"id\":"           + to_string(i)                     + ",";
            json += "\"titlu\":\""      + je(f.getTitlu())                 + "\",";
            json += "\"gen\":\""        + je(f.getGen())                   + "\",";
            json += "\"durata\":"       + to_string(f.getDurata())         + ",";
            json += "\"varstaMinima\":" + to_string(f.getVarstaMinima())   + ",";
            json += "\"idSala\":"       + to_string(f.getSala().getIndex())+ ",";
            json += "\"randuri\":"      + to_string(f.getSala().getNumarRanduri()) + ",";
            json += "\"coloane\":"      + to_string(f.getSala().getNumarColoane());
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/sala/:id
    svr.Get("/api/sala/:id", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        setCors(res);
        int id = stoi(req.path_params.at("id"));
        if (id < 0 || id >= (int)filme.size()) {
            res.status = 404;
            res.set_content("{\"error\":\"Film negasit\"}", "application/json");
            return;
        }
        auto& sala   = filme[id].getSala();
        auto& locuri = sala.getLocuri();
        string json  = "{";
        json += "\"randuri\":"  + to_string(sala.getNumarRanduri()) + ",";
        json += "\"coloane\":"  + to_string(sala.getNumarColoane()) + ",";
        json += "\"locuri\":[";
        for (size_t r = 0; r < locuri.size(); r++) {
            if (r > 0) json += ",";
            json += "[";
            for (size_t c = 0; c < locuri[r].size(); c++) {
                if (c > 0) json += ",";
                json += locuri[r][c] ? "true" : "false";
            }
            json += "]";
        }
        json += "]}";
        res.set_content(json, "application/json");
    });

    // POST /api/rezerva
    svr.Post("/api/rezerva", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        setCors(res);
        int filmIdx = parseJsonInt(req.body, "filmIndex");
        int rand_   = parseJsonInt(req.body, "rand");
        int col     = parseJsonInt(req.body, "col");
        string nume = parseJsonStr(req.body, "nume");

        if (filmIdx < 0 || filmIdx >= (int)filme.size() || rand_ < 0 || col < 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        try {
            filme[filmIdx].getSala().rezervaLoc(rand_, col);
        } catch (exception& e) {
            res.status = 409;
            res.set_content("{\"error\":\"" + je(e.what()) + "\"}", "application/json");
            return;
        }

        int nrRanduri = filme[filmIdx].getSala().getNumarRanduri();
        int pret      = (rand_ <= 1) ? 30 : (rand_ <= nrRanduri - 2) ? 35 : 40;
        string cod    = genCod();

        time_t t = time(nullptr);
        tm*  now = localtime(&t);
        char dataBuf[32];
        strftime(dataBuf, sizeof(dataBuf), "%d.%m.%Y %H:%M", now);

        string json = "{";
        json += "\"cod\":\""    + cod                                                              + "\",";
        json += "\"titlu\":\""  + je(filme[filmIdx].getTitlu())                                    + "\",";
        json += "\"data\":\""   + string(dataBuf)                                                  + "\",";
        json += "\"loc\":\"Randul " + string(1,(char)('A'+rand_)) + ", Scaunul " + to_string(col+1)+ "\",";
        json += "\"pret\":"     + to_string(pret)                                                  + ",";
        json += "\"nume\":\""   + je(nume)                                                         + "\"";
        json += "}";
        res.set_content(json, "application/json");
    });

    // CORS preflight
    svr.Options(".*", [&](const httplib::Request&, httplib::Response& res) {
        setCors(res);
        res.status = 204;
    });

    cout << "\n\033[32m[WEB]\033[0m Server pornit pe \033[33mhttp://localhost:8080\033[0m\n\n";
    svr.listen("0.0.0.0", 8080);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    vector<Film>    filme;
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
            getline(ss, temp, ','); durata       = stoi(temp);
            getline(ss, temp, ','); varstaMinima = stoi(temp);
            getline(ss, temp, ','); idSala       = stoi(temp);
            getline(ss, temp, ','); randuri      = stoi(temp);
            getline(ss, temp, ','); locuri       = stoi(temp);

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
                } catch (...) {}
            }
        }
    }

    ifstream fileSuveniruri("Suveniruri.txt");
    if (fileSuveniruri.is_open()) {
        string line;
        while (getline(fileSuveniruri, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string nume, temp; int pret;
            getline(ss, nume, ',');
            getline(ss, temp, ','); pret = stoi(temp);
            suveniruri.push_back(Suvenir(nume, pret));
        }
        fileSuveniruri.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Suveniruri.txt!\n";
    }

    // ── Start HTTP server in background thread ──────────────────────────────
    thread serverThread([&filme]() {
        startHttpServer(filme);
    });
    serverThread.detach();

    int optiune = 0;

    // ── Splash screen ────────────────────────────────────────────────────────
    auto typewrite = [](const string& text, int delayMs = 30) -> bool {
        for (char c : text) {
            if (_kbhit()) { _getch(); return true; }
            cout << c; cout.flush();
            Sleep(delayMs);
        }
        return false;
    };

    auto sleepOrSkip = [](int ms) -> bool {
        const int step = 50;
        for (int elapsed = 0; elapsed < ms; elapsed += step) {
            if (_kbhit()) { _getch(); return true; }
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
                if (i + 1 < filme.size())
                    linii2 = filme[i + 1].getLiniiAfisare(i + 2);

                for (size_t j = 0; j < linii1.size(); j++) {
                    cout << linii1[j];
                    if (!linii2.empty()) cout << "    " << linii2[j];
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

            if (indexFilm < 0 || indexFilm >= (int)filme.size()) {
                cout << "Index film invalid!\n";
                break;
            }

            {
                lock_guard<mutex> lock(filmeMutex);
                filme[indexFilm].getSala().afiseazaLocuri();
            }

            cout << "Cate locuri vrei sa rezervi: ";
            int nr_locuri = 1;
            cin >> nr_locuri;

            vector<Bilet> bileteCumparate;

            for (int i = 1; i <= nr_locuri; i++)
            {
                int rand_, col;
                {
                    lock_guard<mutex> lock(filmeMutex);
                    filme[indexFilm].getSala().selecteazaLocInteractiv(rand_, col);
                }
                cout << "\nRezervare " << i << ": Rand " << rand_ + 1 << ", Loc " << col + 1 << endl;

                try
                {
                    int nrRanduri;
                    {
                        lock_guard<mutex> lock(filmeMutex);
                        nrRanduri = filme[indexFilm].getSala().getNumarRanduri();
                    }

                    int pretCalculat = 0;
                    if      (rand_ >= 0 && rand_ <= 1)               { pretCalculat = 30; cout << "Pret: 30 lei\n"; }
                    else if (rand_ > 1 && rand_ <= nrRanduri - 2)    { pretCalculat = 35; cout << "Pret: 35 lei\n"; }
                    else                                               { pretCalculat = 40; cout << "Pret: 40 lei\n"; }

                    string conf;
                    cout << "Confirmare (DA/NU): ";
                    cin >> conf;
                    for (char& c : conf) c = toupper(c);

                    if (conf == "DA")
                    {
                        cout << "Rezervare efectuata!\n";
                        {
                            lock_guard<mutex> lock(filmeMutex);
                            filme[indexFilm].getSala().rezervaLoc(rand_, col);
                        }

                        time_t t  = time(nullptr);
                        tm*   now = localtime(&t);

                        const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                        string codBilet = "#";
                        random_device rd; mt19937 gen(rd());
                        uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);
                        for (int k = 0; k < 5; ++k) codBilet += alphanum[distrib(gen)];

                        stringstream locSs, pretSs, dataOraSs;
                        locSs    << "Randul " << (char)('A' + rand_) << ", Scaunul " << col + 1;
                        pretSs   << pretCalculat << " RON";
                        dataOraSs << put_time(now, "%d.%m.%Y  %H:%M");

                        bileteCumparate.emplace_back(filme[indexFilm].getTitlu(), dataOraSs.str(), locSs.str(), pretSs.str(), codBilet);
                    }
                    else
                    {
                        cout << "Rezervare anulata!\n";
                        break;
                    }
                }
                catch (exception& e)
                {
                    cout << "Eroare: " << e.what() << endl;
                }
            }

            if (!bileteCumparate.empty()) {
                cout << "\n--- BILETELE TALE ---\n";
                for (const auto& bilet : bileteCumparate) bilet.afiseaza();
                cout << "\n---------------------\n";
            }

            {
                lock_guard<mutex> lock(filmeMutex);
                filme[indexFilm].getSala().afiseazaLocuri();
            }
            break;
        }

        case 3:
        {
            cout << "\n--- Suveniruri disponibile ---\n";
            for (int i = 0; i < (int)suveniruri.size(); i++) {
                cout << i << ". "; suveniruri[i].afiseaza();
            }
            int alegere;
            cout << "Alege suvenirul: "; cin >> alegere;
            if (alegere < 0 || alegere >= (int)suveniruri.size()) {
                cout << "Alegere invalida!\n"; break;
            }
            string conf;
            cout << "Confirmare cumparare (DA/NU): "; cin >> conf;
            for (char& c : conf) c = toupper(c);
            if (conf == "DA")
                cout << "Ai cumparat: " << suveniruri[alegere].getNume()
                     << " pentru " << suveniruri[alegere].getPret() << " lei\n";
            else
                cout << "Comanda anulata!\n";
            break;
        }

        case 4:
            cout << "La revedere!\n";
            break;

        default:
            cout << "Optiune invalida!\n";
        }
    }
    while (optiune != 4);

    return 0;
}
