#ifndef UTIL_H
#define UTIL_H

#include "grandPrix.h"

/*--------------------------------------------------------------------------------------------------------------------*/

extern void printEvent(EventRace *pEvent);
extern const char *raceTypeToString(RaceType type);
extern RaceType stringToRaceType(const char *pType);

/*--------------------------------------------------------------------------------------------------------------------*/

#endif
