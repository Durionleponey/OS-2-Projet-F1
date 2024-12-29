#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN64
#include <winsock2.h>
#include <ncurses/curses.h>
#else
#include <ncurses.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "grandPrix.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

typedef struct structProgramOptions {
  const char *pListenAddress;
  int listenPort;
} ProgramOptions;

typedef struct structListener {
  struct sockaddr_in serverAddr;
  u_short serverPort;
  socket_t serverSocket;
} Listener;

typedef struct structClientContext {
  struct sockaddr_in clientAddr;
  u_short clientPort;
  socket_t clientSocket;
} ClientContext;

typedef struct structCarStatus {
  char pName[32];
  int currentLap;
  int segments;
  EventType lastEvent;
  uint32_t startLapTimestamp;
  uint32_t lastEventTS;
  uint32_t lastLapTime;
  uint32_t totalLapsTime;
  int s1Time;
  int s2Time;
  int s3Time;
  int pitTime;
  bool active;
} CarStatus;

typedef struct structSortCarStatus {
  int indice;
  CarStatus *pCarStatus;
} SortCarStatus;

typedef struct structLeaderBoard {
  time_t raceStartTime;
  CarStatus *pCars;
  SortCarStatus *pSortIndices;
  int cars;
  int laps;
  uint32_t lastEventTimestamp;
} LeaderBoard;

typedef struct structAcquireThreadCtx {
  ProgramOptions *pOptions;
  Listener *pListener;
  CarStatus *pCarStatus;
  int returnCode;
} AcquireThreadCtx;

/*--------------------------------------------------------------------------------------------------------------------*/

void printHelp(void) {
  printf("Usage:\n");
}

/*--------------------------------------------------------------------------------------------------------------------*/

