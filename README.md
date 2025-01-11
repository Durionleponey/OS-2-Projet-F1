# Linux et MacOS

(Le programme est déjà compilé)

(Re)Compiler le programme:


1. Assurez-vous que GCC ou Clang est installé :
   ```bash
   sudo apt-get install build-essential
   sudo apt-get install libncurses5-dev libncursesw5-dev
   ```
2. Clonez le dépôt :
   ```bash
   git clone https://github.com/NathanColson/grandPrix.git
   ```
3. Configurez et compilez le projet :
   ```bash
   cd grandPrix
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make
   ```

Pour démarrer l'applcation:

cd build
./grandPrix -a

OU

Ouvrir 2 terminaux

## Server side:
cd build
./grandPrix

## Client side:
cd build
./genTime -c 1 -t P1 -s 127.0.0.1 -p 1111 -l 57




 # (WINDOWS)


# Installer MSys64:


```
ucrt64/mingw-w64-ucrt-x86_64-toolchain
ucrt64/mingw-w64-ucrt-x86_64-cmake
ucrt64/mingw-w64-ucrt-x86_64-ncurses
```

## Server side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./grandPrix

## Client side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./genTime -c 1 -t P1 -s 127.0.0.1 -p 1111 -l 57
