#ifndef GRAND_PRIX_H
#define GRAND_PRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

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
#include <linux/limits.h>
#endif

#include "csvParser.h"

/*--------------------------------------------------------------------------------------------------------------------*/

#define RETURN_OK 0
#define RETURN_KO 1

#define MAX_DRIVERS 20
#define MAX_DRIVERS_Q1 MAX_DRIVERS
#define MAX_DRIVERS_Q2 15
#define MAX_DRIVERS_Q3 10
#define MAX_GP 24

#define MILLI_PER_MINUTE (60 * 1000)

#ifdef WIN64
#define socket_t SOCKET
#define socklen_t int
#else
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int socket_t;
#define closesocket(x) close(x)
#define WSAGetLastError() errno
#define O_BINARY 0
#define O_TEXT 0
#endif

/*--------------------------------------------------------------------------------------------------------------------*/

typedef enum enumEventType {
  event_ERROR,
  event_START,
  event_S1,
  event_S2,
  event_S3,
  event_PIT_START,
  event_PIT_END,
  event_OUT,
  event_END
} EventType;

typedef enum enumRaceType {
  race_ERROR,
  race_P1,
  race_P2,
  race_P3,
  race_Q1_SPRINT,
  race_Q2_SPRINT,
  race_Q3_SPRINT,
  race_SPRINT,
  race_Q1_GP,
  race_Q2_GP,
  race_Q3_GP,
  race_GP,
  race_FINISHED,
  race_MAX
} RaceType;

typedef struct structEventRace {
  int id;
  int number;
  RaceType type;
  int car;
  int lap;
  EventType event;
  uint32_t timestamp; // time in milliseconds
} EventRace;

typedef struct structRaceInfo {
  int carId;
  int pits;
  uint32_t raceTime;
  uint32_t pitsTime;
  uint32_t bestLapTime;
  int bestLap;
  uint32_t bestS1;
  uint32_t bestS2;
  uint32_t bestS3;
} RaceInfo;

typedef struct structRace {
  RaceType type;
  RaceInfo pItems[MAX_DRIVERS];
} Race;

typedef struct structGrandPrix {
  int grandPrixId;
  bool specialGP;
  RaceType nextStep;
  Race pPractices[3];
  Race pSprintShootout[3];
  Race sprint;
  Race pQualifications[3];
  Race final;
} GrandPrix;

typedef struct structStandingsTableItem {
  int carId;
  int points;
} StandingsTableItem;

typedef struct structStandingsTable {
  StandingsTableItem pItems[MAX_DRIVERS];
} StandingsTable;


typedef struct structContext {
  CsvRow **ppCsvGrandPrix;
  CsvRow **ppCsvDrivers;
  StandingsTable standingsTable;
  GrandPrix *pGrandPrix;
  int gpHistoricHandle;
  int currentGP;
  int gpYear;
  int speedFactor;
  bool autoLaunch;
  WINDOW *pWindow;
} Context;

/*--------------------------------------------------------------------------------------------------------------------*/

extern const int pSprintScores[];
extern const int pGrandPrixScores[];

/*--------------------------------------------------------------------------------------------------------------------*/

extern int compareStandingsTableItem(const void *pA, const void *pB);

/*--------------------------------------------------------------------------------------------------------------------*/

#endif
