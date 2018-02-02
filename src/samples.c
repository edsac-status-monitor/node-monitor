#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assertions.h"
#include "samples.h"
#include "circuit.h"

void skipWhiteSpace(FILE* file, int* nextChar) {
    for(;;) {
        if(!isblank(*nextChar)) {
            break;
        }
        *nextChar = fgetc(file);
    }
}

int isNewline(int c) {
    return c == '\n' || c == '\r';
}

void readNewline(FILE* file, int* nextChar) {
    for(;;) {
        if(!isNewline(*nextChar)) {
            break;
        }
        *nextChar = fgetc(file);
    }
}

char* readWord(FILE* file, int* nextChar) {
    char* dst;
    int i, len;
    
    len = 32;
    assert((dst = malloc(sizeof(char) * len)) != NULL);
    i = 0;
    for(;;) {
        if(!(isalnum(*nextChar) || *nextChar == '_' || *nextChar == '-')) {
            *(dst + i) = '\0';
            break;
        }
        if(i >= len - 1) {
            len = len + 1;
            assert((dst = realloc(dst, sizeof(char) * len)) != NULL);
        }
        *(dst + i) = *nextChar;
        i++;
        *nextChar = fgetc(file);
    }
    return dst;
}

int readBit(FILE* file, int* nextChar) {
    if(*nextChar == '0' ) {
        *nextChar = fgetc(file);
        return 0;
    } else if(*nextChar == '1' ) {
        *nextChar = fgetc(file);
        return 1;
    } else {
        return -1;
    }
}

void freeSamples(Samples* samples) {
    int i, j;
    
    assert(samples != NULL);
    
    for(i = 0; i < samples->nSamplePoints; i++) {
        free(samples->data[i]);
    }
    free(samples->data);
    free(samples);
}

Samples* createSamplesFromFile(AssertionsSet* set, const char* filename) {
    Samples* samples = NULL;
    FILE * file;
    int error;
    int i, j, c, line, index;
    int* indices;
    char* name;
    
    assert(set != NULL);
    
    file = fopen(filename, "r");
    if(file) {
        error = 0;
        assert((indices = malloc(sizeof(int) * set->nTp)) != NULL);
        
        c = fgetc(file);
        for(i = 0; i < set->nTp; i++) {
            skipWhiteSpace(file, &c);
            name = readWord(file, &c);
            skipWhiteSpace(file, &c);
            if(strlen(name) == 0) {
                fprintf(stderr, "Name %d is empty in file \"%s\"\n", i, filename);
                error = 1;
                break;
            }
            index = getIndexOfTPByName(set, name);
            if(index < 0) {
                fprintf(stderr, "Name %d (\"%s\") is not a valid test point in file \"%s\"\n", i, name, filename);
                free(name);
                error = 1;
                break;
            }
            for(j = 0; j < i; j++) {
                if(indices[j] == index) {
                    fprintf(stderr, "Name %d (\"%s\") is repeated at name %d in file \"%s\"\n", j, i, name, filename);
                    free(name);
                    error = 1;
                    break;
                }
            }
            if(error) {
                break;
            }
            indices[i] = index;
            if(c != ',') {
                fprintf(stderr, "Name %d (\"%s\") is not followed by a comma in file \"%s\"\n", i, name, filename);
                error = 1;
                free(name);
                break;
            }
            c = fgetc(file);
            if(i + 1 == set->nTp) {
                if(isNewline(c)) {
                    readNewline(file, &c);
                } else if(c != EOF) {
                    fprintf(stderr, "This circuit only contains %d test points and name %d (\"%s\") is not followed by newline in file \"%s\"\n",
                            set->nTp, i, name, filename);
                    error = 1;
                    free(name);
                    break;
                }
            } else {
                if(isNewline(c) || c == EOF) {
                    fprintf(stderr, "This circuit contains %d test points but only %d names are specified in file \"%s\"\n",
                            set->nTp, i + 1, filename);
                    error = 1;
                    free(name);
                    break;
                }
            }
            free(name);
        }
        
        if(!error) {
            assert((samples = malloc(sizeof(Samples))) != NULL);
            samples->index = 0;
            samples->data = NULL;
            samples->nSamplePoints = 0;
            
            for(line = 1; !error; line++) {
                if(c == EOF) {
                    break;
                }
                samples->nSamplePoints++;
                assert((samples->data = realloc(samples->data, sizeof(int*) * samples->nSamplePoints)) != NULL);
                assert((samples->data[samples->nSamplePoints - 1] = malloc(sizeof(int) * set->nTp)) != NULL);
                for(i = 0; i < set->nTp; i++) {
                    skipWhiteSpace(file, &c);
                    j = readBit(file, &c);
                    if(j < 0) {
                        fprintf(stderr, "\"%c\" is not a valid bit in file \"%s\", line %d\n", c, filename, line);
                        error = 1;
                        break;
                    }
                    samples->data[samples->nSamplePoints - 1][indices[i]] = j;
                    skipWhiteSpace(file, &c);
                    if(c != ',') {
                        fprintf(stderr, "Comma missing in file \"%s\", line %d\n", filename, line);
                        error = 1;
                        break;
                    }
                    c = fgetc(file);
                    if(i + 1 == set->nTp) {
                        if(isNewline(c)) {
                            readNewline(file, &c);
                        } else if(c != EOF) {
                            fprintf(stderr, "There are too many columns in file \"%s\", line %d\n", filename, line);
                            error = 1;
                            break;
                        }
                    } else {
                        if(isNewline(c) || c == EOF) {
                            fprintf(stderr, "There are too few columns in file \"%s\", line %d\n", filename, line);
                            error = 1;
                            break;
                        }
                    }
                }
            }
            if(error) {
                for(i = 0; i < samples->nSamplePoints; i++) {
                    free(samples->data[i]);
                }
                free(samples->data);
                free(samples);
                samples = NULL;
            }
        }
        fclose(file);
        
        free(indices);
    } else {
        fprintf(stderr, "Could not open file \"%s\" for reading\n", filename);
    }
    
    return samples;
}

int samplesNext(Samples* samples) {
    assert(samples != NULL);
    
    samples->index++;
    if(samples->index >= samples->nSamplePoints) {
        return 0;
    }
    return 1;
}

void samplesGetValues(Wiring* wiring, Samples* samples, int* dest) {
    assert(samples != NULL);
    assert(samples->index < samples->nSamplePoints);
    memcpy(dest, samples->data[samples->index], sizeof(int) * wiring->nWires);
}