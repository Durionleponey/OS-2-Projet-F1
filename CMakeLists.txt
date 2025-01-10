cmake_minimum_required(VERSION 3.25)
project(F1master C)

set(CMAKE_C_STANDARD 11)

set(GCC_ENABLE_WARNINGS "-Wall -Wuninitialized -Wreturn-type -Wstrict-prototypes -Wwrite-strings -Wwrite-strings")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_ENABLE_WARNINGS}")

include_directories(include)

if(CMAKE_SYSTEM MATCHES Windows)
  message(STATUS "Target system is Windows")

  add_compile_definitions(WIN64)

  link_libraries(ws2_32)
  link_libraries(ncursesw)
endif()

if(CMAKE_HOST_SYSTEM MATCHES Linux)
  message(STATUS "Build host runs Linux")

  add_compile_definitions(LINUX)
  add_compile_definitions(_GNU_SOURCE)

  link_libraries(ncurses)
endif()

link_libraries(pthread)

add_executable(genTime
        genTime.c include/grandPrix.h
        util.c include/util.h)

add_executable(testCsvParser
        testCsvParser.c
        csvParser.c include/csvParser.h)

add_executable(grandPrix
        grandPrix.c include/grandPrix.h include/util.h
        saveFile.c include/saveFile.h
        csvParser.c include/csvParser.h
        util.c include/util.h)
