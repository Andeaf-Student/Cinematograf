# 🎬 Proiect POO – Cinematograf (C++)

## 📌 Descriere

Acest proiect reprezintă o aplicație de tip **cinematograf** realizată în C++, folosind concepte de programare orientată pe obiecte:

* clase: `Film`, `Sala`, (și extensibil `Rezervare`)
* relații între obiecte
* vectori de obiecte
* tratarea excepțiilor

Aplicația rulează în **consolă (terminal)** și este compatibilă cu **Linux (Ubuntu)**.


---

## 🐧 Instalare și configurare Ubuntu (WSL)

### 1. Instalare
Ubuntu poate fi instalat din Microsoft Store.
Daca este prima data cand folosesti Ubuntu, asigura-te ca faci urmatorii pasi:

Verifică dacă ai Hyper-V / Virtual Machine Platform activ:

Apasă Win + R
Scrie:
optionalfeatures.exe
Bifează:
✔ Virtual Machine Platform
✔ Windows Subsystem for Linux

---

### 2. Instalare Ubuntu

În PowerShell:

```powershell
wsl --install -d Ubuntu
```

La prima rulare:

* introdu un username
* introdu o parolă

---

### 3. Instalare compilator C++

În terminalul Ubuntu:

```bash
sudo apt update
sudo apt install g++
```

Verificare:

```bash
g++ --version
```

---

## 📂 Accesarea proiectului din Windows

Verifica unde ai salvat folderul proiectului.
Exemplu: D:\Coduri\An2_S2\Proiect POO\Cinematograf\Cinema

În WSL, acesta devine:

```bash
/mnt/"*calea proiectului*"
Exemplu: /mnt/d/Coduri/An2_S2/Proiect\ POO/Cinematograf/Cinema
```

---

## ▶️ Rulare proiect

### 1. Navigare în folder

```bash
cd /mnt/d/Coduri/An2_S2/Proiect\ POO/Cinematograf/Cinema
```

---

### 2. Verificare fișiere

```bash
ls
```

Trebuie să existe fișiere precum:

```
main.cpp
Film.cpp
Film.h
Sala.cpp
Sala.h
```

---

### 3. Compilare

```bash
g++ *.cpp -o cinematograf
```

---

### 4. Rulare

```bash
./cinematograf
```

---

## 🧠 Observații importante

* Nu se folosesc funcții specifice Windows (ex: `system("cls")`)
* Pentru Linux se poate folosi:

```cpp
system("clear");
```

* Proiectul folosește:

  * `vector` pentru colecții de obiecte
  * `try/catch` pentru gestionarea erorilor (loc ocupat, index invalid)

---

## 🚀 Funcționalități

* Afișare listă filme
* Vizualizare locuri din sală (liber/ocupat)
* Rezervare loc
* Validare și tratare erori
* (Opțional) calcul preț pe rând + confirmare rezervare

---

## 📌 Concluzie

Proiectul este complet compatibil cu Linux și poate fi compilat și rulat folosind `g++` în terminal, fără dependențe de Visual Studio sau Windows.

---


