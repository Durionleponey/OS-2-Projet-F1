

# On Windows with MSys64:

To compile the application under Windows (10+), install msys64. By default msys64 uses ucrt64 tool chain.
Install new packages using pacman tool (pacman -S [package name])

```
ucrt64/mingw-w64-ucrt-x86_64-toolchain
ucrt64/mingw-w64-ucrt-x86_64-cmake
ucrt64/mingw-w64-ucrt-x86_64-ncurses
```


# On Raspberry PI:


sudo apt-get install libncurses5-dev libncursesw5-dev


pi@raspberrypi:~/grandPrix $ cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -S. -B./build -G "Unix Makefiles"
pi@raspberrypi:~/grandPrix $ cd build
pi@raspberrypi:~/grandPrix/build $ make



# To run the application:

## Server side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./grandPrix

## Client side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./genTime -c 1 -t P1 -s 127.0.0.1 -p 1111 -l 57


## For automatic launch:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ export PATH=$PATH:$PWD
$ ./grandPrix -a
