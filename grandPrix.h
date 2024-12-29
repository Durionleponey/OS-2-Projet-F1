#ifndef GRAND_PRIX_H
#define GRAND_PRIX_H

#include <stdint.h>

/*--------------------------------------------------------------------------------------------------------------------*/

#define RETURN_OK 0
#define RETURN_KO 1

#define MAX_CARS 20

#ifdef _WIN64
#define socket_t SOCKET
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
  race_Q1,
  race_Q2,
  race_Q3,
  race_SPRINT,
  race_GP,
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

/*--------------------------------------------------------------------------------------------------------------------*/

#endif
