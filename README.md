# 🎬 Proiect POO – Cinematograf Aurora (C++)

## 📌 Descriere
**Cinema Aurora** este o aplicație modernă de tip cinematograf, cu o arhitectură **duală**: o consolă text interactivă C++ și o **interfață web modernă (SPA)** alimentată de un server web multi-threaded REST API în C++.

Aplicația este construită folosind concepte solide de **Programare Orientată pe Obiecte**:
* **Clase**: `Film`, `Sala`, `Bilet`, `Suvenir` și structura `Proiectie`.
* **Relații complexe**: Separare completă între metadatele filmelor și instanțele dinamice de proiecție (ore de rulare, săli dedicate, matrici de locuri).
* **Multi-threading & Sincronizare**: Utilizarea de mutex-uri (`std::mutex`, `std::lock_guard`) pentru a preveni accesul concurent (race conditions) la rezervări.
* **Persistență**: Stocare/încărcare persistentă pe server (`Filme.txt`, `Proiectii.txt`, `Suveniruri.txt`, `istoric.txt` și `cos_temp.txt`).

---

## � Funcționalități de Elită

### 🌐 Interfața Web SPA (Single Page Application)
* **Design Futuristic**: Interfață responsive întunecată (Dark Mode), efecte moderne de sticlă (Glassmorphism) și animații fluide.
* **Integrare TMDB**: Postere de film descărcate și memorate în cache automat.
* **Proiecții Multiple & Status în Timp Real**: Badge-uri dinamice pe ore (Verde - Disponibil, Portocaliu pulsând - Începe în < 2h, Roșu - Sold Out, Gri - Încheiat).
* **Selecție Interactivă de Locuri**: Matrice vizuală a sălii unde utilizatorul selectează locurile în timp real.
* **Coș Persistent**: Coșul de cumpărături (bilete + suveniruri) se sincronizează în fundal pe server (`cos_temp.txt`) și nu se pierde la navigare.
* **Magazin de Suveniruri**: Posibilitatea adăugării de produse tematice direct în coș.
* **Istoric Complet**: Pagina `/istoric` afișează toate comenzile trecute ca niște chitanțe electronice elegante, cu posibilitate de resetare.

### 🖥️ Panoul de Administrare (`/admin`)
* **Gestiune Filme (Metadate)**: Adăugare, editare inline direct în tabel, ștergere în timp real (cu ștergerea în cascadă a proiecțiilor aferente).
* **Gestiune Proiecții**: Creare de noi proiecții cu selector dinamic de săli libere (verifică suprapunerile cu o marjă de siguranță de 30 de minute).
* **Filtrare & Sortare**: Căutare instantanee inline și sortare dinamică ascendentă/descendentă pe headers (`▲` / `▼`).

---

## � Pornire pe Ubuntu (Linux / WSL)

Dacă vrei să rulezi aplicația rapid direct din terminalul Ubuntu, folosește comenzile de mai jos.

### 1. Instalare compilator C++
```bash
sudo apt update
sudo apt install build-essential
```

### 2. Navigarea în folderul proiectului
Înlocuiește calea de mai jos cu cea reală a folderului tău (exemplu pentru WSL):
```bash
cd "/mnt/d/Coduri/An2_S2/Proiect POO/Cinematograf/Cinema"
```

### 3. Compilare (folosind flag-ul `-pthread` necesar pentru server)
```bash
g++ -std=c++17 Bilet.cpp Film.cpp Sala.cpp Suvenir.cpp main.cpp -o Cinema -pthread
```

### 4. Pornire Server
```bash
./Cinema
```

### 5. Deschiderea automată în browser (Modul "Pro") 😎
În timp ce serverul rulează, deschide un **nou tab de terminal** (`Ctrl + Shift + T`), deschide un nou shell (ex: `wsl`) și rulează comanda de mai jos pentru a deschide pagina în Windows:
```bash
cmd.exe /c start http://localhost:8080
```
Sistemul va deschide automat browser-ul tău implicit direct la aplicația Cinematografului! (Dacă folosești Linux nativ, poți rula `xdg-open http://localhost:8080`).

---

## � Structura Principală de Date
* `Filme.txt` - Catalogul global de filme (Metadate).
* `Proiectii.txt` - Planificarea proiecțiilor (ID, Film, Sală, Matrice Locuri, Oră).
* `Suveniruri.txt` - Catalogul magazinului de suveniruri.
* `cos_temp.txt` - Sincronizarea temporară a coșului de cumpărături.
* `istoric.txt` - Istoricul permanent în format JSON al tuturor comenzilor finalizate.
