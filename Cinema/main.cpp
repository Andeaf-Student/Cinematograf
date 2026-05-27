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
#include "Proiectie.h"
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

static void saveFilmeToFile(const vector<Film>& filme) {
    ofstream f("Filme.txt");
    if (!f.is_open()) return;
    for (const auto& film : filme) {
        f << film.getTitlu() << ","
          << film.getGen() << ","
          << film.getDurata() << ","
          << film.getVarstaMinima() << "\n";
    }
    f.close();
}

static void saveProiectiiToFile(const vector<Proiectie>& proiectii) {
    ofstream f("Proiectii.txt");
    if (!f.is_open()) return;
    for (const auto& p : proiectii) {
        f << p.idProiectie << ","
          << p.filmIndex << ","
          << p.idSala << ","
          << p.sala.getNumarRanduri() << ","
          << p.sala.getNumarColoane() << ","
          << p.oraRulare << "\n";
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

static void adaugaInIstoric(const string& bileteJson, const string& suveniruriJson, double total) {
    time_t t_now = time(nullptr);
    tm* l_now = localtime(&t_now);
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", l_now);
    string timestampStr(buf);

    string nouElement = "{\n"
        "  \"timestamp\": \"" + timestampStr + "\",\n"
        "  \"bilete\": " + bileteJson + ",\n"
        "  \"suveniruri\": " + suveniruriJson + ",\n"
        "  \"total\": " + to_string((int)total) + "\n"
        "}";

    string fileContent = readFile("istoric.txt");
    if (fileContent.empty() || fileContent == "[]") {
        fileContent = "[" + nouElement + "]";
    } else {
        size_t lastBrack = fileContent.find_last_of(']');
        if (lastBrack != string::npos) {
            string currentItems = fileContent.substr(0, lastBrack);
            while (!currentItems.empty() && (currentItems.back() == ' ' || currentItems.back() == '\n' || currentItems.back() == '\r' || currentItems.back() == '\t')) {
                currentItems.pop_back();
            }
            if (currentItems == "[") {
                fileContent = "[" + nouElement + "]";
            } else {
                fileContent = currentItems + ",\n" + nouElement + "]";
            }
        } else {
            fileContent = "[" + nouElement + "]";
        }
    }

    ofstream f("istoric.txt");
    if (f.is_open()) {
        f << fileContent;
        f.close();
    }
}

// ─── HTTP Server ─────────────────────────────────────────────────────────────
void startHttpServer(vector<Film>& filme, vector<Proiectie>& proiectii, vector<Suvenir>& suveniruri) {
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
            json += "\"index\":"         + to_string(i)                     + ",";
            json += "\"titlu\":\""      + je(f.getTitlu())                 + "\",";
            json += "\"gen\":\""        + je(f.getGen())                   + "\",";
            json += "\"durata\":"       + to_string(f.getDurata())         + ",";
            json += "\"varstaMinima\":" + to_string(f.getVarstaMinima());
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/proiectii
    svr.Get("/api/proiectii", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        string json = "[";
        for (size_t i = 0; i < proiectii.size(); i++) {
            if (i > 0) json += ",";
            auto& p = proiectii[i];

            int locuriDisponibile = 0;
            for (auto& rand : p.sala.getLocuri()) {
                for (bool ocupat : rand) {
                    if (!ocupat) locuriDisponibile++;
                }
            }

            json += "{";
            json += "\"idProiectie\":"    + to_string(p.idProiectie)          + ",";
            json += "\"filmIndex\":"      + to_string(p.filmIndex)            + ",";
            json += "\"idSala\":"         + to_string(p.idSala)               + ",";
            json += "\"randuri\":"        + to_string(p.sala.getNumarRanduri()) + ",";
            json += "\"coloane\":"        + to_string(p.sala.getNumarColoane()) + ",";
            json += "\"oraRulare\":\""     + je(p.oraRulare)                   + "\",";
            json += "\"locuriDisponibile\":" + to_string(locuriDisponibile);
            json += "}";
        }
        json += "]";
        setCors(res);
        res.set_content(json, "application/json");
    });

    // GET /api/proiectii/:id/sala
    svr.Get("/api/proiectii/:id/sala", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        setCors(res);
        int id = stoi(req.path_params.at("id"));
        
        Proiectie* found = nullptr;
        for (auto& p : proiectii) {
            if (p.idProiectie == id) {
                found = &p;
                break;
            }
        }

        if (!found) {
            res.status = 404;
            res.set_content("{\"error\":\"Proiectie negasita\"}", "application/json");
            return;
        }

        auto& sala   = found->sala;
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

    // POST /api/rezerva-proiectie
    svr.Post("/api/rezerva-proiectie", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        setCors(res);
        int idProiectie = parseJsonInt(req.body, "idProiectie");
        int rand_       = parseJsonInt(req.body, "rand");
        int col         = parseJsonInt(req.body, "col");
        string nume     = parseJsonStr(req.body, "nume");

        Proiectie* found = nullptr;
        for (auto& p : proiectii) {
            if (p.idProiectie == idProiectie) {
                found = &p;
                break;
            }
        }

        if (!found || rand_ < 0 || col < 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        try {
            found->sala.rezervaLoc(rand_, col);
        } catch (exception& e) {
            res.status = 409;
            res.set_content("{\"error\":\"" + je(e.what()) + "\"}", "application/json");
            return;
        }

        int nrRanduri = found->sala.getNumarRanduri();
        int pret      = (rand_ <= 1) ? 30 : (rand_ <= nrRanduri - 2) ? 35 : 40;
        string cod    = genCod();

        time_t t = time(nullptr);
        tm*  now = localtime(&t);
        char dataBuf[32];
        strftime(dataBuf, sizeof(dataBuf), "%d.%m.%Y %H:%M", now);

        string json = "{";
        json += "\"cod\":\""    + cod                                                              + "\",";
        json += "\"titlu\":\""  + je(filme[found->filmIndex].getTitlu())                           + "\",";
        json += "\"data\":\""   + string(dataBuf)                                                  + "\",";
        json += "\"loc\":\"Randul " + string(1,(char)('A'+rand_)) + ", Scaunul " + to_string(col+1)+ "\",";
        json += "\"pret\":"     + to_string(pret)                                                  + ",";
        json += "\"nume\":\""   + je(nume)                                                         + "\"";
        json += "}";
        res.set_content(json, "application/json");
    });

    // ─── Admin Endpoints ───────────────────────────────────────────────────────
    
    // GET /istoric - serve order history page
    svr.Get("/istoric", [](const httplib::Request&, httplib::Response& res) {
        string html = R"HTMLDELIMITER(<!DOCTYPE html>
<html lang="ro">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cinema Aurora - Istoric Comenzilor</title>
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
            --border: rgba(255, 255, 255, 0.08);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Outfit', sans-serif;
            background-color: var(--bg-deep);
            color: var(--text-main);
            min-height: 100vh;
            padding-bottom: 100px; /* footer offset */
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 30px 20px;
        }

        /* Header styles */
        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 40px;
            position: relative;
        }

        .back-btn {
            background: var(--glass);
            border: 1px solid var(--border);
            color: var(--text-main);
            padding: 10px 20px;
            border-radius: 10px;
            cursor: pointer;
            font-size: 0.95rem;
            font-weight: 600;
            transition: all 0.3s;
            text-decoration: none;
            display: inline-flex;
            align-items: center;
            gap: 8px;
        }

        .back-btn:hover {
            background: var(--primary);
            color: #000;
            border-color: var(--primary);
            box-shadow: 0 0 15px var(--primary-glow);
        }

        header h1 {
            font-size: 1.8rem;
            font-weight: 800;
            position: absolute;
            left: 50%;
            transform: translateX(-50%);
            white-space: nowrap;
        }

        /* Order Cards */
        .order-list {
            display: flex;
            flex-direction: column;
            gap: 25px;
        }

        .order-card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 16px;
            overflow: hidden;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }

        .order-header {
            background: rgba(255,255,255,0.02);
            padding: 15px 20px;
            border-bottom: 1px solid var(--border);
            font-weight: 600;
            color: var(--primary);
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .order-section {
            padding: 20px;
            border-bottom: 1px solid var(--border);
        }

        .order-section:last-of-type {
            border-bottom: none;
        }

        .section-title {
            font-size: 0.85rem;
            text-transform: uppercase;
            letter-spacing: 1.5px;
            color: var(--text-muted);
            margin-bottom: 15px;
            font-weight: 800;
        }

        /* Ticket details */
        .ticket-item {
            display: flex;
            gap: 20px;
            margin-bottom: 20px;
        }

        .ticket-item:last-child {
            margin-bottom: 0;
        }

        .ticket-poster-container {
            width: 60px;
            height: 90px;
            background: rgba(255, 255, 255, 0.03);
            border-radius: 8px;
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: center;
            border: 1px solid var(--border);
            flex-shrink: 0;
        }

        .ticket-poster-img {
            width: 100%;
            height: 100%;
            object-fit: cover;
        }

        .ticket-poster-placeholder {
            font-size: 1.8rem;
        }

        .ticket-info {
            display: flex;
            flex-direction: column;
            justify-content: center;
            gap: 5px;
        }

        .ticket-info .movie-title {
            font-size: 1.1rem;
            font-weight: 600;
            color: #fff;
        }

        .ticket-info .meta {
            font-size: 0.9rem;
            color: var(--text-muted);
        }

        .ticket-info .price {
            font-size: 0.9rem;
            color: var(--primary);
            font-weight: 600;
        }

        /* Souvenir details */
        .souvenir-item {
            font-size: 1rem;
            margin-bottom: 8px;
            color: var(--text-main);
            display: flex;
            justify-content: space-between;
        }

        .souvenir-item:last-child {
            margin-bottom: 0;
        }

        /* Order Total */
        .order-total {
            background: rgba(245, 197, 24, 0.03);
            padding: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-top: 1px solid var(--border);
        }

        .order-total span {
            font-weight: 800;
            font-size: 1.2rem;
            color: var(--primary);
        }

        /* Empty State */
        .empty-state {
            text-align: center;
            padding: 80px 20px;
        }

        .empty-icon {
            font-size: 4rem;
            margin-bottom: 20px;
        }

        .empty-state h2 {
            font-size: 1.4rem;
            margin-bottom: 15px;
            color: var(--text-muted);
        }

        .empty-state p {
            color: var(--text-muted);
            margin-bottom: 30px;
            font-size: 1rem;
            line-height: 1.6;
        }

        .btn-action {
            background: var(--primary);
            color: #000;
            border: none;
            padding: 15px 30px;
            border-radius: 12px;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.3s;
            text-decoration: none;
            display: inline-block;
        }

        .btn-action:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px var(--primary-glow);
        }

        /* Footer */
        footer {
            position: fixed;
            bottom: 0;
            left: 0;
            width: 100%;
            background: rgba(10, 10, 12, 0.9);
            backdrop-filter: blur(12px);
            border-top: 1px solid var(--border);
            padding: 15px 20px;
            display: flex;
            justify-content: center;
            z-index: 100;
        }

        .reset-btn {
            background: none;
            border: 1px solid var(--accent);
            color: var(--accent);
            padding: 12px 24px;
            border-radius: 10px;
            cursor: pointer;
            font-size: 0.95rem;
            font-weight: 600;
            transition: all 0.3s;
        }

        .reset-btn:hover {
            background: var(--accent);
            color: #fff;
            box-shadow: 0 0 15px rgba(255, 77, 77, 0.4);
        }
    </style>
</head>
<body>

    <div class="container">
        <header>
            <a href="/" class="back-btn">← Înapoi</a>
            <h1>📋 Istoricul comenzilor</h1>
            <div style="width: 80px;"></div> <!-- spacer -->
        </header>

        <div id="content">
            <!-- Populated via JS -->
        </div>
    </div>

    <footer id="footer" style="display: none;">
        <button class="reset-btn" onclick="reseteazaIstoric()">🗑️ Resetează istoricul</button>
    </footer>

    <script>
        const TMDB_TOKEN = "eyJhbGciOiJIUzI1NiJ9.eyJhdWQiOiI5ZGYzN2U5YzhkYWIxYmFiMTE3Y2Y0YjBiN2Y1NTYwNSIsIm5iZiI6MTc3OTgwMTE5MS4xNiwic3ViIjoiNmExNTljNjc2YzQ1ZGMzZGU4MDUzNDZiIiwic2NvcGVzIjpbImFwaV9yZWFkIl0sInZlcnNpb24iOjF9.tLHUfMWYSW-StnMLdx2HlL3nrJJy2f9g_9lmzHpR8Dk";
        const posterCache = {};

        async function getPosterUrl(titluFilm) {
            const curatat = titluFilm.trim();
            if (posterCache[curatat]) return posterCache[curatat];
            try {
                const url = `https://api.themoviedb.org/3/search/movie?query=${encodeURIComponent(curatat)}&language=en-US`;
                const response = await fetch(url, {
                    method: 'GET',
                    headers: { 'Authorization': `Bearer ${TMDB_TOKEN}`, 'Accept': 'application/json' }
                });
                if (response.ok) {
                    const data = await response.json();
                    if (data.results && data.results.length > 0 && data.results[0].poster_path) {
                        const fullUrl = `https://image.tmdb.org/t/p/w500${data.results[0].poster_path}`;
                        posterCache[curatat] = fullUrl;
                        return fullUrl;
                    }
                }
            } catch (err) {
                console.error(err);
            }
            posterCache[curatat] = null;
            return null;
        }

        function formatData(isoStr) {
            // "2025-05-26T18:30:00" -> "26.05.2025, 18:30"
            if (!isoStr) return "";
            try {
                const an = isoStr.substring(0,4);
                const luna = isoStr.substring(5,7);
                const zi = isoStr.substring(8,10);
                const ora = isoStr.substring(11,16);
                return `${zi}.${luna}.${an}, ${ora}`;
            } catch(e) {
                return isoStr;
            }
        }

        async function loadIstoric() {
            try {
                const response = await fetch('/api/istoric');
                const orders = await response.json();
                const contentDiv = document.getElementById('content');
                const footer = document.getElementById('footer');

                if (!orders || orders.length === 0) {
                    footer.style.display = 'none';
                    contentDiv.innerHTML = `
                        <div class="empty-state">
                            <div class="empty-icon">🎬</div>
                            <h2>Nu ai nicio comandă în istoric</h2>
                            <p>Rezervă primul tău bilet chiar acum!</p>
                            <a href="/" class="btn-action">Vezi filme</a>
                        </div>
                    `;
                    return;
                }

                footer.style.display = 'flex';
                let html = '<div class="order-list">';

                for (let o of orders) {
                    let bileteHtml = '';
                    if (o.bilete && o.bilete.length > 0) {
                        bileteHtml = '<div class="order-section"><div class="section-title">Bilete:</div>';
                        for (let b of o.bilete) {
                            const bId = Math.random().toString(36).substr(2, 9);
                            bileteHtml += `
                                <div class="ticket-item">
                                    <div class="ticket-poster-container">
                                        <div class="ticket-poster-placeholder" id="placeholder-${bId}">🎬</div>
                                        <img class="ticket-poster-img" id="poster-${bId}" style="display: none;">
                                    </div>
                                    <div class="ticket-info">
                                        <div class="movie-title">${b.titluFilm}</div>
                                        <div class="meta">Sala ${b.sala} • ${b.ora}</div>
                                        <div class="meta">Rândul ${b.rand}, Scaunul ${b.col}</div>
                                        <div class="price">${b.pret} RON</div>
                                    </div>
                                </div>
                            `;

                            // Incarca posterul asincron
                            getPosterUrl(b.titluFilm).then(url => {
                                const placeholder = document.getElementById(`placeholder-${bId}`);
                                const img = document.getElementById(`poster-${bId}`);
                                if (url && img && placeholder) {
                                    img.src = url;
                                    img.onload = () => {
                                        placeholder.style.display = 'none';
                                        img.style.display = 'block';
                                    };
                                }
                            });
                        }
                        bileteHtml += '</div>';
                    }

                    let suveniruriHtml = '';
                    if (o.suveniruri && o.suveniruri.length > 0) {
                        suveniruriHtml = '<div class="order-section"><div class="section-title">Suveniruri:</div>';
                        for (let s of o.suveniruri) {
                            suveniruriHtml += `
                                <div class="souvenir-item">
                                    <span>${s.nume} x${s.cantitate}</span>
                                    <span style="font-weight: 600;">${s.pret} RON</span>
                                </div>
                            `;
                        }
                        suveniruriHtml += '</div>';
                    }

                    html += `
                        <div class="order-card">
                            <div class="order-header">
                                📅 ${formatData(o.timestamp)}
                            </div>
                            ${bileteHtml}
                            ${suveniruriHtml}
                            <div class="order-total">
                                <span style="color: var(--text-muted); font-size: 1rem; font-weight: 600;">TOTAL:</span>
                                <span>${o.total} RON</span>
                            </div>
                        </div>
                    `;
                }

                html += '</div>';
                contentDiv.innerHTML = html;
            } catch (err) {
                console.error(err);
            }
        }

        async function reseteazaIstoric() {
            if (!confirm("Ești sigur? Toate comenzile vor fi șterse definitiv.")) return;
            try {
                const response = await fetch('/api/istoric/reseteaza', { method: 'POST' });
                if (response.ok) {
                    loadIstoric();
                }
            } catch (err) {
                console.error(err);
            }
        }

        window.onload = loadIstoric;
    </script>
</body>
</html>)HTMLDELIMITER";
        res.set_content(html, "text/html; charset=utf-8");
    });

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
        .section-title { font-size: 1.8rem; font-weight: 700; margin-bottom: 20px; color: var(--primary); display: flex; justify-content: space-between; align-items: center; }
        
        /* Search and controls */
        .controls-row { display: flex; justify-content: space-between; gap: 20px; margin-bottom: 20px; }
        .search-input { background: var(--bg-deep); border: 1px solid var(--border); color: #fff; padding: 10px 15px; border-radius: 8px; width: 300px; font-size: 0.95rem; }
        .search-input:focus { border-color: var(--primary); outline: none; }

        table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid var(--border); }
        th { color: var(--text-muted); font-weight: 600; text-transform: uppercase; font-size: 0.8rem; cursor: pointer; user-select: none; }
        th:hover { color: #fff; }
        tr:hover { background: var(--glass); }
        
        .btn { padding: 8px 16px; border: none; border-radius: 8px; cursor: pointer; font-weight: 600; transition: all 0.2s; font-size: 0.9rem; }
        .btn-danger { background: var(--accent); color: #fff; }
        .btn-danger:hover { opacity: 0.8; }
        .btn-success { background: var(--success); color: #fff; }
        .btn-success:hover { opacity: 0.8; }
        .btn-primary { background: var(--primary); color: #000; }
        .btn-primary:hover { opacity: 0.8; }
        .btn-secondary { background: var(--glass); color: #fff; border: 1px solid var(--border); }
        .btn-secondary:hover { background: rgba(255,255,255,0.1); }
        
        .form-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .form-group input, .form-group select { width: 100%; background: var(--bg-deep); border: 1px solid var(--border); padding: 12px; border-radius: 8px; color: #fff; font-size: 1rem; }
        .form-group select option { background: var(--bg-card); }
        .form-group label { display: block; margin-bottom: 8px; color: var(--text-muted); font-size: 0.9rem; }
        
        /* Modal */
        .modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.8); display: flex; justify-content: center; align-items: center; z-index: 1000; }
        .modal-content { background: var(--bg-card); border: 1px solid var(--border); border-radius: 20px; padding: 40px; text-align: center; width: 100%; max-width: 500px; box-shadow: 0 10px 40px rgba(0,0,0,0.5); }
        .modal-content h2 { color: var(--primary); margin-bottom: 20px; }
        .modal-content input { width: 100%; background: var(--bg-deep); border: 1px solid var(--border); padding: 15px; border-radius: 10px; color: #fff; font-size: 1.2rem; margin-bottom: 20px; }
        .modal-content button { padding: 15px 40px; background: var(--primary); color: #000; border: none; border-radius: 10px; font-size: 1.1rem; font-weight: 700; cursor: pointer; }
        .hidden { display: none !important; }
        
        /* Inline editing */
        .edit-input { background: var(--bg-deep); border: 1px solid var(--primary); color: #fff; padding: 6px 10px; border-radius: 6px; width: 100%; font-size: 0.95rem; }
    </style>
</head>
<body>
    <div id="password-modal" class="modal">
        <div class="modal-content">
            <h2>Admin Panel</h2>
            <input type="password" id="password-input" placeholder="Introdu parola...">
            <button onclick="checkPassword()">Accesează</button>
        </div>
    </div>

    <!-- Projection Modal -->
    <div id="projection-modal" class="modal hidden">
        <div class="modal-content" style="text-align: left;">
            <h2 id="proj-modal-title" style="margin-bottom: 10px;">Adaugă proiecție</h2>
            <p id="proj-modal-subtitle" style="color: var(--text-muted); margin-bottom: 20px;"></p>
            
            <div class="form-group" style="margin-bottom: 20px;">
                <label>Ora Rulării</label>
                <input type="time" id="proj-ora" value="18:00" onchange="checkSaliForNewProjection()">
            </div>

            <div id="proj-sali-container" style="margin-bottom: 25px;">
                <!-- Dynamically populated -->
            </div>

            <div style="display: flex; gap: 15px; justify-content: flex-end;">
                <button class="btn btn-secondary" onclick="closeProjectionModal()">Anulează</button>
                <button class="btn btn-primary" id="btn-salveaza-proiectie" onclick="salveazaProiectie()" disabled>Adaugă proiecție</button>
            </div>
        </div>
    </div>

    <div id="admin-content" class="hidden">
        <header>
            <button class="back-btn" onclick="window.location.href='/'">← Înapoi</button>
            <div class="logo-text">Cinema Aurora - Admin</div>
        </header>

        <div class="container">
            <!-- SECTIUNEA FILME -->
            <div class="section">
                <div class="section-title">
                    <span>FILME (Metadate)</span>
                </div>
                <div class="controls-row">
                    <input type="text" class="search-input" id="filme-search" placeholder="Caută film..." oninput="filterFilmeTable()">
                </div>
                <table id="filme-table">
                    <thead>
                        <tr>
                            <th onclick="sortTable('filme-table', 0, true)">#</th>
                            <th onclick="sortTable('filme-table', 1)">Titlu</th>
                            <th onclick="sortTable('filme-table', 2)">Gen</th>
                            <th onclick="sortTable('filme-table', 3, true)">Durată</th>
                            <th onclick="sortTable('filme-table', 4, true)">Vârstă minimă</th>
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
                        <input type="number" id="film-durata" value="120">
                    </div>
                    <div class="form-group">
                        <label>Vârstă Minimă</label>
                        <input type="number" id="film-varsta" value="12">
                    </div>
                </div>
                <button class="btn btn-primary" onclick="adaugaFilm()">Adaugă Film</button>
            </div>

            <!-- SECTIUNEA PROIECTII -->
            <div class="section">
                <div class="section-title">
                    <span>PROIECȚII ACTIVE</span>
                </div>
                <div class="controls-row">
                    <input type="text" class="search-input" id="proiectii-search" placeholder="Caută proiecție..." oninput="filterProiectiiTable()">
                </div>
                <table id="proiectii-table">
                    <thead>
                        <tr>
                            <th onclick="sortTable('proiectii-table', 0, true)">ID</th>
                            <th onclick="sortTable('proiectii-table', 1)">Film</th>
                            <th onclick="sortTable('proiectii-table', 2, true)">Sala</th>
                            <th onclick="sortTable('proiectii-table', 3)">Ora Rulării</th>
                            <th onclick="sortTable('proiectii-table', 4, true)">Locuri Ocupate</th>
                            <th>Acțiuni</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
            </div>

            <!-- SECTIUNEA SUVENIRURI -->
            <div class="section">
                <div class="section-title">SUVENIRURI</div>
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

        let globalFilme = [];
        let globalProiectii = [];

        function checkPassword() {
            const password = document.getElementById('password-input').value;
            if (password === 'admin123') {
                document.getElementById('password-modal').classList.add('hidden');
                document.getElementById('admin-content').classList.remove('hidden');
                initAdmin();
            } else {
                alert('Parolă incorectă!');
            }
        }

        async function initAdmin() {
            await loadFilme();
            await loadProiectii();
            await loadSuveniruri();
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
            globalFilme = await response.json();
            renderFilmeTable(globalFilme);
        }

        function renderFilmeTable(list) {
            const tbody = document.querySelector('#filme-table tbody');
            tbody.innerHTML = list.map(f => `
                <tr id="film-row-${f.index}">
                    <td>${f.index}</td>
                    <td class="cell-titlu">${f.titlu}</td>
                    <td class="cell-gen">${f.gen}</td>
                    <td class="cell-durata">${f.durata} min</td>
                    <td class="cell-varsta">${f.varstaMinima}+</td>
                    <td>
                        <button class="btn btn-primary" style="padding: 6px 12px; font-size: 0.85rem;" id="btn-edit-film-${f.index}" onclick="activeazaEditareFilm(${f.index})">Editează</button>
                        <button class="btn btn-danger" style="padding: 6px 12px; font-size: 0.85rem;" onclick="stergeFilm(${f.index})">Șterge</button>
                        <button class="btn btn-success" style="padding: 6px 12px; font-size: 0.85rem;" onclick="openProjectionModal(${f.index})">Proiecție +</button>
                    </td>
                </tr>
            `).join('');
        }

        async function loadProiectii() {
            const response = await apiCall('/api/proiectii');
            if (!response) return;
            globalProiectii = await response.json();
            renderProiectiiTable(globalProiectii);
        }

        function renderProiectiiTable(list) {
            const tbody = document.querySelector('#proiectii-table tbody');
            tbody.innerHTML = list.map(p => {
                const f = globalFilme.find(x => x.index === p.filmIndex);
                const titlu = f ? f.titlu : `Film Index ${p.filmIndex}`;
                const locuriOcupate = (p.randuri * p.coloane) - p.locuriDisponibile;
                const totalLocuri = p.randuri * p.coloane;

                return `
                    <tr id="proiectie-row-${p.idProiectie}">
                        <td>${p.idProiectie}</td>
                        <td>${titlu}</td>
                        <td class="cell-sala">Sala ${p.idSala}</td>
                        <td class="cell-ora">${p.oraRulare}</td>
                        <td>${locuriOcupate} / ${totalLocuri}</td>
                        <td>
                            <button class="btn btn-primary" style="padding: 6px 12px; font-size: 0.85rem;" id="btn-edit-proj-${p.idProiectie}" onclick="activeazaEditareProiectie(${p.idProiectie})">Editează</button>
                            <button class="btn btn-danger" style="padding: 6px 12px; font-size: 0.85rem;" onclick="stergeProiectie(${p.idProiectie})">Șterge</button>
                        </td>
                    </tr>
                `;
            }).join('');
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
                        <button class="btn btn-success" style="padding: 6px 12px; font-size: 0.85rem;" onclick="modificaSuvenir(${i})">Salvează</button>
                        <button class="btn btn-danger" style="padding: 6px 12px; font-size: 0.85rem;" onclick="stergeSuvenir(${i})">Șterge</button>
                    </td>
                </tr>
            `).join('');
        }

        /* --- FILME ACTIONS --- */
        async function adaugaFilm() {
            const titlu = document.getElementById('film-titlu').value;
            const gen = document.getElementById('film-gen').value;
            const durata = parseInt(document.getElementById('film-durata').value);
            const varstaMinima = parseInt(document.getElementById('film-varsta').value);

            if (!titlu || !gen || !durata || varstaMinima === NaN) {
                alert('Completați toate câmpurile corect!');
                return;
            }

            const response = await apiCall('/api/admin/adauga-film', 'POST', {
                titlu, gen, durata, varstaMinima
            });
            
            if (response && response.ok) {
                alert('Film adăugat cu succes!');
                document.getElementById('film-titlu').value = '';
                document.getElementById('film-gen').value = '';
                document.getElementById('film-durata').value = '120';
                document.getElementById('film-varsta').value = '12';
                loadFilme();
            } else {
                alert('Eroare la adăugarea filmului!');
            }
        }

        function activeazaEditareFilm(index) {
            const row = document.getElementById(`film-row-${index}`);
            if (!row) return;

            const cellTitlu = row.querySelector('.cell-titlu');
            const cellGen = row.querySelector('.cell-gen');
            const cellDurata = row.querySelector('.cell-durata');
            const cellVarsta = row.querySelector('.cell-varsta');

            const valTitlu = cellTitlu.innerText;
            const valGen = cellGen.innerText;
            const valDurata = parseInt(cellDurata.innerText);
            const valVarsta = parseInt(cellVarsta.innerText);

            cellTitlu.innerHTML = `<input type="text" class="edit-input" id="edit-film-titlu-${index}" value="${valTitlu}">`;
            cellGen.innerHTML = `<input type="text" class="edit-input" id="edit-film-gen-${index}" value="${valGen}">`;
            cellDurata.innerHTML = `<input type="number" class="edit-input" id="edit-film-durata-${index}" value="${valDurata}" style="width: 80px;">`;
            cellVarsta.innerHTML = `<input type="number" class="edit-input" id="edit-film-varsta-${index}" value="${valVarsta}" style="width: 80px;">`;

            const btnEdit = document.getElementById(`btn-edit-film-${index}`);
            btnEdit.innerText = 'Salvează';
            btnEdit.className = 'btn btn-success';
            btnEdit.onclick = () => salveazaEditareFilm(index);
        }

        async function salveazaEditareFilm(index) {
            const titlu = document.getElementById(`edit-film-titlu-${index}`).value;
            const gen = document.getElementById(`edit-film-gen-${index}`).value;
            const durata = parseInt(document.getElementById(`edit-film-durata-${index}`).value);
            const varstaMinima = parseInt(document.getElementById(`edit-film-varsta-${index}`).value);

            if (!titlu || !gen || !durata || isNaN(varstaMinima)) {
                alert('Date invalide!');
                return;
            }

            const response = await apiCall('/api/admin/modifica-film', 'PUT', {
                filmIndex: index, titlu, gen, durata, varstaMinima
            });

            if (response && response.ok) {
                alert('Film salvat cu succes!');
                await loadFilme();
                await loadProiectii(); // Numele filmului s-ar putea schimba in tabela de proiectii
            } else {
                alert('Eroare la modificarea filmului!');
            }
        }

        async function stergeFilm(index) {
            if (!confirm(`Sigur doriți să ștergeți filmul ${index}? Toate proiecțiile sale vor fi șterse.`)) return;
            const response = await apiCall('/api/admin/sterge-film', 'POST', { filmIndex: index });
            if (response && response.ok) {
                alert('Film șters cu succes!');
                initAdmin();
            } else {
                alert('Eroare la ștergerea filmului!');
            }
        }

        /* --- PROJECTIONS ACTIONS --- */
        let currentModalFilmIndex = null;
        let selectedProjSalaId = null;

        function openProjectionModal(filmIndex) {
            currentModalFilmIndex = filmIndex;
            const f = globalFilme.find(x => x.index === filmIndex);
            if (!f) return;

            document.getElementById('proj-modal-title').innerText = `Adaugă proiecție`;
            document.getElementById('proj-modal-subtitle').innerText = `Pentru filmul "${f.titlu}" (${f.durata} min)`;
            document.getElementById('projection-modal').classList.remove('hidden');

            checkSaliForNewProjection();
        }

        function closeProjectionModal() {
            document.getElementById('projection-modal').classList.add('hidden');
            currentModalFilmIndex = null;
            selectedProjSalaId = null;
        }

        async function checkSaliForNewProjection() {
            const f = globalFilme.find(x => x.index === currentModalFilmIndex);
            if (!f) return;

            const ora = document.getElementById('proj-ora').value;
            const durata = f.durata;
            const container = document.getElementById('proj-sali-container');
            const btnSave = document.getElementById('btn-salveaza-proiectie');

            if (!ora || ora.length < 5) {
                container.innerHTML = '';
                btnSave.disabled = true;
                return;
            }

            const response = await apiCall(`/api/admin/sali-disponibile?ora=${ora}&durata=${durata}`);
            if (!response || !response.ok) {
                container.innerHTML = '<div style="color: #ff4d4d; font-weight: bold;">Eroare la verificare săli!</div>';
                btnSave.disabled = true;
                return;
            }

            const sali = await response.json();
            if (sali.length === 0) {
                container.innerHTML = `<div style="color: #ff4d4d; font-weight: bold;">⛔ Nicio sală disponibilă la ora ${ora} (suprapuneri cu pauză de 30min).</div>`;
                btnSave.disabled = true;
                return;
            }

            let html = `
                <label style="display: block; margin-bottom: 10px; font-weight: bold; color: var(--primary);">Selectează Sală Disponibilă:</label>
                <select id="select-proj-sala" class="form-control" style="width: 100%; padding: 10px; border-radius: 6px; background: #222; color: #fff; border: 1px solid var(--border);" onchange="onProjSalaSelected(this.value)">
                    <option value="">-- Alege o sală --</option>
            `;

            sali.forEach(s => {
                let programText = "Sală liberă toată ziua";
                if (s.filmeAzi && s.filmeAzi.length > 0) {
                    programText = "Program: " + s.filmeAzi.map(f => `${f.titlu} (${f.ora})`).join(', ');
                }
                html += `
                    <option value="${s.idSala}">
                        Sala ${s.idSala} — ${s.locuriTotale} locuri (${programText})
                    </option>
                `;
            });

            html += `</select>`;
            container.innerHTML = html;
            btnSave.disabled = true;
            selectedProjSalaId = null;
        }

        function onProjSalaSelected(val) {
            const btnSave = document.getElementById('btn-salveaza-proiectie');
            if (val) {
                selectedProjSalaId = parseInt(val);
                btnSave.disabled = false;
            } else {
                selectedProjSalaId = null;
                btnSave.disabled = true;
            }
        }

        async function salveazaProiectie() {
            const filmIndex = currentModalFilmIndex;
            const idSala = selectedProjSalaId;
            const oraRulare = document.getElementById('proj-ora').value;

            if (filmIndex === null || !idSala || !oraRulare) {
                alert('Eroare parametrizare!');
                return;
            }

            const response = await apiCall('/api/admin/adauga-proiectie', 'POST', {
                filmIndex, idSala, oraRulare
            });

            if (response && response.ok) {
                alert('Proiecție adăugată cu succes!');
                closeProjectionModal();
                loadProiectii();
            } else {
                alert('Eroare la salvare proiecție!');
            }
        }

        async function stergeProiectie(id) {
            if (!confirm(`Sigur doriți să ștergeți proiecția ID ${id}?`)) return;
            const response = await apiCall('/api/admin/sterge-proiectie', 'DELETE', { idProiectie: id });
            if (response && response.ok) {
                alert('Proiecție ștearsă!');
                loadProiectii();
            } else {
                alert('Eroare la ștergerea proiecției!');
            }
        }

        // Inline editing for projections
        function activeazaEditareProiectie(idProiectie) {
            const p = globalProiectii.find(x => x.idProiectie === idProiectie);
            const f = globalFilme.find(x => x.index === p.filmIndex);
            if (!p || !f) return;

            const row = document.getElementById(`proiectie-row-${idProiectie}`);
            if (!row) return;

            const cellSala = row.querySelector('.cell-sala');
            const cellOra = row.querySelector('.cell-ora');

            cellOra.innerHTML = `<input type="time" class="edit-input" id="edit-proj-ora-${idProiectie}" value="${p.oraRulare}" onchange="checkSaliForEditingProjection(${idProiectie}, ${f.durata})">`;
            cellSala.innerHTML = `<div id="edit-proj-sala-container-${idProiectie}">Se încarcă...</div>`;

            const btnEdit = document.getElementById(`btn-edit-proj-${idProiectie}`);
            btnEdit.innerText = 'Salvează';
            btnEdit.className = 'btn btn-success';
            btnEdit.onclick = () => salveazaEditareProiectie(idProiectie);

            checkSaliForEditingProjection(idProiectie, f.durata);
        }

        async function checkSaliForEditingProjection(idProiectie, durata) {
            const oraInput = document.getElementById(`edit-proj-ora-${idProiectie}`);
            if (!oraInput) return;
            const ora = oraInput.value;
            const container = document.getElementById(`edit-proj-sala-container-${idProiectie}`);

            const response = await apiCall(`/api/admin/sali-disponibile?ora=${ora}&durata=${durata}`);
            if (!response || !response.ok) {
                container.innerHTML = '<span style="color:var(--accent);">Eroare verification</span>';
                return;
            }

            const sali = await response.json();
            
            // Gasim proiectia curenta pentru a predetermina sala curenta
            const p = globalProiectii.find(x => x.idProiectie === idProiectie);
            
            let html = `<select class="edit-input" id="edit-proj-sala-${idProiectie}">`;
            
            // Asiguram ca sala curenta este intotdeauna listata ca optiune
            let existsCurrent = sali.some(s => s.idSala === p.idSala);
            if (!existsCurrent) {
                html += `<option value="${p.idSala}">Sala ${p.idSala} (Curentă)</option>`;
            }

            sali.forEach(s => {
                const selected = s.idSala === p.idSala ? 'selected' : '';
                html += `<option value="${s.idSala}" ${selected}>Sala ${s.idSala}</option>`;
            });
            html += `</select>`;
            container.innerHTML = html;
        }

        async function salveazaEditareProiectie(idProiectie) {
            const oraRulare = document.getElementById(`edit-proj-ora-${idProiectie}`).value;
            const idSalaSelect = document.getElementById(`edit-proj-sala-${idProiectie}`);
            if (!idSalaSelect) return;
            const idSala = parseInt(idSalaSelect.value);

            if (!oraRulare || isNaN(idSala)) {
                alert('Informații invalide!');
                return;
            }

            const response = await apiCall('/api/admin/modifica-proiectie', 'PUT', {
                idProiectie, idSala, oraRulare
            });

            if (response && response.ok) {
                alert('Proiecție modificată cu succes!');
                loadProiectii();
            } else {
                alert('Eroare la salvare proiecție!');
            }
        }

        /* --- SUVENIRURI ACTIONS --- */
        async function adaugaSuvenir() {
            const nume = document.getElementById('suvenir-nume').value;
            const pret = parseFloat(document.getElementById('suvenir-pret').value);

            if (!nume || isNaN(pret) || pret <= 0) {
                alert('Completați toate câmpurile corect!');
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
            if (isNaN(pretNou) || pretNou <= 0) {
                alert('Preț invalid!');
                return;
            }
            const response = await apiCall('/api/admin/modifica-suvenir', 'POST', { suvenirIndex: index, pretNou });
            if (response && response.ok) {
                alert('Preț modificat cu succes!');
                loadSuveniruri();
            } else {
                alert('Eroare la modificarea prețului!');
            }
        }

        /* --- UTILS: SEARCH AND SORT --- */
        function filterFilmeTable() {
            const query = document.getElementById('filme-search').value.toLowerCase();
            const filtered = globalFilme.filter(f => 
                f.titlu.toLowerCase().includes(query) || f.gen.toLowerCase().includes(query)
            );
            renderFilmeTable(filtered);
        }

        function filterProiectiiTable() {
            const query = document.getElementById('proiectii-search').value.toLowerCase();
            const filtered = globalProiectii.filter(p => {
                const f = globalFilme.find(x => x.index === p.filmIndex);
                const titlu = f ? f.titlu.toLowerCase() : "";
                return titlu.includes(query) || `sala ${p.idSala}`.includes(query) || p.oraRulare.includes(query);
            });
            renderProiectiiTable(filtered);
        }

        let sortDirections = {};
        function sortTable(tableId, colIndex, isNumeric = false) {
            const table = document.getElementById(tableId);
            const tbody = table.querySelector('tbody');
            const rows = Array.from(tbody.querySelectorAll('tr'));
            const key = `${tableId}-${colIndex}`;
            const direction = sortDirections[key] === 'asc' ? 'desc' : 'asc';
            sortDirections[key] = direction;

            rows.sort((a, b) => {
                const aCol = a.children[colIndex].innerText.trim();
                const bCol = b.children[colIndex].innerText.trim();

                if (isNumeric) {
                    const aNum = parseFloat(aCol.replace(/[^0-9.-]+/g, "")) || 0;
                    const bNum = parseFloat(bCol.replace(/[^0-9.-]+/g, "")) || 0;
                    return direction === 'asc' ? aNum - bNum : bNum - aNum;
                } else {
                    return direction === 'asc' ? aCol.localeCompare(bCol) : bCol.localeCompare(aCol);
                }
            });

            const headers = table.querySelectorAll('th');
            headers.forEach((th, i) => {
                let text = th.innerText.replace(/ [▲▼]/g, "");
                if (i === colIndex) {
                    text += direction === 'asc' ? ' ▲' : ' ▼';
                }
                th.innerText = text;
            });

            tbody.innerHTML = '';
            rows.forEach(r => tbody.appendChild(r));
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

            for (auto& p : proiectii) {
                if (p.idSala == s) {
                    string exOraStr = p.oraRulare;
                    int exHour = stoi(exOraStr.substr(0, 2));
                    int exMin = stoi(exOraStr.substr(3, 2));
                    int exTotalMin = exHour * 60 + exMin;
                    
                    int exDurata = 120;
                    string titluFilm = "Film";
                    if (p.filmIndex >= 0 && p.filmIndex < (int)filme.size()) {
                        exDurata = filme[p.filmIndex].getDurata();
                        titluFilm = filme[p.filmIndex].getTitlu();
                    }

                    string fJson = "{\"titlu\":\"" + je(titluFilm) + "\",\"ora\":\"" + je(exOraStr) + "\",\"durata\":" + to_string(exDurata) + "}";
                    filmeAziJson.push_back(fJson);

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

        if (titlu.empty() || gen.empty() || durata <= 0 || varstaMinima < 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        filme.push_back(Film(titlu, gen, durata, varstaMinima));
        saveFilmeToFile(filme);
        
        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // PUT /api/admin/modifica-film
    svr.Put("/api/admin/modifica-film", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        int filmIndex = parseJsonInt(req.body, "filmIndex");
        string titlu = parseJsonStr(req.body, "titlu");
        string gen = parseJsonStr(req.body, "gen");
        int durata = parseJsonInt(req.body, "durata");
        int varstaMinima = parseJsonInt(req.body, "varstaMinima");

        if (filmIndex < 0 || filmIndex >= (int)filme.size() || titlu.empty() || gen.empty() || durata <= 0 || varstaMinima < 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        filme[filmIndex] = Film(titlu, gen, durata, varstaMinima);
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
            res.status = 404;
            res.set_content("{\"error\":\"Film negasit\"}", "application/json");
            return;
        }

        // Stergere proiectii asociate si decalare indici
        for (auto it = proiectii.begin(); it != proiectii.end(); ) {
            if (it->filmIndex == filmIndex) {
                it = proiectii.erase(it);
            } else {
                if (it->filmIndex > filmIndex) {
                    it->filmIndex--;
                }
                ++it;
            }
        }
        saveProiectiiToFile(proiectii);

        filme.erase(filme.begin() + filmIndex);
        saveFilmeToFile(filme);

        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // POST /api/admin/adauga-proiectie
    svr.Post("/api/admin/adauga-proiectie", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        int filmIndex = parseJsonInt(req.body, "filmIndex");
        int idSala = parseJsonInt(req.body, "idSala");
        string oraRulare = parseJsonStr(req.body, "oraRulare");

        if (filmIndex < 0 || filmIndex >= (int)filme.size() || idSala <= 0 || oraRulare.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        int maxId = 0;
        for (const auto& p : proiectii) {
            if (p.idProiectie > maxId) maxId = p.idProiectie;
        }
        int idProiectie = maxId + 1;

        auto dims = getSalaDimensions(idSala);
        int randuri = dims.first;
        int coloane = dims.second;

        proiectii.push_back(Proiectie(idProiectie, filmIndex, idSala, randuri, coloane, oraRulare));
        saveProiectiiToFile(proiectii);

        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // DELETE /api/admin/sterge-proiectie
    svr.Delete("/api/admin/sterge-proiectie", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        int idProiectie = parseJsonInt(req.body, "idProiectie");

        auto it = proiectii.end();
        for (auto i = proiectii.begin(); i != proiectii.end(); ++i) {
            if (i->idProiectie == idProiectie) {
                it = i;
                break;
            }
        }

        if (it == proiectii.end()) {
            res.status = 404;
            res.set_content("{\"error\":\"Proiectie negasita\"}", "application/json");
            return;
        }

        proiectii.erase(it);
        saveProiectiiToFile(proiectii);

        setCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // PUT /api/admin/modifica-proiectie
    svr.Put("/api/admin/modifica-proiectie", [&](const httplib::Request& req, httplib::Response& res) {
        if (!checkAuth(req)) {
            res.status = 401;
            res.set_content("{\"error\":\"Neautorizat\"}", "application/json");
            return;
        }
        lock_guard<mutex> lock(filmeMutex);
        int idProiectie = parseJsonInt(req.body, "idProiectie");
        int idSala = parseJsonInt(req.body, "idSala");
        string oraRulare = parseJsonStr(req.body, "oraRulare");

        Proiectie* found = nullptr;
        for (auto& p : proiectii) {
            if (p.idProiectie == idProiectie) {
                found = &p;
                break;
            }
        }

        if (!found || idSala <= 0 || oraRulare.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Parametri invalizi\"}", "application/json");
            return;
        }

        found->idSala = idSala;
        found->oraRulare = oraRulare;

        // update sala index and dimensions if changed
        auto dims = getSalaDimensions(idSala);
        found->sala = Sala(idSala, dims.first, dims.second);

        saveProiectiiToFile(proiectii);

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

    // GET /api/istoric
    svr.Get("/api/istoric", [&](const httplib::Request&, httplib::Response& res) {
        setCors(res);
        string content = readFile("istoric.txt");
        if (content.empty()) {
            res.set_content("[]", "application/json");
        } else {
            res.set_content(content, "application/json");
        }
    });

    // POST /api/istoric/reseteaza
    svr.Post("/api/istoric/reseteaza", [&](const httplib::Request&, httplib::Response& res) {
        setCors(res);
        ofstream f("istoric.txt");
        if (f.is_open()) {
            f << "[]";
            f.close();
            res.set_content("{\"success\":true}", "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"error\":\"Eroare la scriere\"}", "application/json");
        }
    });

    // POST /api/cos/finalizeaza
    svr.Post("/api/cos/finalizeaza", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(filmeMutex);
        setCors(res);

        // 1. Extragere array bilete
        size_t biletePos = req.body.find("\"bilete\":");
        vector<pair<Proiectie*, pair<int, int>>> succesfulReservations;
        string bileteJsonArray = "[";
        double totalBilete = 0;

        if (biletePos != string::npos) {
            size_t start = req.body.find("[", biletePos);
            size_t end = req.body.find("]", start);
            if (start != string::npos && end != string::npos) {
                string bileteStr = req.body.substr(start + 1, end - start - 1);
                size_t pos = 0;
                bool firstBilet = true;
                while (pos < bileteStr.length()) {
                    size_t objStart = bileteStr.find("{", pos);
                    if (objStart == string::npos) break;
                    size_t objEnd = bileteStr.find("}", objStart);
                    if (objEnd == string::npos) break;

                    string objStr = bileteStr.substr(objStart, objEnd - objStart + 1);
                    int idProiectie = parseJsonInt(objStr, "idProiectie");
                    int rand = parseJsonInt(objStr, "rand");
                    int col = parseJsonInt(objStr, "col");

                    Proiectie* found = nullptr;
                    for (auto& p : proiectii) {
                        if (p.idProiectie == idProiectie) {
                            found = &p;
                            break;
                        }
                    }

                    if (!found || rand < 0 || col < 0) {
                        for (auto& roll : succesfulReservations) {
                            roll.first->sala.elibereazaLoc(roll.second.first, roll.second.second);
                        }
                        res.status = 400;
                        res.set_content("{\"succes\":false,\"eroare\":\"Proiectie sau loc invalid.\"}", "application/json");
                        return;
                    }

                    try {
                        found->sala.rezervaLoc(rand, col);
                        succesfulReservations.push_back({found, {rand, col}});
                    } catch (exception& e) {
                        for (auto& roll : succesfulReservations) {
                            roll.first->sala.elibereazaLoc(roll.second.first, roll.second.second);
                        }
                        res.status = 409;
                        string eroare = "Locul Rand " + to_string(rand + 1) + " Loc " + to_string(col + 1) + " este deja ocupat.";
                        res.set_content("{\"succes\":false,\"eroare\":\"" + je(eroare) + "\"}", "application/json");
                        return;
                    }

                    int nrRanduri = found->sala.getNumarRanduri();
                    int pretBilet = (rand <= 1) ? 30 : (rand <= nrRanduri - 2) ? 35 : 40;
                    totalBilete += pretBilet;

                    if (!firstBilet) bileteJsonArray += ",";
                    firstBilet = false;

                    string randLitera = string(1, (char)('A' + rand));
                    string posterUrl = "";
                    bileteJsonArray += "{\n"
                        "  \"titluFilm\": \"" + je(filme[found->filmIndex].getTitlu()) + "\",\n"
                        "  \"posterUrl\": \"" + posterUrl + "\",\n"
                        "  \"sala\": " + to_string(found->idSala) + ",\n"
                        "  \"ora\": \"" + je(found->oraRulare) + "\",\n"
                        "  \"rand\": \"" + randLitera + "\",\n"
                        "  \"col\": " + to_string(col + 1) + ",\n"
                        "  \"pret\": " + to_string(pretBilet) + "\n"
                        "}";

                    pos = objEnd + 1;
                }
            }
        }
        bileteJsonArray += "]";

        // 2. Extragere array suveniruri
        size_t suveniruriPos = req.body.find("\"suveniruri\":");
        string suveniruriJsonArray = "[";
        double totalSuveniruri = 0;

        if (suveniruriPos != string::npos) {
            size_t start = req.body.find("[", suveniruriPos);
            size_t end = req.body.find("]", start);
            if (start != string::npos && end != string::npos) {
                string suveniruriStr = req.body.substr(start + 1, end - start - 1);
                size_t pos = 0;
                bool firstSuv = true;
                while (pos < suveniruriStr.length()) {
                    size_t objStart = suveniruriStr.find("{", pos);
                    if (objStart == string::npos) break;
                    size_t objEnd = suveniruriStr.find("}", objStart);
                    if (objEnd == string::npos) break;

                    string objStr = suveniruriStr.substr(objStart, objEnd - objStart + 1);
                    int suvenirIndex = parseJsonInt(objStr, "suvenirIndex");
                    int cantitate = parseJsonInt(objStr, "cantitate");

                    if (suvenirIndex >= 0 && suvenirIndex < (int)suveniruri.size() && cantitate > 0) {
                        auto& s = suveniruri[suvenirIndex];
                        double pretTotalSuv = s.getPret() * cantitate;
                        totalSuveniruri += pretTotalSuv;

                        if (!firstSuv) suveniruriJsonArray += ",";
                        firstSuv = false;

                        suveniruriJsonArray += "{\n"
                            "  \"nume\": \"" + je(s.getNume()) + "\",\n"
                            "  \"cantitate\": " + to_string(cantitate) + ",\n"
                            "  \"pret\": " + to_string((int)pretTotalSuv) + "\n"
                            "}";
                    }

                    pos = objEnd + 1;
                }
            }
        }
        suveniruriJsonArray += "]";

        double totalGeneral = totalBilete + totalSuveniruri;

        // 3. Salvare în istoric.txt
        adaugaInIstoric(bileteJsonArray, suveniruriJsonArray, totalGeneral);

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

    vector<Film>      filme;
    vector<Proiectie> proiectii;
    vector<Suvenir>   suveniruri;

    ifstream fileFilme("Filme.txt");
    if (fileFilme.is_open()) {
        string line;
        while (getline(fileFilme, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string titlu, gen, temp;
            int durata, varstaMinima;

            getline(ss, titlu, ',');
            getline(ss, gen, ',');
            getline(ss, temp, ','); durata       = stoi(temp);
            getline(ss, temp, ','); varstaMinima = stoi(temp);

            filme.push_back(Film(titlu, gen, durata, varstaMinima));
        }
        fileFilme.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Filme.txt!\n";
    }

    ifstream fileProiectii("Proiectii.txt");
    if (fileProiectii.is_open()) {
        string line;
        while (getline(fileProiectii, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string temp, oraRulare;
            int idProiectie, filmIndex, idSala, randuri, coloane;

            getline(ss, temp, ','); idProiectie = stoi(temp);
            getline(ss, temp, ','); filmIndex   = stoi(temp);
            getline(ss, temp, ','); idSala      = stoi(temp);
            getline(ss, temp, ','); randuri     = stoi(temp);
            getline(ss, temp, ','); coloane     = stoi(temp);
            getline(ss, oraRulare, ',');

            proiectii.push_back(Proiectie(idProiectie, filmIndex, idSala, randuri, coloane, oraRulare));
        }
        fileProiectii.close();
    } else {
        cout << "Avertisment: Nu s-a putut deschide fisierul Proiectii.txt!\n";
    }

    random_device rd_init;
    mt19937 gen_init(rd_init());

    for (auto& p : proiectii) {
        int r = p.sala.getNumarRanduri();
        int c = p.sala.getNumarColoane();
        if (r > 0 && c > 0) {
            uniform_int_distribution<> distR(0, r - 1);
            uniform_int_distribution<> distC(0, c - 1);
            int ocupate = 0;
            while (ocupate < 20 && ocupate < r * c) {
                int randR = distR(gen_init);
                int randC = distC(gen_init);
                try {
                    p.sala.rezervaLoc(randR, randC);
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
    thread serverThread([&filme, &proiectii, &suveniruri]() {
        startHttpServer(filme, proiectii, suveniruri);
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

            vector<size_t> indexProiectii;
            {
                lock_guard<mutex> lock(filmeMutex);
                for (size_t i = 0; i < proiectii.size(); i++) {
                    if (proiectii[i].filmIndex == indexFilm) {
                        indexProiectii.push_back(i);
                    }
                }
            }

            if (indexProiectii.empty()) {
                cout << "Nu exista proiectii active pentru acest film!\n";
                break;
            }

            cout << "\nProiectii active pentru \"" << filme[indexFilm].getTitlu() << "\":\n";
            for (size_t i = 0; i < indexProiectii.size(); i++) {
                size_t idxP = indexProiectii[i];
                cout << "  [" << i + 1 << "] Ora: " << proiectii[idxP].oraRulare 
                     << " | Sala: " << proiectii[idxP].idSala << "\n";
            }

            cout << "Alege proiectia: ";
            int optProj;
            cin >> optProj;
            optProj--;

            if (optProj < 0 || optProj >= (int)indexProiectii.size()) {
                cout << "Alegere proiectie invalida!\n";
                break;
            }

            size_t idxP = indexProiectii[optProj];

            {
                lock_guard<mutex> lock(filmeMutex);
                proiectii[idxP].sala.afiseazaLocuri();
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
                    proiectii[idxP].sala.selecteazaLocInteractiv(rand_, col);
                }
                cout << "\nRezervare " << i << ": Rand " << rand_ + 1 << ", Loc " << col + 1 << endl;

                try
                {
                    int nrRanduri;
                    {
                        lock_guard<mutex> lock(filmeMutex);
                        nrRanduri = proiectii[idxP].sala.getNumarRanduri();
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
                            proiectii[idxP].sala.rezervaLoc(rand_, col);
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
                // Persist the reservations to Proiectii.txt
                lock_guard<mutex> lock(filmeMutex);
                saveProiectiiToFile(proiectii);
            }

            {
                lock_guard<mutex> lock(filmeMutex);
                proiectii[idxP].sala.afiseazaLocuri();
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
