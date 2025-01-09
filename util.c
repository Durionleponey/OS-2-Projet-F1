#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

static Level _level = log_DEBUG;
static FILE *_logFile = NULL;
static const char *const _ppLevelStr[] = {
    [log_NONE] = "",     [log_OFF] = "OFF",     [log_FATAL] = "FATAL", [log_ERROR] = "ERROR", [log_WARN] = "WARN",
    [log_INFO] = "INFO", [log_DEBUG] = "DEBUG", [log_TRACE] = "TRACE", [log_ALL] = "ALL"};

/*--------------------------------------------------------------------------------------------------------------------*/

const char *ppRaceType[] = {[race_ERROR] = "ERROR",
                            [race_P1] = "P1",
                            [race_P2] = "P2",
                            [race_P3] = "P3",
                            [race_Q1_SPRINT] = "SPRINT_Q1",
                            [race_Q2_SPRINT] = "SPRINT_Q2",
                            [race_Q3_SPRINT] = "SPRINT_Q3",
                            [race_SPRINT] = "SPRINT",
                            [race_Q1_GP] = "GP_Q1",
                            [race_Q2_GP] = "GP_Q2",
                            [race_Q3_GP] = "GP_Q3",
                            [race_GP] = "GP"};

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
    if (strcasecmp(ppRaceType[type], pType) == 0) {
      return (RaceType)type;
    }
  }

  printf("ERROR: illegal race type '%s'\n", pType);

  return race_ERROR;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToHour(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;
  int minute;
  int hour;

  milli = timeMs % 1000;
  timeMs /= 1000;
  second = timeMs % 60;
  timeMs /= 60;
  minute = timeMs % 60;
  hour = timeMs / 60;

  snprintf(pOutput, size, "%d:%02d:%02d.%03d", hour, minute, second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToMinute(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;
  int minute;

  milli = timeMs % 1000;
  timeMs /= 1000;
  second = timeMs % 60;
  minute = timeMs / 60;

  snprintf(pOutput, size, "%d:%02d.%03d", minute, second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *timestampToSeconds(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;

  milli = timeMs % 1000;
  second = timeMs / 1000;

  snprintf(pOutput, size, "%d.%03d", second, milli);

  return pOutput;
}

/*--------------------------------------------------------------------------------------------------------------------*/

char *milliToGap(uint32_t timeMs, char *pOutput, int size) {
  int milli;
  int second;

  milli = timeMs % 1000;
  second = timeMs / 1000;

  snprintf(pOutput, size, "+%d.%03ds", second, milli);

  return pOutput;
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

void logger(Level level, const char *pMessage, ...) {
  char pMessageBuffer[4096];
  struct timeval currentTime;
  struct tm timeStorage;
  va_list vaList;
  time_t timestamp;
  size_t size;
  int written;

  if (level == log_NONE || level <= _level) {
    if (_logFile == NULL) {
      _logFile = fopen("stdout.log", "a+");
    }

    gettimeofday(&currentTime, NULL);
    timestamp = (time_t)currentTime.tv_sec;
#ifdef WIN64
    localtime_s(&timeStorage, &timestamp);
#else
    localtime_r(&timestamp, &timeStorage);
#endif

    size = sprintf(pMessageBuffer, "%02d:%02d:%02d.%03d: ", timeStorage.tm_hour, timeStorage.tm_min, timeStorage.tm_sec,
                   (int)currentTime.tv_usec / 1000);
    if (level != log_NONE) {
      size += sprintf(&pMessageBuffer[size], "%s: ", _ppLevelStr[level]);
    }

    va_start(vaList, pMessage);
    size += vsnprintf(&pMessageBuffer[size], sizeof(pMessageBuffer) - size - 1, pMessage, vaList);
    if (pMessageBuffer[size - 1] != '\n') {
      pMessageBuffer[size] = '\n';
      size++;
    }
    va_end(vaList);

    written = fwrite(pMessageBuffer, 1, size, _logFile);
    if (written == -1) {
      printf("FATAL: unable to write %d bytes to log file\n", (int)size);
      exit(EXIT_FAILURE);
    }
  }
}

/*--------------------------------------------------------------------------------------------------------------------*/
