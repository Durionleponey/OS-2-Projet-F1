#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "csvParser.h"

/*--------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *ppArgv[]) {
  const char *pFilePath;
  CsvParser *pCsvParser;
  CsvRow *pCsvHeader;
  CsvRow *pCsvRow;
  bool header;
  int i;
  int opt;

  header = false;
  while ((opt = getopt(argc, ppArgv, "af:h?")) != -1) {
    switch (opt) {
    case 'a':
      header = true;
      break;
    case 'f':
      pFilePath = optarg;
      break;
    case 'h':
    case '?':
    default:
      return EXIT_SUCCESS;
    }
  }

  pCsvParser = csvParserCreate(pFilePath, NULL, header);
  if (pCsvParser == NULL) {
    printf("ERROR: unable to create new parser for '%s'.\n", pFilePath);
    return EXIT_FAILURE;
  }

  if (header) {
    pCsvHeader = csvParserGetHeader(pCsvParser);
    if (pCsvHeader == NULL) {
      printf("ERROR: unable to create get CSV headers, error=%s.\n", csvParserGetErrorMessage(pCsvParser));
      return EXIT_FAILURE;
    }

    for (i = 0; i < pCsvHeader->fields; i++) {
      printf("Header #%d: '%s'\n", i, pCsvHeader->ppFields[i]);
    }
  }

  while (true) {
    pCsvRow = csvParserGetRow(pCsvParser);
    if (pCsvRow == NULL) {
      break;
    }
    printf("{");
    for (i = 0; i < pCsvRow->fields; i++) {
      if (i > 0) {
        printf(",");
      }
      if (header) {
        printf("\"%s\": \"%s\"", pCsvHeader->ppFields[i], pCsvRow->ppFields[i]);
      } else {
        printf("\"FIELD_%d\": \"%s\"", i, pCsvRow->ppFields[i]);
      }
    }
    printf("}\n");
    csvParserDestroyRow(pCsvRow);
  }

  if (header) {
    csvParserDestroyRow(pCsvHeader);
  }
  csvParserDestroy(pCsvParser);

  return EXIT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------------------*/
