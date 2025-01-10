#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "csvParser.h"

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

CsvRow *_csvParserGetRow(CsvParser *pCsvParser);
int _csvParserDelimiterIsAccepted(const char *pDelimiter);
void _csvParserSetErrorMessage(CsvParser *pCsvParser, const char *pErrorMessage);

/*--------------------------------------------------------------------------------------------------------------------*/

CsvParser *csvParserCreate(const char *pFilePath, const char *pDelimiter, bool isFirstLineHeader) {
  CsvParser *pCsvParser;

  pCsvParser = (CsvParser *)malloc(sizeof(CsvParser));
  if (pFilePath == NULL) {
    pCsvParser->pFilePath = NULL;
  } else {
    pCsvParser->pFilePath = strdup(pFilePath);
  }
  pCsvParser->isFirstLineHeader = isFirstLineHeader;
  pCsvParser->pErrorMessage = NULL;
  if (pDelimiter == NULL) {
    pCsvParser->delimiter = ',';
  } else if (_csvParserDelimiterIsAccepted(pDelimiter)) {
    pCsvParser->delimiter = *pDelimiter;
  } else {
    pCsvParser->delimiter = 0;
  }
  pCsvParser->pHeader = NULL;
  pCsvParser->pFileHandler = NULL;
  pCsvParser->fromString = 0;
  pCsvParser->pCsvString = NULL;
  pCsvParser->csvStringIter = 0;

  return pCsvParser;
}

/*--------------------------------------------------------------------------------------------------------------------*/

