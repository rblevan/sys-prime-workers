# ⚙️ Sys-Prime-Workers

> **Calcul distribué de nombres premiers via architecture Master/Worker.**

![C](https://img.shields.io/badge/Language-C99-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-orange)
![IPC](https://img.shields.io/badge/IPC-Pipes%20%26%20Semaphores-green)
![Build](https://img.shields.io/badge/Build-Makefile-lightgrey)

## 🎓 Contexte
Ce projet a été réalisé dans le cadre de l'UE de **Programmation Système** (Licence 3 Informatique).
L'objectif est de concevoir une application multitâche capable de répartir une lourde charge de calcul (tests de primalité) sur plusieurs processus "travailleurs" (Workers), orchestrés par un processus central (Master).

Le projet met en œuvre les mécanismes bas niveau de communication inter-processus (IPC).

Il a été réalisé en duo avec un camarade : Antoine GIRET (je n'ai pas son lien GitHub).

---

## 🏗️ Architecture Technique

Le système repose sur trois types de processus distincts :

### 1. 🧠 Le Master (`master.c`)
Le chef d'orchestre. Il est responsable de :
* Créer et gérer les mécanismes de communication (Tubes nommés / Semaphores).
* Recevoir les requêtes des clients.
* Dispatcher le travail aux Workers disponibles.
* Agréger les résultats et répondre aux clients.

### 2. 👷 Les Workers (`worker.c`)
Les unités de calcul.
* Ils attendent les ordres du Master.
* Ils effectuent les tests de primalité (algorithme brut ou optimisé).
* Ils renvoient le résultat (Vrai/Faux ou liste de nombres).

### 3. 💻 Le Client (`client.c`)
L'interface utilisateur.
* Permet à l'utilisateur d'envoyer une commande (ex: "Est-ce que 105943 est premier ?").
* Affiche la réponse du système.

---

## 🔧 Mécanismes Système Utilisés

Ce projet démontre la maîtrise des appels systèmes UNIX/Linux :
* **Gestion des processus :** `fork()`, `wait()`, gestion des zombies.
* **Communication (IPC) :** Tubes nommés (Named Pipes / FIFOs) pour l'échange de données.
* **Synchronisation :** Sémaphores pour protéger les sections critiques et coordonner les accès aux tubes.
* **Signaux :** Gestion propre de l'arrêt (SIGINT) pour nettoyer les ressources.

---

## 🚀 Installation et Compilation

### Prérequis
* Un environnement Linux/Unix.
* Compilateur `gcc`.
* `make`.

### Compilation
Le projet dispose d'un `Makefile` pour automatiser la compilation.

```bash
# Aller dans le dossier source
cd src

# Compiler tout le projet
make
