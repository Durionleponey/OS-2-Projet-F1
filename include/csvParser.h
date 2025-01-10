#ifndef CSVPARSER_H
#define CSVPARSER_H

#include <stdio.h>
#include <stdbool.h>

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CsvRow {
  char **ppFields;
  int fields;
} CsvRow;

typedef struct CsvParser {
  char *pFilePath;
  char delimiter;
  bool isFirstLineHeader;
  char *pErrorMessage;
  CsvRow *pHeader;
  FILE *pFileHandler;
  int fromString;
  char *pCsvString;
  int csvStringIter;
} CsvParser;

/*--------------------------------------------------------------------------------------------------------------------*/

extern CsvParser *csvParserCreate(const char *pFilePath, const char *pDelimiter, bool isFirstLineHeader);
extern CsvParser *csvParserCreateFromString(const char *pCsvString, const char *pDelimiter, bool isFirstLineHeader);
extern void csvParserDestroy(CsvParser *pCsvParser);
extern void csvParserDestroyRow(const CsvRow *pCsvRow);
extern CsvRow *csvParserGetHeader(CsvParser *pCsvParser);
extern CsvRow *csvParserGetRow(CsvParser *pCsvParser);
extern int csvParserGetNumFields(const CsvRow *pCsvRow);
extern const char **csvParserGetFields(const CsvRow *pCsvRow);
extern const char *csvParserGetErrorMessage(CsvParser *pCsvParser);

/*--------------------------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif
