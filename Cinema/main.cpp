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

static std::pair<int, int> getSalaDimensions(int idSala) {
    if (idSala == 1) return {10, 12};
    if (idSala == 2) return {10, 13};
    if (idSala == 3) return {11, 12};
    if (idSala == 4) return {10, 14};
    if (idSala == 5) return {11, 13};
    if (idSala == 6) return {12, 12};
    if (idSala == 7) return {11, 14};
    return {10, 10}; // implicit default
}

static string readFile(const string& path) {
    ifstream f(path);
    if (!f.is_open()) return "";
    return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}

static double parseJsonDouble(const string& body, const string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == string::npos) return -1;
    pos = body.find(':', pos);
    if (pos == string::npos) return -1;
    auto vpos = body.find_first_of("-0123456789.", pos);
    if (vpos == string::npos) return -1;
    return stod(body.substr(vpos));
}

static bool checkAuth(const httplib::Request& req) {
    auto auth = req.get_header_value("Authorization");
    if (auth.empty()) return false;
    return auth == "Bearer admin123";
}

static void saveFilmeToFile(vector<Film>& filme) {
    // Read existing file to preserve sala dimensions
    vector<pair<int,int>> salaDims;
    ifstream existing("Filme.txt");
    if (existing.is_open()) {
        string line;
        while (getline(existing, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string temp;
            for (int i = 0; i < 5; i++) getline(ss, temp, ',');
            int r, c;
            getline(ss, temp, ','); r = stoi(temp);
            getline(ss, temp, ','); c = stoi(temp);
            salaDims.push_back({r, c});
        }
        existing.close();
    }
    
    ofstream f("Filme.txt");
    if (!f.is_open()) return;
    for (size_t i = 0; i < filme.size(); i++) {
        Film& film = filme[i];
        int idSala = i + 1;
        int randuri = (i < salaDims.size()) ? salaDims[i].first : 5;
        int coloane = (i < salaDims.size()) ? salaDims[i].second : 6;
        f << film.getTitlu() << ","
          << film.getGen() << ","
          << film.getDurata() << ","
          << film.getVarstaMinima() << ","
          << idSala << ","
          << randuri << ","
          << coloane << "\n";
    }
    f.close();
}

static void saveSuveniruriToFile(const vector<Suvenir>& suveniruri) {
    ofstream f("Suveniruri.txt");
    if (!f.is_open()) return;
    for (const auto& s : suveniruri) {
        f << s.getNume() << "," << (int)s.getPret() << "\n";
    }
    f.close();
}

// ─── HTTP Server ─────────────────────────────────────────────────────────────
void startHttpServer(vector<Film>& filme, vector<Suvenir>& suveniruri) {
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

            int locuriDisponibile = 0;
            auto& locuri = f.getSala().getLocuri();
            for (auto& rand : locuri) {
                for (bool ocupat : rand) {
                    if (!ocupat) locuriDisponibile++;
                }
            }

            json += "{";
            json += "\"id\":"           + to_string(i)                     + ",";
            json += "\"titlu\":\""      + je(f.getTitlu())                 + "\",";
            json += "\"gen\":\""        + je(f.getGen())                   + "\",";
            json += "\"durata\":"       + to_string(f.getDurata())         + ",";
            json += "\"varstaMinima\":" + to_string(f.getVarstaMinima())   + ",";
            json += "\"idSala\":"       + to_string(f.getSala().getIndex())+ ",";
            json += "\"randuri\":"      + to_string(f.getSala().getNumarRanduri()) + ",";
            json += "\"coloane\":"      + to_string(f.getSala().getNumarColoane()) + ",";
            json += "\"oraRulare\":\""   + je(f.getOraRulare())              + "\",";
            json += "\"locuriDisponibile\":" + to_string(locuriDisponibile);
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/filme/disponibile
    svr.Get("/api/filme/disponibile", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        time_t now = time(nullptr);
        tm* local = localtime(&now);
        int currentHour = local->tm_hour;
        int currentMin = local->tm_min;
        int currentTotalMin = currentHour * 60 + currentMin;

        string json = "[";
        bool first = true;
        for (size_t i = 0; i < filme.size(); i++) {
            auto& f = filme[i];
            string oraRulare = f.getOraRulare();
            if (oraRulare.length() < 5 || oraRulare[2] != ':') {
                oraRulare = "18:00";
            }
            int filmHour = stoi(oraRulare.substr(0, 2));
            int filmMin = stoi(oraRulare.substr(3, 2));
            int filmTotalMin = filmHour * 60 + filmMin;

            if (currentTotalMin < filmTotalMin) {
                if (!first) json += ",";
                first = false;

                // Calculate available seats
                int locuriDisponibile = 0;
                auto& locuri = f.getSala().getLocuri();
                for (auto& rand : locuri) {
                    for (bool ocupat : rand) {
                        if (!ocupat) locuriDisponibile++;
                    }
                }

                json += "{";
                json += "\"id\":"           + to_string(i)                     + ",";
                json += "\"titlu\":\""      + je(f.getTitlu())                 + "\",";
                json += "\"gen\":\""        + je(f.getGen())                   + "\",";
                json += "\"durata\":"       + to_string(f.getDurata())         + ",";
                json += "\"varstaMinima\":" + to_string(f.getVarstaMinima())   + ",";
                json += "\"idSala\":"       + to_string(f.getSala().getIndex())+ ",";
                json += "\"randuri\":"      + to_string(f.getSala().getNumarRanduri()) + ",";
                json += "\"coloane\":"      + to_string(f.getSala().getNumarColoane()) + ",";
                json += "\"oraRulare\":\""   + je(oraRulare)                    + "\",";
                json += "\"locuriDisponibile\":" + to_string(locuriDisponibile);
                json += "}";
            }
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

    // ─── Admin Endpoints ───────────────────────────────────────────────────────
    
    // GET /cos - serve cart page
    svr.Get("/cos", [](const httplib::Request&, httplib::Response& res) {
        string html = R"HTMLDELIMITER(<!DOCTYPE html>
<html lang="ro">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cinema Aurora - Coș</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-deep: #0a0a0c;
            --bg-card: #151518;
            --primary: #f5c518;
            --primary-glow: rgba(245, 197, 24, 0.3);
            --text-main: #e0e0e0;
            --text-muted: #a0a0a0;
            --accent: #ff4d4d;
            --success: #2ecc71;
            --glass: rgba(255, 255, 255, 0.03);
            --border: rgba(255, 255, 255, 0.1);
        }
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Outfit', sans-serif; }
        body { background-color: var(--bg-deep); color: var(--text-main); min-height: 100vh; padding: 20px; }
        header { display: flex; align-items: center; padding: 20px; margin-bottom: 30px; }
        .back-btn { padding: 10px 20px; background: var(--glass); color: #fff; border: 1px solid var(--border); border-radius: 12px; cursor: pointer; font-weight: 600; transition: all 0.3s; }
        .back-btn:hover { background: var(--primary); color: #000; border-color: var(--primary); }
        .page-title { font-size: 2rem; font-weight: 800; margin-left: 20px; color: var(--primary); }
        .container { max-width: 1200px; margin: 0 auto; }
        .section { background: var(--bg-card); border: 1px solid var(--border); border-radius: 20px; padding: 30px; margin-bottom: 30px; }
        .section-title { font-size: 1.5rem; font-weight: 700; margin-bottom: 20px; color: var(--primary); }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 15px; text-align: left; border-bottom: 1px solid var(--border); }
        th { color: var(--text-muted); font-weight: 600; text-transform: uppercase; font-size: 0.8rem; }
        .btn { padding: 10px 20px; border: none; border-radius: 8px; cursor: pointer; font-weight: 600; transition: all 0.2s; }
        .btn-danger { background: var(--accent); color: #fff; }
        .btn-danger:hover { opacity: 0.8; }
        .btn-primary { background: var(--primary); color: #000; padding: 15px 40px; font-size: 1.1rem; }
        .btn-primary:hover { opacity: 0.8; }
        .total-section { text-align: right; padding: 20px; }
        .total-price { font-size: 2rem; font-weight: 800; color: var(--primary); }
        .modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.8); display: flex; justify-content: center; align-items: center; z-index: 1000; }
        .modal-content { background: var(--bg-card); border: 1px solid var(--border); border-radius: 20px; padding: 40px; text-align: center; max-width: 400px; }
        .modal-content h2 { margin-bottom: 20px; }
        .modal-content p { color: var(--text-muted); margin-bottom: 20px; }
        .modal-btn { padding: 15px 40px; background: var(--primary); color: #000; border: none; border-radius: 10px; font-size: 1.1rem; font-weight: 700; cursor: pointer; }
        .hidden { display: none !important; }
        .empty-cart { text-align: center; padding: 60px 20px; color: var(--text-muted); }
    </style>
</head>
<body>
    <header>
        <button class="back-btn" onclick="window.location.href='/'">← Înapoi</button>
        <h1 class="page-title">🛒 Coșul tău</h1>
    </header>

    <div class="container">
        <div class="section" id="bilete-section">
            <h2 class="section-title">BILETE</h2>
            <table id="bilete-table">
                <thead>
                    <tr>
                        <th>Film</th>
                        <th>Loc</th>
                        <th>Preț</th>
                        <th>Șterge</th>
                    </tr>
                </thead>
                <tbody id="bilete-body"></tbody>
            </table>
        </div>

        <div class="section" id="suveniruri-section">
            <h2 class="section-title">SUVENIRURI</h2>
            <table id="suveniruri-table">
                <thead>
                    <tr>
                        <th>Nume</th>
                        <th>Cantitate</th>
                        <th>Preț</th>
                        <th>Șterge</th>
                    </tr>
                </thead>
                <tbody id="suveniruri-body"></tbody>
            </table>
        </div>

        <div class="section total-section">
            <p style="color: var(--text-muted);">TOTAL:</p>
            <div class="total-price" id="total-price">0 lei</div>
            <button class="btn btn-primary" style="margin-top: 20px;" onclick="finalizeazaComanda()">Finalizează comanda</button>
        </div>
    </div>

    <div class="modal hidden" id="success-modal">
        <div class="modal-content">
            <h2 style="color: var(--success);">✅ Tranzacție realizată cu succes!</h2>
            <p>Biletele tale au fost rezervate.</p>
            <button class="modal-btn" onclick="inchideModal()">OK</button>
        </div>
    </div>

    <div class="modal hidden" id="error-modal">
        <div class="modal-content">
            <h2 style="color: var(--accent);">❌ Eroare</h2>
            <p id="error-message"></p>
            <button class="modal-btn" onclick="inchideModal()">OK</button>
        </div>
    </div>

    <script>
        // Load cart from sessionStorage
        const cartData = sessionStorage.getItem('cosDate');
        window.cosDate = cartData ? JSON.parse(cartData) : { bilete: [], suveniruri: [] };

        function renderCart() {
            const bileteBody = document.getElementById('bilete-body');
            const suveniruriBody = document.getElementById('suveniruri-body');
            const bileteSection = document.getElementById('bilete-section');
            const suveniruriSection = document.getElementById('suveniruri-section');

            // Render tickets
            if (window.cosDate.bilete.length === 0) {
                bileteSection.classList.add('hidden');
            } else {
                bileteSection.classList.remove('hidden');
                bileteBody.innerHTML = window.cosDate.bilete.map((b, i) => `
                    <tr>
                        <td>${b.filmTitlu}</td>
                        <td>Rand ${String.fromCharCode(65 + b.rand)}, Loc ${b.col + 1}</td>
                        <td>${b.pret} RON</td>
                        <td><button class="btn btn-danger" onclick="stergeBilet(${i})">Șterge</button></td>
                    </tr>
                `).join('');
            }

            // Render souvenirs
            if (window.cosDate.suveniruri.length === 0) {
                suveniruriSection.classList.add('hidden');
            } else {
                suveniruriSection.classList.remove('hidden');
                suveniruriBody.innerHTML = window.cosDate.suveniruri.map((s, i) => `
                    <tr>
                        <td>${s.nume}</td>
                        <td>${s.cantitate}</td>
                        <td>${s.pret * s.cantitate} RON</td>
                        <td><button class="btn btn-danger" onclick="stergeSuvenir(${i})">Șterge</button></td>
                    </tr>
                `).join('');
            }

            // Calculate total
            let total = 0;
            window.cosDate.bilete.forEach(b => total += b.pret);
            window.cosDate.suveniruri.forEach(s => total += s.pret * s.cantitate);
            document.getElementById('total-price').textContent = total + ' lei';

            // Show empty cart message
            if (window.cosDate.bilete.length === 0 && window.cosDate.suveniruri.length === 0) {
                document.querySelector('.container').innerHTML = '<div class="empty-cart"><h2>Coșul tău este gol</h2><p>Adaugă bilete sau suveniruri pentru a continua.</p></div>';
            }
        }

        function stergeBilet(index) {
            window.cosDate.bilete.splice(index, 1);
            salveazaCos();
            renderCart();
        }

        function stergeSuvenir(index) {
            window.cosDate.suveniruri.splice(index, 1);
            salveazaCos();
            renderCart();
        }

        function salveazaCos() {
            sessionStorage.setItem('cosDate', JSON.stringify(window.cosDate));
        }

        async function finalizeazaComanda() {
            const response = await fetch('/api/cos/finalizeaza', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    bilete: window.cosDate.bilete.map(b => ({ filmIndex: b.filmIndex, rand: b.rand, col: b.col })),
                    suveniruri: window.cosDate.suveniruri.map(s => ({ suvenirIndex: s.suvenirIndex, cantitate: s.cantitate }))
                })
            });

            const data = await response.json();
            if (data.succes) {
                document.getElementById('success-modal').classList.remove('hidden');
            } else {
                document.getElementById('error-message').textContent = data.eroare;
                document.getElementById('error-modal').classList.remove('hidden');
            }
        }

        function inchideModal() {
            document.getElementById('success-modal').classList.add('hidden');
            document.getElementById('error-modal').classList.add('hidden');

            // If success, clear cart and redirect
            if (!document.getElementById('success-modal').classList.contains('hidden')) {
                window.cosDate = { bilete: [], suveniruri: [] };
                salveazaCos();
                window.location.href = '/';
            }
        }

        renderCart();
    </script>
</body>
</html>)HTMLDELIMITER";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // GET /admin - serve admin page
    svr.Get("/admin", [](const httplib::Request&, httplib::Response& res) {
        string html = R"HTMLDELIMITER(<!DOCTYPE html>
<html lang="ro">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cinema Aurora - Admin</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-deep: #0a0a0c;
            --bg-card: #151518;
            --primary: #f5c518;
            --primary-glow: rgba(245, 197, 24, 0.3);
            --text-main: #e0e0e0;
            --text-muted: #a0a0a0;
            --accent: #ff4d4d;
            --success: #2ecc71;
            --glass: rgba(255, 255, 255, 0.03);
            --border: rgba(255, 255, 255, 0.1);
        }
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Outfit', sans-serif; }
        body { background-color: var(--bg-deep); color: var(--text-main); min-height: 100vh; padding: 20px; }
        header { text-align: center; padding: 40px 20px; position: relative; }
        .back-btn { position: absolute; left: 20px; top: 50%; transform: translateY(-50%); padding: 10px 20px; background: var(--glass); color: #fff; border: 1px solid var(--border); border-radius: 12px; cursor: pointer; font-weight: 600; transition: all 0.3s; }
        .back-btn:hover { background: var(--primary); color: #000; border-color: var(--primary); }
        .logo-text { font-size: 2.5rem; font-weight: 800; letter-spacing: 4px; color: var(--primary); text-shadow: 0 0 20px var(--primary-glow); text-transform: uppercase; }
        .container { max-width: 1400px; margin: 0 auto; }
        .section { background: var(--bg-card); border: 1px solid var(--border); border-radius: 20px; padding: 30px; margin-bottom: 30px; }
        .section-title { font-size: 1.8rem; font-weight: 700; margin-bottom: 20px; color: var(--primary); }
        table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid var(--border); }
        th { color: var(--text-muted); font-weight: 600; text-transform: uppercase; font-size: 0.8rem; }
        tr:hover { background: var(--glass); }
        .btn { padding: 10px 20px; border: none; border-radius: 8px; cursor: pointer; font-weight: 600; transition: all 0.2s; }
        .btn-danger { background: var(--accent); color: #fff; }
        .btn-danger:hover { opacity: 0.8; }
        .btn-success { background: var(--success); color: #fff; }
        .btn-success:hover { opacity: 0.8; }
        .btn-primary { background: var(--primary); color: #000; }
        .btn-primary:hover { opacity: 0.8; }
        .form-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .form-group input { width: 100%; background: var(--bg-deep); border: 1px solid var(--border); padding: 12px; border-radius: 8px; color: #fff; font-size: 1rem; }
        .form-group label { display: block; margin-bottom: 8px; color: var(--text-muted); font-size: 0.9rem; }
        .price-input { width: 80px !important; }
        #password-modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: var(--bg-deep); display: flex; justify-content: center; align-items: center; z-index: 1000; }
        .modal-content { background: var(--bg-card); border: 1px solid var(--border); border-radius: 20px; padding: 40px; text-align: center; }
        .modal-content h2 { color: var(--primary); margin-bottom: 20px; }
        .modal-content input { width: 100%; max-width: 300px; background: var(--bg-deep); border: 1px solid var(--border); padding: 15px; border-radius: 10px; color: #fff; font-size: 1.2rem; margin-bottom: 20px; }
        .modal-content button { padding: 15px 40px; background: var(--primary); color: #000; border: none; border-radius: 10px; font-size: 1.1rem; font-weight: 700; cursor: pointer; }
        .hidden { display: none !important; }
    </style>
</head>
<body>
    <div id="password-modal">
        <div class="modal-content">
            <h2>Admin Panel</h2>
            <input type="password" id="password-input" placeholder="Introdu parola...">
            <button onclick="checkPassword()">Accesează</button>
        </div>
    </div>

    <div id="admin-content" class="hidden">
        <header>
            <button class="back-btn" onclick="window.location.href='/'">← Înapoi</button>
            <div class="logo-text">Cinema Aurora - Admin</div>
        </header>

        <div class="container">
            <div class="section">
                <h2 class="section-title">FILME</h2>
                <table id="filme-table">
                    <thead>
                        <tr>
                            <th>#</th>
                            <th>Titlu</th>
                            <th>Gen</th>
                            <th>Durată</th>
                            <th>Vârstă</th>
                            <th>Oră</th>
                            <th>Sala</th>
                            <th>Locuri</th>
                            <th>Acțiuni</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
                
                <h3 style="margin-top: 30px; margin-bottom: 15px; color: var(--text-muted);">Adaugă Film Nou</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label>Titlu</label>
                        <input type="text" id="film-titlu">
                    </div>
                    <div class="form-group">
                        <label>Gen</label>
                        <input type="text" id="film-gen">
                    </div>
                    <div class="form-group">
                        <label>Durată (min)</label>
                        <input type="number" id="film-durata" onchange="checkSaliAvailability()" onblur="checkSaliAvailability()">
                    </div>
                    <div class="form-group">
                        <label>Vârstă Minimă</label>
                        <input type="number" id="film-varsta">
                    </div>
                    <div class="form-group">
                        <label>Ora Rulării</label>
                        <input type="time" id="film-ora" value="18:00" onchange="checkSaliAvailability()" onblur="checkSaliAvailability()">
                    </div>
                </div>
                
                <div id="sali-container" style="margin-top: 20px; margin-bottom: 20px; display: none; background: rgba(255, 255, 255, 0.05); padding: 15px; border-radius: 8px;"></div>
                
                <button class="btn btn-primary" id="btn-adauga-film" onclick="adaugaFilm()" disabled>Adaugă Film</button>
            </div>

            <div class="section">
                <h2 class="section-title">SUVENIRURI</h2>
                <table id="suveniruri-table">
                    <thead>
                        <tr>
                            <th>#</th>
                            <th>Nume</th>
                            <th>Preț (RON)</th>
                            <th>Acțiuni</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
                
                <h3 style="margin-top: 30px; margin-bottom: 15px; color: var(--text-muted);">Adaugă Suvenir Nou</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label>Nume</label>
                        <input type="text" id="suvenir-nume">
                    </div>
                    <div class="form-group">
                        <label>Preț (RON)</label>
                        <input type="number" id="suvenir-pret">
                    </div>
                </div>
                <button class="btn btn-primary" onclick="adaugaSuvenir()">Adaugă Suvenir</button>
            </div>
        </div>
    </div>

    <script>
        const API_BASE = '';
        const AUTH_TOKEN = 'admin123';

        function checkPassword() {
            const password = document.getElementById('password-input').value;
            if (password === 'admin123') {
                document.getElementById('password-modal').classList.add('hidden');
                document.getElementById('admin-content').classList.remove('hidden');
                loadFilme();
                loadSuveniruri();
            } else {
                alert('Parolă incorectă!');
            }
        }

        async function apiCall(endpoint, method = 'GET', body = null) {
            const headers = {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${AUTH_TOKEN}`
            };
            const options = { method, headers };
            if (body) options.body = JSON.stringify(body);
            
            const response = await fetch(`${API_BASE}${endpoint}`, options);
            if (response.status === 401) {
                alert('Neautorizat!');
                return null;
            }
            return response;
        }

        async function loadFilme() {
            const response = await apiCall('/api/filme');
            if (!response) return;
            const filme = await response.json();
            
            const tbody = document.querySelector('#filme-table tbody');
            tbody.innerHTML = filme.map((f, i) => `
                <tr>
                    <td>${i}</td>
                    <td>${f.titlu}</td>
                    <td>${f.gen}</td>
                    <td>${f.durata} min</td>
                    <td>${f.varstaMinima}+</td>
                    <td>${f.oraRulare || '-'}</td>
                    <td>${f.idSala}</td>
                    <td>${f.randuri}×${f.coloane}</td>
                    <td>
                        <button class="btn btn-danger" onclick="stergeFilm(${i})">Șterge</button>
                        <button class="btn btn-success" onclick="resetSala(${i})">Reset Sală</button>
                    </td>
                </tr>
            `).join('');
        }

        async function loadSuveniruri() {
            const response = await apiCall('/api/admin/suveniruri');
            if (!response) return;
            const suveniruri = await response.json();
            
            const tbody = document.querySelector('#suveniruri-table tbody');
            tbody.innerHTML = suveniruri.map((s, i) => `
                <tr>
                    <td>${i}</td>
                    <td>${s.nume}</td>
                    <td>
                        <input type="number" class="price-input" id="suvenir-pret-${i}" value="${s.pret}">
                    </td>
                    <td>
                        <button class="btn btn-success" onclick="modificaSuvenir(${i})">Salvează</button>
                        <button class="btn btn-danger" onclick="stergeSuvenir(${i})">Șterge</button>
                    </td>
                </tr>
            `).join('');
        }

        let selectedSalaId = null;

        async function checkSaliAvailability() {
            const durata = parseInt(document.getElementById('film-durata').value);
            const ora = document.getElementById('film-ora').value;
            const container = document.getElementById('sali-container');
            const btnAdauga = document.getElementById('btn-adauga-film');

            if (!durata || !ora || durata <= 0 || ora.length < 5) {
                container.style.display = 'none';
                container.innerHTML = '';
                btnAdauga.disabled = true;
                selectedSalaId = null;
                return;
            }

            const response = await apiCall(`/api/admin/sali-disponibile?ora=${ora}&durata=${durata}`);
            if (!response || !response.ok) {
                container.style.display = 'block';
                container.innerHTML = '<div style="color: #ff4d4d; font-weight: bold;">Eroare la verificarea sălilor disponibile!</div>';
                btnAdauga.disabled = true;
                selectedSalaId = null;
                return;
            }

            const sali = await response.json();
            container.style.display = 'block';

            if (sali.length === 0) {
                container.innerHTML = `
                    <div style="color: #ff4d4d; font-weight: bold; line-height: 1.5;">
                        ⛔ Nu există săli disponibile pentru ora ${ora}.<br>
                        Toate sălile sunt ocupate în acest interval.<br>
                        Încearcă o altă oră sau modifică durata filmului.
                    </div>
                `;
                btnAdauga.disabled = true;
                selectedSalaId = null;
                return;
            }

            let selectHtml = `
                <label style="display: block; margin-bottom: 10px; font-weight: bold; color: var(--primary);">Selectează Sala Disponibilă:</label>
                <select id="select-sala" class="form-control" style="width: 100%; padding: 10px; border-radius: 6px; background: #222; color: #fff; border: 1px solid var(--border);" onchange="onSalaSelected(this.value)">
                    <option value="">-- Alege o sală --</option>
            `;

            sali.forEach(s => {
                let programText = "Sală liberă toată ziua";
                if (s.filmeAzi && s.filmeAzi.length > 0) {
                    programText = "Program azi: " + s.filmeAzi.map(f => `${f.titlu} (${f.ora})`).join(' | ');
                }
                selectHtml += `
                    <option value="${s.idSala}">
                        Sala ${s.idSala} — ${s.locuriTotale} locuri (${programText})
                    </option>
                `;
            });

            selectHtml += `</select>`;
            container.innerHTML = selectHtml;

            btnAdauga.disabled = true;
            selectedSalaId = null;
        }

        function onSalaSelected(val) {
            const btnAdauga = document.getElementById('btn-adauga-film');
            if (val) {
                selectedSalaId = parseInt(val);
                btnAdauga.disabled = false;
            } else {
                selectedSalaId = null;
                btnAdauga.disabled = true;
            }
        }

        async function adaugaFilm() {
            const titlu = document.getElementById('film-titlu').value;
            const gen = document.getElementById('film-gen').value;
            const durata = parseInt(document.getElementById('film-durata').value);
            const varstaMinima = parseInt(document.getElementById('film-varsta').value);
            const oraRulare = document.getElementById('film-ora').value;
            const idSala = selectedSalaId;

            if (!titlu || !gen || !durata || !varstaMinima || !oraRulare || !idSala) {
                alert('Completați toate câmpurile și selectați o sală!');
                return;
            }

            const response = await apiCall('/api/admin/adauga-film', 'POST', {
                titlu, gen, durata, varstaMinima, oraRulare, idSala
            });
            
            if (response && response.ok) {
                alert('Film adăugat cu succes!');
                document.getElementById('film-titlu').value = '';
                document.getElementById('film-gen').value = '';
                document.getElementById('film-durata').value = '';
                document.getElementById('film-varsta').value = '';
                document.getElementById('film-ora').value = '18:00';
                
                const container = document.getElementById('sali-container');
                container.style.display = 'none';
                container.innerHTML = '';
                document.getElementById('btn-adauga-film').disabled = true;
                selectedSalaId = null;

                loadFilme();
            } else {
                alert('Eroare la adăugarea filmului!');
            }
        }

        async function stergeFilm(index) {
            if (!confirm(`Sigur doriți să ștergeți filmul ${index}?`)) return;
            
            const response = await apiCall('/api/admin/sterge-film', 'POST', { filmIndex: index });
            if (response && response.ok) {
                alert('Film șters cu succes!');
                loadFilme();
            } else {
                alert('Eroare la ștergerea filmului!');
            }
        }

        async function resetSala(index) {
            if (!confirm(`Sigur doriți să resetați sala pentru filmul ${index}?`)) return;
            
            const response = await apiCall('/api/admin/reset-sala', 'POST', { filmIndex: index });
            if (response && response.ok) {
                alert('Sala resetată cu succes!');
            } else {
                alert('Eroare la resetarea sălii!');
            }
        }

        async function adaugaSuvenir() {
            const nume = document.getElementById('suvenir-nume').value;
            const pret = parseFloat(document.getElementById('suvenir-pret').value);

            if (!nume || !pret) {
                alert('Completați toate câmpurile!');
                return;
            }

            const response = await apiCall('/api/admin/adauga-suvenir', 'POST', { nume, pret });
            if (response && response.ok) {
                alert('Suvenir adăugat cu succes!');
                document.getElementById('suvenir-nume').value = '';
                document.getElementById('suvenir-pret').value = '';
                loadSuveniruri();
            } else {
                alert('Eroare la adăugarea suvenirului!');
            }
        }

        async function stergeSuvenir(index) {
            if (!confirm(`Sigur doriți să ștergeți suvenirul ${index}?`)) return;
            
            const response = await apiCall('/api/admin/sterge-suvenir', 'POST', { suvenirIndex: index });
            if (response && response.ok) {
                alert('Suvenir șters cu succes!');
                loadSuveniruri();
            } else {
                alert('Eroare la ștergerea suvenirului!');
            }
        }

        async function modificaSuvenir(index) {
            const pretNou = parseFloat(document.getElementById(`suvenir-pret-${index}`).value);
            
            const response = await apiCall('/api/admin/modifica-suvenir', 'POST', { suvenirIndex: index, pretNou });
            if (response && response.ok) {
                alert('Preț modificat cu succes!');
                loadSuveniruri();
            } else {
                alert('Eroare la modificarea prețului!');
            }
        }
    </script>
</body>
</html>)HTMLDELIMITER";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // GET /api/admin/suveniruri
    svr.Get("/api/admin/suveniruri", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        string json = "[";
        for (size_t i = 0; i < suveniruri.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"index\":" + to_string(i) + ",";
            json += "\"nume\":\"" + je(suveniruri[i].getNume()) + "\",";
            json += "\"pret\":" + to_string((int)suveniruri[i].getPret());
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/suveniruri (public)
    svr.Get("/api/suveniruri", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        string json = "[";
        for (size_t i = 0; i < suveniruri.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"index\":" + to_string(i) + ",";
            json += "\"nume\":\"" + je(suveniruri[i].getNume()) + "\",";
            json += "\"pret\":" + to_string((int)suveniruri[i].getPret());
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/admin/sali-disponibile
    svr.Get("/api/admin/sali-disponibile", [&](const httplib::Request& req, httplib::Response& res) {
        setCors(res);
        if (!req.has_param("ora") || !req.has_param("durata")) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri lipsa\"}", "application/json");
            return;
        }
        string oraStr = req.get_param_value("ora");
        int durata = stoi(req.get_param_value("durata"));

        if (oraStr.length() < 5 || oraStr[2] != ':') {
            res.status = 400;
            res.set_content("{\"error\":\"Format ora invalid\"}", "application/json");
            return;
        }

        int nouHour = stoi(oraStr.substr(0, 2));
        int nouMin = stoi(oraStr.substr(3, 2));
        int nouTotalMin = nouHour * 60 + nouMin;

        lock_guard<mutex> lock(filmeMutex);
        string json = "[";
        bool firstSala = true;

        for (int s = 1; s <= 7; s++) {
            bool conflict = false;
            vector<string> filmeAziJson;

            for (auto& f : filme) {
                if (f.getSala().getIndex() == s) {
                    string exOraStr = f.getOraRulare();
                    int exHour = stoi(exOraStr.substr(0, 2));
                    int exMin = stoi(exOraStr.substr(3, 2));
                    int exTotalMin = exHour * 60 + exMin;
                    int exDurata = f.getDurata();

                    // Adaugam filmul curent la lista de filme pe azi pentru aceasta sala
                    string fJson = "{\"titlu\":\"" + je(f.getTitlu()) + "\",\"ora\":\"" + je(exOraStr) + "\",\"durata\":" + to_string(exDurata) + "}";
                    filmeAziJson.push_back(fJson);

                    // Verificare suprapunere
                    bool noConflict = (exTotalMin + exDurata + 30 <= nouTotalMin) || (nouTotalMin + durata + 30 <= exTotalMin);
                    if (!noConflict) {
                        conflict = true;
                    }
                }
            }

            if (!conflict) {
                if (!firstSala) json += ",";
                firstSala = false;

                auto dims = getSalaDimensions(s);
                int locuriTotale = dims.first * dims.second;

                json += "{";
                json += "\"idSala\":" + to_string(s) + ",";
                json += "\"randuri\":" + to_string(dims.first) + ",";
                json += "\"coloane\":" + to_string(dims.second) + ",";
                json += "\"locuriTotale\":" + to_string(locuriTotale) + ",";
                json += "\"filmeAzi\":[";
                for (size_t i = 0; i < filmeAziJson.size(); i++) {
                    if (i > 0) json += ",";
                    json += filmeAziJson[i];
                }
                json += "]";
                json += "}";
            }
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    // POST /api/admin/adauga-film
    svr.Post("/api/admin/adauga-film", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        
        string titlu = parseJsonStr(req.body, "titlu");
        string gen = parseJsonStr(req.body, "gen");
        int durata = parseJsonInt(req.body, "durata");
        int varstaMinima = parseJsonInt(req.body, "varstaMinima");
        int idSala = parseJsonInt(req.body, "idSala");
        string oraRulare = parseJsonStr(req.body, "oraRulare");

        if (titlu.empty() || gen.empty() || durata <= 0 || varstaMinima < 0 || idSala <= 0 || oraRulare.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        int randuri = 10, coloane = 10;
        bool gasit = false;
        for (auto& f : filme) {
            if (f.getSala().getIndex() == idSala) {
                randuri = f.getSala().getNumarRanduri();
                coloane = f.getSala().getNumarColoane();
                gasit = true;
                break;
            }
        }
        if (!gasit) {
            auto dims = getSalaDimensions(idSala);
            randuri = dims.first;
            coloane = dims.second;
        }

        Sala s(idSala, randuri, coloane);
        filme.push_back(Film(titlu, gen, durata, varstaMinima, s, oraRulare));
        saveFilmeToFile(filme);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/sterge-film
    svr.Post("/api/admin/sterge-film", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        
        int filmIndex = parseJsonInt(req.body, "filmIndex");
        if (filmIndex < 0 || filmIndex >= (int)filme.size()) {
            res.status = 400;
            res.set_content("{\"error\":\"Index invalid\"}", "application/json");
            return;
        }
        
        filme.erase(filme.begin() + filmIndex);
        saveFilmeToFile(filme);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/reset-sala
    svr.Post("/api/admin/reset-sala", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        
        int filmIndex = parseJsonInt(req.body, "filmIndex");
        if (filmIndex < 0 || filmIndex >= (int)filme.size()) {
            res.status = 400;
            res.set_content("{\"error\":\"Index invalid\"}", "application/json");
            return;
        }
        
        int randuri = filme[filmIndex].getSala().getNumarRanduri();
        int coloane = filme[filmIndex].getSala().getNumarColoane();
        int idSala = filme[filmIndex].getSala().getIndex();
        string oraRulare = filme[filmIndex].getOraRulare();

        Sala s(idSala, randuri, coloane);
        filme[filmIndex] = Film(filme[filmIndex].getTitlu(), filme[filmIndex].getGen(),
                                  filme[filmIndex].getDurata(), filme[filmIndex].getVarstaMinima(), s, oraRulare);
        saveFilmeToFile(filme);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/adauga-suvenir
    svr.Post("/api/admin/adauga-suvenir", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        
        string nume = parseJsonStr(req.body, "nume");
        double pret = parseJsonDouble(req.body, "pret");
        
        if (nume.empty() || pret <= 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }
        
        suveniruri.push_back(Suvenir(nume, pret));
        saveSuveniruriToFile(suveniruri);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/sterge-suvenir
    svr.Post("/api/admin/sterge-suvenir", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        
        int suvenirIndex = parseJsonInt(req.body, "suvenirIndex");
        if (suvenirIndex < 0 || suvenirIndex >= (int)suveniruri.size()) {
            res.status = 400;
            res.set_content("{\"error\":\"Index invalid\"}", "application/json");
            return;
        }
        
        suveniruri.erase(suveniruri.begin() + suvenirIndex);
        saveSuveniruriToFile(suveniruri);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/modifica-suvenir
    svr.Post("/api/admin/modifica-suvenir", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);

        int suvenirIndex = parseJsonInt(req.body, "suvenirIndex");
        double pretNou = parseJsonDouble(req.body, "pretNou");

        if (suvenirIndex < 0 || suvenirIndex >= (int)suveniruri.size() || pretNou <= 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        string nume = suveniruri[suvenirIndex].getNume();
        suveniruri[suvenirIndex] = Suvenir(nume, pretNou);
        saveSuveniruriToFile(suveniruri);

        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/cos/finalizeaza
    svr.Post("/api/cos/finalizeaza", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);

        // Parse bilete array
        size_t biletePos = req.body.find("\"bilete\":");
        if (biletePos == string::npos) {
            res.status = 400;
            res.set_content("{\"succes\":false,\"eroare\":\"Format invalid\"}", "application/json");
            return;
        }

        vector<pair<int,pair<int,int>>> bileteRezervate; // filmIndex, (rand, col)
        size_t start = req.body.find("[", biletePos);
        size_t end = req.body.find("]", start);
        if (start == string::npos || end == string::npos) {
            res.status = 400;
            res.set_content("{\"succes\":false,\"eroare\":\"Format invalid\"}", "application/json");
            return;
        }

        string bileteStr = req.body.substr(start + 1, end - start - 1);
        size_t pos = 0;
        while (pos < bileteStr.length()) {
            size_t objStart = bileteStr.find("{", pos);
            if (objStart == string::npos) break;
            size_t objEnd = bileteStr.find("}", objStart);
            if (objEnd == string::npos) break;

            string objStr = bileteStr.substr(objStart, objEnd - objStart + 1);
            int filmIndex = parseJsonInt(objStr, "filmIndex");
            int rand = parseJsonInt(objStr, "rand");
            int col = parseJsonInt(objStr, "col");

            if (filmIndex >= 0 && filmIndex < (int)filme.size()) {
                bileteRezervate.push_back({filmIndex, {rand, col}});
            }

            pos = objEnd + 1;
        }

        // Try to reserve all seats
        for (auto& b : bileteRezervate) {
            int filmIndex = b.first;
            int rand = b.second.first;
            int col = b.second.second;

            // Check if film has already started
            time_t now = time(nullptr);
            tm* local = localtime(&now);
            int currentHour = local->tm_hour;
            int currentMin = local->tm_min;
            int currentTotalMin = currentHour * 60 + currentMin;

            string oraRulare = filme[filmIndex].getOraRulare();
            if (oraRulare.length() < 5 || oraRulare[2] != ':') {
                oraRulare = "18:00";
            }
            int filmHour = stoi(oraRulare.substr(0, 2));
            int filmMin = stoi(oraRulare.substr(3, 2));
            int filmTotalMin = filmHour * 60 + filmMin;

            if (currentTotalMin >= filmTotalMin) {
                res.status = 409;
                string eroare = "Filmul " + je(filme[filmIndex].getTitlu()) + " a început deja.";
                res.set_content("{\"succes\":false,\"eroare\":\"" + eroare + "\"}", "application/json");
                return;
            }

            try {
                filme[filmIndex].getSala().rezervaLoc(rand, col);
            } catch (exception& e) {
                // Note: Rollback not possible with current Sala API (no release method)
                // User will need to retry the transaction
                res.status = 409;
                string eroare = "Locul Rand " + to_string(rand + 1) + " Loc " + to_string(col + 1) + " era deja ocupat";
                res.set_content("{\"succes\":false,\"eroare\":\"" + je(eroare) + "\"}", "application/json");
                return;
            }
        }

        setCors(res);
        res.set_content("{\"succes\":true}", "application/json");
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
            string titlu, gen, temp, oraRulare;
            int durata, varstaMinima, idSala, randuri, locuri;

            getline(ss, titlu, ',');
            getline(ss, gen, ',');
            getline(ss, temp, ','); durata       = stoi(temp);
            getline(ss, temp, ','); varstaMinima = stoi(temp);
            getline(ss, temp, ','); idSala       = stoi(temp);
            getline(ss, temp, ','); randuri      = stoi(temp);
            getline(ss, temp, ','); locuri       = stoi(temp);
            getline(ss, oraRulare, ',');
            if (oraRulare.empty() || oraRulare.length() < 5) {
                oraRulare = "18:00";
            }

            Sala s(idSala, randuri, locuri);
            filme.push_back(Film(titlu, gen, durata, varstaMinima, s, oraRulare));
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
            while (ocupate < 20 && ocupate < r * c) {
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
    thread serverThread([&filme, &suveniruri]() {
        startHttpServer(filme, suveniruri);
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