int startListening(ProgramOptions *pOptions, Listener *pListener) {
  int optionValue;
  int code;

  memset(&pListener->serverAddr, 0, sizeof(pListener->serverAddr));

  pListener->serverAddr.sin_port = htons(pOptions->listenPort);
  pListener->serverAddr.sin_family = AF_INET;
  pListener->serverAddr.sin_addr.s_addr = INADDR_ANY;

  pListener->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (pListener->serverSocket == INVALID_SOCKET) {
    printf("ERROR: unable to allocate a new socket, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  optionValue = 1;
  code = setsockopt(pListener->serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optionValue, sizeof(optionValue));
  if (code == SOCKET_ERROR) {
    printf("ERROR: unable to set socket option SO_REUSEADDR, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  if (pOptions->pListenAddress != NULL) {
    pListener->serverAddr.sin_addr.s_addr = inet_addr(pOptions->pListenAddress);
    if (pListener->serverAddr.sin_addr.s_addr == INADDR_NONE) {
      printf("ERROR: illegal listening address '%s'\n", pOptions->pListenAddress);
      return RETURN_KO;
    }
  }

  if (bind(pListener->serverSocket, (struct sockaddr *)&pListener->serverAddr, sizeof(pListener->serverAddr)) ==
      SOCKET_ERROR) {
    printf("ERROR: unable to bind listening address '%s', code=%d\n", pOptions->pListenAddress, WSAGetLastError());
    return RETURN_KO;
  }

  if (listen(pListener->serverSocket, 8) == SOCKET_ERROR) {
    printf("ERROR: unable to start listening on address '%s:%d', code=%d\n", pOptions->pListenAddress,
           pOptions->listenPort, WSAGetLastError());
    return RETURN_KO;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int acceptConnection(Listener *pListener, ClientContext *pClientCtx) {
  socket_t clientSocket;
  char pAddrString[64];
  socklen_t addressSize;
#ifdef _WIN64
  DWORD stringSize;
  int code;
#endif

  while (true) {
    addressSize = sizeof(pClientCtx->clientAddr);
    clientSocket = accept(pListener->serverSocket, (struct sockaddr *)&pClientCtx->clientAddr, &addressSize);
    if (clientSocket != INVALID_SOCKET) {
      break;
    }
#ifdef _WIN64
    printf("ERROR: unable to accept new incoming connection, code=%d\n", WSAGetLastError());
    return RETURN_KO;
#endif
#ifdef LINUX
    if (errno != EINTR) {
      printf("ERROR: unable to accept new incoming connection, errno=%d\n", errno);
      return RETURN_KO;
    }
#endif
  }

#ifdef _WIN64
  stringSize = sizeof(pAddrString);
  code = WSAAddressToString((struct sockaddr *)&pClientCtx->clientAddr, sizeof(pClientCtx->clientAddr), NULL,
                            pAddrString, &stringSize);
  if (code == 0) {
    printf("INFO: connection from %s accepted\n", pAddrString);
  }
#else
  inet_ntop(AF_INET, &pClientCtx->clientAddr.sin_addr, pAddrString, sizeof(pAddrString));
//  printf("INFO: connection from %s accepted\n", pAddrString);
#endif

  pClientCtx->clientSocket = clientSocket;

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int readFully(socket_t socket, void *pBuffer, int size) {
  uint8_t *pRecvBuffer;
  int bytesRead;
  int code;

  bytesRead = 0;
  pRecvBuffer = (uint8_t *)pBuffer;
  while (bytesRead < size) {
    code = recv(socket, (char *)pRecvBuffer, size - bytesRead, 0);
    if (code > 0) {
      bytesRead += code;
      pRecvBuffer += code;
    } else if (code == 0) {
      return 0;
    } else {
#ifdef _WIN64
      if (WSAGetLastError() == WSAECONNRESET) {
        return 0;
      }
#endif
      printf("ERROR: unable to read from socket, errno=%d\n", WSAGetLastError());
      return -1;
    }
  }

  return bytesRead;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToHour(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;
  int minute;
  int hour;

  milli = timeMs % 1000;
  timeMs /= 1000;
  second = timeMs % 60;
  timeMs /= 60;
  minute = timeMs % 60;
  hour = timeMs / 60;

  snprintf(pOutput, size, "%d:%02d:%02d.%03d", hour, minute, second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToMinute(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;
  int minute;

  milli = timeMs % 1000;
  timeMs /= 1000;
  second = timeMs % 60;
  minute = timeMs / 60;

  snprintf(pOutput, size, "%d:%02d.%03d", minute, second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToSeconds(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;

  milli = timeMs % 1000;
  second = timeMs / 1000;

  snprintf(pOutput, size, "%d.%03d", second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int compareCarStatus(const void *pLeft, const void *pRight) {
  SortCarStatus *pCarA;
  SortCarStatus *pCarB;
  int compare;

  pCarA = (SortCarStatus *)pLeft;
  pCarB = (SortCarStatus *)pRight;
  compare = pCarB->pCarStatus->segments - pCarA->pCarStatus->segments;
  if (compare == 0) {
    compare = pCarA->pCarStatus->totalLapsTime - pCarB->pCarStatus->totalLapsTime;
    if (compare == 0) {
      compare = pCarA->indice - pCarB->indice;
    }
  }
  return compare;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayLeaderBoard(WINDOW *pWindow, LeaderBoard *pLeaderBoard) {
  char pDisplay[32];
  SortCarStatus *pSortIndices;
  CarStatus *pCars;
  CarStatus *pCar;
  int i;

  pCars = pLeaderBoard->pCars;
  pSortIndices = pLeaderBoard->pSortIndices;
  qsort(pSortIndices, pLeaderBoard->cars, sizeof(SortCarStatus), compareCarStatus);

  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_RED, COLOR_BLACK);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 1, "Pos  Car's name   Lap #    S1 time   S2 time   S3 time   Last lap time   Total time");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < pLeaderBoard->cars; i++) {
    mvwprintw(pWindow, i + 3, 1, "%3d", i + 1);

    pCar = &pCars[pSortIndices[i].indice];
    if (pCar->active == false) {
      wattron(pWindow, COLOR_PAIR(3));
      mvwprintw(pWindow, i + 3, 1, "%.10s", pCar->pName);
      wattroff(pWindow, COLOR_PAIR(3));
      continue;
    }
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, i + 3, 6, "%.10s", pCar->pName);
    wattroff(pWindow, A_BOLD);

    mvwprintw(pWindow, i + 3, 19, "%3d", pCar->currentLap);

    if (pCar->lastEvent == event_START || pCar->lastEvent == event_S3) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 25, "%9s", timestampToSeconds(pCar->s1Time, pDisplay, sizeof(pDisplay)));
    wattroff(pWindow, COLOR_PAIR(2));
    if (pCar->lastEvent == event_S1) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 35, "%9s", timestampToSeconds(pCar->s2Time, pDisplay, sizeof(pDisplay)));
    wattroff(pWindow, COLOR_PAIR(2));
    if (pCar->lastEvent == event_S2) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 45, "%9s", timestampToSeconds(pCar->s3Time, pDisplay, sizeof(pDisplay)));
    wattroff(pWindow, COLOR_PAIR(2));

    mvwprintw(pWindow, i + 3, 59, "%10s", timestampToMinute(pCar->lastLapTime, pDisplay, sizeof(pDisplay)));
    mvwprintw(pWindow, i + 3, 70, "%15s", timestampToHour(pCar->totalLapsTime, pDisplay, sizeof(pDisplay)));
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int processEvent(AcquireThreadCtx *pCtx, EventRace *pEvent) {
  CarStatus *pCar;

  assert(pEvent->car >= 0 && pEvent->car < MAX_CARS);

  pCar = &pCtx->pCarStatus[pEvent->car];
  if (pCar->active == false) {
    printf("WARN: an event for inactive car '%s' was received\n", pCar->pName);
    return RETURN_OK;
  }

  switch (pEvent->event) {
  case event_ERROR:
    printf("ERROR: an illegal event was received\n");
    return RETURN_KO;
  case event_START:
    pCar->currentLap = 0;
    break;
  case event_S1:
    pCar->s1Time = pEvent->timestamp - pCar->lastEventTS;
    pCar->s2Time = 0;
    pCar->totalLapsTime += pCar->s1Time;
    pCar->segments++;
    break;
  case event_S2:
    pCar->s2Time = pEvent->timestamp - pCar->lastEventTS;
    pCar->s3Time = 0;
    pCar->totalLapsTime += pCar->s2Time;
    pCar->segments++;
    break;
  case event_S3:
    pCar->s3Time = pEvent->timestamp - pCar->lastEventTS;
    pCar->lastLapTime = pEvent->timestamp - pCar->startLapTimestamp;
    pCar->s1Time = 0;
    pCar->currentLap = pEvent->lap + 1;
    pCar->startLapTimestamp = pEvent->timestamp;
    pCar->totalLapsTime += pCar->s3Time;
    pCar->segments++;
    break;
  case event_OUT:
    pCar->active = false;
    break;
  case event_END:
    break;
  case event_PIT_START:
    printf("INFO: should be implemented\n");
    break;
  case event_PIT_END:
    pCar->pitTime = pEvent->timestamp - pCar->lastEventTS;
    break;
  }

  pCar->lastEvent = pEvent->event;
  pCar->lastEventTS = pEvent->timestamp;

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void *acquireData(void *pThreadArg) {
  AcquireThreadCtx *pCtx;
  ClientContext clientCtx;
  EventRace event;
  int code;

  pCtx = (AcquireThreadCtx *)pThreadArg;

  printf("INFO: waiting for new connection.\n");
  code = acceptConnection(pCtx->pListener, &clientCtx);
  if (code) {
    pCtx->returnCode = code;
    goto acquireDataException;
  }

  while (true) {
    code = readFully(clientCtx.clientSocket, &event, sizeof(event));
    if (code <= 0) {
      break;
    }

    processEvent(pCtx, &event);
    //    printEvent(&event);
  }

acquireDataException:
  closesocket(clientCtx.clientSocket);

  return pThreadArg;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayResultCore(ProgramOptions *pOptions) {
  AcquireThreadCtx acquireThreadCtx;
  Listener listener;
  CarStatus pCarStatus[MAX_CARS];
  LeaderBoard leaderBoard;
  WINDOW *pLeaderWindow;
  SortCarStatus *pSortCars;
  pthread_t threadId;
  int returnCode;
  int code;
  int i;

#ifdef _WIN64
  WSADATA info;

  if (WSAStartup(MAKEWORD(1, 1), &info) != 0) {
    printf("ERROR: unable to initialize Windows Socket API, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }
#endif

  pSortCars = (SortCarStatus *)malloc(sizeof(SortCarStatus) * MAX_CARS);
  if (pSortCars == NULL) {
    printf("ERROR: unable to allocate memory to sort cars\n");
    returnCode = RETURN_KO;
    goto displayResultCoreExit;
  }

  memset(pCarStatus, 0, sizeof(pCarStatus));
  for (i = 0; i < MAX_CARS; i++) {
    sprintf(pCarStatus[i].pName, "CAR #%02d", i);
    pCarStatus[i].active = true;
  }

  code = startListening(pOptions, &listener);
  if (code) {
    returnCode = code;
    goto displayResultCoreExit1;
  }

  acquireThreadCtx.pOptions = pOptions;
  acquireThreadCtx.pListener = &listener;
  acquireThreadCtx.pCarStatus = pCarStatus;
  code = pthread_create(&threadId, NULL, acquireData, &acquireThreadCtx);
  if (code) {
    printf("ERROR: unable to create new thread to capture data, code=%d\n", errno);
    returnCode = RETURN_KO;
    goto displayResultCoreExit1;
  }

  printf("INFO: waiting data...\n");

  leaderBoard.cars = 20;
  leaderBoard.pCars = pCarStatus;
  leaderBoard.lastEventTimestamp = 0;
  leaderBoard.raceStartTime = 0;
  leaderBoard.pSortIndices = pSortCars;
  for (i = 0; i < MAX_CARS; i++) {
    pSortCars[i].indice = i;
    pSortCars[i].pCarStatus = &pCarStatus[i];
  }

  initscr();
  if (has_colors() == FALSE) {
    endwin();
    printf("Your terminal does not support color\n");
    exit(1);
  }
  start_color();
  pLeaderWindow = newwin(24, 87, (LINES - 24) / 2, (COLS - 87) / 2);
  box(pLeaderWindow, ACS_VLINE, ACS_HLINE);

  while (true) {
    sleep(1);
    displayLeaderBoard(pLeaderWindow, &leaderBoard);
  }

  delwin(pLeaderWindow);
  endwin();

  code = pthread_join(threadId, NULL);
  if (code) {
    printf("ERROR: unable to join acquisition thread, code=%d\n", errno);
    returnCode = RETURN_KO;
    goto displayResultCoreExit1;
  }

  returnCode = acquireThreadCtx.returnCode;

displayResultCoreExit1:
  free((void *)pSortCars);

displayResultCoreExit:
#ifdef _WIN64
  WSACleanup();
#endif
  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  ProgramOptions options;
  int code;
  int opt;

  memset(&options, 0, sizeof(options));

  while ((opt = getopt(argc, ppArgv, "p:l:h?")) != -1) {
    switch (opt) {
    case 'p':
      options.listenPort = atoi(optarg);
      break;
    case 'l':
      options.pListenAddress = optarg;
      break;
    case 'h':
    case '?':
    default:
      printHelp();
      return EXIT_SUCCESS;
    }
  }

  if (options.listenPort == 0) {
    printf("ERROR: unspecified listening port. Please use option -p.\n");
    return EXIT_FAILURE;
  }

  code = displayResultCore(&options);
  if (code) {
    printf("ERROR: an error was encounterred during execution of displayResultCore(), code=%d\n", code);
  }

  return EXIT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------------------*/
