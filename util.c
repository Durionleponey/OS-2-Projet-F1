#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

const char *ppRaceType[] = {
    [race_ERROR] = "ERROR", [race_P1] = "P1", [race_P2] = "P2",         [race_P3] = "P3", [race_Q1] = "Q1",
    [race_Q2] = "Q2",       [race_Q3] = "Q3", [race_SPRINT] = "SPRINT", [race_GP] = "GP"};

/*--------------------------------------------------------------------------------------------------------------------*/

const char *raceTypeToString(RaceType type) {
  if (type < 0 || type >= race_MAX) {
    return ppRaceType[race_ERROR];
  }
  return ppRaceType[type];
}

/*--------------------------------------------------------------------------------------------------------------------*/

RaceType stringToRaceType(const char *pType) {
  int type;

  for (type = 1; type < race_MAX; type++) {
    if (strcmp(ppRaceType[type], pType) == 0) {
      return (RaceType)type;
    }
  }

  printf("ERROR: illegal race type '%s'\n", pType);

  return race_ERROR;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void printEvent(EventRace *pEvent) {
  printf("*-----\n");
  printf("Type: %s\n", raceTypeToString(pEvent->type));
  printf("Race number: %d\n", pEvent->number);
  printf("Event: %d\n", pEvent->event);
  printf("Car number: %d\n", pEvent->car);
  printf("Lap number: %d\n", pEvent->lap);
  printf("Timestamp: %u\n", pEvent->timestamp);
}

/*--------------------------------------------------------------------------------------------------------------------*/
