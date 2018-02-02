#ifndef SAMPLES_H
#define SAMPLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "circuit.h"
    
typedef struct {
    int index;
    int** data;
    int nSamplePoints;
} Samples;

Samples* createSamplesFromFile(AssertionsSet* set, const char* filename);
void freeSamples(Samples* samples);
int samplesNext(Samples* samples);
void samplesGetValues(Wiring* wiring, Samples* samples, int* dest);
    
#ifdef __cplusplus
}
#endif

#endif /* SAMPLES_H */