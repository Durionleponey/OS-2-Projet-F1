#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys\types.h>
#include <sys\stat.h>

#include <windows.h>

#include "grandPrix.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  char pFileName[MAX_PATH];
  GrandPrix grandPrix;
  int fileHandle;
  int code;

  memset(&grandPrix, 0, sizeof(GrandPrix));

  sprintf(pFileName, "GrandPrix.%d.bin", 2024);
  fileHandle = open(pFileName, O_CREAT | O_BINARY | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fileHandle == -1) {
    printf("ERROR: unable to open file %s, errno=%d\n", pFileName, errno);
    return EXIT_FAILURE;
  }

  lseek(fileHandle, sizeof(GrandPrix) * 3, SEEK_SET);

  grandPrix.grandPrixId = 1;
  grandPrix.specialGP = true;
  grandPrix.pPractices[0].type = race_P1;
  grandPrix.pPractices[1].type = race_P2;
  grandPrix.pPractices[2].type = race_P3;

  code = write(fileHandle, &grandPrix, sizeof(grandPrix));
  if (code < (int)sizeof(grandPrix)) {
    printf("ERROR: unable to serialize structure GrandPrix, errno=%d [%s]\n", errno, strerror(errno));
  }

  code = write(fileHandle, &grandPrix, sizeof(grandPrix));
  if (code < (int)sizeof(grandPrix)) {
    printf("ERROR: unable to serialize structure GrandPrix, errno=%d [%s]\n", errno, strerror(errno));
  }

  close(fileHandle);
}

/*--------------------------------------------------------------------------------------------------------------------*/
