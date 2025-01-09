#ifndef GRAND_PRIX_H
#define GRAND_PRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

/*--------------------------------------------------------------------------------------------------------------------*/

#define RETURN_OK 0
#define RETURN_KO 1

#define MAX_DRIVERS 20
#define MAX_GP 24

#ifdef WIN64
#define socket_t SOCKET
#define socklen_t int
#else
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int socket_t;
#define closesocket(x) close(x)
#define WSAGetLastError() errno
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

typedef struct structPracticeItem {
  int carId;
  uint32_t bestLapTime;
  int bestLap;
  uint32_t bestS1;
  uint32_t bestS2;
  uint32_t bestS3;
} PracticeItem;

typedef struct structPractice {
  RaceType type;
  PracticeItem pItems[MAX_DRIVERS];
} Practice;

typedef struct structQualificationItem {
  int carId;
  uint32_t bestLapTime;
  int bestLap;
  uint32_t bestS1;
  uint32_t bestS2;
  uint32_t bestS3;
} QualificationItem;

typedef struct structQualification {
  RaceType type;
  QualificationItem pItems[MAX_DRIVERS];
} Qualification;

typedef struct structSprintItem {
  int carId;
  uint32_t racingTime;
} SprintItem;

typedef struct structSprint {
  RaceType type;
  SprintItem pItems[MAX_DRIVERS];
} Sprint;

typedef struct structRaceItem {
  int carId;
  uint32_t racingTime;
  int pits;
  uint32_t pitsTime;
} RaceItem;

typedef struct structRace {
  RaceType type;
  RaceItem pItems[MAX_DRIVERS];
} Race;

typedef struct structGrandPrix {
  int grandPrixId;
  bool specialGP;
  RaceType nextStep;
  Practice pPractices[3];
  Qualification pSprintShootout[3];
  Sprint sprint;
  Qualification pQualifications[3];
  Race race;
} GrandPrix;

typedef struct structStandingsTableItem {
  int carId;
  int points;
} StandingsTableItem;

typedef struct structStandingsTable {
  StandingsTableItem pItems[MAX_DRIVERS];
} StandingsTable;

/*--------------------------------------------------------------------------------------------------------------------*/

#endif
