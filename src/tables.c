#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"

#define ROW_DELIMITER '-'
#define COLUMN_DELIMITER '|'
#define WHITE_SPACE ' '
#define NEWLINE '\n'
#define CELL_PADDING 2

void printN(FILE* stream, char fill, int n) {
    int i;
    for(i = 0; i < n; i++) {
        fputc(fill, stream);
    }
}

void printCell(FILE* stream, const char* contents, int contentsLength, int cellWidth, char fillCharacter) {
    int right, left, len;
    if(contentsLength < 0) {
        len = strlen(contents);
    } else {
        len = contentsLength;
    }
    left = cellWidth - len;
    right = (int) floor(left / 2);
    left = left - right;
    printN(stream, fillCharacter, left);
    fputs(contents, stream);
    printN(stream, fillCharacter, right);
}

void printTable(FILE* stream, const char* title, char** headers, int nHeaders, char*** rows, int nRows) {
    assert(title != NULL);
    assert(headers != NULL);
    assert(rows != NULL);
    int i, j, k, totalLen;
    int** lengths;
    int* maxLengths;
    
    assert((lengths = malloc(sizeof(int*) * nHeaders)) != NULL);
    assert((maxLengths = malloc(sizeof(int) * nHeaders)) != NULL);
    totalLen = 1;
    for(i = 0; i < nHeaders; i++) {
        assert((lengths[i] = malloc(sizeof(int) * (nRows + 1))) != NULL);
        k = strlen(headers[i]);
        lengths[i][0] = k;
        maxLengths[i] = k;
        for(j = 0; j < nRows; j++) {
            k = strlen(rows[j][i]);
            lengths[i][j + 1] = k;
            if(k > maxLengths[i]) {
                maxLengths[i] = k;
            }
        }
        maxLengths[i] += CELL_PADDING;
        totalLen += maxLengths[i] + 1;
    }
    
    // Title
    printCell(stream, title, -1, totalLen, ROW_DELIMITER);
    fputc(NEWLINE, stream);
    
    // Headers
    fputc(COLUMN_DELIMITER, stream);
    for(i = 0; i < nHeaders; i++) {
        printCell(stream, headers[i], lengths[i][0], maxLengths[i], WHITE_SPACE);
        fputc(COLUMN_DELIMITER, stream);
    }
    fputc(NEWLINE, stream);
    
    // Rule
    printN(stream, ROW_DELIMITER, totalLen);
    fputc(NEWLINE, stream);
    
    for(j = 0; j < nRows; j++) {
        // Row i
        fputc(COLUMN_DELIMITER, stream);
        for(i = 0; i < nHeaders; i++) {
            printCell(stream, rows[j][i], lengths[i][j + 1], maxLengths[i], WHITE_SPACE);
            fputc(COLUMN_DELIMITER, stream);
        }
        fputc(NEWLINE, stream);
    }
    
    // Rule
    printN(stream, ROW_DELIMITER, totalLen);
    fputc(NEWLINE, stream);
    
    for(i = 0; i < nHeaders; i++) {
        free(lengths[i]);
    }
    free(lengths);
    free(maxLengths);
}