CsvParser *csvParserCreateFromString(const char *pCsvString, const char *pDelimiter, bool isFirstLineHeader) {
  CsvParser *pCsvParser;

  pCsvParser = csvParserCreate(NULL, pDelimiter, isFirstLineHeader);
  pCsvParser->fromString = true;
  if (pCsvString != NULL) {
    pCsvParser->pCsvString = strdup(pCsvString);
  }
  return pCsvParser;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void csvParserDestroy(CsvParser *pCsvParser) {
  if (pCsvParser == NULL) {
    return;
  }
  if (pCsvParser->pFilePath != NULL) {
    free(pCsvParser->pFilePath);
  }
  if (pCsvParser->pErrorMessage != NULL) {
    free(pCsvParser->pErrorMessage);
  }
  if (pCsvParser->pFileHandler != NULL) {
    fclose(pCsvParser->pFileHandler);
  }
  if (pCsvParser->pHeader != NULL) {
    csvParserDestroyRow(pCsvParser->pHeader);
  }
  if (pCsvParser->pCsvString != NULL) {
    free(pCsvParser->pCsvString);
  }
  free(pCsvParser);
}

/*--------------------------------------------------------------------------------------------------------------------*/

void csvParserDestroyRow(const CsvRow *pCsvRow) {
  int i;

  for (i = 0; i < pCsvRow->fields; i++) {
    free(pCsvRow->ppFields[i]);
  }
  free(pCsvRow->ppFields);
  free((void *)pCsvRow);
}

/*--------------------------------------------------------------------------------------------------------------------*/

CsvRow *csvParserGetHeader(CsvParser *pCsvParser) {
  if (!pCsvParser->isFirstLineHeader) {
    _csvParserSetErrorMessage(pCsvParser, "Cannot supply header, as current CsvParser object does not support header");
    return NULL;
  }
  if (pCsvParser->pHeader == NULL) {
    pCsvParser->pHeader = _csvParserGetRow(pCsvParser);
  }
  return pCsvParser->pHeader;
}

/*--------------------------------------------------------------------------------------------------------------------*/

CsvRow *csvParserGetRow(CsvParser *pCsvParser) {
  if (pCsvParser->isFirstLineHeader && pCsvParser->pHeader == NULL) {
    pCsvParser->pHeader = _csvParserGetRow(pCsvParser);
  }
  return _csvParserGetRow(pCsvParser);
}

/*--------------------------------------------------------------------------------------------------------------------*/

int csvParserGetNumFields(const CsvRow *pCsvRow) {
  return pCsvRow->fields;
}

/*--------------------------------------------------------------------------------------------------------------------*/

const char **csvParserGetFields(const CsvRow *pCsvRow) {
  return (const char **)pCsvRow->ppFields;
}

/*--------------------------------------------------------------------------------------------------------------------*/

CsvRow *_csvParserGetRow(CsvParser *pCsvParser) {
  int numRowRealloc = 0;
  int acceptedFields = 64;
  int acceptedCharsInField = 64;

  if (pCsvParser->pFilePath == NULL && (!pCsvParser->fromString)) {
    _csvParserSetErrorMessage(pCsvParser, "Supplied CSV file path is NULL");
    return NULL;
  }
  if (pCsvParser->pCsvString == NULL && pCsvParser->fromString) {
    _csvParserSetErrorMessage(pCsvParser, "Supplied CSV string is NULL");
    return NULL;
  }
  if (pCsvParser->delimiter == '\0') {
    _csvParserSetErrorMessage(pCsvParser, "Supplied delimiter is not supported");
    return NULL;
  }
  if (!pCsvParser->fromString) {
    if (pCsvParser->pFileHandler == NULL) {
      pCsvParser->pFileHandler = fopen(pCsvParser->pFilePath, "r");
      if (pCsvParser->pFileHandler == NULL) {
        int errorNum = errno;
        const char *errStr = strerror(errorNum);
        char *errMsg = (char *)malloc(1024 + strlen(errStr));
        sprintf(errMsg, "Error opening CSV file for reading: %s : %s", pCsvParser->pFilePath, errStr);
        _csvParserSetErrorMessage(pCsvParser, errMsg);
        free(errMsg);
        return NULL;
      }
    }
  }

  CsvRow *pCsvRow = (CsvRow *)malloc(sizeof(CsvRow));
  pCsvRow->ppFields = (char **)malloc(acceptedFields * sizeof(char *));
  pCsvRow->fields = 0;
  int fieldIter = 0;
  char *currField = (char *)malloc(acceptedCharsInField);
  int insideComplexField = 0;
  int currFieldCharIter = 0;
  int seriesOfQuotesLength = 0;
  bool isLastCharQuote = false;
  bool isEndOfFile = false;

  while (true) {
    char currChar =
        (pCsvParser->fromString) ? pCsvParser->pCsvString[pCsvParser->csvStringIter] : fgetc(pCsvParser->pFileHandler);
    pCsvParser->csvStringIter++;
    int endOfFileIndicator;
    if (pCsvParser->fromString) {
      endOfFileIndicator = (currChar == '\0');
    } else {
      endOfFileIndicator = feof(pCsvParser->pFileHandler);
    }
    if (endOfFileIndicator) {
      if (currFieldCharIter == 0 && fieldIter == 0) {
        _csvParserSetErrorMessage(pCsvParser, "Reached EOF");
        free(currField);
        csvParserDestroyRow(pCsvRow);
        return NULL;
      }
      currChar = '\n';
      isEndOfFile = true;
    }
    if (currChar == '\r') {
      continue;
    }
    if (currFieldCharIter == 0 && !isLastCharQuote) {
      if (currChar == '\"') {
        insideComplexField = 1;
        isLastCharQuote = 1;
        continue;
      }
    } else if (currChar == '\"') {
      seriesOfQuotesLength++;
      insideComplexField = (seriesOfQuotesLength % 2 == 0);
      if (insideComplexField) {
        currFieldCharIter--;
      }
    } else {
      seriesOfQuotesLength = 0;
    }
    if (isEndOfFile || ((currChar == pCsvParser->delimiter || currChar == '\n') && !insideComplexField)) {
      currField[isLastCharQuote ? currFieldCharIter - 1 : currFieldCharIter] = 0;
      pCsvRow->ppFields[fieldIter] = (char *)malloc(currFieldCharIter + 1);
      strcpy(pCsvRow->ppFields[fieldIter], currField);
      free(currField);
      pCsvRow->fields++;
      if (currChar == '\n') {
        return pCsvRow;
      }
      if (pCsvRow->fields != 0 && pCsvRow->fields % acceptedFields == 0) {
        pCsvRow->ppFields =
            (char **)realloc(pCsvRow->ppFields, ((numRowRealloc + 2) * acceptedFields) * sizeof(char *));
        numRowRealloc++;
      }
      acceptedCharsInField = 64;
      currField = (char *)malloc(acceptedCharsInField);
      currFieldCharIter = 0;
      fieldIter++;
      insideComplexField = 0;
    } else {
      currField[currFieldCharIter] = currChar;
      currFieldCharIter++;
      if (currFieldCharIter == acceptedCharsInField - 1) {
        acceptedCharsInField *= 2;
        currField = (char *)realloc(currField, acceptedCharsInField);
      }
    }
    isLastCharQuote = (currChar == '\"');
  }
}

/*--------------------------------------------------------------------------------------------------------------------*/

int _csvParserDelimiterIsAccepted(const char *pDelimiter) {
  char delimiter;

  delimiter = *pDelimiter;
  if (delimiter == '\n' || delimiter == '\r' || delimiter == '\0' || delimiter == '\"') {
    return false;
  }
  return true;
}

/*--------------------------------------------------------------------------------------------------------------------*/

void _csvParserSetErrorMessage(CsvParser *pCsvParser, const char *pErrorMessage) {
  if (pCsvParser->pErrorMessage != NULL) {
    free(pCsvParser->pErrorMessage);
  }
  pCsvParser->pErrorMessage = strdup(pErrorMessage);
}

/*--------------------------------------------------------------------------------------------------------------------*/

const char *csvParserGetErrorMessage(CsvParser *pCsvParser) {
  return pCsvParser->pErrorMessage;
}

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
