#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#ifdef LINUX
#include <linux/limits.h>
#endif

#include "saveFile.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

int saveRace(Context *pCtx, FILE *pFile, Race *pRace) {
  uint32_t previousTime;
  RaceInfo *pRaceInfo;
  char **ppDriverInfo;
  char pBestLapTime[32];
  char pBestS1Time[32];
  char pBestS2Time[32];
  char pBestS3Time[32];
  char pTotalTime[32];
  char pGapTime[32];
  int carId;
  int cars;
  int i;

  pRaceInfo = pRace->pItems;
  fprintf(pFile, "\n\n");
  fprintf(pFile, "Epreuve: %s\n", raceTypeToString(pRace->type));
  fprintf(pFile, "Pos   Number     Driver                      Team                       Best S1  Best S2  "
                 "Best S3   Best lap time     Gap     Laps   Total time\n");
  fprintf(pFile, "------------------------------------------------------------------------------------------"
                 "------------------------------------------------------\n");

  if (pRace->type == race_Q2_GP || pRace->type == race_Q2_SPRINT) {
    cars = MAX_DRIVERS_Q2;
  } else if (pRace->type == race_Q3_GP || pRace->type == race_Q3_SPRINT) {
    cars = MAX_DRIVERS_Q3;
  } else {
    cars = MAX_DRIVERS_Q1;
  }
  for (i = 0; i < cars; i++) {
    carId = pRaceInfo->carId;
    ppDriverInfo = pCtx->ppCsvDrivers[carId]->ppFields;

    pBestLapTime[0] = 0;
    pGapTime[0] = 0;
    pBestS1Time[0] = pBestS2Time[0] = pBestS3Time[0] = 0;
    if (pRaceInfo->bestLapTime > 0) {
      timestampToSecond(pRaceInfo->bestS1, pBestS1Time, sizeof(pBestS1Time));
      timestampToSecond(pRaceInfo->bestS2, pBestS2Time, sizeof(pBestS2Time));
      timestampToSecond(pRaceInfo->bestS3, pBestS3Time, sizeof(pBestS3Time));
      timestampToMinute(pRaceInfo->bestLapTime, pBestLapTime, sizeof(pBestLapTime));
      timestampToHour(pRaceInfo->raceTime, pTotalTime, sizeof(pTotalTime));
      if (i == 0) {
        previousTime = pRaceInfo->bestLapTime;
      } else {
        milliToGap(pRaceInfo->bestLapTime - previousTime, pGapTime, sizeof(pGapTime));
      }
    }

    fprintf(pFile, "%3d  %5s    %-20.20s   %-30.30s   %8s %8s %8s     %10s   %10s  %3d    %10s\n", i + 1,
            ppDriverInfo[0], ppDriverInfo[1], ppDriverInfo[2], pBestS1Time, pBestS2Time, pBestS3Time, pBestLapTime,
            pGapTime, pRaceInfo->bestLap + 1, pTotalTime);
    pRaceInfo++;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int saveSprintOrFinal(Context *pCtx, FILE *pFile, Race *pRace) {
  uint32_t previousTime;
  RaceInfo *pRaceInfo;
  char **ppDriverInfo;
  char pBestLapTime[32];
  char pBestS1Time[32];
  char pBestS2Time[32];
  char pBestS3Time[32];
  char pTotalTime[32];
  char pPitsTime[32];
  char pGapTime[32];
  int carId;
  int cars;
  int i;

  pRaceInfo = pRace->pItems;
  fprintf(pFile, "\n\n");
  fprintf(pFile, "Epreuve: %s\n", raceTypeToString(pRace->type));
  fprintf(pFile, "Pos   Number     Driver                      Team                       Best S1  Best S2  "
                 "Best S3   Best lap time   Laps   Pits  Pits time   Total time     Gap      Points\n");
  fprintf(pFile, "------------------------------------------------------------------------------------------"
                 "---------------------------------------------------------------------------------\n");

  if (pRace->type == race_Q2_GP || pRace->type == race_Q2_SPRINT) {
    cars = MAX_DRIVERS_Q2;
  } else if (pRace->type == race_Q3_GP || pRace->type == race_Q3_SPRINT) {
    cars = MAX_DRIVERS_Q3;
  } else {
    cars = MAX_DRIVERS_Q1;
  }
  for (i = 0; i < cars; i++) {
    carId = pRaceInfo->carId;
    ppDriverInfo = pCtx->ppCsvDrivers[carId]->ppFields;

    pBestLapTime[0] = 0;
    pGapTime[0] = 0;
    pBestS1Time[0] = pBestS2Time[0] = pBestS3Time[0] = 0;
    if (pRaceInfo->bestLapTime > 0) {
      timestampToSecond(pRaceInfo->bestS1, pBestS1Time, sizeof(pBestS1Time));
      timestampToSecond(pRaceInfo->bestS2, pBestS2Time, sizeof(pBestS2Time));
      timestampToSecond(pRaceInfo->bestS3, pBestS3Time, sizeof(pBestS3Time));
      timestampToMinute(pRaceInfo->bestLapTime, pBestLapTime, sizeof(pBestLapTime));
      timestampToMinute(pRaceInfo->pitsTime, pPitsTime, sizeof(pPitsTime));
      timestampToHour(pRaceInfo->raceTime, pTotalTime, sizeof(pTotalTime));
    }
    if (i == 0) {
      previousTime = pRaceInfo->raceTime;
    } else {
      milliToGap(pRaceInfo->raceTime - previousTime, pGapTime, sizeof(pGapTime));
    }

    fprintf(pFile, "%3d  %5s    %-20.20s   %-30.30s   %8s %8s %8s     %10s     %3d   %3d   %10s  %10s %10s  %4d\n",
            i + 1, ppDriverInfo[0], ppDriverInfo[1], ppDriverInfo[2], pBestS1Time, pBestS2Time, pBestS3Time,
            pBestLapTime, pRaceInfo->bestLap + 1, pRaceInfo->pits, pPitsTime, pTotalTime, pGapTime,
            pRace->type == race_SPRINT ? pSprintScores[i] : pGrandPrixScores[i]);
    pRaceInfo++;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int saveStartingGrid(Context *pCtx, FILE *pFile, RaceType type, Race *pRace) {
  RaceInfo *pRaceInfo;
  char **ppDriverInfo;
  char pBestLapTime[32];
  int carId;
  int i;

  pRaceInfo = pRace->pItems;
  fprintf(pFile, "\n\n");
  fprintf(pFile, "Grille de départ: %s\n", raceTypeToString(type));
  fprintf(pFile, "Pos   Number     Driver                      Team                       Best time\n");
  fprintf(pFile, "---------------------------------------------------------------------------------\n");

  for (i = 0; i < MAX_DRIVERS; i++) {
    if (i == 0) {
      pRaceInfo = (RaceInfo *)pRace[2].pItems;
    } else if (i == MAX_DRIVERS_Q3) {
      pRaceInfo = (RaceInfo *)&pRace[1].pItems[MAX_DRIVERS_Q3];
    } else if (i == MAX_DRIVERS_Q2) {
      pRaceInfo = (RaceInfo *)&pRace[0].pItems[MAX_DRIVERS_Q2];
    }

    carId = pRaceInfo->carId;
    ppDriverInfo = pCtx->ppCsvDrivers[carId]->ppFields;

    fprintf(pFile, "%3d  %5s    %-20.20s   %-30.30s    %10s\n", i + 1, ppDriverInfo[0], ppDriverInfo[1],
            ppDriverInfo[2], timestampToMinute(pRaceInfo->bestLapTime, pBestLapTime, sizeof(pBestLapTime)));
    pRaceInfo++;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int saveStandings(Context *pCtx, FILE *pFile) {
  StandingsTableItem *pStandingsTableItem;
  RaceInfo *pSprintInfo;
  RaceInfo *pGrandPrixInfo;
  GrandPrix *pGrandPrix;
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

  fprintf(pFile, "\n\n");
  fprintf(pFile, "Classement général\n");
  fprintf(pFile, "Pos   Number     Driver                      Team                   Total points\n");
  fprintf(pFile, "--------------------------------------------------------------------------------\n");

  pStandingsTableItem = (StandingsTableItem *)pCtx->standingsTable.pItems;
  for (i = 0; i < MAX_DRIVERS; i++) {
    carId = pStandingsTableItem->carId;
    ppFields = pCtx->ppCsvDrivers[carId]->ppFields;
    fprintf(pFile, "%3d  %5s    %-20.20s   %-30.30s   %5d\n", i + 1, ppFields[0], ppFields[1], ppFields[2],
            pStandingsTableItem->points);
    pStandingsTableItem++;
  }

  return RETURN_OK;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int saveGrandPrixToFile(Context *pCtx, int grandPrixId) {
  char pFileName[PATH_MAX];
  GrandPrix *pGrandPrix;
  Race *pRace;
  bool special;
  char **ppGPInfo;
  FILE *pFile;
  int returnCode;
  int code;
  int i;

  pGrandPrix = &pCtx->pGrandPrix[grandPrixId];
  special = pGrandPrix->specialGP;
  ppGPInfo = pCtx->ppCsvGrandPrix[grandPrixId]->ppFields;

  sprintf(pFileName, "GrandPrix_%d_%d.txt", pCtx->gpYear, grandPrixId + 1);
  pFile = fopen(pFileName, "w");
  if (pFile == NULL) {
    logger(log_ERROR, "an error has occurred while opening file %s for writing, errno=%d\n", pFileName, errno);
    return RETURN_KO;
  }

  fprintf(pFile, "Championnat %d Formule 1 Grand Prix %s/%s\n", pCtx->gpYear, ppGPInfo[0], ppGPInfo[1]);

  returnCode = RETURN_OK;

  for (i = 0; i < (special ? 1 : 3); i++) {
    pRace = &pGrandPrix->pPractices[i];
    if (pRace->type == race_ERROR) {
      goto saveGrandPrixToFileExit;
    }
    code = saveRace(pCtx, pFile, pRace);
    if (code) {
      returnCode = code;
      goto saveGrandPrixToFileExit;
    }
  }

  if (special) {
    for (i = 0; i < 3; i++) {
      pRace = &pGrandPrix->pSprintShootout[i];
      if (pRace->type == race_ERROR) {
        goto saveGrandPrixToFileExit;
      }
      code = saveRace(pCtx, pFile, pRace);
      if (code) {
        returnCode = code;
        goto saveGrandPrixToFileExit;
      }
    }

    code = saveStartingGrid(pCtx, pFile, race_SPRINT, pGrandPrix->pSprintShootout);
    if (code) {
      returnCode = code;
      goto saveGrandPrixToFileExit;
    }

    pRace = &pGrandPrix->sprint;
    if (pRace->type == race_ERROR) {
      goto saveGrandPrixToFileExit;
    }
    code = saveSprintOrFinal(pCtx, pFile, pRace);
    if (code) {
      returnCode = code;
      goto saveGrandPrixToFileExit;
    }
  }

  for (i = 0; i < 3; i++) {
    pRace = &pGrandPrix->pQualifications[i];
    if (pRace->type == race_ERROR) {
      goto saveGrandPrixToFileExit;
    }
    code = saveRace(pCtx, pFile, pRace);
    if (code) {
      returnCode = code;
      goto saveGrandPrixToFileExit;
    }
  }

  code = saveStartingGrid(pCtx, pFile, race_GP, pGrandPrix->pQualifications);
  if (code) {
    returnCode = code;
    goto saveGrandPrixToFileExit;
  }

  pRace = &pGrandPrix->final;
  if (pRace->type == race_ERROR) {
    goto saveGrandPrixToFileExit;
  }
  code = saveSprintOrFinal(pCtx, pFile, pRace);
  if (code) {
    returnCode = code;
    goto saveGrandPrixToFileExit;
  }

saveGrandPrixToFileExit:
  if (code == RETURN_OK) {
    saveStandings(pCtx, pFile);
  }

  fclose(pFile);

  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/
