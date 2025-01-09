# F1 Grand Prix Simulation - README

## Introduction

Bienvenue dans le projet de simulation de course de Formule 1. Cette application a été conçue en langage C dans le cadre d'un projet académique visant à explorer les aspects techniques de la programmation système. L'application permet de simuler des courses de F1 en temps réel en utilisant des sockets pour la communication entre machines et des fichiers CSV pour la gestion des données. Elle est compatible avec plusieurs systèmes d'exploitation, notamment Windows, Linux et MacOS.

## Fonctionnalités principales

1. **Simulation de courses :**
   - Génération aléatoire des performances des pilotes basée sur des paramètres prédéfinis.
   - Gestion des incidents en course pour un réalisme accru.

2. **Communication inter-machines :**
   - Utilisation de sockets pour permettre une exécution distribuée.

3. **Gestion des données :**
   - Lecture et écriture de fichiers CSV contenant les détails des pilotes et les résultats des courses.

4. **Affichage des résultats :**
   - Interface textuelle pour présenter les classements de manière claire.

5. **Compatibilité multi-plateforme :**
   - Adaptations pour les environnements Windows, Linux et MacOS.

## Installation

### Prérequis

- Un compilateur C compatible (GCC, Clang, MinGW, etc.).
- CMake pour gérer la configuration du projet.
- Bibliothèques NCurses (également disponibles sous Windows via MSys2).

### Instructions pour Windows

1. Installez **MSys2** et assurez-vous que l'environnement `ucrt64` est configuré.
2. Installez les paquets nécessaires :
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-toolchain
   pacman -S mingw-w64-ucrt-x86_64-cmake
   pacman -S mingw-w64-ucrt-x86_64-ncurses
   ```
3. Clonez le dépôt :
   ```bash
   git clone https://github.com/username/grandPrix.git
   ```
4. Accédez au dossier et configurez le projet :
   ```bash
   cd grandPrix
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   ```
5. Compilez le projet :
   ```bash
   cmake --build .
   ```

### Instructions pour Linux/MacOS

1. Assurez-vous que GCC ou Clang est installé :
   ```bash
   sudo apt-get install build-essential
   sudo apt-get install libncurses5-dev libncursesw5-dev
   ```
2. Clonez le dépôt :
   ```bash
   git clone https://github.com/username/grandPrix.git
   ```
3. Configurez et compilez le projet :
   ```bash
   cd grandPrix
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make
   ```

## Utilisation

### Exécution de l'application

1. Lancez l'exécutable depuis le répertoire `build` :
   ```bash
   ./grandPrix
   ```

2. Suivez les instructions affichées dans le terminal pour :
   - Charger les fichiers CSV des pilotes et des courses.
   - Lancer une simulation.
   - Afficher les résultats.

### Structure des fichiers CSV

- **Pilotes (Drivers.csv)** :
  - Contient les informations sur chaque pilote : Nom, Équipe, Nationalité, etc.
- **Courses (F1_Grand_Prix_2024.csv)** :
  - Contient les détails des circuits et des paramètres de course.

Exemple de format :
```
Nom,Equipe,Nationalité
Max Verstappen,Red Bull Racing,Pays-Bas
Lewis Hamilton,Mercedes,Grande-Bretagne
```

## Problèmes connus

- Des différences de compatibilité entre Windows et Linux peuvent occasionner des erreurs de compilation.
- Les fonctions aléatoires peuvent ne pas fournir une distribution optimale, ce qui pourrait affecter les résultats des simulations.

## Améliorations futures

- Ajout d'une interface graphique pour une meilleure expérience utilisateur.
- Gestion avancée des erreurs pour une plus grande robustesse.
- Possibilité de rejouer des courses précédentes et de comparer les performances.

## Contribution

Les contributions sont les bienvenues. Pour signaler un bug ou proposer une fonctionnalité, veuillez créer une issue ou soumettre une pull request via GitHub.

## Auteurs

- Nathan Colson 
- Nathan Brico
- Clarembaux Robin
- Jones Cyan

## Licence

Ce projet est sous licence MIT. Consultez le fichier `LICENSE` pour plus d'informations.

