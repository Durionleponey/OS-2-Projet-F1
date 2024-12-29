




sudo apt-get install libncurses5-dev libncursesw5-dev


pi@raspberrypi:~/grandPrix $ cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -S/home/pi/grandPrix -B/home/pi/grandPrix/build -G "Unix Makefiles"
pi@raspberrypi:~/grandPrix $ cd build
pi@raspberrypi:~/grandPrix/build $ make



To run the application:

Server side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./displayResult -p 1111

Client side:
UCRT64 /c/Users/DP69SA/cpp/workspace/grandPrix/build
$ ./genTime -c 10 -t GP -s 127.0.0.1 -p 1111 -l 2
