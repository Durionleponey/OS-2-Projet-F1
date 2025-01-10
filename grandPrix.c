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
#ifdef WIN64
#include <sys\stat.h>
#endif

#include "grandPrix.h"
#include "saveFile.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

#define GRANDPRIX_FILENAME "F1_Grand_Prix_2024.csv"
#define DRIVERS_FILENAME "Drivers.csv"

const int pSprintScores[] = {8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const int pGrandPrixScores[] = {25, 20, 15, 10, 8, 6, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*--------------------------------------------------------------------------------------------------------------------*/

typedef struct structProgramOptions {
  const char *pListenAddress;
  int listenPort;
  int gpYear;
  int speedFactor;
  bool autoLaunch;
} ProgramOptions;

typedef struct structMenuItem {
  const char *pItem;
  int (*pMenuAction)(Context *pCtx, int choice, void *pUserData);
} MenuItem;

typedef struct structDisplayMenuContext {
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
  uint32_t lastSegmentTS;
  int bestLap;
  int bestS1Time;
  int bestS2Time;
  int bestS3Time;
  int s1Time;
  int s2Time;
  int s3Time;
  uint32_t totalPitsTime;
  int pitTime;
  int pits;
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
int displayCompletedGPMenu(Context *pCtx, int choice, void *pUserData);

/*--------------------------------------------------------------------------------------------------------------------*/

// clang-format off
static MenuItem pMainMenu[] = {
  { "Afficher liste des Grand Prix", displayListGPs },
  { "Afficher liste des pilotes", displayListDrivers },
  { "Afficher des resultats de courses terminees", displayCompletedGPMenu },
  { "Afficher le classement general", displayStandings },
  { "Lancer la capture de l'etape suivante", captureEvents },
  { "Quitter le programme", NULL },
  { NULL, NULL }
};

static MenuItem pGPMenu[] = {
  { "Afficher les essais 1", NULL },
  { "Afficher les essais 2", NULL },
  { "Afficher les essais 3", NULL },
  { "Afficher les qualifications 1", NULL },
  { "Afficher les qualifications 2", NULL },
  { "Afficher les qualifications 3", NULL },
  { "Afficher la grille de depart du Grand Prix", NULL },
  { "Afficher le Grand Prix", NULL },
  { NULL, NULL }
};

static MenuItem pGPSpecialMenu[] = {
  { "Afficher les essais 1", NULL },
  { "Afficher les qualifications Sprint 1", NULL },
  { "Afficher les qualifications Sprint 2", NULL },
  { "Afficher les qualifications Sprint 3", NULL },
  { "Afficher la grille de depart du Sprint", NULL },
  { "Afficher le Sprint", NULL },
  { "Afficher les qualifications 1", NULL },
  { "Afficher les qualifications 2", NULL },
  { "Afficher les qualifications 3", NULL },
  { "Afficher la grille de depart du Grand Prix", NULL },
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
  int compare;

  compare = ((StandingsTableItem *)pB)->points - ((StandingsTableItem *)pA)->points;
  if (compare == 0) {
    compare = ((StandingsTableItem *)pA)->carId - ((StandingsTableItem *)pB)->carId;
  }
  return compare;
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
  char pFileName[PATH_MAX];
  GrandPrix *pGrandPrix;
  int fileHandle;
  int code;
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

void fillHistoricRace(Race *pRace, RaceType type, LeaderBoard *pLeaderBoard) {
  RaceInfo *pItems;
  CarStatus *pCars;
  int *pSortIndices;
  int order;
  int i;

  pSortIndices = pLeaderBoard->pSortIndices;
  pRace->type = type;
  pItems = pRace->pItems;
  pCars = pLeaderBoard->pCars;
  for (i = 0; i < MAX_DRIVERS; i++) {
    order = *pSortIndices++;
    pCars = &pLeaderBoard->pCars[order];
    pItems->carId = pCars->cardId;
    pItems->raceTime = pCars->totalLapsTime;
    pItems->bestLapTime = pCars->bestLapTime;
    pItems->bestLap = pCars->bestLap;
    pItems->bestS1 = pCars->bestS1Time;
    pItems->bestS2 = pCars->bestS2Time;
    pItems->bestS3 = pCars->bestS3Time;
    pItems->pits = pCars->pits;
    pItems->pitsTime = pCars->totalPitsTime;
    pItems++;
  }
}

/*--------------------------------------------------------------------------------------------------------------------*/

int fillHistoric(Context *pCtx, LeaderBoard *pLeaderBoard) {
  GrandPrix *pGrandPrix;
  bool specialGP;
  int currentGP;

  currentGP = pCtx->currentGP;
  pGrandPrix = &pCtx->pGrandPrix[currentGP];
  specialGP = strcasecmp(pCtx->ppCsvGrandPrix[currentGP]->ppFields[3], "True") == 0;
  switch (pGrandPrix->nextStep) {
  case race_P1:
    fillHistoricRace(&pGrandPrix->pPractices[0], race_P1, pLeaderBoard);
    pGrandPrix->nextStep = specialGP ? race_Q1_SPRINT : race_P2;
    break;
  case race_P2:
    fillHistoricRace(&pGrandPrix->pPractices[1], race_P2, pLeaderBoard);
    pGrandPrix->nextStep = race_P3;
    break;
  case race_P3:
    fillHistoricRace(&pGrandPrix->pPractices[2], race_P3, pLeaderBoard);
    pGrandPrix->nextStep = race_Q1_GP;
    break;
  case race_Q1_SPRINT:
    fillHistoricRace(&pGrandPrix->pSprintShootout[0], race_Q1_SPRINT, pLeaderBoard);
    pGrandPrix->nextStep = race_Q2_SPRINT;
    break;
  case race_Q2_SPRINT:
    fillHistoricRace(&pGrandPrix->pSprintShootout[1], race_Q2_SPRINT, pLeaderBoard);
    pGrandPrix->nextStep = race_Q3_SPRINT;
    break;
  case race_Q3_SPRINT:
    fillHistoricRace(&pGrandPrix->pSprintShootout[2], race_Q3_SPRINT, pLeaderBoard);
    pGrandPrix->nextStep = race_SPRINT;
    break;
  case race_SPRINT:
    fillHistoricRace(&pGrandPrix->sprint, race_SPRINT, pLeaderBoard);
    pGrandPrix->nextStep = race_Q1_GP;
    break;
  case race_Q1_GP:
    fillHistoricRace(&pGrandPrix->pQualifications[0], race_Q1_GP, pLeaderBoard);
    pGrandPrix->nextStep = race_Q2_GP;
    break;
  case race_Q2_GP:
    fillHistoricRace(&pGrandPrix->pQualifications[1], race_Q2_GP, pLeaderBoard);
    pGrandPrix->nextStep = race_Q3_GP;
    break;
  case race_Q3_GP:
    fillHistoricRace(&pGrandPrix->pQualifications[2], race_Q3_GP, pLeaderBoard);
    pGrandPrix->nextStep = race_GP;
    break;
  case race_GP:
    fillHistoricRace(&pGrandPrix->final, race_GP, pLeaderBoard);
    pGrandPrix->nextStep = race_FINISHED;
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

int displayRace(Context *pCtx, int grandPrixId, Race *pRace) {
  char pFormat[32];
  WINDOW *pWindow;
  RaceInfo *pRaceInfo;
  uint32_t previousTime;
  char **ppFields;
  int carId;
  int i;

  ppFields = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;
  pWindow = pCtx->pWindow;

  werase(pWindow);
  if (pRace->type == race_ERROR) {
    wattron(pWindow, COLOR_PAIR(3));
    mvwprintw(pWindow, 10, 20, "Cette course n'a pas encore eu lieu");
    wattroff(pWindow, COLOR_PAIR(3));
  } else {
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, 1, 20, "%d %s/%s - %s", pCtx->gpYear, ppFields[0], ppFields[1], raceTypeToString(pRace->type));
    mvwprintw(pWindow, 2, 1,
              "Pos   Number     Driver                      Team                        Time         Gap    Points");
    wattroff(pWindow, A_BOLD);

    pRaceInfo = (RaceInfo *)pRace->pItems;
    for (i = 0; i < MAX_DRIVERS; i++) {
      carId = pRaceInfo->carId;
      ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
      mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s", i + 1, ppFields[0], ppFields[1], ppFields[2]);
      if (pRaceInfo->raceTime > 0) {
        mvwprintw(pWindow, i + 3, 70, "%10s", timestampToHour(pRaceInfo->raceTime, pFormat, sizeof(pFormat)));
        if (i == 0) {
          previousTime = pRaceInfo->raceTime;
        } else {
          mvwprintw(pWindow, i + 3, 83, "%10s",
                    milliToGap(pRaceInfo->raceTime - previousTime, pFormat, sizeof(pFormat)));
        }
        mvwprintw(pWindow, i + 3, 94, "%4d", pGrandPrixScores[i]);
      }
      pRaceInfo++;
    }
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayQualification(Context *pCtx, int grandPrixId, Race *pQualification) {
  char pFormat[32];
  WINDOW *pWindow;
  RaceInfo *pRaceInfo;
  uint32_t previousTime;
  char **ppFields;
  int drivers;
  int carId;
  int i;

  ppFields = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;
  pWindow = pCtx->pWindow;

  werase(pWindow);
  if (pQualification->type == race_ERROR) {
    wattron(pWindow, COLOR_PAIR(3));
    mvwprintw(pWindow, 10, 20, "Cette course 'Qualification' n'a pas encore eu lieu");
    wattroff(pWindow, COLOR_PAIR(3));
  } else {
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, 1, 20, "%d %s/%s - %s", pCtx->gpYear, ppFields[0], ppFields[1],
              raceTypeToString(pQualification->type));
    mvwprintw(pWindow, 2, 1,
              "Pos   Number     Driver                      Team               Best time    Gap     Laps");
    wattroff(pWindow, A_BOLD);

    if (pQualification->type == race_Q1_GP || pQualification->type == race_Q1_SPRINT) {
      drivers = MAX_DRIVERS;
    } else if (pQualification->type == race_Q2_GP || pQualification->type == race_Q2_SPRINT) {
      drivers = 15;
    } else if (pQualification->type == race_Q3_GP || pQualification->type == race_Q3_SPRINT) {
      drivers = 10;
    } else {
      logger(log_FATAL, "illegal race type '%s'. It should be a qualification race\n",
             raceTypeToString(pQualification->type));
      return RETURN_KO;
    }
    pRaceInfo = (RaceInfo *)pQualification->pItems;
    for (i = 0; i < drivers; i++) {
      carId = pRaceInfo->carId;
      ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
      mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s", i + 1, ppFields[0], ppFields[1], ppFields[2]);
      if (pRaceInfo->raceTime > 0) {
        mvwprintw(pWindow, i + 3, 64, "%10s", timestampToMinute(pRaceInfo->bestLapTime, pFormat, sizeof(pFormat)));
        if (i == 0) {
          previousTime = pRaceInfo->bestLapTime;
        } else {
          mvwprintw(pWindow, i + 3, 75, "%10s",
                    milliToGap(pRaceInfo->bestLapTime - previousTime, pFormat, sizeof(pFormat)));
        }
        mvwprintw(pWindow, i + 3, 86, "%3d", pRaceInfo->bestLap + 1);
      }
      pRaceInfo++;
    }
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayPractice(Context *pCtx, int grandPrixId, Race *pPractice) {
  char pFormat[32];
  WINDOW *pWindow;
  RaceInfo *pRaceInfo;
  uint32_t previousTime;
  char **ppFields;
  int carId;
  int i;

  ppFields = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;
  pWindow = pCtx->pWindow;

  werase(pWindow);
  if (pPractice->type == race_ERROR) {
    wattron(pWindow, COLOR_PAIR(3));
    mvwprintw(pWindow, 10, 20, "Cette course 'Practice' n'a pas encore eu lieu");
    wattroff(pWindow, COLOR_PAIR(3));
  } else {
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, 1, 20, "%d %s/%s - %s", pCtx->gpYear, ppFields[0], ppFields[1],
              raceTypeToString(pPractice->type));
    mvwprintw(pWindow, 2, 1,
              "Pos   Number     Driver                      Team               Best time    Gap     Laps");
    wattroff(pWindow, A_BOLD);

    pRaceInfo = (RaceInfo *)pPractice->pItems;
    for (i = 0; i < MAX_DRIVERS; i++) {
      carId = pRaceInfo->carId;
      ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
      mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s", i + 1, ppFields[0], ppFields[1], ppFields[2]);
      if (pRaceInfo->bestLapTime > 0) {
        mvwprintw(pWindow, i + 3, 64, "%10s", timestampToMinute(pRaceInfo->bestLapTime, pFormat, sizeof(pFormat)));
        if (i == 0) {
          previousTime = pRaceInfo->bestLapTime;
        } else {
          mvwprintw(pWindow, i + 3, 75, "%10s",
                    milliToGap(pRaceInfo->bestLapTime - previousTime, pFormat, sizeof(pFormat)));
        }
        mvwprintw(pWindow, i + 3, 86, "%3d", pRaceInfo->bestLap + 1);
      }
      pRaceInfo++;
    }
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayStartingGrid(Context *pCtx, int grandPrixId, Race *pQualification) {
  char pFormat[32];
  WINDOW *pWindow;
  RaceInfo *pRaceInfo;
  char **ppFields;
  int carId;
  int i;

  ppFields = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;
  pWindow = pCtx->pWindow;

  werase(pWindow);
  if (pQualification[2].type == race_ERROR) {
    wattron(pWindow, COLOR_PAIR(3));
    mvwprintw(pWindow, 10, 20, "Les 3 courses de 'Qualification' ne sont pas terminees");
    wattroff(pWindow, COLOR_PAIR(3));
  } else {
    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, 1, 15, "%d %s/%s - Grille de depart", pCtx->gpYear, ppFields[0], ppFields[1]);
    mvwprintw(pWindow, 2, 1, "Pos   Number     Driver                      Team                       Best time");
    wattroff(pWindow, A_BOLD);

    for (i = 0; i < MAX_DRIVERS; i++) {
      if (i == 0) {
        pRaceInfo = (RaceInfo *)pQualification[2].pItems;
      } else if (i == 10) {
        pRaceInfo = (RaceInfo *)&pQualification[1].pItems[10];
      } else if (i == 15) {
        pRaceInfo = (RaceInfo *)&pQualification[0].pItems[15];
      }
      carId = pRaceInfo->carId;
      ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
      mvwprintw(pWindow, i + 3, 1, "%3d  %5s    %-20.20s   %-30.30s    %10s", i + 1, ppFields[0], ppFields[1],
                ppFields[2], timestampToMinute(pRaceInfo->bestLapTime, pFormat, sizeof(pFormat)));
      pRaceInfo++;
    }
  }
  wrefresh(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayStandings(Context *pCtx, int choice, void *pUserData) {
  StandingsTableItem *pStandingsTableItem;
  RaceInfo *pSprintInfo;
  RaceInfo *pGrandPrixInfo;
  GrandPrix *pGrandPrix;
  WINDOW *pWindow;
  char **ppFields;
  int carId;
  int car;
  int i;

  pGrandPrix = pCtx->pGrandPrix;
  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_DRIVERS; i++) {
    pStandingsTableItem->carId = i;
    pStandingsTableItem->points = 0;
    pStandingsTableItem++;
  }

  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_GP; i++) {
    if (pGrandPrix->specialGP && pGrandPrix->nextStep > race_SPRINT) {
      pSprintInfo = (RaceInfo *)pGrandPrix->sprint.pItems;
      for (car = 0; car < MAX_DRIVERS; car++) {
        pStandingsTableItem[pSprintInfo->carId].points += pSprintScores[car];
        pSprintInfo++;
      }
    }
    if (pGrandPrix->nextStep > race_GP) {
      pGrandPrixInfo = (RaceInfo *)pGrandPrix->final.pItems;
      for (car = 0; car < MAX_DRIVERS; car++) {
        pStandingsTableItem[pGrandPrixInfo->carId].points += pGrandPrixScores[car];
        pGrandPrixInfo++;
      }
    }
    pGrandPrix++;
  }
  qsort(&pCtx->standingsTable.pItems, MAX_DRIVERS, sizeof(StandingsTableItem), compareStandingsTableItem);

  pWindow = pCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 25, "%d - Classement des pilotes", pCtx->gpYear);
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
  WINDOW *pWindow;
  char **ppFields;
  bool specialGP;
  int i;

  pWindow = pCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 30, "%d - Liste des Grand Prix", pCtx->gpYear);
  mvwprintw(pWindow, 2, 1, "        Name                       Circuit                     Laps");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < MAX_GP; i++) {
    ppFields = pCtx->ppCsvGrandPrix[i]->ppFields;
    specialGP = strcasecmp(ppFields[3], "True") == 0;
    mvwprintw(pWindow, i + 3, 1, "%3d  %-20.20s   %-30.30s   %5s %c", i + 1, ppFields[0], ppFields[1], ppFields[2],
              specialGP ? '*' : ' ');
  }
  wrefresh(pWindow);

  wgetch(pWindow);

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayListDrivers(Context *pCtx, int choice, void *pUserData) {
  WINDOW *pWindow;
  char **ppFields;
  int i;

  pWindow = pCtx->pWindow;
  werase(pWindow);

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 25, "%d - Liste des pilotes", pCtx->gpYear);
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

int displayMenu(Context *pCtx, MenuItem *pMenu, bool back, int position, void *pUserData) {
  const char *pBackMessage = "Retour en arriere";
  size_t maxTextLength;
  size_t length;
  WINDOW *pMenuWin;
  int currentChoice;
  int returnCode;
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
  if (back) {
    length = strlen(pBackMessage);
    if (length > maxTextLength) {
      maxTextLength = length;
    }
    menuItems++;
  }

  assert(menuItems > 0);

  pMenuWin = newwin(2 + menuItems, maxTextLength + 14, LINES - (4 + menuItems), 6);
  box(pMenuWin, ACS_VLINE, ACS_HLINE);
  wrefresh(pMenuWin);
  noecho();
  curs_set(0);
  keypad(pMenuWin, TRUE);

  currentChoice = position < menuItems && position >= 0 ? position : 0;

  while (true) {
    for (i = 0; i < menuItems; i++) {
      if (i == currentChoice) {
        wattron(pMenuWin, A_REVERSE);
      }
      if (back && i == menuItems - 1) {
        mvwprintw(pMenuWin, i + 1, 3, pBackMessage);
      } else {
        mvwprintw(pMenuWin, i + 1, 3, pMenu[i].pItem);
      }
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
      if (back && currentChoice == menuItems - 1) {
        returnCode = -1;
        goto displayMenuExit;
      } else {
        if (pMenu[currentChoice].pMenuAction != NULL) {
          werase(pMenuWin);
          wrefresh(pMenuWin);
          pMenu[currentChoice].pMenuAction(pCtx, currentChoice, pUserData);
        }
        returnCode = currentChoice;
      }
      goto displayMenuExit;
    default:
      break;
    }
  }

  returnCode = RETURN_OK;

displayMenuExit:
  werase(pMenuWin);
  wrefresh(pMenuWin);
  delwin(pMenuWin);

  return returnCode;
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

int displayRaceResult(Context *pCtx, int grandPrixId) {
  GrandPrix *pGrandPrix;
  bool special;
  int race;

  race = 0;
  pGrandPrix = &pCtx->pGrandPrix[grandPrixId];
  special = pGrandPrix->specialGP;

  if (special) {
    while (true) {
      race = displayMenu(pCtx, pGPSpecialMenu, true, race, NULL);
      if (race == -1) {
        break;
      }

      if (race == 0) {
        displayPractice(pCtx, grandPrixId, &pGrandPrix->pPractices[0]);
      } else if (race == 1) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pSprintShootout[0]);
      } else if (race == 2) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pSprintShootout[1]);
      } else if (race == 3) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pSprintShootout[2]);
      } else if (race == 4) {
        displayStartingGrid(pCtx, grandPrixId, pGrandPrix->pSprintShootout);
      } else if (race == 5) {
        displayRace(pCtx, grandPrixId, &pGrandPrix->sprint);
      } else if (race == 6) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[0]);
      } else if (race == 7) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[1]);
      } else if (race == 8) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[2]);
      } else if (race == 9) {
        displayStartingGrid(pCtx, grandPrixId, pGrandPrix->pQualifications);
      } else if (race == 10) {
        displayRace(pCtx, grandPrixId, &pGrandPrix->final);
      }
    }
  } else {
    while (true) {
      race = displayMenu(pCtx, pGPMenu, true, race, NULL);
      if (race == -1) {
        break;
      }

      if (race == 0) {
        displayPractice(pCtx, grandPrixId, &pGrandPrix->pPractices[0]);
      } else if (race == 1) {
        displayPractice(pCtx, grandPrixId, &pGrandPrix->pPractices[1]);
      } else if (race == 2) {
        displayPractice(pCtx, grandPrixId, &pGrandPrix->pPractices[2]);
      } else if (race == 3) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[0]);
      } else if (race == 4) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[1]);
      } else if (race == 5) {
        displayQualification(pCtx, grandPrixId, &pGrandPrix->pQualifications[2]);
      } else if (race == 6) {
        displayStartingGrid(pCtx, grandPrixId, pGrandPrix->pQualifications);
      } else if (race == 7) {
        displayRace(pCtx, grandPrixId, &pGrandPrix->final);
      }
    }
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayCompletedGPMenu(Context *pCtx, int choice, void *pUserData) {
  MenuItem *pMenuItems;
  char *pMenuLabel;
  char **ppFields;
  size_t length;
  int gps;
  int gp;
  int i;

  gps = pCtx->currentGP + 1;
  pMenuItems = (MenuItem *)malloc(sizeof(MenuItem) * (gps + 1));
  if (pMenuItems == NULL) {
    logger(log_FATAL, "unable to allocate %d bytes for GrandPrix menu.\n", sizeof(MenuItem) * (gps + 1));
    return RETURN_KO;
  }

  for (i = 0; i < gps; i++) {
    ppFields = pCtx->ppCsvGrandPrix[i]->ppFields;
    length = strlen(ppFields[0]) + strlen(ppFields[1]) + 2;
    pMenuLabel = (char *)malloc(length);
    if (pMenuLabel == NULL) {
      logger(log_FATAL, "unable to allocate %d bytes for GP menu item.\n", length);
      goto displayAllGPMenuExit;
    }
    sprintf(pMenuLabel, "%s/%s", ppFields[0], ppFields[1]);

    pMenuItems[i].pItem = pMenuLabel;
    pMenuItems[i].pMenuAction = NULL;
  }
  pMenuItems[i].pItem = NULL;
  pMenuItems[i].pMenuAction = NULL;

  gp = 0;
  while (true) {
    gp = displayMenu(pCtx, pMenuItems, true, gp, NULL);
    if (gp == -1) {
      break;
    }
    displayRaceResult(pCtx, gp);
  }

displayAllGPMenuExit:
  for (i = 0; i <= gps; i++) {
    if (pMenuItems[i].pItem == NULL) {
      break;
    }
    free((void *)pMenuItems[i].pItem);
  }

  free(pMenuItems);

  return RETURN_OK;
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

  if (pCarA->active && pCarB->active) {
    compare = pCarB->segments - pCarA->segments;
    if (compare == 0) {
      compare = pCarA->totalLapsTime - pCarB->totalLapsTime;
      if (compare == 0) {
        compare = pCarA->cardId - pCarB->cardId;
      }
    }
    return compare;
  }

  if (!pCarA->active && !pCarB->active) {
    return pCarA->cardId - pCarB->cardId;
  }
  return pCarA->active ? -1 : 1;
}

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef WIN64
int compareCarStatusBestLap(void *pUserData, const void *pLeft, const void *pRight) {
#else
int compareCarStatusBestLap(const void *pLeft, const void *pRight, void *pUserData) {
#endif
  CarStatus *pCarStatus;
  CarStatus *pCarA;
  CarStatus *pCarB;

  pCarStatus = (CarStatus *)pUserData;
  pCarA = &pCarStatus[*(int *)pLeft];
  pCarB = &pCarStatus[*(int *)pRight];

  if (pCarA->active && pCarB->active) {
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

  if (!pCarA->active && !pCarB->active) {
    return pCarA->cardId - pCarB->cardId;
  }
  return pCarA->active ? -1 : 1;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int displayLeaderBoard(Context *pCtx, WINDOW *pWindow, LeaderBoard *pLeaderBoard) {

  char pDisplay[32];
  int *pSortIndices;
  EventType event;
  CarStatus *pCars;
  CarStatus *pCar;
  bool bestLap;
  int i;

  pCars = pLeaderBoard->pCars;
  pSortIndices = pLeaderBoard->pSortIndices;
  bestLap = pLeaderBoard->type != race_SPRINT && pLeaderBoard->type != race_GP;
#ifdef WIN64
  if (bestLap) {
    qsort_s(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatusBestLap, pCars);
  } else {
    qsort_s(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatus, pCars);
  }
#else
  if (bestLap) {
    qsort_r(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatusBestLap, pCars);
  } else {
    qsort_r(pSortIndices, pLeaderBoard->cars, sizeof(int), compareCarStatus, pCars);
  }
#endif

  wattron(pWindow, A_BOLD);
  mvwprintw(pWindow, 1, 1, "Pos  Car's name             Lap #    S1 time   S2 time   S3 time   Best lap time   %s",
            bestLap ? "Best lap" : "Pits   Total time");
  wattroff(pWindow, A_BOLD);

  for (i = 0; i < pLeaderBoard->cars; i++) {
    mvwprintw(pWindow, i + 3, 1, "%3d", i + 1);

    pCar = &pCars[pSortIndices[i]];
    if (pCar->active == false) {
      wattron(pWindow, COLOR_PAIR(3));
      mvwprintw(pWindow, i + 3, 6, "%-20.20s", pCtx->ppCsvDrivers[pCar->cardId]->ppFields[1]);
      wattroff(pWindow, COLOR_PAIR(3));
      continue;
    }

    event = pCar->lastEvent;
    if (event == event_PIT_START) {
      wattron(pWindow, COLOR_PAIR(4));
    }

    wattron(pWindow, A_BOLD);
    mvwprintw(pWindow, i + 3, 6, "%-20.20s", pCtx->ppCsvDrivers[pCar->cardId]->ppFields[1]);
    wattroff(pWindow, A_BOLD);

    if (event == event_END) {
      mvwprintw(pWindow, i + 3, 29, "  *");
    } else {
      mvwprintw(pWindow, i + 3, 29, "%3d", pCar->currentLap + 1);
    }

    if (event == event_START || event == event_S3) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 35, "%9s", timestampToSecond(pCar->s1Time, pDisplay, sizeof(pDisplay)));
    if (event == event_START || event == event_S3) {
      wattroff(pWindow, COLOR_PAIR(2));
    }
    if (event == event_S1) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 45, "%9s", timestampToSecond(pCar->s2Time, pDisplay, sizeof(pDisplay)));
    if (event == event_S1) {
      wattroff(pWindow, COLOR_PAIR(2));
    }
    if (event == event_S2 || event == event_PIT_END) {
      wattron(pWindow, COLOR_PAIR(2));
    }
    mvwprintw(pWindow, i + 3, 55, "%9s", timestampToSecond(pCar->s3Time, pDisplay, sizeof(pDisplay)));
    if (event == event_S2 || event == event_PIT_END) {
      wattroff(pWindow, COLOR_PAIR(2));
    }

    mvwprintw(pWindow, i + 3, 69, "%10s", timestampToMinute(pCar->bestLapTime, pDisplay, sizeof(pDisplay)));
    if (bestLap) {
      mvwprintw(pWindow, i + 3, 84, "%5d", pCar->bestLap + 1);
    } else {
      mvwprintw(pWindow, i + 3, 80, "%5d  %15s", pCar->pits,
                timestampToHour(pCar->totalLapsTime, pDisplay, sizeof(pDisplay)));
    }

    if (event == event_PIT_START) {
      wattroff(pWindow, COLOR_PAIR(4));
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
    pCar->s1Time = pEvent->timestamp - pCar->lastSegmentTS;
    if (pCar->bestS1Time == 0 || pCar->bestS1Time > pCar->s1Time) {
      pCar->bestS1Time = pCar->s1Time;
    }
    pCar->s2Time = 0;
    pCar->totalLapsTime += pCar->s1Time;
    pCar->pitTime = 0;
    pCar->lastSegmentTS = pEvent->timestamp;
    pCar->segments++;
    break;
  case event_S2:
    pCar->s2Time = pEvent->timestamp - pCar->lastSegmentTS;
    if (pCar->bestS2Time == 0 || pCar->bestS2Time > pCar->s2Time) {
      pCar->bestS2Time = pCar->s2Time;
    }
    pCar->s3Time = 0;
    pCar->totalLapsTime += pCar->s2Time;
    pCar->lastSegmentTS = pEvent->timestamp;
    pCar->segments++;
    break;
  case event_S3:
    pCar->s3Time = pEvent->timestamp - pCar->lastSegmentTS;
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
    pCar->lastSegmentTS = pEvent->timestamp;
    pCar->segments++;
    break;
  case event_OUT:
    pCar->active = false;
    break;
  case event_END:
    break;
  case event_PIT_START:
    break;
  case event_PIT_END:
    pCar->pits++;
    pCar->pitTime = pEvent->timestamp - pCar->lastEventTS;
    pCar->totalPitsTime += pCar->pitTime;
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

  pThreadCtx->threadStillAlive = false;

  return pThreadArg;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int captureEvents(Context *pCtx, int choice, void *pUserData) {
  AcquireThreadCtx acquireThreadCtx;
  CarStatus pCarStatus[MAX_DRIVERS];
  LeaderBoard leaderBoard;
  GrandPrix *pGrandPrix;
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

  assert(pCtx->currentGP >= 0 && pCtx->currentGP < MAX_GP);

  pGrandPrix = &pCtx->pGrandPrix[pCtx->currentGP];

  leaderBoard.grandPrixId = pCtx->currentGP;
  leaderBoard.type = pGrandPrix->nextStep;
  leaderBoard.pCars = pCarStatus;
  leaderBoard.cars = MAX_DRIVERS;
  leaderBoard.lastEventTimestamp = 0;
  leaderBoard.raceStartTime = 0;
  leaderBoard.pSortIndices = (int *)malloc(sizeof(int) * leaderBoard.cars);
  for (i = 0; i < MAX_DRIVERS; i++) {
    leaderBoard.pSortIndices[i] = i;
  }

  memset(pCarStatus, 0, sizeof(pCarStatus));
  if (leaderBoard.type == race_Q2_GP) {
    for (i = 0; i < MAX_DRIVERS; i++) {
      pCarStatus[i].cardId = pGrandPrix->pQualifications[0].pItems[i].carId;
      pCarStatus[i].active = i < 15;
    }
  } else if (leaderBoard.type == race_Q2_SPRINT) {
    for (i = 0; i < MAX_DRIVERS; i++) {
      pCarStatus[i].cardId = pGrandPrix->pSprintShootout[0].pItems[i].carId;
      pCarStatus[i].active = i < 15;
    }
  } else if (leaderBoard.type == race_Q3_GP) {
    for (i = 0; i < MAX_DRIVERS; i++) {
      pCarStatus[i].cardId = pGrandPrix->pQualifications[1].pItems[i].carId;
      pCarStatus[i].active = i < 10;
    }
  } else if (leaderBoard.type == race_Q3_SPRINT) {
    for (i = 0; i < MAX_DRIVERS; i++) {
      pCarStatus[i].cardId = pGrandPrix->pSprintShootout[1].pItems[i].carId;
      pCarStatus[i].active = i < 10;
    }
  } else {
    for (i = 0; i < MAX_DRIVERS; i++) {
      pCarStatus[i].cardId = i;
      pCarStatus[i].active = true;
    }
  }

  pWindow = pCtx->pWindow;
  werase(pWindow);

  getmaxyx(pWindow, maxY, maxX);

  pMessage = "En attente de connexion des postes S1, S2 et S3...";
  mvwprintw(pWindow, maxY / 2, (maxX - strlen(pMessage)) / 2, pMessage);
  sprintf(pLastGrandPrix, "La prochaine etape est: %s - %s", pCtx->ppCsvGrandPrix[pCtx->currentGP]->ppFields[0],
          raceTypeToString(leaderBoard.type));
  mvwprintw(pWindow, maxY / 2 - 1, (maxX - strlen(pLastGrandPrix)) / 2, pLastGrandPrix);
  wrefresh(pWindow);

#ifdef LINUX
  if (pCtx->autoLaunch) {
    sleep(5);

    code = fork();
    if (code == -1) {
      logger(log_ERROR, "unable to create a new process, errno=%d\n", errno);
      goto captureEventsExit;
    }
    if (code == 0) {
      const char *ppArgs[12];
      char pGrandPrixId[16];
      char pSpeedFactor[16];

      sprintf(pGrandPrixId, "-c%d", leaderBoard.grandPrixId);
      ppArgs[0] = "genTime";
      ppArgs[1] = pGrandPrixId;
      ppArgs[2] = "-t";
      ppArgs[3] = raceTypeToString(leaderBoard.type);
      ppArgs[4] = "-s127.0.0.1";
      ppArgs[5] = "-p1111";
      ppArgs[6] = "-l";
      ppArgs[7] = pCtx->ppCsvGrandPrix[pCtx->currentGP]->ppFields[leaderBoard.type == race_SPRINT ? 4 : 2];
      if (pCtx->speedFactor > 0) {
        sprintf(pSpeedFactor, "-%.*s", pCtx->speedFactor, "ffffff");
        ppArgs[8] = pSpeedFactor;
        ppArgs[9] = NULL;
      } else {
        ppArgs[8] = NULL;
      }

      code = execv("genTime", (char *const *)ppArgs);
      if (code) {
        logger(log_ERROR, "unable to launch executable 'gentime', errno=%d\n", errno);
      }
      exit(code);
    }
    logger(log_INFO, "a new process (pid=%d) will simulate the race\n", code);
  }
#endif

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
  acquireThreadCtx.threadStillAlive = true;
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
  displayLeaderBoard(pCtx, pWindow, &leaderBoard);

  code = pthread_join(threadId, NULL);
  if (code) {
    logger(log_ERROR, "unable to join acquisition thread, code=%d\n", errno);
    returnCode = code;
    goto captureEventsExit;
  }

  closesocket(clientCtx.clientSocket);

  code = fillHistoric(pCtx, &leaderBoard);
  if (code) {
    logger(log_ERROR, "unable to fill historic race, code=%d\n", code);
    returnCode = code;
    goto captureEventsExit;
  }

  code = saveHistoric(pCtx);
  if (code) {
    return code;
  }

  code = saveGrandPrixToFile(pCtx, pCtx->currentGP);
  if (code) {
    return code;
  }

  returnCode = RETURN_OK;

captureEventsExit:
  free((void *)leaderBoard.pSortIndices);

  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int grandPrixCore(ProgramOptions *pOptions) {
  WINDOW *pWindow;
  Context ctx;
  int code;

  code = readConfiguration(&ctx);
  if (code) {
    return code;
  }
  ctx.speedFactor = pOptions->speedFactor;
  ctx.autoLaunch = pOptions->autoLaunch;
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

  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_RED, COLOR_BLACK);
  init_pair(4, COLOR_YELLOW, COLOR_BLACK);

  pWindow = newwin(27, 120, 1, 1);
  ctx.pWindow = pWindow;

  code = 0;
  while (true) {
    code = displayMenu(&ctx, pMainMenu, false, code, NULL);
    if (code == 5) {
      break;
    }
  }

  delwin(pWindow);
  endwin();

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
  options.autoLaunch = false;
  options.speedFactor = 0;

  while ((opt = getopt(argc, ppArgv, "al:p:s:y:h?")) != -1) {
    switch (opt) {
    case 'a':
      options.autoLaunch = true;
      break;
    case 'p':
      options.listenPort = atoi(optarg);
      break;
    case 'l':
      options.pListenAddress = optarg;
      break;
    case 's':
      options.speedFactor = atoi(optarg);
      if (options.speedFactor < 0) {
        options.speedFactor = 0;
      } else if (options.speedFactor > 5) {
        options.speedFactor = 5;
      }
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
