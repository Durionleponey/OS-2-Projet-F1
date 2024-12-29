cmake_minimum_required(VERSION 3.25)
project(F1master C)

set(CMAKE_C_STANDARD 11)

set(GCC_ENABLE_WARNINGS "-Wall -Wuninitialized -Wreturn-type -Wstrict-prototypes -Wwrite-strings -Wwrite-strings")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_ENABLE_WARNINGS}")


link_libraries(ws2_32)
link_libraries(ncursesw)
link_libraries(pthread)

add_executable(genTime
        genTime.c grandPrix.h
        util.c util.h)

add_executable(displayResult
        displayResult.c grandPrix.h
        util.c util.h)
