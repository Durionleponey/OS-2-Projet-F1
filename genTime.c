#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "grandPrix.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

static int pSleepTime[] = {1000, 500, 250, 100, 50, 25, 10};

typedef struct structProgramOptions {
  int raceNumber;
  RaceType raceType;
  int duration;
  int laps;
  const char *pServerAddress;
  int serverPort;
  int sleepIndex;
  bool verbose;
} ProgramOptions;

/*--------------------------------------------------------------------------------------------------------------------*/

void printHelp(void) {
  printf("Usage:\n");
  printf("\t-c\trace number (1-24)\n");
}

/*--------------------------------------------------------------------------------------------------------------------*/

int getRandomTime(int minTime, int maxTime) {
  return minTime + (unsigned int)((rand() * rand())) % (maxTime - minTime + 1);
}

/*--------------------------------------------------------------------------------------------------------------------*/

int compareEventTimestamp(const void *pLeft, const void *pRight) {
  int compare;

  compare = ((EventRace *)pLeft)->timestamp - ((EventRace *)pRight)->timestamp;
  if (compare == 0) {
    compare = ((EventRace *)pLeft)->id - ((EventRace *)pRight)->id;
  }
  return compare;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int writeFully(socket_t socket, void *pBuffer, int size) {
  uint8_t *pRecvBuffer;
  int bytesWritten;
  int code;

  bytesWritten = 0;
  pRecvBuffer = (uint8_t *)pBuffer;
  while (bytesWritten < size) {
    code = send(socket, (char *)pRecvBuffer, size - bytesWritten, 0);
    if (code > 0) {
      bytesWritten += code;
      pRecvBuffer += code;
    } else if (code == 0) {
      return 0;
    } else {
      printf("ERROR: unable to send to socket, errno=%d\n", WSAGetLastError());
      return -1;
    }
  }

  return bytesWritten;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int sendEvent(ClientContext *pClientCtx, EventRace *pEvent) {
  int code;

  code = writeFully(pClientCtx->clientSocket, pEvent, sizeof(*pEvent));
  if (code != sizeof(*pEvent)) {
    return RETURN_KO;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int connectServer(ProgramOptions *pOptions, ClientContext *pClientCtx) {
  int code;

  pClientCtx->clientSocket = INVALID_SOCKET;

#ifdef WIN64
  WSADATA info;

  if (WSAStartup(MAKEWORD(2, 2), &info) != 0) {
    printf("ERROR: unable to initialize Windows Socket API, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }
#endif

  pClientCtx->clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (pClientCtx->clientSocket == INVALID_SOCKET) {
    printf("ERROR: unable to allocate a new socket, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  pClientCtx->clientAddr.sin_family = AF_INET;
  pClientCtx->clientAddr.sin_port = htons(pOptions->serverPort);
  pClientCtx->clientAddr.sin_addr.s_addr = inet_addr(pOptions->pServerAddress);
  if (pClientCtx->clientAddr.sin_addr.s_addr == INADDR_NONE) {
    printf("ERROR: illegal listening address '%s'\n", pOptions->pServerAddress);
    return RETURN_KO;
  }

  code = connect(pClientCtx->clientSocket, (struct sockaddr *)&pClientCtx->clientAddr, sizeof(pClientCtx->clientAddr));
  if (code == SOCKET_ERROR) {
    printf("ERROR; unable to connect to server, code=%d\n", WSAGetLastError());
    return RETURN_KO;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int disconnectServer(ClientContext *pClientCtx) {
  if (pClientCtx->clientSocket != INVALID_SOCKET) {
    closesocket(pClientCtx->clientSocket);
  }

#ifdef WIN64
  WSACleanup();
#endif

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int genTimeCore(ProgramOptions *pParms) {
  ClientContext clientCtx;
  uint32_t maxRaceTime;
  uint32_t timestamp;
  EventRace *pEvents;
  EventRace *pEvent;
  bool *pPits;
  int returnCode;
  int maxEvents;
  int events;
  int sleep;
  int code;
  int pits;
  int cars;
  int car;
  int pit;
  int lap;
  int id;
  int i;

  srand(time(NULL));
  cars = MAX_DRIVERS;

  if (pParms->raceType == race_P1 || pParms->raceType == race_P2 || pParms->raceType == race_P3) {
    maxRaceTime = 60 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q1_GP) {
    maxRaceTime = 18 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q2_GP) {
    maxRaceTime = 15 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q3_GP) {
    maxRaceTime = 12 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q1_SPRINT) {
    maxRaceTime = 12 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q2_SPRINT) {
    maxRaceTime = 10 * MILLI_PER_MINUTE;
  } else if (pParms->raceType == race_Q3_SPRINT) {
    maxRaceTime = 8 * MILLI_PER_MINUTE;
  } else {
    maxRaceTime = 0;
  }

  if (pParms->verbose) {
    if (maxRaceTime == 0) {
      printf("INFO: the race events generation will be limited to %d laps\n", pParms->laps);
    } else {
      printf("INFO: the race events generation will be limited to %d minutes\n", maxRaceTime / MILLI_PER_MINUTE);
    }
  }

  pPits = (bool *)malloc(pParms->laps * sizeof(bool));

  maxEvents = cars * (5 * pParms->laps + 2);
  pEvents = (EventRace *)calloc(maxEvents, sizeof(EventRace));
  if (pEvents == NULL) {
    printf("ERROR: unable to allocate %lu bytes for events\n", (u_long)(sizeof(EventRace) * maxEvents));
    return RETURN_KO;
  }

  id = 0;
  pEvent = pEvents;
  for (car = 0; car < cars; car++) {
    memset(pPits, 0, sizeof(bool) * pParms->laps);
    if (pParms->raceType == race_SPRINT || pParms->raceType == race_GP) {
      if (pParms->raceType == race_GP) {
        pits = 1 + rand() % 4;
      } else {
        pits = ((rand() % 100) == 0);
      }
      for (pit = 0; pit < pits; pit++) {
        while (true) {
          i = rand() % pParms->laps;
          if (pPits[i] == false) {
            pPits[i] = true;
            break;
          }
        }
      }
    }

    timestamp = 0;
    pEvent->timestamp = timestamp;
    pEvent->number = pParms->raceNumber;
    pEvent->type = pParms->raceType;
    pEvent->event = event_START;
    pEvent->car = car;
    pEvent->id = id++;
    pEvent++;

    for (lap = 0; lap < pParms->laps && (maxRaceTime == 0 || timestamp < maxRaceTime); lap++) {
      timestamp += getRandomTime(25000, 45000);
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S1;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;
      pEvent++;

      timestamp += getRandomTime(25000, 45000);
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S2;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;
      pEvent++;

      if (pPits[lap]) {
        timestamp += 10000;
        pEvent->timestamp = timestamp;
        pEvent->number = pParms->raceNumber;
        pEvent->type = pParms->raceType;
        pEvent->event = event_PIT_START;
        pEvent->car = car;
        pEvent->lap = lap;
        pEvent->id = id++;
        pEvent++;

        timestamp += getRandomTime(20000, 30000);
        pEvent->timestamp = timestamp;
        pEvent->number = pParms->raceNumber;
        pEvent->type = pParms->raceType;
        pEvent->event = event_PIT_END;
        pEvent->car = car;
        pEvent->lap = lap;
        pEvent->id = id++;
        pEvent++;
      }

      timestamp += getRandomTime(25000, 45000);
      if (pPits[lap]) {
        timestamp -= 10000;
      }
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S3;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;
      pEvent++;
    }

    pEvent->timestamp = timestamp;
    pEvent->number = pParms->raceNumber;
    pEvent->type = pParms->raceType;
    pEvent->event = event_END;
    pEvent->car = car;
    pEvent->lap = lap;
    pEvent->id = id++;
    pEvent++;
  }
  events = pEvent - pEvents;

  qsort(pEvents, events, sizeof(EventRace), compareEventTimestamp);

  if (pParms->verbose) {
    pEvent = pEvents;
    for (i = 0; i < events; i++) {
      printEvent(pEvent);
      pEvent++;
    }
  }

  code = connectServer(pParms, &clientCtx);
  if (code) {
    returnCode = code;
    goto genTimeCoreException;
  }

  sleep = 0;
  pEvent = pEvents;
  for (i = 0; i < events; i++) {
    if (pEvent->timestamp > sleep) {
      usleep((pEvent->timestamp - sleep) * pSleepTime[pParms->sleepIndex]);
      sleep = pEvent->timestamp;
    }

    code = sendEvent(&clientCtx, pEvent);
    if (code) {
      printf("ERROR: unable to send event %d\n", i);
      break;
    }
    pEvent++;
  }

  returnCode = RETURN_OK;

genTimeCoreException:
  disconnectServer(&clientCtx);

  free((void *)pEvents);

  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  ProgramOptions options;
  int code;
  int opt;

  memset(&options, 0, sizeof(options));
  options.sleepIndex = false;
  options.verbose = false;

  while ((opt = getopt(argc, ppArgv, "c:d:fl:p:s:t:vh?")) != -1) {
    switch (opt) {
    case 'c':
      options.raceNumber = atoi(optarg);
      break;
    case 't':
      options.raceType = stringToRaceType(optarg);
      break;
    case 'd':
      options.duration = atoi(optarg);
      break;
    case 'f':
      options.sleepIndex++;
      if (options.sleepIndex > sizeof(pSleepTime) / sizeof(pSleepTime[0])) {
        options.sleepIndex = sizeof(pSleepTime) / sizeof(pSleepTime[0]) - 1;
      }
      break;
    case 'l':
      options.laps = atoi(optarg);
      break;
    case 'p':
      options.serverPort = atoi(optarg);
      break;
    case 's':
      options.pServerAddress = optarg;
      break;
    case 'v':
      options.verbose = true;
      break;
    case 'h':
    case '?':
    default:
      printHelp();
      return EXIT_SUCCESS;
    }
  }

  if (options.raceNumber < 1 || options.raceNumber > 24) {
    printf("ERROR: illegal race number. Valid value is between 1 and 24.\n");
    return EXIT_FAILURE;
  }

  if (options.raceType == race_ERROR) {
    printf("ERROR: illegal race type. Valid values are 'P1, 'P2', ... 'Q3', 'SPRINT' and 'GP'.\n");
    return EXIT_FAILURE;
  }

  if (options.pServerAddress == NULL) {
    printf("ERROR: unspecified server address. Please use option -s.\n");
    return EXIT_FAILURE;
  }

  if (options.serverPort == 0) {
    printf("ERROR: unspecified server port. Please use option -p.\n");
    return EXIT_FAILURE;
  }

  printf("INFO: Race number: %d\n", options.raceNumber);
  printf("INFO: Race type: %s\n", raceTypeToString(options.raceType));

  code = genTimeCore(&options);
  if (code) {
    printf("ERROR: an error was encounterred during execution of genTimeCore, code=%d\n", code);
  }

  return EXIT_SUCCESS;
}
