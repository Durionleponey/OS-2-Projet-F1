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
  printf("-c 1 -t P1 -s 127.0.0.1 -p 1111 -l 57 -w bandicoot");
  printf("-w is juste for fun ^^");
}

/*--------------------------------------------------------------------------------------------------------------------*/

int getRandomTime(int minTime, int maxTime) {
  return minTime + (unsigned int)((rand() * rand())) % (maxTime - minTime + 1);
  //rand() * rand() to incread the scop and reasign to unsigned int to avoid overflow
  //% give a number between maxTime and minTime

}

/*--------------------------------------------------------------------------------------------------------------------*/
//compare and 2 EventRace type
int compareEventTimestamp(const void *pLeft, const void *pRight) {//EventRace
  //void mean that type is not set != const int *a
  int compare;

  compare = ((EventRace *)pLeft)->timestamp - ((EventRace *)pRight)->timestamp;
  //EventRace cast: act as pLeft is a pointer to a EnventRace  acces to timestamp from pLeft structure
  //compare witch timestand
  if (compare == 0) {
    compare = ((EventRace *)pLeft)->id - ((EventRace *)pRight)->id;
  }
  //compare with id
  return compare;
}

/*--------------------------------------------------------------------------------------------------------------------*/
//send bit to the socket
int writeFully(socket_t socket, void *pBuffer, int size) {
  uint8_t *pRecvBuffer;
  //8bit unsign integer
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



/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------------------------------------------------*/



// typedef struct structEventRace {
//   int id;
//   int number;
//   RaceType type;
//   int car;
//   int lap;
//   EventType event;
//   uint32_t timestamp; // time in milliseconds
// } EventRace;

int genTimeCore(ProgramOptions *pParms) {
  uint32_t maxRaceTime;
  //non sign int
  uint32_t timestamp;
  EventRace *pEvents;  //hey this pointeur exist and is of type EventRace
  EventRace *pEvent; //hey this other pointeur exist and is of type EventRace
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
  //init the random number generator
  cars = MAX_DRIVERS;//20

  if (pParms->raceType == race_P1 || pParms->raceType == race_P2 || pParms->raceType == race_P3) {//si c'est practice
    maxRaceTime = 60 * MILLI_PER_MINUTE;//1000
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

  printf("maxRaceTime 🏃🏃 = %d\n", maxRaceTime);

  if (!maxRaceTime) {
    printf("no max run time, the event is a GP i guess!\n");
  }

  if (pParms->verbose) {
    printf("verbose info:");//on affiche les infos, ou limité par un le nombre de tour ou limité par le nombre de minute
    if (maxRaceTime == 0) {
      printf("INFO: the race events generation will be limited to %d laps\n", pParms->laps);
    } else {
      printf("INFO: the race events generation will be limited to %d minutes\n", maxRaceTime / MILLI_PER_MINUTE);
    }
  }

  pPits = (bool *)malloc(pParms->laps * sizeof(bool));
  //assignement of a range of memory of type bool -> number of laps * sizeof(a bool)

  maxEvents = cars * (5 * pParms->laps + 2);
  //+2 start and finish ?

  printf("maxEvents 🥔🥔🥔---> %i\n", maxEvents);

  pEvents = (EventRace *)calloc(maxEvents, sizeof(EventRace));
  //calloc all octets set to zero and give the adress to pEvents

  if (pEvents == NULL) {
    printf("ERROR: unable to allocate %lu bytes for events\n", (u_long)(sizeof(EventRace) * maxEvents));
    return RETURN_KO;
  }

  id = 0;
  pEvent = pEvents;
  for (car = 0; car < cars; car++) {//cars=20
    memset(pPits, 0, sizeof(bool) * pParms->laps);

    //printf("-->%d",pParms->raceType);
    if (pParms->raceType == race_SPRINT || pParms->raceType == race_GP) {
      if (pParms->raceType == race_GP) {
        pits = 1 + rand() % 4;
        //rand() % 4 ---> (0,3)
        //(0,3)+1 = (1,4)
        printf("voiture num-----> 🏎️🏎️🏎️ %i  ", car);
        printf("pilote ????");
        printf("nombre de pit stop-----> 🥂 %i\n\n", pits);
      } else {
        pits = ((rand() % 100) == 0);
        //une chance sur 100 d'avoir un arret si on est pas en course
      }
      for (pit = 0; pit < pits; pit++) {
        while (true) {
          i = rand() % pParms->laps;
          //i = (0-nbrLaps)
          if (pPits[i] == false) {
            pPits[i] = true;
            printf("there will be a pit stop a lap-->%i 🛑\n",i);
            break;
          }
        }

      }

      // for (int i = 0; i < pParms->laps; i++) {
      //   printf("pPits[%d] = %d\n", i, pPits[i]);
      // }

      for (int i = 0; i < pParms->laps; i++) {
        printf("%d", pPits[i]);
      }
      printf("\n");



      //printf("")


    }


    //start pour toutes les voitures
    timestamp = 0;
    pEvent->timestamp = timestamp;
    pEvent->number = pParms->raceNumber;
    pEvent->type = pParms->raceType;
    pEvent->event = event_START;
    pEvent->car = car;
    pEvent->id = id++;

    printf("START timestamp 0🏁🏁🏁\n");
    printf("Event: %d\n\n\n\n\n", pEvent->event);

    //evenement suivant pour la voiture
    pEvent++;

    for (lap = 0; lap < pParms->laps && (maxRaceTime == 0 || timestamp < maxRaceTime); lap++) {//pour le nombre de tour ou le temp

      //first gate
      timestamp += getRandomTime(25000, 45000);//random number + current time
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S1;//replace with the value in enum --> no magic number
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;


      printf(" S1 timestamp: %d\n", pEvent->timestamp);
      printf("Event: %d\n", pEvent->event);
      pEvent++;

      //second gate
      timestamp += getRandomTime(25000, 45000);
      pEvent->timestamp = timestamp;
      pEvent->number = pParms->raceNumber;
      pEvent->type = pParms->raceType;
      pEvent->event = event_S2;
      pEvent->car = car;
      pEvent->lap = lap;
      pEvent->id = id++;


      printf(" S2 timestamp: %d\n", pEvent->timestamp);
      printf("Event: %d\n", pEvent->event);
      pEvent++;

      if (pPits[lap]) {//if pPits
        timestamp += 10000;
        pEvent->timestamp = timestamp;
        pEvent->number = pParms->raceNumber;
        pEvent->type = pParms->raceType;
        pEvent->event = event_PIT_START;
        pEvent->car = car;
        pEvent->lap = lap;
        pEvent->id = id++;


        printf(" event_PIT_START timestamp: %d\n", pEvent->timestamp);
        printf("Event: %d\n", pEvent->event);
        pEvent++;

        timestamp += getRandomTime(20000, 30000);
        pEvent->timestamp = timestamp;
        pEvent->number = pParms->raceNumber;
        pEvent->type = pParms->raceType;
        pEvent->event = event_PIT_END;
        pEvent->car = car;
        pEvent->lap = lap;
        pEvent->id = id++;


        printf(" event_PIT_END timestamp: %d\n", pEvent->timestamp);
        printf("Event: %d\n", pEvent->event);
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


      printf(" S3 timestamp: %d\n", pEvent->timestamp);
      printf("Event: %d\n", pEvent->event);
      pEvent++;
    }

    pEvent->timestamp = timestamp;
    pEvent->number = pParms->raceNumber;
    pEvent->type = pParms->raceType;
    pEvent->event = event_END;
    pEvent->car = car;
    pEvent->lap = lap;
    pEvent->id = id++;



    printf(" event_END timestamp: %d\n", pEvent->timestamp);
    printf("Event: %d\n\n\n\n\n", pEvent->event);
    pEvent++;
  }
  events = pEvent - pEvents;
  //events = event lower pointer - higher pointer

  qsort(pEvents, events, sizeof(EventRace), compareEventTimestamp);

  if (pParms->verbose) {
    pEvent = pEvents;
    for (i = 0; i < events; i++) {
      printEvent(pEvent);
      pEvent++;
    }
  }



  sleep = 0;
  pEvent = pEvents;
  for (i = 0; i < events; i++) {
    if (pEvent->timestamp > sleep) {
      usleep((pEvent->timestamp - sleep) * pSleepTime[pParms->sleepIndex]);
      sleep = pEvent->timestamp;
    }

    if (code) {
      printf("ERROR: unable to send event %d\n", i);
      break;
    }
    pEvent++;
  }

  returnCode = RETURN_OK;

genTimeCoreException:

  free((void *)pEvents);

  return returnCode;
}

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  //number of para + [char] -> parametter
  ProgramOptions options;


  int code;
  int opt;

  memset(&options, 0, sizeof(options));
  //take options pointeur and put 0 on every things
  options.sleepIndex = false;
  options.verbose = false;

  while ((opt = getopt(argc, ppArgv, "w:c:d:fl:p:s:t:vh?")) != -1) {
    //getopt is a build in function

    //when : wait for a value when no : wait for a bool
    switch (opt) {
    case 'w':
      printf("w selected!--> %s\n",optarg);
      break;
    case 'c':
      options.raceNumber = atoi(optarg);
      printf("race Number %d\n",atoi(optarg));
      //atoi char* --> int
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
      if (options.laps < 4) {
        printf("number of tour must be > 4! (if pist stop random > 4 && number of tour > 4 --> infinite loop 😱😱😱)\n");
        return 1;
      }
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
  printf("INFO: Race number of lap: %d\n", options.laps);



  code = genTimeCore(&options);

  if (code) {
    printf("ERROR: an error was encounterred during execution of genTimeCore, code=%d\n", code);
  }

  return EXIT_SUCCESS;
}
