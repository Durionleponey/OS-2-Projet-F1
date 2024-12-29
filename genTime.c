#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#ifdef _WIN64
#include <windows.h>
#include <Winbase.h>
#endif

#include "grandPrix.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

typedef struct structProgramOptions {
  int raceNumber;
  RaceType raceType;
  int duration;
  int laps;
  const char *pServerAddress;
  int serverPort;
} ProgramOptions;

typedef struct structClientContext {
  struct sockaddr_in clientAddr;
  u_short clientPort;
  socket_t clientSocket;
} ClientContext;

/*--------------------------------------------------------------------------------------------------------------------*/

void printHelp(void) {
  printf("Usage:\n");
  printf("\t-c\trace number (1-24)\n");
}

/*--------------------------------------------------------------------------------------------------------------------*/

int getLapTime(EventType type) {
  return 25000 + (rand() * rand()) % 21000;
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

#ifdef _WIN64
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

  code = connect(pClientCtx->clientSocket, (SOCKADDR *)&pClientCtx->clientAddr, sizeof(pClientCtx->clientAddr));
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

#ifdef _WIN64
  WSACleanup();
#endif

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int genTimeCore(ProgramOptions *pParms) {
  ClientContext clientCtx;
  uint32_t timestamp;
  EventRace *pEvents;
  EventRace *pEvent;
  int returnCode;
  int maxEvents;
  int events;
  int sleep;
  int code;
  int cars;
  int car;
  int lap;
  int id;
  int i;

  cars = 20;

  maxEvents = cars * (5 * pParms->laps + 2);
  pEvents = (EventRace *)calloc(maxEvents, sizeof(EventRace));
  if (pEvents == NULL) {
    printf("ERROR: unable to allocate %llu bytes for events\n", sizeof(EventRace) * maxEvents);
    return RETURN_KO;
  }

  id = 0;
  pEvent = pEvents;
  for (car = 0; car < cars; car++) {
    timestamp = 0;
    pEvent->timestamp = timestamp;
    pEvent->number = pParms->raceNumber;
    pEvent->type = pParms->raceType;
    pEvent->event = event_START;
    pEvent->car = car;
    pEvent->id = id++;
    pEvent++;

    for (lap = 0; lap < pParms->laps; lap++) {
      timestamp += getLapTime(event_S1);
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S1;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;
      pEvent++;

      timestamp += getLapTime(event_S2);
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S2;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;
      pEvent++;

      timestamp += getLapTime(event_S3);
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

  pEvent = pEvents;
  for (i = 0; i < events; i++) {
    printEvent(pEvent);
    pEvent++;
  }

  code = connectServer(pParms, &clientCtx);
  if (code) {
    returnCode = code;
    goto genTimeCoreException;
  }

  sleep = 0;
  pEvent = pEvents;
  for (i = 0; i < events; i++) {
    Sleep(pEvent->timestamp - sleep);
    sleep = pEvent->timestamp;

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

  while ((opt = getopt(argc, ppArgv, "c:d:l:p:s:t:h?")) != -1) {
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
    case 'l':
      options.laps = atoi(optarg);
      break;
    case 'p':
      options.serverPort = atoi(optarg);
      break;
    case 's':
      options.pServerAddress = optarg;
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
