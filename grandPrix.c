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
#include <sys\stat.h>

#ifdef WIN64
#include <winsock2.h>
#include <ncurses/curses.h>
#endif

#ifdef LINUX
#include <ncurses.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "grandPrix.h"
#include "util.h"
#include "csvParser.h"

/*--------------------------------------------------------------------------------------------------------------------*/

#define GRANDPRIX_FILENAME "F1_Grand_Prix_2024.csv"
#define DRIVERS_FILENAME "Drivers.csv"

static const int pSprintScores[] = {8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const int pGrandPrixScores[] = {25, 20, 15, 10, 8, 6, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*--------------------------------------------------------------------------------------------------------------------*/

typedef struct structProgramOptions {
  const char *pListenAddress;
  int listenPort;
  int gpYear;
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

typedef struct structContext {
  CsvRow **ppCsvGrandPrix;
  CsvRow **ppCsvDrivers;
  StandingsTable standingsTable;
  GrandPrix *pGrandPrix;
  int gpHistoricHandle;
  int currentGP;
  int gpYear;
  Listener listener;
} Context;

typedef struct structMenuItem {
  const char *pItem;
  int (*pMenuAction)(Context *pCtx, int choice, void *pUserData);
} MenuItem;

typedef struct structDisplayMenuContext {
  WINDOW *pWindow;
} DisplayMenuContext;

typedef struct structCarStatus {
  int cardId;
  int currentLap;
  int segments;
  EventType lastEvent;
  uint32_t startLapTimestamp;
  uint32_t lastEventTS;
  uint32_t lastLapTime;
  uint32_t bestLapTime;
  uint32_t totalLapsTime;
  int bestLap;
  int bestS1Time;
  int bestS2Time;
  int bestS3Time;
  int s1Time;
  int s2Time;
  int s3Time;
  int pitTime;
  bool active;
} CarStatus;

typedef struct structLeaderBoard {
  int grandPrixId;
  RaceType type;
  time_t raceStartTime;
  CarStatus *pCars;
  int *pSortIndices;
  int cars;
  int laps;
  uint32_t lastEventTimestamp;
} LeaderBoard;

typedef struct structAcquireThreadCtx {
  Context *pCtx;
  ClientContext *pClientCtx;
  CarStatus *pCarStatus;
  bool threadStillAlive;
  int returnCode;
} AcquireThreadCtx;

/*--------------------------------------------------------------------------------------------------------------------*/

int displayListGPs(Context *pCtx, int choice, void *pUserData);
int displayListDrivers(Context *pCtx, int choice, void *pUserData);
int displayStandings(Context *pCtx, int choice, void *pUserData);
int captureEvents(Context *pCtx, int choice, void *pUserData);

/*--------------------------------------------------------------------------------------------------------------------*/

// clang-format off
static MenuItem pDisplayMainMenu[] = {
  { "Afficher liste des Grand Prix", displayListGPs },
  { "Afficher liste des pilotes", displayListDrivers },
  { "Afficher des resultats de courses terminees", NULL },
  { "Afficher le classement general", displayStandings },
  { "Lancer la capture de l'etape suivante", captureEvents },
  { "Quitter du programme", NULL },
  { NULL, NULL }
};

static MenuItem pDisplayGPMenu[] = {
  { "Afficher les Practices 1", NULL },
  { "Afficher les Practices 2", NULL },
  { "Afficher les Practices 3", NULL },
  { "Afficher les Qualifications 1", NULL },
  { "Afficher les Qualifications 2", NULL },
  { "Afficher les Qualifications 3", NULL },
  { "Afficher le Grand Prix", NULL },
  { NULL, NULL }
};

static MenuItem pDisplayGPSpecialMenu[] = {
  { "Afficher les Practices 1", NULL },
  { "Afficher les Sprint Qualifications 1", NULL },
  { "Afficher les Sprint Qualifications 2", NULL },
  { "Afficher les Sprint Qualifications 3", NULL },
  { "Afficher le Sprint", NULL },
  { "Afficher les Qualifications 1", NULL },
  { "Afficher les Qualifications 2", NULL },
  { "Afficher les Qualifications 3", NULL },
  { "Afficher le Grand Prix", NULL },
  { NULL, NULL }
};
// clang-format on

/*--------------------------------------------------------------------------------------------------------------------*/

void printHelp(void) {
  printf("Usage: [OPTIONS]\n");
  printf("Options:\n");
  printf("  -l <address>      Specify the listen address. Default: 127.0.0.1\n");
  printf("  -p <port>         Specify the listen port. Default: 1111\n");
  printf("  -y <year>         Specify the GP year. Default: 2025\n");
  printf("  -h, -?            Display this help message.\n");
  printf("\nExample:\n");
  printf("  ./program -l 192.168.1.1 -p 8080 -y 2024\n");
}

/*--------------------------------------------------------------------------------------------------------------------*/

int readConfiguration(Context *pCtx) {
  CsvParser *pCsvParser;
  CsvRow **ppCsvRowArray;
  CsvRow *pCsvRow;
  int i;

  ppCsvRowArray = malloc(sizeof(CsvRow *) * MAX_GP);
  if (ppCsvRowArray == NULL) {
    logger(log_FATAL, "unable to allocate %d bytes for GrandPrix table.\n", sizeof(CsvRow *) * MAX_GP);
    return RETURN_KO;
  }

  pCsvParser = csvParserCreate(GRANDPRIX_FILENAME, NULL, true);
  if (pCsvParser == NULL) {
    logger(log_ERROR, "unable to create new parser for '%s'.\n", GRANDPRIX_FILENAME);
    return RETURN_KO;
  }

  for (i = 0; i < MAX_GP; i++) {
    pCsvRow = csvParserGetRow(pCsvParser);
    if (pCsvRow == NULL) {
      logger(log_ERROR, "file %s contains not enough lines (%d < %d).\n", GRANDPRIX_FILENAME, i, MAX_GP);
      return RETURN_KO;
    }
    ppCsvRowArray[i] = pCsvRow;
  }
  pCtx->ppCsvGrandPrix = ppCsvRowArray;

  csvParserDestroy(pCsvParser);

  ppCsvRowArray = malloc(sizeof(CsvRow *) * MAX_DRIVERS);
  if (ppCsvRowArray == NULL) {
    logger(log_FATAL, "unable to allocate %d bytes for Drivers table.\n", sizeof(CsvRow *) * MAX_DRIVERS);
    return RETURN_KO;
  }

  pCsvParser = csvParserCreate(DRIVERS_FILENAME, NULL, false);
  if (pCsvParser == NULL) {
    logger(log_ERROR, "unable to create new parser for '%s'.\n", DRIVERS_FILENAME);
    return RETURN_KO;
  }

  for (i = 0; i < MAX_DRIVERS; i++) {
    pCsvRow = csvParserGetRow(pCsvParser);
    if (pCsvRow == NULL) {
      logger(log_ERROR, "file %s contains not enough lines (%d < %d).\n", DRIVERS_FILENAME, i, MAX_DRIVERS);
      return RETURN_KO;
    }
    ppCsvRowArray[i] = pCsvRow;
  }
  pCtx->ppCsvDrivers = ppCsvRowArray;

  csvParserDestroy(pCsvParser);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void freeConfiguration(Context *pCtx) {
  free((void *)pCtx->ppCsvGrandPrix);
  free((void *)pCtx->ppCsvDrivers);
}

/*--------------------------------------------------------------------------------------------------------------------*/

void initializeGP(Context *pCtx, int grandPrixId, GrandPrix *pGrandPrix) {
  pGrandPrix->grandPrixId = grandPrixId;
  pGrandPrix->specialGP = strcasecmp(pCtx->ppCsvGrandPrix[grandPrixId]->ppFields[3], "True") == 0;
  pGrandPrix->nextStep = race_P1;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int compareStandingsTableItem(const void *pA, const void *pB) {
  return ((StandingsTableItem *)pA)->points - ((StandingsTableItem *)pB)->points;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int startListening(Context *pCtx, ProgramOptions *pOptions) {
  Listener *pListener;
  int optionValue;
  int code;

#ifdef WIN64
  WSADATA info;

  if (WSAStartup(MAKEWORD(1, 1), &info) != 0) {
    logger(log_ERROR, "unable to initialize Windows Socket API, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }
#endif

  pListener = &pCtx->listener;
  memset(&pListener->serverAddr, 0, sizeof(pListener->serverAddr));

  pListener->serverAddr.sin_port = htons(pOptions->listenPort);
  pListener->serverAddr.sin_family = AF_INET;
  pListener->serverAddr.sin_addr.s_addr = INADDR_ANY;

  pListener->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (pListener->serverSocket == INVALID_SOCKET) {
    logger(log_ERROR, "unable to allocate a new socket, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  optionValue = 1;
  code = setsockopt(pListener->serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optionValue, sizeof(optionValue));
  if (code == SOCKET_ERROR) {
    logger(log_ERROR, "unable to set socket option SO_REUSEADDR, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  if (pOptions->pListenAddress != NULL) {
    pListener->serverAddr.sin_addr.s_addr = inet_addr(pOptions->pListenAddress);
    if (pListener->serverAddr.sin_addr.s_addr == INADDR_NONE) {
      logger(log_ERROR, "illegal listening address '%s'\n", pOptions->pListenAddress);
      return RETURN_KO;
    }
  }

  if (bind(pListener->serverSocket, (struct sockaddr *)&pListener->serverAddr, sizeof(pListener->serverAddr)) ==
      SOCKET_ERROR) {
    logger(log_ERROR, "unable to bind listening address '%s', code=%d\n", pOptions->pListenAddress, WSAGetLastError());
    return RETURN_KO;
  }

  if (listen(pListener->serverSocket, 8) == SOCKET_ERROR) {
    logger(log_ERROR, "unable to start listening on address '%s:%d', code=%d\n", pOptions->pListenAddress,
           pOptions->listenPort, WSAGetLastError());
    return RETURN_KO;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void stopListening(Context *pCtx) {
  closesocket(pCtx->listener.serverSocket);

#ifdef WIN64
  WSACleanup();
#endif
}

/*--------------------------------------------------------------------------------------------------------------------*/

int acceptConnection(Listener *pListener, ClientContext *pClientCtx) {
  socket_t clientSocket;
  char pAddrString[64];
  socklen_t addressSize;
#ifdef WIN64
  DWORD stringSize;
  int code;
#endif

  while (true) {
    addressSize = sizeof(pClientCtx->clientAddr);
    clientSocket = accept(pListener->serverSocket, (struct sockaddr *)&pClientCtx->clientAddr, &addressSize);
    if (clientSocket != INVALID_SOCKET) {
      break;
    }
#ifdef WIN64
    logger(log_ERROR, "unable to accept new incoming connection, code=%d\n", WSAGetLastError());
    return RETURN_KO;
#endif
#ifdef LINUX
    if (errno != EINTR) {
      logger(log_ERROR, "unable to accept new incoming connection, errno=%d\n", errno);
      return RETURN_KO;
    }
#endif
  }

#ifdef WIN64
  stringSize = sizeof(pAddrString);
  code = WSAAddressToString((struct sockaddr *)&pClientCtx->clientAddr, sizeof(pClientCtx->clientAddr), NULL,
                            pAddrString, &stringSize);
  if (code == 0) {
    logger(log_INFO, "connection from %s accepted\n", pAddrString);
  }
#endif
#ifdef LINUX
  inet_ntop(AF_INET, &pClientCtx->clientAddr.sin_addr, pAddrString, sizeof(pAddrString));
  logger(log_INFO, "connection from %s accepted\n", pAddrString);
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
#ifdef WIN64
      if (WSAGetLastError() == WSAECONNRESET) {
        return 0;
      }
#endif
      logger(log_ERROR, "unable to read from socket, errno=%d\n", WSAGetLastError());
      return -1;
    }
  }

  return bytesRead;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int readHistoric(Context *pCtx) {
  char pFileName[MAX_PATH];
  GrandPrix *pGrandPrix;
  StandingsTableItem *pStandingsTableItem;
  SprintItem *pSprintItem;
  RaceItem *pRaceItem;
  int fileHandle;
  int code;
  int car;
  int i;

  pGrandPrix = (GrandPrix *)calloc(MAX_GP, sizeof(GrandPrix));
  if (pGrandPrix == NULL) {
    logger(log_FATAL, "unable to allocate %d bytes for GrandPrix table.\n", sizeof(GrandPrix *) * MAX_GP);
    return RETURN_KO;
  }

  sprintf(pFileName, "GrandPrix.%d.bin", pCtx->gpYear);
  fileHandle = open(pFileName, O_CREAT | O_BINARY | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fileHandle == -1) {
    logger(log_ERROR, "unable to open file %s, errno=%d\n", pFileName, errno);
    return RETURN_KO;
  }

  for (i = 0; i < MAX_GP; i++) {
    code = read(fileHandle, &pGrandPrix[i], sizeof(GrandPrix));
    if (code == 0) {
      break;
    }
    if (code != sizeof(GrandPrix)) {
      close(fileHandle);
      logger(log_ERROR, "an error has occurred while reading file %s\n", pFileName);
      return RETURN_KO;
    }
  }

  if (i == 0) {
    initializeGP(pCtx, 0, &pGrandPrix[0]);
    pCtx->currentGP = i;
  } else {
    if (pGrandPrix[i - 1].nextStep == race_FINISHED) {
      if (i == MAX_GP) {
        logger(log_INFO, "The race for year %d is finished.\n", pCtx->gpYear);
        pCtx->currentGP = i - 1;
      } else {
        initializeGP(pCtx, i, &pGrandPrix[i]);
        pCtx->currentGP = i;
      }
    } else {
      pCtx->currentGP = i - 1;
    }
  }

  pCtx->pGrandPrix = pGrandPrix;
  pCtx->gpHistoricHandle = fileHandle;

  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_DRIVERS; i++) {
    pStandingsTableItem->carId = i;
    pStandingsTableItem->points = 0;
    pStandingsTableItem++;
  }

  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_GP; i++) {
    if (pGrandPrix->specialGP && pGrandPrix->nextStep > race_SPRINT) {
      pSprintItem = (SprintItem *)pGrandPrix->sprint.pItems;
      for (car = 0; car < MAX_DRIVERS; car++) {
        pStandingsTableItem[pSprintItem->carId].points += pSprintScores[car];
        pSprintItem++;
      }
    }
    if (pGrandPrix->nextStep > race_GP) {
      pRaceItem = (RaceItem *)pGrandPrix->race.pItems;
      for (car = 0; car < MAX_DRIVERS; car++) {
        pStandingsTableItem[pRaceItem->carId].points += pGrandPrixScores[car];
        pRaceItem++;
      }
    }
    pGrandPrix++;
  }
  qsort(&pCtx->standingsTable.pItems, MAX_DRIVERS, sizeof(StandingsTableItem), compareStandingsTableItem);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int saveHistoric(Context *pCtx) {
  int code;

  lseek(pCtx->gpHistoricHandle, pCtx->currentGP * sizeof(GrandPrix), SEEK_SET);
  code = write(pCtx->gpHistoricHandle, &pCtx->pGrandPrix[pCtx->currentGP], sizeof(GrandPrix));
  if (code != (int)sizeof(GrandPrix)) {
    logger(log_ERROR, "an error has occurred while writing to historic file\n");
    return RETURN_KO;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void fillHistoricPractice(Practice *pPractice, RaceType type, LeaderBoard *pLeaderBoard) {
  PracticeItem *pItems;
  CarStatus *pCars;
  int *pSortIndices;
  int order;
  int i;

  pSortIndices = pLeaderBoard->pSortIndices;
  pPractice->type = type;
  pItems = pPractice->pItems;
  pCars = pLeaderBoard->pCars;
  for (i = 0; i < MAX_DRIVERS; i++) {
    order = *pSortIndices++;
    pCars = &pLeaderBoard->pCars[order];
    pItems->carId = pCars->cardId;
    pItems->bestLapTime = pCars->bestLapTime;
    pItems->bestLap = pCars->bestLap;
    pItems->bestS1 = pCars->bestS1Time;
    pItems->bestS2 = pCars->bestS2Time;
    pItems->bestS3 = pCars->bestS3Time;
    pItems++;
  }
}

/*--------------------------------------------------------------------------------------------------------------------*/

int fillHistoric(Context *pCtx, LeaderBoard *pLeaderBoard) {
  GrandPrix *pGrandPrix;
  int currentGP;
  bool specialGP;

  currentGP = pCtx->currentGP;
  pGrandPrix = &pCtx->pGrandPrix[currentGP];
  specialGP = strcasecmp(pCtx->ppCsvGrandPrix[currentGP]->ppFields[3], "True") == 0;
  switch (pGrandPrix->nextStep) {
  case race_P1:
    fillHistoricPractice(&pGrandPrix->pPractices[0], race_P1, pLeaderBoard);
    pGrandPrix->nextStep = specialGP ? race_Q1_SPRINT : race_P2;
    break;
  case race_P2:
    fillHistoricPractice(&pGrandPrix->pPractices[1], race_P2, pLeaderBoard);
    pGrandPrix->nextStep = race_P3;
    break;
  case race_P3:
    fillHistoricPractice(&pGrandPrix->pPractices[2], race_P3, pLeaderBoard);
    pGrandPrix->nextStep = race_Q1_GP;
    break;
  default:
    break;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void freeHistoric(Context *pCtx) {
  free(pCtx->pGrandPrix);
  close(pCtx->gpHistoricHandle);
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayPractice(Context *pCtx, WINDOW *pWindow, int grandPrixId, Practice *pPractice) {
  char pFormat[32];
  PracticeItem *pPracticeItem;
  uint32_t previousTime;
  char **ppFields;
  int carId;
  int i;

  ppFields = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;

  werase(pWindow);
  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 20, "%d %s/%s - %s", pCtx->gpYear, ppFields[0], ppFields[1], raceTypeToString(pPractice->type));
  mvwprintw(pWindow, 2, 1, "Pos   Number     Driver                      Team                   Time     Gap     Laps");
  wattroff(pWindow, A_BOLD);

  pPracticeItem = (PracticeItem *)pPractice->pItems;
  for (i = 0; i < MAX_DRIVERS; i++) {
    carId = pPracticeItem->carId;
    ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
    mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s", i + 1, ppFields[0], ppFields[1], ppFields[2]);
    if (pPracticeItem->bestLapTime > 0) {
      mvwprintw(pWindow, i + 3, 64, "%10s", timestampToMinute(pPracticeItem->bestLapTime, pFormat, sizeof(pFormat)));
      if (i == 0) {
        previousTime = pPracticeItem->bestLapTime;
      } else {
        mvwprintw(pWindow, i + 3, 75, "%10s",
                  milliToGap(pPracticeItem->bestLapTime - previousTime, pFormat, sizeof(pFormat)));
      }
      mvwprintw(pWindow, i + 3, 86, "%3d", pPracticeItem->bestLap);
    }
    pPracticeItem++;
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayStandings(Context *pCtx, int choice, void *pUserData) {
  StandingsTableItem *pStandingsTableItem;
  DisplayMenuContext *pDisplayCtx;
  WINDOW *pWindow;
  char **ppFields;
  int carId;
  int i;

  pDisplayCtx = (DisplayMenuContext *)pUserData;
  pWindow = pDisplayCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 25, "%d - Driver Standings", pCtx->gpYear);
  mvwprintw(pWindow, 2, 1, "Pos   Number     Driver                      Team                   Total points");
  wattroff(pWindow, A_BOLD);

  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_DRIVERS; i++) {
    carId = pStandingsTableItem->carId;
    ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
    mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s   %5d", i + 1, ppFields[0], ppFields[1], ppFields[2],
              pStandingsTableItem->points);
    pStandingsTableItem++;
  }
  wrefresh(pWindow);

  wgetch(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayListGPs(Context *pCtx, int choice, void *pUserData) {
  DisplayMenuContext *pDisplayCtx;
  WINDOW *pWindow;
  char **ppFields;
  int i;

  pDisplayCtx = (DisplayMenuContext *)pUserData;
  pWindow = pDisplayCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 30, "%d - List of Grand Prix", pCtx->gpYear);
  mvwprintw(pWindow, 2, 1, "        Name                       Circuit                     Laps");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < MAX_GP; i++) {
    ppFields = pCtx->ppCsvGrandPrix[i]->ppFields;
    mvwprintw(pWindow, i + 3, 1, "%3d  %-20.20s   %-30.30s   %5s", i + 1, ppFields[0], ppFields[1], ppFields[2]);
  }
  wrefresh(pWindow);

  wgetch(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayListDrivers(Context *pCtx, int choice, void *pUserData) {
  DisplayMenuContext *pDisplayCtx;
  WINDOW *pWindow;
  char **ppFields;
  int i;

  pDisplayCtx = (DisplayMenuContext *)pUserData;
  pWindow = pDisplayCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 25, "%d - List of drivers", pCtx->gpYear);
  mvwprintw(pWindow, 2, 1, " Number        Name                          Team");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < MAX_DRIVERS; i++) {
    ppFields = pCtx->ppCsvDrivers[i]->ppFields;
    mvwprintw(pWindow, i + 3, 1, "%5s     %-30.30s   %-30.30s", ppFields[0], ppFields[1], ppFields[2]);
  }
  wrefresh(pWindow);

  wgetch(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayMenu(Context *pCtx, MenuItem *pMenu, void *pUserData) {
  size_t maxTextLength;
  size_t length;
  WINDOW *pMenuWin;
  int menuItems;
  int key;
  int i;

  maxTextLength = 0;
  menuItems = 0;
  while (pMenu[menuItems].pItem != NULL) {
    length = strlen(pMenu[menuItems].pItem);
    if (length > maxTextLength) {
      maxTextLength = length;
    }
    menuItems++;
  }

  pMenuWin = newwin(2 + menuItems, maxTextLength + 14, LINES - (4 + menuItems), 6);
  box(pMenuWin, ACS_VLINE, ACS_HLINE);
  wrefresh(pMenuWin);
  noecho();
  curs_set(0);
  keypad(pMenuWin, TRUE);

  int currentChoice = 0;

  while (true) {
    for (i = 0; i < menuItems; i++) {
      if (i == currentChoice) {
        wattron(pMenuWin, A_REVERSE);
      }
      mvwprintw(pMenuWin, i + 1, 3, pMenu[i].pItem);
      if (i == currentChoice) {
        wattroff(pMenuWin, A_REVERSE);
      }
    }

    key = wgetch(pMenuWin);
    switch (key) {
    case KEY_UP:
      if (currentChoice == 0) {
        currentChoice = menuItems - 1;
      } else {
        currentChoice--;
      }

      break;
    case KEY_DOWN:
      if (currentChoice == menuItems - 1) {
        currentChoice = 0;
      } else {
        currentChoice++;
      }
      break;
    case 10:
      if (pMenu[currentChoice].pMenuAction != NULL) {
        werase(pMenuWin);
        wrefresh(pMenuWin);
        delwin(pMenuWin);
        pMenu[currentChoice].pMenuAction(pCtx, currentChoice, pUserData);
      }
      return currentChoice;
    default:
      break;
    }
  }

  werase(pMenuWin);
  wrefresh(pMenuWin);
  delwin(pMenuWin);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int readString(WINDOW *pWindow, char *pString, int maxLength) {
  int size;
  int ch;

  size = 0;
  ch = wgetch(pWindow);
  while (size < maxLength && ch != '\n') {
    if (ch == KEY_BACKSPACE) {
      if (size > 0) {
        wdelch(pWindow);
        pString--;
        size--;
      }
    } else {
      *pString++ = ch;
      size++;
    }
    ch = wgetch(pWindow);
  }

  *pString = 0;

  return size;
}

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef WIN64
int compareCarStatus(void *pUserData, const void *pLeft, const void *pRight) {
#else
int compareCarStatus(const void *pLeft, const void *pRight, void *pUserData) {
#endif
  CarStatus *pCarStatus;
  CarStatus *pCarA;
  CarStatus *pCarB;
  int compare;

  pCarStatus = (CarStatus *)pUserData;
  pCarA = &pCarStatus[*(int *)pLeft];
  pCarB = &pCarStatus[*(int *)pRight];

  compare = pCarB->segments - pCarA->segments;
  if (compare == 0) {
    compare = pCarA->totalLapsTime - pCarB->totalLapsTime;
    if (compare == 0) {
      compare = pCarA->cardId - pCarB->cardId;
    }
  }
  return compare;
}

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef WIN64
int compareCarStatusPractice(void *pUserData, const void *pLeft, const void *pRight) {
#else
int compareCarStatusPractice(const void *pLeft, const void *pRight, void *pUserData) {
#endif
  CarStatus *pCarStatus;
  CarStatus *pCarA;
  CarStatus *pCarB;

  pCarStatus = (CarStatus *)pUserData;
  pCarA = &pCarStatus[*(int *)pLeft];
  pCarB = &pCarStatus[*(int *)pRight];

  if (pCarA->bestLapTime == 0) {
    if (pCarB->bestLapTime == 0) {
      return pCarA->cardId - pCarB->cardId;
    }
    return 1;
  }

  if (pCarB->bestLapTime == 0) {
    return -1;
  }

  return pCarA->bestLapTime - pCarB->bestLapTime;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayLeaderBoard(Context *pCtx, WINDOW *pWindow, LeaderBoard *pLeaderBoard) {

  char pDisplay[32];
  int *pSortIndices;
  CarStatus *pCars;
  CarStatus *pCar;
  bool practice;
  int i;

  pCars = pLeaderBoard->pCars;
  pSortIndices = pLeaderBoard->pSortIndices;
  practice = pLeaderBoard->type == race_P1 || pLeaderBoard->type == race_P2 || pLeaderBoard->type == race_P2;
#ifdef WIN64
  if (practice) {
    qsort_s(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatusPractice, pCars);
  } else {
    qsort_s(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatus, pCars);
  }
#else
  if (practice) {
    qsort_r(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatusPractice, pCars);
  } else {
    qsort_r(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatus, pCars);
  }
#endif

  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_RED, COLOR_BLACK);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 1, "Pos  Car's name   Lap #    S1 time   S2 time   S3 time   Best lap time   %s",
            practice ? "Best lap" : "Total time");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < pLeaderBoard->cars; i++) {
    mvwprintw(pWindow, i + 3, 1, "%3d", i + 1);

    pCar = &pCars[pSortIndices[i]];
    if (pCar->active == false) {
      wattron(pWindow, COLOR_PAIR(3));
      mvwprintw(pWindow, i + 3, 1, "%.10s", pCtx->ppCsvDrivers[pCar->cardId]->ppFields[1]);
      wattroff(pWindow, COLOR_PAIR(3));
      continue;
    }
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, i + 3, 6, "%.10s", pCtx->ppCsvDrivers[pCar->cardId]->ppFields[1]);
    wattroff(pWindow, A_BOLD);

    if (pCar->lastEvent == event_END) {
      mvwprintw(pWindow, i + 3, 19, "  *");
    } else {
      mvwprintw(pWindow, i + 3, 19, "%3d", pCar->currentLap + 1);
    }

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

    mvwprintw(pWindow, i + 3, 59, "%10s", timestampToMinute(pCar->bestLapTime, pDisplay, sizeof(pDisplay)));
    if (practice) {
      mvwprintw(pWindow, i + 3, 74, "%5d", pCar->bestLap + 1);
    } else {
      mvwprintw(pWindow, i + 3, 70, "%15s", timestampToHour(pCar->totalLapsTime, pDisplay, sizeof(pDisplay)));
    }
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int processEvent(AcquireThreadCtx *pThreadCtx, EventRace *pEvent) {
  CarStatus *pCar;

  assert(pEvent->car >= 0 && pEvent->car < MAX_DRIVERS);

  pCar = &pThreadCtx->pCarStatus[pEvent->car];
  if (pCar->active == false) {
    logger(log_WARN, "an event for inactive car #%d was received\n", pEvent->car);
    return RETURN_OK;
  }

  switch (pEvent->event) {
  case event_ERROR:
    logger(log_ERROR, "an illegal event was received\n");
    return RETURN_KO;
  case event_START:
    pCar->currentLap = 0;
    break;
  case event_S1:
    pCar->s1Time = pEvent->timestamp - pCar->lastEventTS;
    if (pCar->bestS1Time == 0 || pCar->bestS1Time > pCar->s1Time) {
      pCar->bestS1Time = pCar->s1Time;
    }
    pCar->s2Time = 0;
    pCar->totalLapsTime += pCar->s1Time;
    pCar->segments++;
    break;
  case event_S2:
    pCar->s2Time = pEvent->timestamp - pCar->lastEventTS;
    if (pCar->bestS2Time == 0 || pCar->bestS2Time > pCar->s2Time) {
      pCar->bestS2Time = pCar->s2Time;
    }
    pCar->s3Time = 0;
    pCar->totalLapsTime += pCar->s2Time;
    pCar->segments++;
    break;
  case event_S3:
    pCar->s3Time = pEvent->timestamp - pCar->lastEventTS;
    if (pCar->bestS3Time == 0 || pCar->bestS3Time > pCar->s3Time) {
      pCar->bestS3Time = pCar->s3Time;
    }
    pCar->lastLapTime = pEvent->timestamp - pCar->startLapTimestamp;
    if (pCar->bestLapTime == 0 || pCar->bestLapTime > pCar->lastLapTime) {
      pCar->bestLapTime = pCar->lastLapTime;
      pCar->bestLap = pCar->currentLap;
    }
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
    logger(log_INFO, "should be implemented\n");
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
  AcquireThreadCtx *pThreadCtx;
  EventRace event;
  socket_t socket;
  int code;

  pThreadCtx = (AcquireThreadCtx *)pThreadArg;
  socket = pThreadCtx->pClientCtx->clientSocket;

  while (true) {
    code = readFully(socket, &event, sizeof(event));
    if (code <= 0) {
      break;
    }

    processEvent(pThreadCtx, &event);
  }

  closesocket(socket);

  pThreadCtx->threadStillAlive = false;

  return pThreadArg;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int captureEvents(Context *pCtx, int choice, void *pUserData) {
  AcquireThreadCtx acquireThreadCtx;
  DisplayMenuContext *pDisplayCtx;
  CarStatus pCarStatus[MAX_DRIVERS];
  LeaderBoard leaderBoard;
  WINDOW *pWindow;
  char pLastGrandPrix[128];
  const char *pMessage;
  ClientContext clientCtx;
  pthread_t threadId;
  int returnCode;
  int maxY;
  int maxX;
  int code;
  int i;

  memset(pCarStatus, 0, sizeof(pCarStatus));
  for (i = 0; i < MAX_DRIVERS; i++) {
    pCarStatus[i].cardId = i;
    pCarStatus[i].active = true;
  }

  leaderBoard.grandPrixId = pCtx->currentGP;
  leaderBoard.type = pCtx->pGrandPrix[leaderBoard.grandPrixId].nextStep;
  leaderBoard.cars = MAX_DRIVERS;
  leaderBoard.pCars = pCarStatus;
  leaderBoard.lastEventTimestamp = 0;
  leaderBoard.raceStartTime = 0;
  leaderBoard.pSortIndices = (int *)malloc(sizeof(int) * leaderBoard.cars);
  for (i = 0; i < MAX_DRIVERS; i++) {
    leaderBoard.pSortIndices[i] = i;
  }

  pDisplayCtx = (DisplayMenuContext *)pUserData;
  pWindow = pDisplayCtx->pWindow;
  werase(pWindow);

  getmaxyx(pWindow, maxY, maxX);

  pMessage = "En attente de connexion des postes S1, S2 et S3...";
  mvwprintw(pWindow, maxY / 2, (maxX - strlen(pMessage)) / 2, pMessage);
  sprintf(pLastGrandPrix, "La prochaine etape est: %s - %s", pCtx->ppCsvGrandPrix[pCtx->currentGP]->ppFields[0],
          raceTypeToString(leaderBoard.type));
  mvwprintw(pWindow, maxY / 2 - 1, (maxX - strlen(pLastGrandPrix)) / 2, pLastGrandPrix);
  wrefresh(pWindow);

  code = acceptConnection(&pCtx->listener, &clientCtx);
  if (code) {
    returnCode = code;
    goto captureEventsExit;
  }

  wclear(pWindow);
  wrefresh(pWindow);

  acquireThreadCtx.pCtx = pCtx;
  acquireThreadCtx.pClientCtx = &clientCtx;
  acquireThreadCtx.pCarStatus = pCarStatus;
  code = pthread_create(&threadId, NULL, acquireData, &acquireThreadCtx);
  if (code) {
    logger(log_ERROR, "unable to create new thread to capture data, code=%d\n", errno);
    returnCode = code;
    goto captureEventsExit;
  }

  while (acquireThreadCtx.threadStillAlive) {
    sleep(1);
    displayLeaderBoard(pCtx, pWindow, &leaderBoard);
  }

  code = pthread_join(threadId, NULL);
  if (code) {
    logger(log_ERROR, "unable to join acquisition thread, code=%d\n", errno);
    returnCode = code;
    goto captureEventsExit;
  }

  code = fillHistoric(pCtx, &leaderBoard);
  if (code) {
    logger(log_ERROR, "unable to fill historic race, code=%d\n", code);
    returnCode = code;
    goto captureEventsExit;
  }

  returnCode = RETURN_OK;

captureEventsExit:
  free((void *)leaderBoard.pSortIndices);

  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int grandPrixCore(ProgramOptions *pOptions) {
  DisplayMenuContext menuCtx;
  WINDOW *pWindow;
  Context ctx;
  int code;

  code = readConfiguration(&ctx);
  if (code) {
    return code;
  }
  ctx.gpYear = pOptions->gpYear;

  code = readHistoric(&ctx);
  if (code) {
    return code;
  }

  code = startListening(&ctx, pOptions);
  if (code) {
    return code;
  }

  initscr();
  start_color();
  curs_set(0);

  pWindow = newwin(27, 120, 1, 1);

  code = displayPractice(&ctx, pWindow, 0, &ctx.pGrandPrix[0].pPractices[0]);
  sleep(1);

  menuCtx.pWindow = pWindow;
  while (true) {
    code = displayMenu(&ctx, pDisplayMainMenu, &menuCtx);
    if (code == 5) {
      break;
    }
  }

  delwin(pWindow);
  endwin();

  code = saveHistoric(&ctx);
  if (code) {
    return code;
  }

  stopListening(&ctx);

  freeHistoric(&ctx);
  freeConfiguration(&ctx);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  ProgramOptions options;
  int code;
  int opt;

  memset(&options, 0, sizeof(options));
  options.pListenAddress = "127.0.0.1";
  options.listenPort = 1111;
  options.gpYear = 2025;

  while ((opt = getopt(argc, ppArgv, "l:p:y:h?")) != -1) {
    switch (opt) {
    case 'p':
      options.listenPort = atoi(optarg);
      break;
    case 'l':
      options.pListenAddress = optarg;
      break;
    case 'y':
      options.gpYear = atoi(optarg);
      break;
    case 'h':
    case '?':
    default:
      printHelp();
      return EXIT_SUCCESS;
    }
  }

  code = grandPrixCore(&options);
  if (code) {
    printf("ERROR: an error was encounterred during execution of grandPrixCore(), code=%d\n", code);
    printf("ERROR: see log file for more information\n");
  }

  return EXIT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------------------*/
