#ifndef TABLES_H
#define TABLES_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdio.h>
    
void printTable(FILE* stream, const char* title, char** headers, int nHeaders, char*** rows, int nRows);

#ifdef __cplusplus
}
#endif

#endif /* TABLES_H */

