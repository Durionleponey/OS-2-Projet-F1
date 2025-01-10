#ifndef UTIL_H
#define UTIL_H

#include "grandPrix.h"

/*--------------------------------------------------------------------------------------------------------------------*/

typedef enum enumLevel {
  log_NONE,
  log_OFF,
  log_FATAL,
  log_ERROR,
  log_WARN,
  log_INFO,
  log_DEBUG,
  log_TRACE,
  log_ALL
} Level;

/*--------------------------------------------------------------------------------------------------------------------*/

extern void logger(Level level, const char *pMessage, ...);
extern char *timestampToHour(uint32_t timeMs, char *pOutput, int size);
extern char *timestampToMinute(uint32_t timeMs, char *pOutput, int size);
extern char *timestampToSecond(uint32_t timeMs, char *pOutput, int size);
extern char *milliToGap(uint32_t timeMs, char *pOutput, int size);
extern void printEvent(EventRace *pEvent);
extern const char *raceTypeToString(RaceType type);
extern RaceType stringToRaceType(const char *pType);

/*--------------------------------------------------------------------------------------------------------------------*/

#endif
